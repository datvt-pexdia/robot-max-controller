// firmware/src/Devices/WheelsDevice.cpp
#include "WheelsDevice.h"
#include "../Config.h"
#include <math.h>

static constexpr int8_t PCT_DEADZONE = 2;

void WheelsDevice::begin() {
  targetPctL_ = targetPctR_ = 0;
  currentPctL_ = currentPctR_ = 0;
  slewAccumL_ = slewAccumR_ = 0;
  lastSentPctL_ = lastSentPctR_ = 127; // 127 = "unset"
  lastCmdAt_ = millis();
  deadlineAt_ = 0;
  lastBusWriteMs_ = 0;
  lastNonZeroMs_ = 0;
  lastTickMs_ = millis();
  lastBusErrorMs_ = 0;
  consecutiveBusErrors_ = 0;

  // Init MAX bus (REAL-only)
  if (!maxBus_) {
    maxBus_ = new MeccaChannel(MAX_DATA_PIN);
  }

  // Send initial communication to activate devices on bus
  if (maxBus_) {
    maxBus_->communicate();
    // Verify bus is working
    if (!verifyBusCommunication()) {
      Serial.println("[WHEELS] WARNING: MAX bus initialization check failed");
    }
  } else {
    Serial.println("[WHEELS] ERROR: Failed to allocate MAX bus");
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
  currentPctL_ = currentPctR_ = 0;
  slewAccumL_ = slewAccumR_ = 0;
  if (maxBus_) {
    bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
    bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
    uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
    uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
    maxBus_->communicateAllByte(dirR, dirL, MAXProtocol::CMD_STOP, MAXProtocol::CMD_STOP); // STOP both
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
  return MAXProtocol::dirByte(pos, ccw);
}

// Map percentage (-100..100) to speed byte (CMD_STOP=0x40, 0x42..0x4F are 14 speed steps)
uint8_t WheelsDevice::speedByteFromPct(int8_t pct) {
  if (pct <= 0) return MAXProtocol::CMD_STOP;
  uint8_t s = constrain(abs(pct), 0, 100);
  // Map to 14 speed steps: 0x42 (min) to 0x4F (max)
  uint8_t code = MAXProtocol::CMD_SPEED_MIN + round(s * (MAXProtocol::CMD_SPEED_MAX - MAXProtocol::CMD_SPEED_MIN) / 100);
  return min(code, MAXProtocol::CMD_SPEED_MAX);
}

// sendMotor() removed - functionality handled directly in tickWheels() for efficiency

void WheelsDevice::tickWheels() {
  unsigned long now = millis();
  
  // Slew-rate limiting: ±0.67 per frame (≈20 per second at 30Hz)
  // Using fixed-point math (scaled by 100) for fractional accumulator
  const int16_t MAX_DELTA_PER_FRAME_SCALED = 67; // 0.67 * 100
  uint32_t deltaMs = now - lastTickMs_;
  if (deltaMs >= WHEELS_TICK_MS) {
    // Calculate desired change (scaled by 100)
    int16_t targetDeltaL = (targetPctL_ - currentPctL_) * 100;
    int16_t targetDeltaR = (targetPctR_ - currentPctR_) * 100;
    
    // Add to fractional accumulator, clamped to ±67 per frame
    int16_t deltaAccumL = constrain(targetDeltaL, -MAX_DELTA_PER_FRAME_SCALED, MAX_DELTA_PER_FRAME_SCALED);
    int16_t deltaAccumR = constrain(targetDeltaR, -MAX_DELTA_PER_FRAME_SCALED, MAX_DELTA_PER_FRAME_SCALED);
    
    slewAccumL_ += deltaAccumL;
    slewAccumR_ += deltaAccumR;
    
    // Apply integer portion to current speed, keep remainder
    int8_t deltaL = slewAccumL_ / 100;
    int8_t deltaR = slewAccumR_ / 100;
    slewAccumL_ -= deltaL * 100;
    slewAccumR_ -= deltaR * 100;
    
    currentPctL_ = constrain(currentPctL_ + deltaL, -100, 100);
    currentPctR_ = constrain(currentPctR_ + deltaR, -100, 100);
    
    lastTickMs_ = now;
  }

  // Soft/hard stop guards
  if (targetPctL_ == 0 && targetPctR_ == 0) {
    if (now - lastNonZeroMs_ > SOFT_STOP_TIMEOUT_MS) {
      if (lastSentPctL_ != 0) {
        if (maxBus_) {
          bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
          bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
          uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
          uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
          maxBus_->communicateAllByte(dirR, dirL, MAXProtocol::CMD_STOP, MAXProtocol::CMD_STOP); // STOP
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
          maxBus_->communicateAllByte(dirR, dirL, MAXProtocol::CMD_STOP, MAXProtocol::CMD_STOP); // STOP
          lastBusWriteMs_ = now;
        }
        lastSentPctR_ = 0;
      }
    }
  } else {
    lastNonZeroMs_ = now;
  }

  bool needKeepalive = (now - lastBusWriteMs_) >= MAX_KEEPALIVE_MS;
  bool changedL = (abs(currentPctL_ - lastSentPctL_) >= PCT_DEADZONE);
  bool changedR = (abs(currentPctR_ - lastSentPctR_) >= PCT_DEADZONE);

  if (changedL || changedR || needKeepalive) {
    // Prepare direction and speed bytes for both motors using current (slew-rate limited) values
    bool wantCCW_L = (currentPctL_ >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
    uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
    uint8_t spL = speedByteFromPct(currentPctL_);
    
    bool wantCCW_R = (currentPctR_ >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
    uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
    uint8_t spR = speedByteFromPct(currentPctR_);

    if (maxBus_) {
      // Send: rightDir, leftDir, rightSpeed, leftSpeed
      maxBus_->communicateAllByte(dirR, dirL, spR, spL);
      
      // Verify communication success (meager check - communicateAllByte doesn't return status)
      // At minimum, update error tracking
      if (verifyBusCommunication()) {
        consecutiveBusErrors_ = 0;
      } else {
        consecutiveBusErrors_++;
        lastBusErrorMs_ = now;
        if (consecutiveBusErrors_ >= 5) {
          Serial.printf("[WHEELS] ERROR: %u consecutive bus communication failures\n", consecutiveBusErrors_);
        }
      }
      
      lastBusWriteMs_ = now;
#if DEBUG_LOGS
      Serial.printf("[WHEELS] L=%d%% R=%d%% (current L=%d%% R=%d%%) -> dirL=0x%02X dirR=0x%02X spL=0x%02X spR=0x%02X\n",
                    targetPctL_, targetPctR_, currentPctL_, currentPctR_, dirL, dirR, spL, spR);
#endif
    }
    lastSentPctL_ = currentPctL_;
    lastSentPctR_ = currentPctR_;
  }

  // Hard stop if we somehow haven't written for too long
  if (now - lastBusWriteMs_ > HARD_STOP_TIMEOUT_MS) {
    bool wantCCW_L = (0 >= 0) ? (LEFT_FORWARD_IS_CCW != 0) : !(LEFT_FORWARD_IS_CCW != 0);
    bool wantCCW_R = (0 >= 0) ? (RIGHT_FORWARD_IS_CCW != 0) : !(RIGHT_FORWARD_IS_CCW != 0);
    uint8_t dirL = dirByte(MAX_LEFT_POS, wantCCW_L);
    uint8_t dirR = dirByte(MAX_RIGHT_POS, wantCCW_R);
    if (maxBus_) {
      maxBus_->communicateAllByte(dirR, dirL, MAXProtocol::CMD_STOP, MAXProtocol::CMD_STOP); // STOP both
      lastBusWriteMs_ = now;
#if DEBUG_LOGS
      Serial.println("[WHEELS] HARD STOP timeout");
#endif
    }
    lastSentPctL_ = lastSentPctR_ = 0;
    currentPctL_ = currentPctR_ = 0;
    targetPctL_ = targetPctR_ = 0;
    slewAccumL_ = slewAccumR_ = 0;
  }
}

bool WheelsDevice::verifyBusCommunication() {
  // Basic verification: check if bus object exists and is initialized
  // Note: MeccaChannel doesn't provide explicit status, so we do minimal verification
  if (!maxBus_) {
    return false;
  }
  
  // If we haven't written recently, assume bus is idle (not an error)
  uint32_t now = millis();
  if (now - lastBusWriteMs_ > 1000) {
    return true;  // Idle state is valid
  }
  
  // Basic sanity check: if consecutive errors exceed threshold, bus may be disconnected
  if (consecutiveBusErrors_ >= 10) {
    Serial.println("[WHEELS] MAX bus may be disconnected - too many errors");
    return false;
  }
  
  return true;
}
