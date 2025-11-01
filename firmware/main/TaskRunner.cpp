// firmware/src/TaskRunner.cpp
#include "TaskRunner.h"
#include "Config.h"

void TaskRunner::begin() {
  wheels_.begin();
  lastWheelsTick_ = millis();
  lastDriveProcessMs_ = millis();
}

void TaskRunner::loop() {
  const uint32_t now = millis();
  if (now - lastWheelsTick_ >= WHEELS_TICK_MS) {
    wheels_.tick();
    lastWheelsTick_ = now;
  }
  
  // Process pending drive commands every ~33ms (coalesce spam)
  processPendingDrive();
}

void TaskRunner::handleDriveTask(int8_t leftPct, int8_t rightPct, uint32_t durationMs) {
  // Queue the latest drive command (overwrite previous if not yet processed)
  pendingDrive_.left = leftPct;
  pendingDrive_.right = rightPct;
  pendingDrive_.durationMs = durationMs;
  pendingDrive_.hasPending = true;
}

void TaskRunner::processPendingDrive() {
  if (!pendingDrive_.hasPending) return;
  
  const uint32_t now = millis();
  // Only process every ~33ms to prevent spam
  if (now - lastDriveProcessMs_ < 33) return;
  
  wheels_.setTarget(pendingDrive_.left, pendingDrive_.right, pendingDrive_.durationMs);
  pendingDrive_.hasPending = false;
  lastDriveProcessMs_ = now;
}

void TaskRunner::onDisconnected() {
  wheels_.emergencyStop();
  pendingDrive_.hasPending = false;
}
