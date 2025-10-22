import express from 'express';
import { z } from 'zod';
import { deviceIdSchema, taskUnionSchema } from './models';
import { WsHub } from './wsHub';
import { httpLog } from './logger';

const tasksPayloadSchema = z.object({
  tasks: z.array(taskUnionSchema).min(1)
});

const cancelPayloadSchema = z.object({
  device: deviceIdSchema
});

function normalizeTasks(tasks: z.infer<typeof taskUnionSchema>[]) {
  return tasks.map((task) => ({
    ...task,
    enqueue: task.enqueue ?? false
  }));
}

export function buildRouter(wsHub: WsHub): express.Router {
  const router = express.Router();

  router.post('/robot/tasks/replace', (req, res, next) => {
    try {
      const parsed = tasksPayloadSchema.parse(req.body);
      const tasks = normalizeTasks(parsed.tasks);
      httpLog(`POST /robot/tasks/replace (${tasks.length} task${tasks.length === 1 ? '' : 's'})`);
      wsHub.sendReplaceTasks(tasks);
      res.status(202).json({ status: 'queued', mode: 'replace', count: tasks.length });
    } catch (err) {
      next(err);
    }
  });

  router.post('/robot/tasks/enqueue', (req, res, next) => {
    try {
      const parsed = tasksPayloadSchema.parse(req.body);
      const tasks = normalizeTasks(parsed.tasks).map((task) => ({ ...task, enqueue: true }));
      httpLog(`POST /robot/tasks/enqueue (${tasks.length} task${tasks.length === 1 ? '' : 's'})`);
      wsHub.sendEnqueueTasks(tasks);
      res.status(202).json({ status: 'queued', mode: 'enqueue', count: tasks.length });
    } catch (err) {
      next(err);
    }
  });

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

  router.get('/robot/status', (_req, res) => {
    const status = wsHub.getStatus();
    res.json(status);
  });

  router.use((err: unknown, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
    if (err instanceof z.ZodError) {
      res.status(400).json({ error: 'invalid_payload', details: err.errors });
      return;
    }
    httpLog('Unexpected error', err);
    res.status(500).json({ error: 'internal_error' });
  });

  return router;
}
