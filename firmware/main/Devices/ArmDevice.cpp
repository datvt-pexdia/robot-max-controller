#include "ArmDevice.h"

#include "../Config.h"

ArmDevice::ArmDevice() : state_(DeviceState::IDLE), startMs(0), durationMs(0) {
  current.taskId = "";
}

const char* ArmDevice::deviceName() const { return "arm"; }

void ArmDevice::startTask(const TaskEnvelope& task, uint32_t now) {
  current = task;
  state_ = DeviceState::RUNNING;
  startMs = now;
  durationMs = task.durationMs > 0 ? task.durationMs : 800;
  // TODO: integrate with Servo hardware
}

void ArmDevice::tick(uint32_t now) {
  if (state_ != DeviceState::RUNNING) {
    return;
  }
  // Transition to COMPLETED when duration expires
  if (now - startMs >= durationMs) {
    state_ = DeviceState::COMPLETED;
  }
}

void ArmDevice::cancel(uint32_t now) {
  (void)now;
  if (state_ == DeviceState::IDLE) {
    return;
  }
  state_ = DeviceState::ERROR;
  current.taskId = "";
}

void ArmDevice::finish() {
  state_ = DeviceState::IDLE;
  current.taskId = "";
}

bool ArmDevice::isRunning() const { 
  return state_ == DeviceState::RUNNING;
}

bool ArmDevice::isCompleted(uint32_t now) const {
  (void)now;
  return state_ == DeviceState::COMPLETED && current.taskId.length() > 0;
}

uint8_t ArmDevice::progress(uint32_t now) const {
  if (state_ != DeviceState::RUNNING) {
    return (state_ == DeviceState::COMPLETED) ? 100 : 0;
  }
  const uint32_t elapsed = now - startMs;
  if (durationMs == 0) {
    return 0;
  }
  uint32_t pct = (elapsed * 100) / durationMs;
  if (pct > 100) pct = 100;
  return static_cast<uint8_t>(pct);
}

String ArmDevice::currentTaskId() const { 
  return current.taskId; 
}
