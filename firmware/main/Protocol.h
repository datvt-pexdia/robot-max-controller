#pragma once

// ==== Protocol Command Constants ====
// These constants ensure consistent command strings across firmware and server
namespace Protocol {
  // Message kinds (outbound from server)
  static constexpr const char* CMD_HELLO = "hello";
  static constexpr const char* CMD_TASK_REPLACE = "task.replace";
  static constexpr const char* CMD_TASK_ENQUEUE = "task.enqueue";
  static constexpr const char* CMD_TASK_CANCEL = "task.cancel";
  static constexpr const char* CMD_PING = "ping";
  static constexpr const char* CMD_DRIVE = "drive";
  
  // Message kinds (inbound to server)
  static constexpr const char* RESP_ACK = "ack";
  static constexpr const char* RESP_PROGRESS = "progress";
  static constexpr const char* RESP_DONE = "done";
  static constexpr const char* RESP_ERROR = "error";
  static constexpr const char* RESP_PONG = "pong";
  
  // Device names
  static constexpr const char* DEVICE_WHEELS = "wheels";
  static constexpr const char* DEVICE_ARM = "arm";
  static constexpr const char* DEVICE_NECK = "neck";
  
  // Task types
  static constexpr const char* TASK_TYPE_DRIVE = "drive";
  static constexpr const char* TASK_TYPE_MOVE_ANGLE = "moveAngle";
}
