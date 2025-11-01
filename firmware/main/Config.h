#pragma once

// Default Wi-Fi credentials (override via Config.local.h)
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif
#ifndef WS_HOST
#define WS_HOST "192.168.1.5"
#endif

// Allow override via Config.local.h
#if __has_include("Config.local.h")
  #include "Config.local.h"
#endif

// ==== DEBUG logs (optional: enable serial logs) ====
static constexpr bool DEBUG_LOGS = true;

// ==== Wi-Fi & WS endpoint ====
// Note: String literals must remain as #define for WiFi.begin() compatibility
static constexpr uint16_t WS_PORT = 8080;
static constexpr const char* WS_PATH = "/robot";
static constexpr uint32_t WS_RECONNECT_BASE_MS = 1000;
static constexpr uint32_t WS_RECONNECT_MAX_MS = 5000;

// ==== Meccano M.A.X bus wiring (REAL-only) ====
static constexpr uint8_t MAX_DATA_PIN = D4;
static constexpr uint8_t MAX_LEFT_POS = 0;
static constexpr uint8_t MAX_RIGHT_POS = 1;
static constexpr bool LEFT_FORWARD_IS_CCW = true;
static constexpr bool RIGHT_FORWARD_IS_CCW = false;
static constexpr uint32_t WHEELS_TICK_MS = 33;
static constexpr uint32_t SOFT_STOP_TIMEOUT_MS = 150;
static constexpr uint32_t HARD_STOP_TIMEOUT_MS = 400;
static constexpr uint32_t MAX_KEEPALIVE_MS = 250;

// WebSocket heartbeat settings (must match server)
static constexpr uint32_t WS_HEARTBEAT_INTERVAL_MS = 15000;  // ping every 15s
static constexpr uint32_t WS_HEARTBEAT_TIMEOUT_MS = 3000;    // wait for pong 3s
static constexpr uint8_t WS_HEARTBEAT_TRIES = 2;             // fail after 2 missed pongs

// ==== Meccano M.A.X Protocol Constants ====
namespace MAXProtocol {
  // Speed commands
  static constexpr uint8_t CMD_STOP = 0x40;
  static constexpr uint8_t CMD_SPEED_MIN = 0x42;  // Lowest non-zero speed
  static constexpr uint8_t CMD_SPEED_MAX = 0x4F;  // Highest speed (14 steps total)
  
  // Direction masks (applied to device position)
  static constexpr uint8_t DIR_CW_MASK = 0x20;   // Clockwise: 0x2n
  static constexpr uint8_t DIR_CCW_MASK = 0x30; // Counter-clockwise: 0x3n
  static constexpr uint8_t DIR_POS_MASK = 0x0F; // Position mask (n in 0x2n/0x3n)
  
  // Helper to create direction byte
  static constexpr uint8_t dirByte(uint8_t pos, bool ccw) {
    return (ccw ? DIR_CCW_MASK : DIR_CW_MASK) | (pos & DIR_POS_MASK);
  }
}