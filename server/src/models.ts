import { z } from 'zod';

export const deviceIdSchema = z.enum(['arm', 'neck', 'wheels']);
export type DeviceId = z.infer<typeof deviceIdSchema>;

export const taskBaseSchema = z.object({
  taskId: z.string().min(1),
  device: deviceIdSchema,
  type: z.string().min(1),
  enqueue: z.boolean().optional(),
  durationMs: z.number().int().positive().optional()
});

export type TaskBase = z.infer<typeof taskBaseSchema>;

export const armTaskSchema = taskBaseSchema.extend({
  device: z.literal('arm'),
  type: z.literal('moveAngle'),
  angle: z.number().int().min(0).max(180)
});
export type ArmTask = z.infer<typeof armTaskSchema>;

export const neckTaskSchema = taskBaseSchema.extend({
  device: z.literal('neck'),
  type: z.literal('moveAngle'),
  angle: z.number().int().min(0).max(180)
});
export type NeckTask = z.infer<typeof neckTaskSchema>;

export const wheelsTaskSchema = taskBaseSchema.extend({
  device: z.literal('wheels'),
  type: z.literal('drive'),
  left: z.number().int().min(-100).max(100),
  right: z.number().int().min(-100).max(100),
  durationMs: z.number().int().positive().max(60_000).optional()
});
export type WheelsTask = z.infer<typeof wheelsTaskSchema>;

export const taskUnionSchema = z.union([armTaskSchema, neckTaskSchema, wheelsTaskSchema]);
export type AnyTask = z.infer<typeof taskUnionSchema>;

export type OutboundEnvelope =
  | { kind: 'hello'; serverTime: number }
  | { kind: 'task.replace'; tasks: AnyTask[] }
  | { kind: 'task.enqueue'; tasks: AnyTask[] }
  | { kind: 'task.cancel'; device: DeviceId }
  | { kind: 'ping'; t: number };

export type InboundEnvelope =
  | { kind: 'hello'; espId: string; fw: string; seq?: number }
  | { kind: 'ack'; taskId: string; seq?: number }
  | { kind: 'progress'; taskId: string; pct: number; note?: string; seq?: number }
  | { kind: 'done'; taskId: string; seq?: number }
  | { kind: 'error'; taskId?: string; message: string; seq?: number }
  | { kind: 'pong'; t: number; seq?: number };

export interface DeviceStatus {
  runningTaskId?: string;
  queueSize: number;
  lastUpdated: number | null;
}

export interface ServerStatus {
  connected: boolean;
  lastHello?: string;
  devices: Record<DeviceId, DeviceStatus>;
  queueSizes: Record<DeviceId, number>;
}
