// firmware/src/Devices/WheelsDevice.h
#pragma once
#include <Arduino.h>
#include "../TaskTypes.h"
#include "../Config.h"

#include <MeccaChannel.h>
#include <MeccaMaxDrive.h>

class WheelsDevice {
public:
  void begin();
  void tick(); // Called every WHEELS_TICK_MS

  // Receive drive command from TaskRunner (units: -100..100)
  void setTarget(int8_t leftPct, int8_t rightPct, uint32_t durationMs = 0);

  // When socket/WS drops ⇒ cancel immediately
  void emergencyStop();

private:
  // Direct percentage-based state
  int8_t targetPctL_{0};
  int8_t targetPctR_{0};
  int8_t lastSentPctL_{127};  // 127 = "unset"
  int8_t lastSentPctR_{127};  // 127 = "unset"
  int8_t currentPctL_{0};     // Current speed after slew-rate limiting
  int8_t currentPctR_{0};     // Current speed after slew-rate limiting
  int16_t slewAccumL_{0};     // Fractional accumulator for slew-rate (scaled by 100)
  int16_t slewAccumR_{0};     // Fractional accumulator for slew-rate (scaled by 100)
  uint32_t lastCmdAt_{0};
  uint32_t deadlineAt_{0};
  uint32_t lastBusWriteMs_{0};
  uint32_t lastNonZeroMs_{0};
  uint32_t lastTickMs_{0};    // For slew-rate limiting

  // MAX bus (REAL-only)
  MeccaChannel* maxBus_ = nullptr;
  
  // Bus communication error tracking

  // Helper functions for MAX mapping
  static inline uint8_t dirByte(uint8_t pos, bool ccw);
  static inline uint8_t speedByteFromPct(int8_t pct);
  void tickWheels();
  
  // MAX bus error handling
  bool verifyBusCommunication();
  uint32_t lastBusErrorMs_;
  uint8_t consecutiveBusErrors_;
};
