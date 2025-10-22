#include "NeckDevice.h"

#include "../Config.h"

NeckDevice::NeckDevice() : running(false), hasTask(false), startMs(0), durationMs(0) {
  current.taskId = "";
}

const char* NeckDevice::deviceName() const { return "neck"; }

void NeckDevice::startTask(const TaskEnvelope& task, uint32_t now) {
  current = task;
  hasTask = true;
  running = true;
  startMs = now;
  durationMs = task.durationMs > 0 ? task.durationMs : 600;
#if SIMULATION
  Serial.printf("[NECK] moving to %u° (taskId=%s) duration=%lu ms\n", task.angle, task.taskId.c_str(), durationMs);
#else
  // TODO: integrate with Servo hardware
#endif
}

void NeckDevice::tick(uint32_t now) {
  if (!running) {
    return;
  }
  if (now - startMs >= durationMs) {
    running = false;
#if SIMULATION
    Serial.printf("[NECK] completed taskId=%s\n", current.taskId.c_str());
#endif
  }
}

void NeckDevice::cancel(uint32_t now) {
  if (!hasTask) {
    return;
  }
#if SIMULATION
  Serial.printf("[NECK] cancel taskId=%s at %lu ms\n", current.taskId.c_str(), now);
#endif
  running = false;
  hasTask = false;
  current.taskId = "";
}

void NeckDevice::finish() {
  hasTask = false;
  current.taskId = "";
}

bool NeckDevice::isRunning() const { return running; }

bool NeckDevice::isCompleted(uint32_t now) const {
  (void)now;
  return hasTask && !running && current.taskId.length() > 0;
}

uint8_t NeckDevice::progress(uint32_t now) const {
  if (!running) {
    return hasTask ? 100 : 0;
  }
  const uint32_t elapsed = now - startMs;
  if (durationMs == 0) {
    return 0;
  }
  uint32_t pct = (elapsed * 100) / durationMs;
  if (pct > 100) pct = 100;
  return static_cast<uint8_t>(pct);
}

String NeckDevice::currentTaskId() const { return current.taskId; }
