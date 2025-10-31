// firmware/src/Devices/WheelsDevice.cpp
#include "WheelsDevice.h"
#include "../Config.h"
#include <math.h>

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void WheelsDevice::begin() {
  curL_ = curR_ = 0.0f;
  tgtL_ = tgtR_ = 0.0f;
  lastCmdAt_ = millis();
  deadlineAt_ = 0;

#if !SIMULATION
  // Khởi tạo MAX bus + drive controller (theo ví dụ DriveTest.ino)
  if (!maxBus_) maxBus_ = new MeccaChannel(MAX_DATA_PIN);
  if (!drive_)  drive_  = new MeccaMaxDrive(maxBus_);

  // Nếu cần, gọi communicate() để khởi tạo bus (xem DriveTest.ino)
  if (maxBus_) {
    maxBus_->communicate();
  }
#endif
}

void WheelsDevice::setTarget(int8_t leftPct, int8_t rightPct, uint32_t durationMs) {
  // map -100..100 → -1..1
  tgtL_ = clampf(leftPct / 100.0f, -1.0f, 1.0f);
  tgtR_ = clampf(rightPct / 100.0f, -1.0f, 1.0f);
  lastCmdAt_ = millis();
  deadlineAt_ = durationMs ? (lastCmdAt_ + durationMs) : 0;
}

void WheelsDevice::emergencyStop() {
  curL_ = curR_ = 0.0f;
  tgtL_ = tgtR_ = 0.0f;
  driveMotors(0, 0);
}

void WheelsDevice::applySlewToward(float &cur, float tgt) {
  const float SLEW = 0.08f; // mỗi tick đổi tối đa 0.08
  float d = tgt - cur;
  if (fabs(d) <= SLEW) cur = tgt;
  else cur += (d > 0 ? SLEW : -SLEW);
}

void WheelsDevice::tick() {
  const uint32_t now = millis();

  // Timeout mềm: không có lệnh mới trong một thời gian ngắn ⇒ tự giảm về 0
  if ((now - lastCmdAt_) > SOFT_STOP_TIMEOUT_MS) { tgtL_ = 0.0f; tgtR_ = 0.0f; }
  // Deadline tác vụ (nếu có)
  if (deadlineAt_ && now >= deadlineAt_) { tgtL_ = 0.0f; tgtR_ = 0.0f; }

  // Timeout cứng: mất kết nối lâu ⇒ STOP ngay
  if ((now - lastCmdAt_) > HARD_STOP_TIMEOUT_MS) { emergencyStop(); return; }

  // Slew-rate để mượt
  applySlewToward(curL_, tgtL_);
  applySlewToward(curR_, tgtR_);

  // Gửi lệnh xuống hardware (hoặc log nếu SIMULATION)
  driveMotors(curL_, curR_);
}

// Map [-1..1] → speed code {0x40 (stop)} U {0x42..0x4F}
// 0x41 không dùng; ngưỡng |v|<0.02 coi như 0 → STOP
uint8_t WheelsDevice::speedCodeFromNorm(float v) const {
  float a = fabs(v);
  if (a < 0.02f) return 0x40; // STOP
  int step = (int)roundf(a * 14.0f); // 1..14
  if (step < 1) step = 1;
  if (step > 14) step = 14;
  return (uint8_t)(0x41 + step); // 0x42..0x4F
}

// Trả về 0x2n (CW) hoặc 0x3n (CCW) tuỳ hướng "tiến" cấu hình cho mỗi bánh
// n = speed level (0x2..0xF) từ speed code (0x42..0x4F → n = 0x2..0xF)
uint8_t WheelsDevice::dirCodeForWheel(bool isLeft, float v) const {
  const uint8_t DIR_CW_BASE  = 0x20;
  const uint8_t DIR_CCW_BASE = 0x30;
  const bool forwardIsCCW = isLeft ? (LEFT_FORWARD_IS_CCW != 0) : (RIGHT_FORWARD_IS_CCW != 0);
  const bool forwardCmd   = (v >= 0.0f);
  bool wantCCW = forwardCmd ? forwardIsCCW : !forwardIsCCW;
  
  uint8_t speedCode = speedCodeFromNorm(v);
  if (speedCode == 0x40) return 0x40; // STOP
  
  // Extract speed level (0x42..0x4F → 0x2..0xF)
  uint8_t speedLevel = speedCode - 0x40; // 0x42-0x40=0x2, ..., 0x4F-0x40=0xF
  
  return wantCCW ? (DIR_CCW_BASE | speedLevel) : (DIR_CW_BASE | speedLevel);
}

void WheelsDevice::driveMotors(float normL, float normR) {
#if SIMULATION
  static uint32_t lastLog = 0;
  const uint32_t now = millis();
  if (now - lastLog >= 100) {
    Serial.printf("[WHEELS] L=%.2f R=%.2f\n", normL, normR);
    lastLog = now;
  }
#else
  if (!maxBus_ || !drive_) return;

  // Map normalized values to MAX command bytes
  uint8_t cmdL = dirCodeForWheel(true, normL);
  uint8_t cmdR = dirCodeForWheel(false, normR);

  // Gửi lệnh xuống MAX bus (theo pattern trong DriveTest.ino)
  // communicateAllByte(leftByte, rightByte, 0xFE, 0xFE)
  maxBus_->communicateAllByte(cmdL, cmdR, 0xFE, 0xFE);
  
  // Gợi ý: nếu thấy "chatter", có thể cache step hiện tại, chỉ gửi khi step/dir đổi.
#endif
}
