// firmware/src/TaskRunner.h
#pragma once
#include <Arduino.h>
#include "Devices/WheelsDevice.h"

class TaskRunner {
public:
  void begin();
  void loop(); // gọi trong Arduino loop()

  // Hooks từ NetClient
  void handleDriveTask(int8_t leftPct, int8_t rightPct, uint32_t durationMs);

  // Khi WS rớt
  void onDisconnected();

private:
  WheelsDevice wheels_;
  uint32_t lastWheelsTick_ = 0;
  
  // Drive command coalescing - only process latest command every ~33ms
  struct PendingDrive {
    int8_t left;
    int8_t right;
    uint32_t durationMs;
    bool hasPending;
  };
  PendingDrive pendingDrive_{0, 0, 0, false};
  uint32_t lastDriveProcessMs_ = 0;
  
  void processPendingDrive();
};
