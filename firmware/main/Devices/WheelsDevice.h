#pragma once

#include "DeviceBase.h"
#include <Arduino.h>
#include <MeccaChannel.h>
#include <MeccaMaxDrive.h>

/**
 * WheelsDevice
 * - Điều khiển 2 bánh của Meccano MAX qua MeccaChannel (1-wire).
 * - Quy ước dây: communicateAllByte(b1, b2, ...) => b1 = RIGHT, b2 = LEFT.
 * - Hỗ trợ: chạy từng bánh độc lập (bánh = 0 => speed=0x40, dir=0x20).
 */
class WheelsDevice : public DeviceBase {
 public:
  WheelsDevice();

  // DeviceBase overrides
  const char* deviceName() const override;
  void startTask(const TaskEnvelope& task, uint32_t now) override;
  void tick(uint32_t now) override;
  void cancel(uint32_t now) override;
  void finish() override;
  bool isRunning() const override;
  bool isCompleted(uint32_t now) const override;
  uint8_t progress(uint32_t now) const override;
  String currentTaskId() const override;
  bool tryUpdate(const TaskEnvelope& task, uint32_t now) override;

 private:
  // Trạng thái hiện tại của tác vụ
  TaskEnvelope current;
  bool        running;
  bool        hasTask;
  uint32_t    startMs;
  uint32_t    durationMs;
  uint8_t     lastLoggedProgress;
  uint32_t    lastCommandMs;   // thời điểm cuối gửi lệnh motor (keep-alive)
  uint8_t     leftSpeedByte;   // speed byte hiện hành (0x40..0x4F)
  uint8_t     rightSpeedByte;  // speed byte hiện hành (0x40..0x4F)

  // Meccano MAX motor control
  MeccaChannel*  channelMOTOR;
  MeccaMaxDrive* drive;
  bool           motorInitialized;

  // ===== Motor control methods =====
  void initializeMotors();
  void setMotorSpeed(uint8_t leftSpeed, uint8_t rightSpeed);
  void stopMotors();

  // Giữ vì tương thích API cũ; bản .cpp sẽ cho phép SPEED_STOP (0x40) đi qua
  uint8_t clampSpeed(uint8_t speed);

  // Helper thống nhất thứ tự gửi theo wiring (Byte[1] = RIGHT, Byte[2] = LEFT)
  void sendRL_(uint8_t rightByte, uint8_t leftByte);
};
