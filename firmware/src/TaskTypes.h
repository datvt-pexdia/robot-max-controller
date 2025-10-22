#pragma once

#include <Arduino.h>

struct ArmTask {
  String taskId;
  uint16_t angle;
  uint32_t durationMs;
};

struct NeckTask {
  String taskId;
  uint16_t angle;
  uint32_t durationMs;
};

struct WheelsTask {
  String taskId;
  int16_t left;
  int16_t right;
  uint32_t durationMs;
};

struct TaskEnvelope {
  String taskId;
  String device;
  String type;
  uint16_t angle;
  int16_t left;
  int16_t right;
  uint32_t durationMs;
};
