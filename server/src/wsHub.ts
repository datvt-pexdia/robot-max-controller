import http from 'http';
import WebSocket, { WebSocketServer } from 'ws';
import { HTTP_PORT, WS_PATH, WS_PING_INTERVAL_MS, WS_PONG_TIMEOUT_MS } from './config';
import { espLog, taskLog, wsLog } from './logger';
import { AnyTask, DeviceId, InboundEnvelope, OutboundEnvelope, ServerStatus } from './models';
import { applyEnqueueTasks, applyReplaceTasks, cancelDevice, serializeManagers } from './taskQueue';

export class WsHub {
  private readonly wss: WebSocketServer;
  private espSocket?: WebSocket;
  private heartbeatTimer?: NodeJS.Timeout;
  private lastPong = 0;
  private buffer: OutboundEnvelope[] = [];
  private lastHello?: string;

  constructor(server: http.Server) {
    this.wss = new WebSocketServer({ server, path: WS_PATH });
    this.wss.on('connection', (socket, req) => this.handleConnection(socket, req));
    wsLog(`WebSocket hub ready on ws://0.0.0.0:${HTTP_PORT}${WS_PATH}`);
  }

  sendReplaceTasks(tasks: AnyTask[]): void {
    applyReplaceTasks(tasks);
    if (tasks.length === 0) return;
    const envelope: OutboundEnvelope = { kind: 'task.replace', tasks };
    this.enqueueEnvelope(envelope, `[WS->ESP] task.replace (${tasks.length} task${tasks.length === 1 ? '' : 's'})`);
  }

  sendEnqueueTasks(tasks: AnyTask[]): void {
    applyEnqueueTasks(tasks);
    if (tasks.length === 0) return;
    const envelope: OutboundEnvelope = { kind: 'task.enqueue', tasks };
    this.enqueueEnvelope(envelope, `[WS->ESP] task.enqueue (${tasks.length} task${tasks.length === 1 ? '' : 's'})`);
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
        wheels: { ...managers.wheels, lastUpdated: managers.wheels.lastUpdated }
      },
      queueSizes: {
        arm: managers.arm.queueSize,
        neck: managers.neck.queueSize,
        wheels: managers.wheels.queueSize
      }
    };
  }

  private enqueueEnvelope(envelope: OutboundEnvelope, logMessage: string): void {
    taskLog(logMessage);
    if (this.espSocket && this.espSocket.readyState === WebSocket.OPEN) {
      this.sendEnvelope(envelope);
    } else {
      wsLog('ESP offline, buffering message', envelope.kind);
      this.buffer.push(envelope);
    }
  }

  private handleConnection(socket: WebSocket, req: http.IncomingMessage): void {
    if (this.espSocket && this.espSocket.readyState === WebSocket.OPEN) {
      wsLog('Rejecting additional ESP connection from', req.socket.remoteAddress);
      socket.close(1000, 'Only one ESP client supported');
      return;
    }
    this.espSocket = socket;
    wsLog('ESP connected from', req.socket.remoteAddress);
    this.lastPong = Date.now();
    socket.on('message', (data) => this.handleMessage(data));
    socket.on('close', (code, reason) => this.handleClose(code, reason.toString()));
    socket.on('error', (err) => wsLog('Socket error', err));
    this.startHeartbeat();
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
        espLog(`progress taskId=${message.taskId} pct=${message.pct} note=${message.note ?? ''}`);
        break;
      case 'done':
        espLog(`done taskId=${message.taskId}`);
        break;
      case 'error':
        espLog(`error taskId=${message.taskId ?? 'n/a'} message=${message.message}`);
        break;
      case 'pong':
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

  private startHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
    }
    this.heartbeatTimer = setInterval(() => {
      if (!this.espSocket || this.espSocket.readyState !== WebSocket.OPEN) {
        return;
      }
      const now = Date.now();
      if (now - this.lastPong > WS_PONG_TIMEOUT_MS) {
        wsLog('Pong timeout, terminating connection');
        this.espSocket.terminate();
        return;
      }
      const envelope: OutboundEnvelope = { kind: 'ping', t: now };
      this.sendEnvelope(envelope);
    }, WS_PING_INTERVAL_MS);
  }

  private sendEnvelope(envelope: OutboundEnvelope): void {
    if (!this.espSocket || this.espSocket.readyState !== WebSocket.OPEN) {
      this.buffer.push(envelope);
      return;
    }
    const payload = JSON.stringify(envelope);
    this.espSocket.send(payload);
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
      if (envelope) {
        this.sendEnvelope(envelope);
      }
    }
  }
}
