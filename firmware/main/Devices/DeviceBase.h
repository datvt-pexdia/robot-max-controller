#pragma once

#include <Arduino.h>
#include "../TaskTypes.h"

class DeviceBase {
 public:
  virtual ~DeviceBase() = default;
  virtual const char* deviceName() const = 0;
  virtual void startTask(const TaskEnvelope& task, uint32_t now) = 0;
  virtual void tick(uint32_t now) = 0;
  virtual void cancel(uint32_t now) = 0;
  virtual void finish() = 0;
  virtual bool isRunning() const = 0;
  virtual bool isCompleted(uint32_t now) const = 0;
  virtual uint8_t progress(uint32_t now) const = 0;
  virtual String currentTaskId() const = 0;
};
