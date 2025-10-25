/**
 * Device Adapter - Converts new drive protocol to ESP firmware protocol
 * Acts as a bridge between DriveRelay and existing WsHub
 */

import { DriveIntent } from './driveRelay';
import { WsHub } from './wsHub';
import { taskLog } from './logger';

export class DeviceAdapter {
  private wsHub: WsHub;
  private lastSeq = 0;
  private lastAppliedLeft = 0;
  private lastAppliedRight = 0;

  constructor(wsHub: WsHub) {
    this.wsHub = wsHub;
  }

  /**
   * Handle drive intent from DriveRelay
   * Convert to legacy task format for ESP
   */
  handleDriveIntent(intent: DriveIntent): void {
    // Seq monotonicity check
    if (intent.seq <= this.lastSeq) {
      taskLog(`Dropping out-of-order packet: seq=${intent.seq} <= ${this.lastSeq}`);
      return;
    }
    this.lastSeq = intent.seq;

    // Convert normalized [-1..1] to percentage [-100..100]
    const leftPct = Math.round(intent.left * 100);
    const rightPct = Math.round(intent.right * 100);

    // Dedupe: skip if same as last applied
    if (leftPct === this.lastAppliedLeft && rightPct === this.lastAppliedRight) {
      return;
    }

    this.lastAppliedLeft = leftPct;
    this.lastAppliedRight = rightPct;

    // Convert to legacy task format
    const task = {
      device: 'wheels' as const,
      type: 'drive' as const,
      left: leftPct,
      right: rightPct,
      taskId: `drive-seq${intent.seq}`,
      durationMs: 0, // Continuous until next command
    };

    // Send via legacy replace (clears queue, applies immediately)
    this.wsHub.sendReplaceTasks([task]);
  }

  /**
   * Reset state (called on disconnect)
   */
  reset(): void {
    this.lastSeq = 0;
    this.lastAppliedLeft = 0;
    this.lastAppliedRight = 0;
  }
}

