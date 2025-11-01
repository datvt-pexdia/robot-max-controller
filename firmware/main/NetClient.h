// firmware/src/net/NetClient.h
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "Config.h"
#include "TaskTypes.h"

class TaskRunner;

// =========================
// WS/Network Tunables (used by .cpp)
// =========================
#ifndef WS_RECONNECT_BASE_MS
#define WS_RECONNECT_BASE_MS 1000     // base reconnect delay
#endif

#ifndef WS_RECONNECT_MAX_MS
#define WS_RECONNECT_MAX_MS 10000     // max backoff
#endif

#ifndef WS_HEARTBEAT_INTERVAL_MS
#define WS_HEARTBEAT_INTERVAL_MS 15000  // ws.enableHeartbeat(pingEveryMs, ...)
#endif

#ifndef WS_HEARTBEAT_TIMEOUT_MS
#define WS_HEARTBEAT_TIMEOUT_MS 3000    // time to wait for PONG
#endif

#ifndef WS_HEARTBEAT_TRIES
#define WS_HEARTBEAT_TRIES 2            // fail after N missed PONGs
#endif

class NetClient {
 public:
  NetClient();

  // Lấy HOST/PORT/PATH từ Config.h bên trong .cpp
  void begin(TaskRunner* runner);
  void loop();

  // ==== Outbound events to server ====
  void sendAck(const String& taskId);
  void sendProgress(const String& taskId, uint8_t pct, const String& note);
  void sendDone(const String& taskId);
  void sendError(const String& taskId, const String& message);

 private:
  // ==== WS state ====
  WebSocketsClient ws;
  TaskRunner* runner;
  bool connected;
  uint32_t lastConnectAttempt;
  uint32_t reconnectDelay;
  
  // Wi-Fi non-blocking connection state
  bool wifiConnecting_;
  uint32_t lastWifiCheckMs_;
  
  // Message sequencing and rate limiting
  uint32_t msgSeq_;
  uint32_t lastTelemetryMs_;
  uint32_t telemetryCount_;
  
  // Reusable JSON buffers (preallocated to reduce heap churn)
  StaticJsonDocument<384> helloDoc_;
  StaticJsonDocument<256> ackDoc_;
  StaticJsonDocument<256> progressDoc_;
  StaticJsonDocument<256> doneDoc_;
  StaticJsonDocument<384> errorDoc_;
  StaticJsonDocument<96> pongDoc_;
  
  // Shared char buffer for JSON serialization (avoids String heap allocation)
  static constexpr size_t JSON_BUFFER_SIZE = 512;
  char jsonBuffer_[JSON_BUFFER_SIZE];

  // Static trampoline vì WebSocketsClient callback là C-style function ptr
  static NetClient* s_instance;
  static void onWsEventThunk(WStype_t type, uint8_t* payload, size_t length);

  // ==== Internal helpers ====
  void connect();
  void scheduleReconnect();
  void handleEvent(WStype_t type, uint8_t* payload, size_t length);
  void handleMessage(const String& payload);
  void sendHello();
  void sendEnvelope(JsonDocument& doc);
  bool canSendTelemetry();
};
