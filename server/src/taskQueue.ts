import { LOCAL_SIMULATION } from './config';
import { taskLog } from './logger';
import { AnyTask, DeviceId } from './models';

type SerializedState = {
  runningTaskId?: string;
  queueSize: number;
  lastUpdated: number | null;
};

function describeTask(task: AnyTask): string {
  switch (task.device) {
    case 'arm':
      return `${task.device}: moveAngle ${task.angle}° (taskId=${task.taskId})`;
    case 'neck':
      return `${task.device}: moveAngle ${task.angle}° (taskId=${task.taskId})`;
    case 'wheels': {
      const duration = task.durationMs ?? 1000;
      return `${task.device}: drive L=${task.left} R=${task.right} ${duration}ms (taskId=${task.taskId})`;
    }
  }
}

class DeviceTaskManager {
  private runningTask?: AnyTask;
  private queue: AnyTask[] = [];
  private lastUpdated: number | null = null;

  constructor(public readonly device: DeviceId) {}

  runReplace(task: AnyTask): void {
    this.runningTask = task;
    this.queue = [];
    this.lastUpdated = Date.now();
    taskLog(`replace ${describeTask(task)}`);
    this.simulateSend([task]);
  }

  replaceBatch(tasks: AnyTask[]): void {
    if (tasks.length === 0) return;
    this.runningTask = tasks[0];
    this.queue = tasks.slice(1);
    this.lastUpdated = Date.now();
    taskLog(`replace ${describeTask(tasks[0])}`);
    if (this.queue.length > 0) {
      taskLog(`queued ${this.queue.length} additional task(s) for ${this.device}`);
    }
    this.simulateSend(tasks);
  }

  enqueue(task: AnyTask): void {
    if (!this.runningTask) {
      this.runReplace(task);
      return;
    }
    this.queue.push(task);
    this.lastUpdated = Date.now();
    taskLog(`enqueue ${describeTask(task)}`);
    this.simulateSend([task]);
  }

  enqueueBatch(tasks: AnyTask[]): void {
    if (tasks.length === 0) return;
    if (!this.runningTask) {
      this.replaceBatch(tasks);
      return;
    }
    this.queue.push(...tasks);
    this.lastUpdated = Date.now();
    tasks.forEach((task) => taskLog(`enqueue ${describeTask(task)}`));
    this.simulateSend(tasks);
  }

  cancel(): void {
    if (this.runningTask || this.queue.length > 0) {
      taskLog(`cancel device ${this.device} (running=${this.runningTask?.taskId ?? 'none'}, queue=${this.queue.length})`);
    }
    this.runningTask = undefined;
    this.queue = [];
    this.lastUpdated = Date.now();
  }

  serializeState(): SerializedState {
    return {
      runningTaskId: this.runningTask?.taskId,
      queueSize: this.queue.length,
      lastUpdated: this.lastUpdated
    };
  }

  private simulateSend(tasks: AnyTask[]): void {
    if (!LOCAL_SIMULATION) return;
    taskLog(`[SIM] would send ${tasks.length} task(s) to ${this.device}`);
  }
}

const managers: Record<DeviceId, DeviceTaskManager> = {
  arm: new DeviceTaskManager('arm'),
  neck: new DeviceTaskManager('neck'),
  wheels: new DeviceTaskManager('wheels')
};

export function getDeviceManager(device: DeviceId): DeviceTaskManager {
  return managers[device];
}

export function getAllManagers(): Record<DeviceId, DeviceTaskManager> {
  return managers;
}

export function applyReplaceTasks(tasks: AnyTask[]): void {
  const byDevice: Partial<Record<DeviceId, AnyTask[]>> = {};
  tasks.forEach((task) => {
    if (!byDevice[task.device]) {
      byDevice[task.device] = [];
    }
    byDevice[task.device]!.push(task);
  });
  (Object.keys(byDevice) as DeviceId[]).forEach((device) => {
    managers[device].replaceBatch(byDevice[device]!);
  });
}

export function applyEnqueueTasks(tasks: AnyTask[]): void {
  const byDevice: Partial<Record<DeviceId, AnyTask[]>> = {};
  tasks.forEach((task) => {
    if (!byDevice[task.device]) {
      byDevice[task.device] = [];
    }
    byDevice[task.device]!.push(task);
  });
  (Object.keys(byDevice) as DeviceId[]).forEach((device) => {
    managers[device].enqueueBatch(byDevice[device]!);
  });
}

export function cancelDevice(device: DeviceId): void {
  managers[device].cancel();
}

export function serializeManagers(): Record<DeviceId, SerializedState> {
  return {
    arm: managers.arm.serializeState(),
    neck: managers.neck.serializeState(),
    wheels: managers.wheels.serializeState()
  };
}
