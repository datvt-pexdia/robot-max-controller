import { AnyTask } from './models';
import { v4 as uuid } from 'uuid';

export function sampleWheelsDrive(durationMs = 1500, left = 60, right = 60): AnyTask {
  return {
    taskId: uuid(),
    device: 'wheels',
    type: 'drive',
    left,
    right,
    durationMs
  };
}

export function sampleArmMove(angle = 120): AnyTask {
  return {
    taskId: uuid(),
    device: 'arm',
    type: 'moveAngle',
    angle
  };
}

export function sampleNeckMove(angle = 80): AnyTask {
  return {
    taskId: uuid(),
    device: 'neck',
    type: 'moveAngle',
    angle
  };
}

export function exampleTaskBatch(): AnyTask[] {
  return [sampleWheelsDrive(), sampleArmMove(), sampleNeckMove()];
}
