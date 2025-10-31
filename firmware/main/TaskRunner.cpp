// firmware/src/TaskRunner.cpp
#include "TaskRunner.h"
#include "Config.h"

void TaskRunner::begin() {
  wheels_.begin();
  lastWheelsTick_ = millis();
}

void TaskRunner::loop() {
  const uint32_t now = millis();
  if (now - lastWheelsTick_ >= WHEELS_TICK_MS) {
    wheels_.tick();
    lastWheelsTick_ = now;
  }
}

void TaskRunner::handleDriveTask(int8_t leftPct, int8_t rightPct, uint32_t durationMs) {
  wheels_.setTarget(leftPct, rightPct, durationMs);
}

void TaskRunner::onDisconnected() {
  wheels_.emergencyStop();
}
