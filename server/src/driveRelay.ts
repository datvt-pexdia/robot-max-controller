/**
 * Drive Relay - Server-side logic for tank drive control
 * Implements watchdog, validation, and forwarding
 */

import WebSocket from 'ws';
import { wsLog, taskLog } from './logger';

export interface DriveIntent {
  type: 'drive';
  left: number;   // [-1..1]
  right: number;  // [-1..1]
  seq: number;    // uint32
  ts: number;     // uint32 ms since UI load
}

interface HelloMessage {
  type: 'hello';
  role: 'ui' | 'device';
  version: string;
}

type ClientMessage = DriveIntent | HelloMessage;

interface Client {
  socket: WebSocket;
  role: 'ui' | 'device';
  lastSeenAt: number;
}

export class DriveRelay {
  private clients = new Map<WebSocket, Client>();
  private activeUI?: WebSocket;
  private lastUIPacketAt = 0;
  private synthSeq = 0;
  private watchdogTimer?: NodeJS.Timeout;
  private onDriveIntentCallback?: (intent: DriveIntent) => void;

  private readonly UI_TIMEOUT_MS = 300;
  private readonly WATCHDOG_INTERVAL_MS = 50;

  constructor() {
    this.startWatchdog();
  }

  /**
   * Set callback for when drive intent is received
   */
  onDriveIntent(callback: (intent: DriveIntent) => void): void {
    this.onDriveIntentCallback = callback;
  }

  handleConnection(socket: WebSocket): void {
    wsLog('New connection');

    socket.on('message', (data) => this.handleMessage(socket, data));
    socket.on('close', () => this.handleDisconnect(socket));
    socket.on('error', (err) => wsLog(`Socket error: ${err.message}`));
  }

  private handleMessage(socket: WebSocket, data: WebSocket.RawData): void {
    try {
      const text = data.toString();
      const msg = JSON.parse(text) as ClientMessage;

      if (!this.validateMessage(msg)) {
        wsLog('Invalid message shape');
        return;
      }

      if (msg.type === 'hello') {
        this.handleHello(socket, msg);
      } else if (msg.type === 'drive') {
        this.handleDriveIntent(socket, msg);
      }
    } catch (err) {
      wsLog(`Parse error: ${err}`);
    }
  }

  private validateMessage(msg: any): msg is ClientMessage {
    if (!msg || typeof msg !== 'object' || typeof msg.type !== 'string') {
      return false;
    }

    if (msg.type === 'hello') {
      return typeof msg.role === 'string' && ['ui', 'device'].includes(msg.role);
    }

    if (msg.type === 'drive') {
      return (
        typeof msg.left === 'number' &&
        typeof msg.right === 'number' &&
        typeof msg.seq === 'number' &&
        typeof msg.ts === 'number'
      );
    }

    return false;
  }

  private handleHello(socket: WebSocket, msg: HelloMessage): void {
    wsLog(`Hello from ${msg.role}, version ${msg.version}`);

    if (msg.role === 'ui') {
      // Last UI wins
      if (this.activeUI && this.activeUI !== socket) {
        this.send(this.activeUI, { type: 'controller_taken' });
        wsLog('Previous UI controller taken over');
      }
      this.activeUI = socket;
    }

    this.clients.set(socket, {
      socket,
      role: msg.role,
      lastSeenAt: Date.now(),
    });
  }

  private handleDriveIntent(socket: WebSocket, intent: DriveIntent): void {
    const client = this.clients.get(socket);
    if (!client || client.role !== 'ui') {
      wsLog('Drive intent from non-UI client, ignored');
      return;
    }

    if (socket !== this.activeUI) {
      wsLog('Drive intent from inactive UI, ignored');
      return;
    }

    // Validate and clamp
    const left = this.clamp(intent.left, -1, 1);
    const right = this.clamp(intent.right, -1, 1);

    // Check staleness
    const age = Date.now() - intent.ts;
    if (age > 150) {
      wsLog(`Stale packet: age=${age}ms`);
    }

    // Update watchdog
    this.lastUIPacketAt = Date.now();
    client.lastSeenAt = Date.now();

    // Forward to all devices
    const forwarded: DriveIntent = {
      type: 'drive',
      left,
      right,
      seq: intent.seq,
      ts: intent.ts,
    };

    this.broadcastToDevices(forwarded);
    
    // Call callback for device adapter
    if (this.onDriveIntentCallback) {
      this.onDriveIntentCallback(forwarded);
    }
    
    taskLog(`Drive: L=${left.toFixed(2)} R=${right.toFixed(2)} seq=${intent.seq}`);
  }

  private handleDisconnect(socket: WebSocket): void {
    const client = this.clients.get(socket);
    if (client) {
      wsLog(`Client disconnected: ${client.role}`);
      if (client.role === 'ui' && socket === this.activeUI) {
        this.activeUI = undefined;
        this.lastUIPacketAt = 0;
        // Send STOP to devices
        this.synthesizeStop();
      }
    }
    this.clients.delete(socket);
  }

  private startWatchdog(): void {
    this.watchdogTimer = setInterval(() => {
      const now = Date.now();
      
      // Check UI timeout
      if (this.activeUI && this.lastUIPacketAt > 0) {
        const silence = now - this.lastUIPacketAt;
        if (silence > this.UI_TIMEOUT_MS) {
          wsLog(`UI silent for ${silence}ms, synthesizing STOP`);
          this.synthesizeStop();
          this.lastUIPacketAt = now; // Reset to avoid spam
        }
      }
    }, this.WATCHDOG_INTERVAL_MS);
  }

  private synthesizeStop(): void {
    const stop: DriveIntent = {
      type: 'drive',
      left: 0,
      right: 0,
      seq: ++this.synthSeq + 1000000, // High seq to override
      ts: Date.now(),
    };
    this.broadcastToDevices(stop);
    
    // Call callback for device adapter
    if (this.onDriveIntentCallback) {
      this.onDriveIntentCallback(stop);
    }
    
    taskLog('Synthesized STOP');
  }

  private broadcastToDevices(intent: DriveIntent): void {
    for (const [socket, client] of this.clients) {
      if (client.role === 'device') {
        this.send(socket, intent);
      }
    }
  }

  private send(socket: WebSocket, msg: any): void {
    if (socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify(msg));
    }
  }

  private clamp(value: number, min: number, max: number): number {
    return Math.max(min, Math.min(max, value));
  }

  stop(): void {
    if (this.watchdogTimer) {
      clearInterval(this.watchdogTimer);
    }
  }
}

