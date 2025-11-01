// firmware/src/Devices/WheelsDevice.cpp
#include "WheelsDevice.h"
#include "../Config.h"
#include <math.h>

static constexpr int8_t PCT_DEADZONE = 2;

void WheelsDevice::begin() {
  targetPctL_ = targetPctR_ = 0;
  lastSentPctL_ = lastSentPctR_ = 127; // 127 = "unset"
  lastCmdAt_ = millis();
  deadlineAt_ = 0;
  lastBusWriteMs_ = 0;
  lastNonZeroMs_ = 0;

  // Init MAX bus (REAL-only)
  if (!maxBus_) {
    maxBus_ = new MeccaChannel(MAX_DATA_PIN);
  }

  // Send initial communication to activate devices on bus
  if (maxBus_) {
    maxBus_->communicate();
  }
}

void WheelsDevice::setTarget(int8_t leftPct, int8_t rightPct, uint32_t durationMs) {
  // Constrain to -100..100
  targetPctL_ = constrain(leftPct, -100, 100);
  targetPctR_ = constrain(rightPct, -100, 100);
  lastCmdAt_ = millis();
  deadlineAt_ = durationMs ? (lastCmdAt_ + durationMs) : 0;
}

void WheelsDevice::emergencyStop() {
  targetPctL_ = targetPctR_ = 0;
  if (maxBus_) {
    bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
    bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
    uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
    uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
    maxBus_->communicateAllByte(dirR, dirL, 0x40, 0x40); // STOP both
    lastBusWriteMs_ = millis();
  }
  lastSentPctL_ = lastSentPctR_ = 0;
}

void WheelsDevice::tick() {
  const uint32_t now = millis();

  // Task deadline check
  if (deadlineAt_ && now >= deadlineAt_) {
    targetPctL_ = 0;
    targetPctR_ = 0;
    deadlineAt_ = 0;
  }

  // Hard stop: connection lost too long
  if ((now - lastCmdAt_) > HARD_STOP_TIMEOUT_MS) {
    emergencyStop();
    return;
  }

  // Main wheel control logic with soft/hard stop guards
  tickWheels();
}

// Return 0x2n (CW) or 0x3n (CCW) where n is device position
uint8_t WheelsDevice::dirByte(uint8_t pos, bool ccw) {
  return (ccw ? 0x30 : 0x20) | (pos & 0x0F);
}

// Map percentage (-100..100) to speed byte (0x40=STOP, 0x42..0x4F are 14 steps)
uint8_t WheelsDevice::speedByteFromPct(int8_t pct) {
  if (pct == 0) return 0x40; // STOP
  uint8_t step = (uint8_t)round((abs(pct) * 14.0) / 100.0); // 1..14
  if (step < 1) step = 1;
  if (step > 14) step = 14;
  return (uint8_t)(0x41 + step); // => 0x42..0x4F
}

// Send motor command: direction byte + speed byte
// Note: This is a helper; actual sending is batched in tickWheels() for efficiency
void WheelsDevice::sendMotor(uint8_t pos, int8_t pct, bool forwardIsCCW) {
  // This function is kept for API compatibility but tickWheels handles the actual sending
  (void)pos;
  (void)pct;
  (void)forwardIsCCW;
}

void WheelsDevice::tickWheels() {
  unsigned long now = millis();

  // Soft/hard stop guards
  if (targetPctL_ == 0 && targetPctR_ == 0) {
    if (now - lastNonZeroMs_ > SOFT_STOP_TIMEOUT_MS) {
      if (lastSentPctL_ != 0) {
        if (maxBus_) {
          bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
          bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
          uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
          uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
          maxBus_->communicateAllByte(dirR, dirL, 0x40, 0x40); // STOP
          lastBusWriteMs_ = now;
        }
        lastSentPctL_ = 0;
      }
      if (lastSentPctR_ != 0) {
        if (maxBus_) {
          bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
          bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
          uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
          uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
          maxBus_->communicateAllByte(dirR, dirL, 0x40, 0x40); // STOP
          lastBusWriteMs_ = now;
        }
        lastSentPctR_ = 0;
      }
    }
  } else {
    lastNonZeroMs_ = now;
  }

  bool needKeepalive = (now - lastBusWriteMs_) >= MAX_KEEPALIVE_MS;
  bool changedL = (abs(targetPctL_ - lastSentPctL_) >= PCT_DEADZONE);
  bool changedR = (abs(targetPctR_ - lastSentPctR_) >= PCT_DEADZONE);

  if (changedL || changedR || needKeepalive) {
    // Prepare direction and speed bytes for both motors
    bool wantCCW_L = (targetPctL_ >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
    uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
    uint8_t spL = speedByteFromPct(targetPctL_);
    
    bool wantCCW_R = (targetPctR_ >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
    uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
    uint8_t spR = speedByteFromPct(targetPctR_);

    if (maxBus_) {
      // Send: rightDir, leftDir, rightSpeed, leftSpeed
      maxBus_->communicateAllByte(dirR, dirL, spR, spL);
      lastBusWriteMs_ = now;
#if DEBUG_LOGS
      Serial.printf("[WHEELS] L=%d%% R=%d%% -> dirL=0x%02X dirR=0x%02X spL=0x%02X spR=0x%02X\n",
                    targetPctL_, targetPctR_, dirL, dirR, spL, spR);
#endif
    }
    lastSentPctL_ = targetPctL_;
    lastSentPctR_ = targetPctR_;
  }

  // Hard stop if we somehow haven't written for too long
  if (now - lastBusWriteMs_ > HARD_STOP_TIMEOUT_MS) {
    bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
    bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
    uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
    uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
    if (maxBus_) {
      maxBus_->communicateAllByte(dirR, dirL, 0x40, 0x40); // STOP both
      lastBusWriteMs_ = now;
#if DEBUG_LOGS
      Serial.println("[WHEELS] HARD STOP timeout");
#endif
    }
    lastSentPctL_ = lastSentPctR_ = 0;
  }
}
