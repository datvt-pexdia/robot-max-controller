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
};
