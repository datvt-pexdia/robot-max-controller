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

  lastDirL_ = lastDirR_ = 0xFF;
  lastSpeedL_ = lastSpeedR_ = 0xFF;
  lastFrameAt_ = 0;

#if !SIMULATION
  if (!maxBus_)  maxBus_ = new MeccaChannel(MAX_DATA_PIN);
  if (!motorL_)  motorL_ = new MeccaMaxMotorDevice(*maxBus_, MAX_LEFT_POS);
  if (!motorR_)  motorR_ = new MeccaMaxMotorDevice(*maxBus_, MAX_RIGHT_POS);

  // Gửi một lượt communicate để kích hoạt thiết bị trên bus (giống DriveTest.ino)
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

// Trả về 0x2n(CW) / 0x3n(CCW) theo hướng "tiến" cấu hình cho từng bánh
uint8_t WheelsDevice::dirCodeForWheel(bool isLeft, float v) const {
  const uint8_t DIR_CW  = 0x20;
  const uint8_t DIR_CCW = 0x30;
  const bool forwardIsCCW = isLeft ? (LEFT_FORWARD_IS_CCW != 0) : (RIGHT_FORWARD_IS_CCW != 0);
  const bool forwardCmd   = (v >= 0.0f);
  bool wantCCW = forwardCmd ? forwardIsCCW : !forwardIsCCW;
  return wantCCW ? DIR_CCW : DIR_CW;
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
  if (!maxBus_) return;
  const uint32_t now = millis();

  // Frame hiện tại
  const uint8_t spL = speedCodeFromNorm(normL);
  const uint8_t spR = speedCodeFromNorm(normR);
  const uint8_t dirL = (spL == 0x40) ? dirCodeForWheel(true,  0.0f) : dirCodeForWheel(true,  normL);
  const uint8_t dirR = (spR == 0x40) ? dirCodeForWheel(false, 0.0f) : dirCodeForWheel(false, normR);

  const bool chgL = (dirL != lastDirL_) || (spL != lastSpeedL_);
  const bool chgR = (dirR != lastDirR_) || (spR != lastSpeedR_);
  const bool needKeepAlive = (now - lastFrameAt_) >= MAX_KEEPALIVE_MS;

  if (!(chgL || chgR || needKeepAlive)) {
    return;
  }

  // Theo mapping community: Byte[0] = Right dir, Byte[1] = Left dir, Byte[2] = Right speed, Byte[3] = Left speed
  // Sử dụng communicateAllByte() như trong DriveTest.ino
  maxBus_->communicateAllByte(dirR, dirL, spR, spL);

  lastDirL_ = dirL;
  lastSpeedL_ = spL;
  lastDirR_ = dirR;
  lastSpeedR_ = spR;
  lastFrameAt_ = now;
#endif
}
