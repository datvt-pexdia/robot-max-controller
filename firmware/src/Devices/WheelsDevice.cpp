#include "WheelsDevice.h"

#include "../Config.h"

WheelsDevice::WheelsDevice()
    : running(false), hasTask(false), startMs(0), durationMs(0), lastLoggedProgress(0) {
  current.taskId = "";
}

const char* WheelsDevice::deviceName() const { return "wheels"; }

void WheelsDevice::startTask(const TaskEnvelope& task, uint32_t now) {
  current = task;
  hasTask = true;
  running = true;
  startMs = now;
  durationMs = task.durationMs > 0 ? task.durationMs : 1000;
  lastLoggedProgress = 0;
#if SIMULATION
  Serial.printf("[WHEELS] drive L=%d R=%d duration=%lu ms (taskId=%s)\n", task.left, task.right, durationMs,
                task.taskId.c_str());
#else
  // TODO: integrate with motor driver hardware
#endif
}

void WheelsDevice::tick(uint32_t now) {
  if (!running) {
    return;
  }
  const uint8_t pct = progress(now);
  if (pct >= lastLoggedProgress + 25 && pct < 100) {
#if SIMULATION
    Serial.printf("[WHEELS] progress %u%% taskId=%s\n", pct, current.taskId.c_str());
#endif
    lastLoggedProgress = pct;
  }
  if (now - startMs >= durationMs) {
    running = false;
#if SIMULATION
    Serial.printf("[WHEELS] completed taskId=%s\n", current.taskId.c_str());
#endif
  }
}

void WheelsDevice::cancel(uint32_t now) {
  if (!hasTask) {
    return;
  }
#if SIMULATION
  Serial.printf("[WHEELS] cancel taskId=%s at %lu ms\n", current.taskId.c_str(), now);
#endif
  running = false;
  hasTask = false;
  current.taskId = "";
}

void WheelsDevice::finish() {
  hasTask = false;
  current.taskId = "";
}

bool WheelsDevice::isRunning() const { return running; }

bool WheelsDevice::isCompleted(uint32_t now) const {
  (void)now;
  return hasTask && !running && current.taskId.length() > 0;
}

uint8_t WheelsDevice::progress(uint32_t now) const {
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

String WheelsDevice::currentTaskId() const { return current.taskId; }
