import http from 'http';
import WebSocket, { WebSocketServer } from 'ws';
import {
  HTTP_PORT,
  WS_PATH,
  WS_HEARTBEAT_MS,
  WS_LIVENESS_GRACE_MS,
  WS_MAX_PAYLOAD,
  WS_PERMESSAGE_DEFLATE,
} from './config';
import { espLog, taskLog, wsLog } from './logger';
import {
  AnyTask,
  DeviceId,
  InboundEnvelope,
  OutboundEnvelope,
  ServerStatus,
} from './models';
import {
  applyEnqueueTasks,
  applyReplaceTasks,
  cancelDevice,
  serializeManagers,
} from './taskQueue';

/**
 * Tùy chỉnh nội bộ (fallback nếu bạn chưa có trong config)
 * - BUFFER_MAX: tối đa số envelope giữ tạm khi ESP offline
 * - REPLACE_DEBOUNCE_MS: gom các task.replace dồn dập trong khoảng thời gian ngắn
 */
const BUFFER_MAX = Number(process.env.WS_BUFFER_MAX ?? 500);
const REPLACE_DEBOUNCE_MS = Number(process.env.WS_REPLACE_DEBOUNCE_MS ?? 80);

export class WsHub {
  private readonly wss: WebSocketServer;
  private espSocket?: WebSocket;
  private heartbeatTimer?: NodeJS.Timeout;
  private lastPong = 0;

  // Buffer gửi khi ESP offline
  private buffer: OutboundEnvelope[] = [];

  // Lưu hello gần nhất (ISO string)
  private lastHello?: string;

  // Debounce cho replace
  private replaceBuffer: AnyTask[] = [];
  private replaceTimer?: NodeJS.Timeout;

  // Callback để xử lý browser connections (UI)
  private browserConnectionHandler?: (socket: WebSocket) => void;

  constructor(server: http.Server) {
    this.wss = new WebSocketServer({
      server,
      path: WS_PATH,
      perMessageDeflate: WS_PERMESSAGE_DEFLATE,
      maxPayload: WS_MAX_PAYLOAD,
      noServer: false,
      clientTracking: true,
    });
    this.wss.on('connection', (socket, req) => this.handleConnection(socket, req));
    wsLog(`WebSocket hub ready on ws://0.0.0.0:${HTTP_PORT}${WS_PATH}`);
  }

  /**
   * Đăng ký handler cho browser connections
   */
  setBrowserConnectionHandler(handler: (socket: WebSocket) => void): void {
    this.browserConnectionHandler = handler;
  }

  // ======================
  // Public API
  // ======================

  /**
   * Debounce replace: gom các lệnh replace trong REPLACE_DEBOUNCE_MS,
   * chỉ gửi "last-by-device" để tránh spam.
   */
  sendReplaceTasks(tasks: AnyTask[]): void {
    if (!tasks?.length) return;
    // Gom vào buffer đợi debounce
    this.replaceBuffer.push(...tasks);
    if (this.replaceTimer) return;

    this.replaceTimer = setTimeout(() => {
      this.replaceTimer = undefined;

      // Lấy task cuối theo mỗi device
      const lastByDevice = new Map<DeviceId, AnyTask>();
      for (const t of this.replaceBuffer) lastByDevice.set(t.device, t);
      const coalesced = Array.from(lastByDevice.values());
      this.replaceBuffer = [];

      // Áp dụng vào queue trước khi gửi để giữ đồng bộ trạng thái server
      applyReplaceTasks(coalesced);

      const envelope: OutboundEnvelope = { kind: 'task.replace', tasks: coalesced };
      this.enqueueEnvelope(
        envelope,
        `[WS->ESP] task.replace (debounced ${coalesced.length} task${coalesced.length === 1 ? '' : 's'})`
      );
    }, REPLACE_DEBOUNCE_MS);
  }

  sendEnqueueTasks(tasks: AnyTask[]): void {
    applyEnqueueTasks(tasks);
    if (!tasks?.length) return;
    const envelope: OutboundEnvelope = { kind: 'task.enqueue', tasks };
    this.enqueueEnvelope(
      envelope,
      `[WS->ESP] task.enqueue (${tasks.length} task${tasks.length === 1 ? '' : 's'})`
    );
  }

  sendCancel(device: DeviceId): void {
    cancelDevice(device);
    const envelope: OutboundEnvelope = { kind: 'task.cancel', device };
    this.enqueueEnvelope(envelope, `[WS->ESP] task.cancel (${device})`);
  }

  getStatus(): ServerStatus {
    const managers = serializeManagers();
    return {
      connected: !!this.espSocket && this.espSocket.readyState === WebSocket.OPEN,
      lastHello: this.lastHello,
      devices: {
        arm: { ...managers.arm, lastUpdated: managers.arm.lastUpdated },
        neck: { ...managers.neck, lastUpdated: managers.neck.lastUpdated },
        wheels: { ...managers.wheels, lastUpdated: managers.wheels.lastUpdated },
      },
      queueSizes: {
        arm: managers.arm.queueSize,
        neck: managers.neck.queueSize,
        wheels: managers.wheels.queueSize,
      },
    };
  }

  // ======================
  // Internal helpers
  // ======================

  private enqueueEnvelope(envelope: OutboundEnvelope, logMessage: string): void {
    taskLog(logMessage);
    if (this.espSocket && this.espSocket.readyState === WebSocket.OPEN) {
      this.sendEnvelope(envelope);
      return;
    }

    // ESP offline -> buffer
    if (this.buffer.length >= BUFFER_MAX) {
      // drop oldest để tránh phình bộ nhớ
      const dropped = this.buffer.shift();
      wsLog('Buffer full — dropping oldest message', dropped?.kind);
    }
    wsLog('ESP offline, buffering message', envelope.kind);
    this.buffer.push(envelope);
  }

  private handleConnection(socket: WebSocket, req: http.IncomingMessage): void {
    // Phân biệt browser vs ESP dựa vào User-Agent
    const userAgent = req.headers['user-agent'] || '';
    const isBrowser = userAgent.includes('Mozilla') || userAgent.includes('Chrome') || userAgent.includes('Safari');
    
    wsLog(`Connection from ${req.socket.remoteAddress}, User-Agent: ${userAgent.substring(0, 50)}...`);
    wsLog(`Detected as: ${isBrowser ? 'BROWSER (UI)' : 'ESP (Device)'}`);

    // Nếu là browser và có handler, delegate cho handler đó
    if (isBrowser && this.browserConnectionHandler) {
      wsLog('Delegating to browser connection handler');
      this.browserConnectionHandler(socket);
      return;
    }

    // Xử lý ESP connection (logic cũ)
    // Chỉ cho phép 1 ESP kết nối
    if (this.espSocket && this.espSocket.readyState === WebSocket.OPEN) {
      wsLog('Rejecting additional ESP connection from', req.socket.remoteAddress);
      socket.close(1000, 'Only one ESP client supported');
      return;
    }

    this.espSocket = socket;
    wsLog('ESP connected from', req.socket.remoteAddress);

    // IMPORTANT: enable TCP_NODELAY on underlying socket for low-latency
    try {
      (socket as any)._socket?.setNoDelay?.(true);
    } catch {}

    // Đánh dấu đã nhận "pong" ngay lúc connect để tránh timeout sớm
    this.lastPong = Date.now();

    // Listen protocol-level pings/pongs (client-side heartbeat của firmware/WS lib)
    socket.on('pong', () => {
      this.lastPong = Date.now();
    });

    // (Optional) Nếu client gửi ping (protocol-level), có thể log:
    socket.on('ping', () => {
      // ws lib sẽ tự động reply pong; không cần làm gì thêm
    });

    socket.on('message', (data) => this.handleMessage(data));
    socket.on('close', (code, reason) => this.handleClose(code, reason.toString()));
    socket.on('error', (err) => wsLog('Socket error', err));

    this.startHeartbeat();
    // Gửi hello (application-level) — giữ tương thích với firmware
    this.sendEnvelope({ kind: 'hello', serverTime: Date.now() });
    this.flushBuffer();
  }

  private handleMessage(data: WebSocket.RawData): void {
    try {
      const text = data.toString();
      const payload = JSON.parse(text) as InboundEnvelope;
      this.routeInbound(payload);
    } catch (err) {
      wsLog('Failed to parse inbound message', err);
    }
  }

  private routeInbound(message: InboundEnvelope): void {
    switch (message.kind) {
      case 'hello':
        this.lastHello = new Date().toISOString();
        wsLog(`ESP hello id=${message.espId} fw=${message.fw}`);
        break;

      case 'ack':
        espLog(`ack taskId=${message.taskId}`);
        break;

      case 'progress':
        espLog(
          `progress taskId=${message.taskId} pct=${message.pct} note=${message.note ?? ''}`
        );
        break;

      case 'done':
        espLog(`done taskId=${message.taskId}`);
        break;

      case 'error':
        espLog(`error taskId=${message.taskId ?? 'n/a'} message=${message.message}`);
        break;

      case 'pong':
        // application-level pong (giữ tương thích nếu firmware gửi JSON pong)
        this.lastPong = Date.now();
        break;

      default:
        wsLog('Unknown message from ESP', message);
    }
  }

  private handleClose(code: number, reason: string): void {
    wsLog(`ESP disconnected code=${code} reason=${reason}`);
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = undefined;
    }
    this.espSocket = undefined;
  }

  /**
   * Heartbeat:
   * - Gửi protocol-level ping (socket.ping()) mỗi WS_HEARTBEAT_MS (15s).
   * - Nếu quá WS_LIVENESS_GRACE_MS (30s) không thấy pong, terminate.
   */
  private startHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
    }
    this.heartbeatTimer = setInterval(() => {
      if (!this.espSocket || this.espSocket.readyState !== WebSocket.OPEN) {
        return;
      }
      const now = Date.now();
      
      // Liveness check: if no pong within grace period, terminate
      if (now - this.lastPong > WS_LIVENESS_GRACE_MS) {
        wsLog('[WS] no pong within grace; terminating socket');
        try {
          this.espSocket.terminate();
        } catch {}
        this.espSocket = undefined;
        return;
      }

      // Protocol-level ping (browsers auto-pong)
      try {
        this.espSocket.ping();
      } catch (e) {
        wsLog(`[WS] ping error: ${(e as Error).message}`);
      }
    }, WS_HEARTBEAT_MS);
  }

  private sendEnvelope(envelope: OutboundEnvelope): void {
    if (!this.espSocket || this.espSocket.readyState !== WebSocket.OPEN) {
      // ESP vừa drop trước khi gửi -> buffer lại
      if (this.buffer.length >= BUFFER_MAX) {
        const dropped = this.buffer.shift();
        wsLog('Buffer full — dropping oldest message', dropped?.kind);
      }
      this.buffer.push(envelope);
      return;
    }
    try {
      const payload = JSON.stringify(envelope);
      this.espSocket.send(payload);
    } catch (e) {
      wsLog('Failed to send envelope, buffering', e);
      if (this.buffer.length >= BUFFER_MAX) {
        const dropped = this.buffer.shift();
        wsLog('Buffer full — dropping oldest message', dropped?.kind);
      }
      this.buffer.push(envelope);
    }
  }

  private flushBuffer(): void {
    if (!this.espSocket || this.espSocket.readyState !== WebSocket.OPEN) {
      return;
    }
    if (this.buffer.length > 0) {
      wsLog(`Flushing ${this.buffer.length} buffered message(s)`);
    }
    while (this.buffer.length > 0) {
      const envelope = this.buffer.shift();
      if (!envelope) continue;
      this.sendEnvelope(envelope);
      // Không delay ở đây; rely vào TCP backpressure
    }
  }
}
