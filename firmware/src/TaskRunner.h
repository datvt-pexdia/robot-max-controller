#pragma once

#include <Arduino.h>
#include <queue>
#include <vector>

#include "TaskTypes.h"
#include "Devices/ArmDevice.h"
#include "Devices/NeckDevice.h"
#include "Devices/WheelsDevice.h"

class NetClient;

class TaskRunner {
 public:
  TaskRunner();
  void setNetClient(NetClient* client);
  void tick(uint32_t now);
  void replaceTasks(const std::vector<TaskEnvelope>& tasks);
  void enqueueTasks(const std::vector<TaskEnvelope>& tasks);
  void cancelDevice(const String& device);
  void cancelAll(const char* reason);
  void onDisconnect();

 private:
  struct DeviceState {
    DeviceBase* device;
    std::queue<TaskEnvelope> queue;
    uint32_t lastProgressSent;
  };

  ArmDevice arm;
  NeckDevice neck;
  WheelsDevice wheels;
  DeviceState armState;
  DeviceState neckState;
  DeviceState wheelsState;
  NetClient* netClient;

  DeviceState* stateForDevice(const String& device);
  void startNext(DeviceState& state, uint32_t now);
  void clearDevice(DeviceState& state, uint32_t now, const char* reason);
  void acceptTasks(const std::vector<TaskEnvelope>& tasks, bool enqueueMode, uint32_t now);
};
