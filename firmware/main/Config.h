#pragma once

// ==== Wi-Fi & WS endpoint ====
static const char* WIFI_SSID = "Huy Minh";
static const char* WIFI_PASS = "23052004";
static const char* WS_HOST = "192.168.1.5";
static const uint16_t WS_PORT = 8080;
static const char* WS_PATH = "/robot";
static const uint32_t WS_RECONNECT_BASE_MS = 1000;
static const uint32_t WS_RECONNECT_MAX_MS = 5000;

// ==== Meccano M.A.X bus wiring (REAL-only) ====
#ifndef MAX_DATA_PIN
  #define MAX_DATA_PIN            D4      // dây trắng (D) nối vào ESP8266
#endif
#ifndef MAX_LEFT_POS
  #define MAX_LEFT_POS            0
#endif
#ifndef MAX_RIGHT_POS
  #define MAX_RIGHT_POS           1
#endif

// Hướng "tiến" mặc định cho từng bánh (đảo 0/1 nếu đẩy tiến mà xe lùi)
#ifndef LEFT_FORWARD_IS_CCW
  #define LEFT_FORWARD_IS_CCW     1
#endif
#ifndef RIGHT_FORWARD_IS_CCW
  #define RIGHT_FORWARD_IS_CCW    0
#endif

// ==== Timing: 30 Hz + safety ====
#ifndef WHEELS_TICK_MS
  #define WHEELS_TICK_MS          33
#endif
#ifndef SOFT_STOP_TIMEOUT_MS
  #define SOFT_STOP_TIMEOUT_MS    150
#endif
#ifndef HARD_STOP_TIMEOUT_MS
  #define HARD_STOP_TIMEOUT_MS    400
#endif

// Gửi lại frame định kỳ để giữ thiết bị "awake" (không spam bus)
#ifndef MAX_KEEPALIVE_MS
  #define MAX_KEEPALIVE_MS        250
#endif

// WebSocket heartbeat settings (phải match với server)
#define WS_HEARTBEAT_INTERVAL_MS 15000  // ping mỗi 15s
#define WS_HEARTBEAT_TIMEOUT_MS 3000    // đợi pong 3s
#define WS_HEARTBEAT_TRIES 2            // fail sau 2 lần miss