#pragma once

// ==== Wi-Fi & WS endpoint ====
#define WIFI_SSID      "Huy Minh"
#define WIFI_PASSWORD  "23052004"
#define WS_HOST        "192.168.1.5"
#define WS_PORT        8080
#define WS_PATH        "/robot"

// ==== Modes ====
#define SIMULATION     true   // true: log hành vi; false: điều khiển phần cứng thật

// ==== Wheels Continuous Mode tuning ====
#define WHEELS_TICK_MS            33     // ~30Hz
#define SOFT_STOP_TIMEOUT_MS      150    // >150ms không có lệnh ⇒ ramp về 0
#define HARD_STOP_TIMEOUT_MS      400    // >400ms ⇒ STOP cứng

// ==== JSON buffers (ArduinoJson v6) ====
#define JSON_RX_DOC_CAPACITY  512  // cân đối theo payload thực tế
#define JSON_TX_DOC_CAPACITY  256

// ==== Legacy compatibility (for existing code) ====
static const char* WIFI_PASS = WIFI_PASSWORD;
static const uint32_t WS_RECONNECT_BASE_MS = 1000;
static const uint32_t WS_RECONNECT_MAX_MS = 5000;

// WebSocket heartbeat settings (phải match với server)
#define WS_HEARTBEAT_INTERVAL_MS 15000  // ping mỗi 15s
#define WS_HEARTBEAT_TIMEOUT_MS 3000    // đợi pong 3s
#define WS_HEARTBEAT_TRIES 2            // fail sau 2 lần miss