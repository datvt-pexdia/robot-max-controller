#pragma once

#include "DeviceBase.h"

class WheelsDevice : public DeviceBase {
 public:
  WheelsDevice();
  const char* deviceName() const override;
  void startTask(const TaskEnvelope& task, uint32_t now) override;
  void tick(uint32_t now) override;
  void cancel(uint32_t now) override;
  void finish() override;
  bool isRunning() const override;
  bool isCompleted(uint32_t now) const override;
  uint8_t progress(uint32_t now) const override;
  String currentTaskId() const override;

 private:
  TaskEnvelope current;
  bool running;
  bool hasTask;
  uint32_t startMs;
  uint32_t durationMs;
  uint8_t lastLoggedProgress;
};
