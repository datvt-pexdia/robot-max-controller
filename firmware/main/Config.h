#pragma once

#if __has_include("Config.local.h")
  #include "Config.local.h"  // defines WIFI_SSID, WIFI_PASS overrides
#endif

// ==== SIMULATION (disabled - REAL mode only) ====
#define SIMULATION            0

// ==== DEBUG logs (optional: enable serial logs without SIMULATION) ====
#define DEBUG_LOGS            1

// ==== Wi-Fi & WS endpoint ====
static const char* WIFI_SSID = "Huy Minh";
static const char* WIFI_PASS = "23052004";
static const char* WS_HOST = "192.168.1.5";
static const uint16_t WS_PORT = 8080;
static const char* WS_PATH = "/robot";
static const uint32_t WS_RECONNECT_BASE_MS = 1000;
static const uint32_t WS_RECONNECT_MAX_MS = 5000;

// ==== Meccano M.A.X bus wiring (REAL-only) ====
#define MAX_DATA_PIN          D4
#define MAX_LEFT_POS          0
#define MAX_RIGHT_POS         1
#define LEFT_FORWARD_IS_CCW   1
#define RIGHT_FORWARD_IS_CCW  0
#define WHEELS_TICK_MS        33
#define SOFT_STOP_TIMEOUT_MS  150
#define HARD_STOP_TIMEOUT_MS  400
#define MAX_KEEPALIVE_MS      250

// WebSocket heartbeat settings (phải match với server)
#define WS_HEARTBEAT_INTERVAL_MS 15000  // ping mỗi 15s
#define WS_HEARTBEAT_TIMEOUT_MS 3000    // đợi pong 3s
#define WS_HEARTBEAT_TRIES 2            // fail sau 2 lần miss