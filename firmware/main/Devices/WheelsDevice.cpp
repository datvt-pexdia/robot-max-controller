// firmware/src/Devices/WheelsDevice.cpp
#include "WheelsDevice.h"
#include "../Config.h"

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void WheelsDevice::begin() {
  curL_ = curR_ = 0.0f;
  tgtL_ = tgtR_ = 0.0f;
  lastCmdAt_ = millis();
  deadlineAt_ = 0;
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

void WheelsDevice::driveMotors(float normL, float normR) {
#if SIMULATION
  static uint32_t lastLog = 0;
  const uint32_t now = millis();
  if (now - lastLog >= 100) {
    Serial.printf("[WHEELS] L=%.2f R=%.2f\n", normL, normR);
    lastLog = now;
  }
#else
  // TODO: tích hợp Meccano M.A.X motor driver ở đây:
  // - tách dấu → hướng (0x2n/0x3n), lượng hoá 14 mức → 0x42..0x4F, 0x40 là STOP
  // - refresh lệnh đều mỗi tick (~30Hz) để giữ module "awake".
#endif
}
