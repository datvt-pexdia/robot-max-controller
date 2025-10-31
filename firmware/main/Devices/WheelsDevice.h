// firmware/src/Devices/WheelsDevice.h
#pragma once
#include <Arduino.h>
#include "../TaskTypes.h"
#include "../Config.h"

#if !SIMULATION
  #include <MeccaChannel.h>
  #include <MeccaMaxDrive.h>
#endif

class WheelsDevice {
public:
  void begin();
  void tick(); // gọi đều theo WHEELS_TICK_MS

  // Nhận lệnh drive từ TaskRunner (đơn vị: -100..100)
  void setTarget(int8_t leftPct, int8_t rightPct, uint32_t durationMs = 0);

  // Khi socket/WS đứt ⇒ cancel ngay lập tức
  void emergencyStop();

private:
  float curL_ = 0.0f, curR_ = 0.0f;   // tốc độ hiện tại (-1..1)
  float tgtL_ = 0.0f, tgtR_ = 0.0f;   // mục tiêu (-1..1)
  uint32_t lastCmdAt_ = 0;            // để soft-stop
  uint32_t deadlineAt_ = 0;           // nếu durationMs > 0

#if !SIMULATION
  MeccaChannel*  maxBus_ = nullptr;
  MeccaMaxDrive* drive_   = nullptr;
#endif

  void applySlewToward(float &cur, float tgt);
  void driveMotors(float normL, float normR); // gắn driver thật ở đây

  // helpers
  uint8_t speedCodeFromNorm(float v) const;         // → 0x40 hoặc 0x42..0x4F
  uint8_t dirCodeForWheel(bool isLeft, float v) const; // → 0x2n (CW) / 0x3n (CCW) tuỳ cấu hình forward
};
