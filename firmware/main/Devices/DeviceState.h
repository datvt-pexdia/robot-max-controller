#pragma once

// ==== Device State Machine ====
enum class DeviceState : uint8_t {
  IDLE = 0,        // No task active
  RUNNING = 1,     // Task is executing
  COMPLETED = 2,   // Task finished successfully
  ERROR = 3        // Task failed or was cancelled
};
