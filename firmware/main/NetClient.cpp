#include "NetClient.h"

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <vector>

#include "TaskRunner.h"
#include "Protocol.h"

// static
NetClient* NetClient::s_instance = nullptr;

NetClient::NetClient()
    : runner(nullptr),
      connected(false),
      lastConnectAttempt(0),
      reconnectDelay(WS_RECONNECT_BASE_MS),
      msgSeq_(0),
      lastTelemetryMs_(0),
      telemetryCount_(0),
      wifiConnecting_(false),
      lastWifiCheckMs_(0) {
  // Initialize JSON buffer
  jsonBuffer_[0] = '\0';
}

void NetClient::begin(TaskRunner* r) {
  runner = r;
  s_instance = this;

  Serial.println("[NET] Initializing WebSocket client...");

  // Configure Wi-Fi (non-blocking)
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);  // Prevent modem-sleep to avoid PONG delays

  // Start Wi-Fi connection (non-blocking)
  Serial.printf("[NET] Starting WiFi connection to: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiConnecting_ = true;
  lastWifiCheckMs_ = millis();

  // Callback C-style -> trampoline
  ws.onEvent(&NetClient::onWsEventThunk);

  // Set reconnect interval to 2000ms to prevent reconnect spam
  // Note: We still manage exponential backoff manually in scheduleReconnect()
  // but the library's internal reconnect will respect this interval
  ws.setReconnectInterval(2000);

  // Heartbeat: ping every WS_HEARTBEAT_INTERVAL_MS, wait for PONG WS_HEARTBEAT_TIMEOUT_MS,
  // fail after WS_HEARTBEAT_TRIES attempts -> library will generate DISCONNECTED/ERROR for backoff
  ws.enableHeartbeat(WS_HEARTBEAT_INTERVAL_MS,
                     WS_HEARTBEAT_TIMEOUT_MS,
                     WS_HEARTBEAT_TRIES);

  Serial.printf("[NET] WS cfg host=%s port=%u path=%s hb=%ums/%umsx%u\n",
                WS_HOST, WS_PORT, WS_PATH,
                (unsigned)WS_HEARTBEAT_INTERVAL_MS,
                (unsigned)WS_HEARTBEAT_TIMEOUT_MS,
                (unsigned)WS_HEARTBEAT_TRIES);

  // WebSocket connection will be attempted in loop() once WiFi is ready
}

void NetClient::loop() {
  ws.loop();      // Must be called frequently (< ~50ms)
  yield();        // Feed Wi-Fi stack to avoid starvation

  const uint32_t now = millis();
  
  // Check Wi-Fi connection status periodically (non-blocking)
  if (wifiConnecting_) {
    if (now - lastWifiCheckMs_ >= 500) {  // Check every 500ms
      lastWifiCheckMs_ = now;
      wl_status_t status = WiFi.status();
      
      if (status == WL_CONNECTED) {
        wifiConnecting_ = false;
        Serial.printf("[NET] WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
      } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
        // Connection failed, retry after delay
        wifiConnecting_ = false;
        Serial.println("[NET] WiFi connection failed, will retry");
        lastWifiCheckMs_ = now + 5000;  // Retry after 5s
      }
      // Otherwise still connecting, keep waiting
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    // WiFi disconnected, attempt reconnection
    if (now - lastWifiCheckMs_ >= 5000) {
      Serial.println("[NET] WiFi disconnected, attempting reconnect...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      wifiConnecting_ = true;
      lastWifiCheckMs_ = now;
    }
  }

  // Attempt WebSocket connection if WiFi is ready and not already connected
  if (!connected && WiFi.status() == WL_CONNECTED) {
    if (now - lastConnectAttempt >= reconnectDelay) {
      connect();
    }
  }
}

void NetClient::sendAck(const String& taskId) {
  if (!connected || !canSendTelemetry()) return;
  ackDoc_.clear();
  ackDoc_["kind"] = Protocol::RESP_ACK;
  ackDoc_["taskId"] = taskId;
  ackDoc_["seq"] = ++msgSeq_;
  sendEnvelope(ackDoc_);
}

void NetClient::sendProgress(const String& taskId, uint8_t pct, const String& note) {
  if (!connected || !canSendTelemetry()) return;
  progressDoc_.clear();
  progressDoc_["kind"] = Protocol::RESP_PROGRESS;
  progressDoc_["taskId"] = taskId;
  progressDoc_["pct"] = pct;
  progressDoc_["seq"] = ++msgSeq_;
  if (note.length() > 0) {
    progressDoc_["note"] = note;
  }
  sendEnvelope(progressDoc_);
}

void NetClient::sendDone(const String& taskId) {
  if (!connected || !canSendTelemetry()) return;
  doneDoc_.clear();
  doneDoc_["kind"] = Protocol::RESP_DONE;
  doneDoc_["taskId"] = taskId;
  doneDoc_["seq"] = ++msgSeq_;
  sendEnvelope(doneDoc_);
}

void NetClient::sendError(const String& taskId, const String& message) {
  if (!connected || !canSendTelemetry()) return;
  errorDoc_.clear();
  errorDoc_["kind"] = Protocol::RESP_ERROR;
  errorDoc_["seq"] = ++msgSeq_;
  if (taskId.length() > 0) {
    errorDoc_["taskId"] = taskId;
  }
  errorDoc_["message"] = message;
  sendEnvelope(errorDoc_);
}

void NetClient::connect() {
  // Tránh gọi chồng lấn
  if (connected) {
    Serial.println("[NET] Already connected; skip connect()");
    return;
  }

  // Chỉ thử khi Wi-Fi đã kết nối
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[NET] WiFi not connected (status=%d). Backoff...\n", WiFi.status());
    scheduleReconnect();
    return;
  }

  lastConnectAttempt = millis();
  Serial.printf("[NET] Connecting ws://%s:%u%s  (IP=%s)\n",
                WS_HOST, WS_PORT, WS_PATH, WiFi.localIP().toString().c_str());

  // Nếu server có yêu cầu subprotocol, truyền ở tham số 4.
  // Nhiều server không cần -> có thể để "".
  ws.begin(WS_HOST, WS_PORT, WS_PATH /*, "arduino"*/);
}

void NetClient::scheduleReconnect() {
  connected = false;
  // Exponential backoff with jitter (1s→5s)
  uint32_t nextDelay = reconnectDelay * 2;
  if (nextDelay < WS_RECONNECT_BASE_MS) nextDelay = WS_RECONNECT_BASE_MS;
  if (nextDelay > WS_RECONNECT_MAX_MS) nextDelay = WS_RECONNECT_MAX_MS;
  
  // Add jitter: ±20% random variation
  uint32_t jitter = (nextDelay * 20) / 100;
  uint32_t jittered = nextDelay + random(-jitter, jitter + 1);
  reconnectDelay = constrain(jittered, WS_RECONNECT_BASE_MS, WS_RECONNECT_MAX_MS);
  
  Serial.printf("[NET] Reconnect scheduled in %lu ms\n", reconnectDelay);
}

void NetClient::onWsEventThunk(WStype_t type, uint8_t* payload, size_t length) {
  if (s_instance) {
    s_instance->handleEvent(type, payload, length);
  }
}

void NetClient::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      connected = true;
      reconnectDelay = WS_RECONNECT_BASE_MS;
      msgSeq_ = 0;
      lastTelemetryMs_ = millis();
      telemetryCount_ = 0;
      Serial.println("[NET] WebSocket CONNECTED");
      sendHello();
      
      // Wheels are already initialized and running via TaskRunner::loop()
      // No need to start continuous task here - tick() handles it
      break;
    }
    case WStype_PONG: {
      // Heartbeat pong received - connection is alive
      // The library's enableHeartbeat() handles this automatically
      break;
    }
    case WStype_DISCONNECTED: {
      // length không phải “code” chuẩn; chỉ log tối thiểu
      Serial.println("[NET] WebSocket DISCONNECTED");
      scheduleReconnect();
      if (runner) runner->onDisconnected();
      break;
    }
    case WStype_ERROR: {
      Serial.print("[NET] WebSocket ERROR: ");
      // payload có thể không null-terminated
      for (size_t i = 0; i < length; ++i) Serial.print((char)payload[i]);
      Serial.println();
      scheduleReconnect();
      break;
    }
    case WStype_TEXT: {
      String msg;
      msg.reserve(length + 1);
      for (size_t i = 0; i < length; ++i) msg += (char)payload[i];
      handleMessage(msg);
      break;
    }
    case WStype_PING:
      // lib sẽ tự PONG, không cần log
      break;
    default:
      break;
  }
}

void NetClient::handleMessage(const String& payload) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[NET] JSON parse error: %s\n", err.c_str());
    return;
  }

  const char* kind = doc["kind"];
  if (!kind) {
    Serial.println("[NET] Missing 'kind'");
    return;
  }

  if (strcmp(kind, Protocol::CMD_HELLO) == 0) {
    Serial.println("[NET] Server hello");
    return;
  }

  // Legacy task protocol (kept for backward compatibility, but new system uses "drive" messages)
  if (strcmp(kind, Protocol::CMD_TASK_REPLACE) == 0 || strcmp(kind, Protocol::CMD_TASK_ENQUEUE) == 0) {
    // Try to extract wheels drive tasks and convert to simplified protocol
    JsonArrayConst arr = doc["tasks"].as<JsonArrayConst>();
    for (JsonVariantConst item : arr) {
      if (!item.is<JsonObjectConst>()) continue;
      JsonObjectConst obj = item.as<JsonObjectConst>();
      const char* device = obj["device"] | "";
      if (strcmp(device, Protocol::DEVICE_WHEELS) == 0) {
        int left = obj["left"] | 0;
        int right = obj["right"] | 0;
        uint32_t dur = obj["durationMs"] | 0;
        if (runner) {
          ((TaskRunner*)runner)->handleDriveTask((int8_t)left, (int8_t)right, dur);
        }
      }
    }
    return;
  }

  if (strcmp(kind, Protocol::CMD_TASK_CANCEL) == 0) {
    const String device = doc["device"] | "";
    Serial.printf("[NET] cancel device=%s\n", device.c_str());
    if (strcmp(device.c_str(), Protocol::DEVICE_WHEELS) == 0 && runner) {
      runner->onDisconnected(); // Emergency stop wheels
    }
    return;
  }

  if (strcmp(kind, Protocol::CMD_PING) == 0) {
    pongDoc_.clear();
    pongDoc_["kind"] = Protocol::RESP_PONG;
    pongDoc_["t"] = doc["t"] | (uint32_t)millis();
    pongDoc_["seq"] = ++msgSeq_;
    sendEnvelope(pongDoc_);
    return;
  }

  // Simplified drive protocol (low-latency wheels control)
  if (strcmp(kind, Protocol::CMD_DRIVE) == 0) {
    // Validate required fields
    if (!doc.containsKey("left") || !doc.containsKey("right")) {
      Serial.println("[NET] drive command missing left/right fields");
      sendError("", "Missing left/right fields in drive command");
      return;
    }
    
    int left = doc["left"];
    int right = doc["right"];
    uint32_t dur = doc["durationMs"] | 0;
    
    // Validate ranges
    if (left < -100 || left > 100 || right < -100 || right > 100) {
      Serial.printf("[NET] drive command out of range: left=%d right=%d\n", left, right);
      sendError("", "Drive command values must be in range [-100, 100]");
      return;
    }
    
    if (dur > 60000) {  // Max 60 seconds
      Serial.printf("[NET] drive command duration too long: %u ms\n", dur);
      dur = 60000;  // Cap at 60s
    }
    
    if (runner) {
      // Call new simplified TaskRunner interface
      ((TaskRunner*)runner)->handleDriveTask((int8_t)left, (int8_t)right, dur);
    }
    return;
  }

  Serial.printf("[NET] Unknown kind=%s\n", kind);
  sendError("", String("Unknown command kind: ") + kind);
}

void NetClient::sendHello() {
  helloDoc_.clear();
  helloDoc_["kind"] = Protocol::CMD_HELLO;
  helloDoc_["espId"] = String(ESP.getChipId(), HEX);
  helloDoc_["fw"] = "robot-max-fw/1.0";
  helloDoc_["rssi"] = WiFi.RSSI();
  helloDoc_["ip"] = WiFi.localIP().toString();
  helloDoc_["seq"] = ++msgSeq_;
  sendEnvelope(helloDoc_);
}

bool NetClient::canSendTelemetry() {
  uint32_t now = millis();
  // Reset counter every second
  if (now - lastTelemetryMs_ >= 1000) {
    telemetryCount_ = 0;
    lastTelemetryMs_ = now;
  }
  // Limit to 10 messages per second
  if (telemetryCount_ >= 10) {
    return false;
  }
  telemetryCount_++;
  return true;
}

void NetClient::sendEnvelope(JsonDocument& doc) {
  if (!connected) return;
  // Double-check WebSocket connection state before sending
  // getConnectionState() returns WStype_CONNECTED when connected
  if (ws.getConnectionState() != WStype_CONNECTED) {
    connected = false;
    return;
  }
  // Use pre-allocated char buffer instead of String to reduce heap usage
  size_t len = serializeJson(doc, jsonBuffer_, JSON_BUFFER_SIZE);
  if (len > 0 && len < JSON_BUFFER_SIZE) {
    ws.sendTXT(jsonBuffer_, len);
  } else {
    // Fallback to String if buffer too small (shouldn't happen with current message sizes)
    String buffer;
    serializeJson(doc, buffer);
    ws.sendTXT(buffer);
  }
}
