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
      reconnectDelay(WS_RECONNECT_BASE_MS) {}

void NetClient::begin(TaskRunner* r) {
  runner = r;
  s_instance = this;

  Serial.println("[NET] Initializing WebSocket client...");

  // Wi-Fi khuyến nghị cho WS ổn định
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);  // tránh modem-sleep làm trễ PONG

  // Callback C-style -> trampoline
  ws.onEvent(&NetClient::onWsEventThunk);

  // Tự mình quản lý backoff, nên tắt auto-reconnect bên trong lib
  ws.setReconnectInterval(0);

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
  if (!connected) return;
  StaticJsonDocument<JSON_TX_DOC_CAPACITY> doc;
  doc["kind"] = "ack";
  doc["taskId"] = taskId;
  sendEnvelope(doc);
}

void NetClient::sendProgress(const String& taskId, uint8_t pct, const String& note) {
  if (!connected) return;
  StaticJsonDocument<JSON_TX_DOC_CAPACITY> doc;
  doc["kind"] = "progress";
  doc["taskId"] = taskId;
  doc["pct"] = pct;
  if (note.length() > 0) {
    doc["note"] = note;
  }
  sendEnvelope(doc);
}

void NetClient::sendDone(const String& taskId) {
  if (!connected) return;
  StaticJsonDocument<JSON_TX_DOC_CAPACITY> doc;
  doc["kind"] = "done";
  doc["taskId"] = taskId;
  sendEnvelope(doc);
}

void NetClient::sendError(const String& taskId, const String& message) {
  if (!connected) return;
  StaticJsonDocument<JSON_TX_DOC_CAPACITY> doc;
  doc["kind"] = "error";
  if (taskId.length() > 0) {
    doc["taskId"] = taskId;
  }
  doc["message"] = message;
  sendEnvelope(doc);
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
  // Exponential backoff với trần
  uint32_t nextDelay = reconnectDelay * 2;
  if (nextDelay < WS_RECONNECT_BASE_MS) nextDelay = WS_RECONNECT_BASE_MS;
  if (nextDelay > WS_RECONNECT_MAX_MS) nextDelay = WS_RECONNECT_MAX_MS;
  reconnectDelay = nextDelay;
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
      Serial.println("[NET] WebSocket CONNECTED");
      sendHello();
      
      // Wheels are already initialized and running via TaskRunner::loop()
      // No need to start continuous task here - tick() handles it
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
    case WStype_PONG:
      // ok
      break;
    default:
      break;
  }
}

void NetClient::handleMessage(const String& payload) {
  StaticJsonDocument<JSON_RX_DOC_CAPACITY> doc;
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
    StaticJsonDocument<JSON_TX_DOC_CAPACITY> pong;
    pong["kind"] = "pong";
    pong["t"] = doc["t"] | (uint32_t)millis();
    sendEnvelope(pong);
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

void NetClient::parseTasks(JsonArrayConst arr, bool enqueueMode) {
  if (!runner) return;

  std::vector<TaskEnvelope> tasks;
  for (JsonVariantConst item : arr) {
    if (!item.is<JsonObjectConst>()) continue;
    JsonObjectConst obj = item.as<JsonObjectConst>();

    TaskEnvelope env;
    env.taskId     = obj["taskId"]    | String();
    env.device     = obj["device"]    | String();
    env.type       = obj["type"]      | String();
    env.angle      = obj["angle"]     | (uint16_t)0;
    env.left       = obj["left"]      | (int16_t)0;
    env.right      = obj["right"]     | (int16_t)0;
    env.durationMs = obj["durationMs"]| (uint32_t)0;

    // Sanity clamp (tránh giá trị out-of-range)
    if (env.left < -100)  env.left  = -100;
    if (env.left > 100)   env.left  = 100;
    if (env.right < -100) env.right = -100;
    if (env.right > 100)  env.right = 100;

    if (env.taskId.length() == 0 || env.device.length() == 0) {
      sendError("", "invalid task data");
      continue;
    }
    tasks.push_back(env);
  }

  if (tasks.empty()) {
    Serial.println("[NET] No tasks");
    return;
  }

  Serial.printf("[NET] %s %u task(s)\n", enqueueMode ? "enqueue" : "replace",
                (unsigned)tasks.size());
  if (enqueueMode) {
    runner->enqueueTasks(tasks);
  } else {
    runner->replaceTasks(tasks);
  }
}

void NetClient::sendHello() {
  StaticJsonDocument<JSON_TX_DOC_CAPACITY> doc;
  doc["kind"] = "hello";
  doc["espId"] = String(ESP.getChipId(), HEX);
  doc["fw"] = "robot-max-fw/1.0";
  doc["rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  sendEnvelope(doc);
}

void NetClient::sendEnvelope(JsonDocument& doc) {
  if (!connected) return;
  String buffer;
  serializeJson(doc, buffer);
  ws.sendTXT(buffer);
}
