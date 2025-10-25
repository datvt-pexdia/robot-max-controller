import { AnyTask } from './models';

export function sampleWheelsDrive(durationMs = 1500, left = 60, right = 60): AnyTask {
  return {
    taskId: `sample-${Date.now()}-${Math.random().toString(36).slice(2)}`,
    device: 'wheels',
    type: 'drive',
    left,
    right,
    durationMs
  };
}

export function sampleArmMove(angle = 120): AnyTask {
  return {
    taskId: `sample-${Date.now()}-${Math.random().toString(36).slice(2)}`,
    device: 'arm',
    type: 'moveAngle',
    angle
  };
}

export function sampleNeckMove(angle = 80): AnyTask {
  return {
    taskId: `sample-${Date.now()}-${Math.random().toString(36).slice(2)}`,
    device: 'neck',
    type: 'moveAngle',
    angle
  };
}

export function exampleTaskBatch(): AnyTask[] {
  return [sampleWheelsDrive(), sampleArmMove(), sampleNeckMove()];
}
