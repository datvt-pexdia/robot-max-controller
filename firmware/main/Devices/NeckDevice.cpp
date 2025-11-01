#include "NeckDevice.h"

#include "../Config.h"

NeckDevice::NeckDevice() : state_(DeviceState::IDLE), startMs(0), durationMs(0) {
  current.taskId = "";
}

const char* NeckDevice::deviceName() const { return "neck"; }

void NeckDevice::startTask(const TaskEnvelope& task, uint32_t now) {
  current = task;
  state_ = DeviceState::RUNNING;
  startMs = now;
  durationMs = task.durationMs > 0 ? task.durationMs : 600;
  // TODO: integrate with Servo hardware
}

void NeckDevice::tick(uint32_t now) {
  if (state_ != DeviceState::RUNNING) {
    return;
  }
  // Transition to COMPLETED when duration expires
  if (now - startMs >= durationMs) {
    state_ = DeviceState::COMPLETED;
  }
}

void NeckDevice::cancel(uint32_t now) {
  (void)now;
  if (state_ == DeviceState::IDLE) {
    return;
  }
  state_ = DeviceState::ERROR;
  current.taskId = "";
}

void NeckDevice::finish() {
  state_ = DeviceState::IDLE;
  current.taskId = "";
}

bool NeckDevice::isRunning() const { 
  return state_ == DeviceState::RUNNING;
}

bool NeckDevice::isCompleted(uint32_t now) const {
  (void)now;
  return state_ == DeviceState::COMPLETED && current.taskId.length() > 0;
}

uint8_t NeckDevice::progress(uint32_t now) const {
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

String NeckDevice::currentTaskId() const { 
  return current.taskId; 
}
