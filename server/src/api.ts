// server/src/api.ts
import express from 'express';
import { z } from 'zod';
import { AnyTask, deviceIdSchema, taskUnionSchema } from './models';
import { WsHub } from './wsHub';
import { httpLog } from './logger';

// ---- Validation schemas ----
const tasksPayloadSchema = z.object({
  tasks: z.array(taskUnionSchema).min(1),
});

const cancelPayloadSchema = z.object({
  device: deviceIdSchema,
});

// ---- Helpers ----
function normalizeTasks(tasks: z.infer<typeof taskUnionSchema>[]) {
  // Ensure enqueue flag always present (default false)
  return tasks.map((task) => ({
    ...task,
    enqueue: task.enqueue ?? false,
  }));
}

function clampLR(t: AnyTask): AnyTask {
  // Clamp wheel percentages if present
  if (t.device === 'wheels') {
    return {
      ...t,
      left: Math.max(-100, Math.min(100, t.left)),
      right: Math.max(-100, Math.min(100, t.right)),
    };
  }
  return t;
}

// Make every taskId unique to avoid log confusion when replacing rapidly
function withUniqueIds(
  tasks: AnyTask[],
  mode: 'replace' | 'enqueue'
): AnyTask[] {
  const ts = Date.now();
  let seq = 0;
  return tasks.map((t) => {
    const base =
      (t.taskId && String(t.taskId).trim()) ||
      `${t.device || 'device'}-${t.type || 'task'}`;
    return {
      ...t,
      taskId: `${base}-v${ts}-${++seq}-${mode}`,
    } as AnyTask;
  });
}

export function buildRouter(wsHub: WsHub): express.Router {
  const router = express.Router();

  // Replace = drop current queues per-device then push new tasks
  router.post('/robot/tasks/replace', (req, res, next) => {
    try {
      const parsed = tasksPayloadSchema.parse(req.body);
      // normalize + clamp + unique IDs
      const tasks = withUniqueIds(
        normalizeTasks(parsed.tasks).map((t) => clampLR(t)),
        'replace'
      );

      httpLog(
        `POST /robot/tasks/replace (${tasks.length} task${tasks.length === 1 ? '' : 's'})`
      );
      for (const t of tasks) {
        // Brief, structured log
        httpLog(
          `[TASK] replace ${t.device}: ${t.type}` +
            (typeof (t as any).left === 'number' && typeof (t as any).right === 'number'
              ? ` L=${(t as any).left} R=${(t as any).right}`
              : '') +
            (typeof (t as any).durationMs === 'number' ? ` ${((t as any).durationMs)}ms` : '') +
            ` (taskId=${t.taskId})`
        );
      }

      wsHub.sendReplaceTasks(tasks);
      res.status(202).json({ status: 'queued', mode: 'replace', count: tasks.length });
    } catch (err) {
      next(err);
    }
  });

  // Enqueue = append to per-device queues
  router.post('/robot/tasks/enqueue', (req, res, next) => {
    try {
      const parsed = tasksPayloadSchema.parse(req.body);
      const tasks = withUniqueIds(
        normalizeTasks(parsed.tasks)
          .map((t) => ({ ...t, enqueue: true }))
          .map((t) => clampLR(t)),
        'enqueue'
      );

      httpLog(
        `POST /robot/tasks/enqueue (${tasks.length} task${tasks.length === 1 ? '' : 's'})`
      );
      for (const t of tasks) {
        httpLog(
          `[TASK] enqueue ${t.device}: ${t.type}` +
            (typeof (t as any).left === 'number' && typeof (t as any).right === 'number'
              ? ` L=${(t as any).left} R=${(t as any).right}`
              : '') +
            (typeof (t as any).durationMs === 'number' ? ` ${((t as any).durationMs)}ms` : '') +
            ` (taskId=${t.taskId})`
        );
      }

      wsHub.sendEnqueueTasks(tasks);
      res.status(202).json({ status: 'queued', mode: 'enqueue', count: tasks.length });
    } catch (err) {
      next(err);
    }
  });

  // Cancel device's current task/queue
  router.post('/robot/tasks/cancel', (req, res, next) => {
    try {
      const parsed = cancelPayloadSchema.parse(req.body);
      httpLog(`POST /robot/tasks/cancel device=${parsed.device}`);
      wsHub.sendCancel(parsed.device);
      res.status(202).json({ status: 'canceled', device: parsed.device });
    } catch (err) {
      next(err);
    }
  });

  // Status
  router.get('/robot/status', (_req, res) => {
    const status = wsHub.getStatus();
    res.json(status);
  });

  // Error handler
  router.use(
    (
      err: unknown,
      _req: express.Request,
      res: express.Response,
      _next: express.NextFunction
    ) => {
      if (err instanceof z.ZodError) {
        res.status(400).json({ error: 'invalid_payload', details: err.errors });
        return;
      }
      httpLog('Unexpected error', err);
      res.status(500).json({ error: 'internal_error' });
    }
  );

  return router;
}
