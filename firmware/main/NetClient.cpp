#include "NetClient.h"

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <vector>

#include "TaskRunner.h"

// static
NetClient* NetClient::s_instance = nullptr;

NetClient::NetClient()
    : runner(nullptr),
      connected(false),
      lastConnectAttempt(0),
      reconnectDelay(WS_RECONNECT_BASE_MS),
      msgSeq_(0),
      lastTelemetryMs_(0),
      telemetryCount_(0) {}

void NetClient::begin(TaskRunner* r) {
  runner = r;
  s_instance = this;

  Serial.println("[NET] Initializing WebSocket client...");

  // Wi-Fi khuyến nghị cho WS ổn định
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);  // tránh modem-sleep làm trễ PONG

  // Ensure Wi‑Fi connects before attempting WebSocket
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 10000) { // 10s timeout
    delay(100);
    yield();
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[NET] WiFi connect timeout, will backoff & retry in loop()");
  } else {
    Serial.printf("[NET] WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
  }

  // Callback C-style -> trampoline
  ws.onEvent(&NetClient::onWsEventThunk);

  // Set reconnect interval to 2000ms to prevent reconnect spam
  // Note: We still manage exponential backoff manually in scheduleReconnect()
  // but the library's internal reconnect will respect this interval
  ws.setReconnectInterval(2000);

  // Heartbeat phía client: ping mỗi WS_HEARTBEAT_INTERVAL_MS, đợi PONG WS_HEARTBEAT_TIMEOUT_MS,
  // fail sau WS_HEARTBEAT_TRIES lần -> lib sẽ phát sinh DISCONNECTED/ERROR để mình backoff.
  ws.enableHeartbeat(WS_HEARTBEAT_INTERVAL_MS,
                     WS_HEARTBEAT_TIMEOUT_MS,
                     WS_HEARTBEAT_TRIES);

  // Nếu server yêu cầu subprotocol/headers riêng thì cân nhắc thêm; mặc định không cần.
  // ws.setExtraHeaders("Connection: Upgrade");

  Serial.printf("[NET] WS cfg host=%s port=%u path=%s hb=%ums/%umsx%u\n",
                WS_HOST, WS_PORT, WS_PATH,
                (unsigned)WS_HEARTBEAT_INTERVAL_MS,
                (unsigned)WS_HEARTBEAT_TIMEOUT_MS,
                (unsigned)WS_HEARTBEAT_TRIES);

  connect();
}

void NetClient::loop() {
  ws.loop();      // phải gọi thường xuyên (< ~50ms)
  yield();        // nuôi Wi-Fi stack, tránh starvation

  if (!connected) {
    const uint32_t now = millis();
    if (now - lastConnectAttempt >= reconnectDelay) {
      connect();
    }
  }
}

void NetClient::sendAck(const String& taskId) {
  if (!connected || !canSendTelemetry()) return;
  ackDoc_.clear();
  ackDoc_["kind"] = "ack";
  ackDoc_["taskId"] = taskId;
  ackDoc_["seq"] = ++msgSeq_;
  sendEnvelope(ackDoc_);
}

void NetClient::sendProgress(const String& taskId, uint8_t pct, const String& note) {
  if (!connected || !canSendTelemetry()) return;
  progressDoc_.clear();
  progressDoc_["kind"] = "progress";
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
  doneDoc_["kind"] = "done";
  doneDoc_["taskId"] = taskId;
  doneDoc_["seq"] = ++msgSeq_;
  sendEnvelope(doneDoc_);
}

void NetClient::sendError(const String& taskId, const String& message) {
  if (!connected || !canSendTelemetry()) return;
  errorDoc_.clear();
  errorDoc_["kind"] = "error";
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

  if (strcmp(kind, "hello") == 0) {
    Serial.println("[NET] Server hello");
    return;
  }

  // Legacy task protocol (kept for backward compatibility, but new system uses "drive" messages)
  if (strcmp(kind, "task.replace") == 0 || strcmp(kind, "task.enqueue") == 0) {
    // Try to extract wheels drive tasks and convert to simplified protocol
    JsonArrayConst arr = doc["tasks"].as<JsonArrayConst>();
    for (JsonVariantConst item : arr) {
      if (!item.is<JsonObjectConst>()) continue;
      JsonObjectConst obj = item.as<JsonObjectConst>();
      const char* device = obj["device"] | "";
      if (strcmp(device, "wheels") == 0) {
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

  if (strcmp(kind, "task.cancel") == 0) {
    const String device = doc["device"] | "";
    Serial.printf("[NET] cancel device=%s\n", device.c_str());
    if (strcmp(device.c_str(), "wheels") == 0 && runner) {
      runner->onDisconnected(); // Emergency stop wheels
    }
    return;
  }

  if (strcmp(kind, "ping") == 0) {
    pongDoc_.clear();
    pongDoc_["kind"] = "pong";
    pongDoc_["t"] = doc["t"] | (uint32_t)millis();
    pongDoc_["seq"] = ++msgSeq_;
    sendEnvelope(pongDoc_);
    return;
  }

  // Simplified drive protocol (low-latency wheels control)
  if (strcmp(kind, "drive") == 0) {
    int left = doc["left"] | 0;
    int right = doc["right"] | 0;
    uint32_t dur = doc["durationMs"] | 0;
    if (runner) {
      // Call new simplified TaskRunner interface
      ((TaskRunner*)runner)->handleDriveTask((int8_t)left, (int8_t)right, dur);
    }
    return;
  }

  Serial.printf("[NET] Unknown kind=%s\n", kind);
}

void NetClient::sendHello() {
  helloDoc_.clear();
  helloDoc_["kind"] = "hello";
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
  String buffer;
  serializeJson(doc, buffer);
  ws.sendTXT(buffer);
}
