#include "NetClient.h"

#include <ArduinoJson.h>
#include <vector>

#include "TaskRunner.h"

NetClient::NetClient()
    : runner(nullptr), connected(false), lastConnectAttempt(0), reconnectDelay(WS_RECONNECT_BASE_MS) {}

void NetClient::begin(TaskRunner* r) {
  runner = r;
  Serial.println("[NET] Initializing WebSocket client...");
  
  ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) { this->handleEvent(type, payload, length); });
  ws.setReconnectInterval(0);
  ws.enableHeartbeat(15000, 3000, 2);
  
  // Add timeout for connection
  ws.setExtraHeaders("Connection: Upgrade");
  
  Serial.printf("[NET] WebSocket configured - Host: %s, Port: %d, Path: %s\n", WS_HOST, WS_PORT, WS_PATH);
  Serial.println("[NET] Starting connection...");
  
  connect();
}

void NetClient::loop() {
  ws.loop();
  if (!connected) {
    uint32_t now = millis();
    if (now - lastConnectAttempt >= reconnectDelay) {
      connect();
    }
  }
}

void NetClient::sendAck(const String& taskId) {
  if (!connected) return;
  StaticJsonDocument<128> doc;
  doc["kind"] = "ack";
  doc["taskId"] = taskId;
  sendEnvelope(doc);
}

void NetClient::sendProgress(const String& taskId, uint8_t pct, const String& note) {
  if (!connected) return;
  StaticJsonDocument<160> doc;
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
  StaticJsonDocument<128> doc;
  doc["kind"] = "done";
  doc["taskId"] = taskId;
  sendEnvelope(doc);
}

void NetClient::sendError(const String& taskId, const String& message) {
  if (!connected) return;
  StaticJsonDocument<192> doc;
  doc["kind"] = "error";
  if (taskId.length() > 0) {
    doc["taskId"] = taskId;
  }
  doc["message"] = message;
  sendEnvelope(doc);
}

void NetClient::connect() {
  // Prevent multiple simultaneous connection attempts
  if (connected) {
    Serial.println("[NET] Already connected, skipping connection attempt...");
    return;
  }
  
  lastConnectAttempt = millis();
  Serial.printf("[NET] connecting to ws://%s:%u%s\n", WS_HOST, WS_PORT, WS_PATH);
  Serial.printf("[NET] WiFi status: %d\n", WiFi.status());
  Serial.printf("[NET] Local IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Test basic connectivity first
  WiFiClient client;
  if (client.connect(WS_HOST, WS_PORT)) {
    Serial.println("[NET] TCP connection successful");
    client.stop();
  } else {
    Serial.println("[NET] TCP connection failed!");
    return;
  }
  
  // Set subprotocol to "arduino" as expected by server
  Serial.println("[NET] Starting WebSocket handshake...");
  ws.begin(WS_HOST, WS_PORT, WS_PATH, "arduino");
}

void NetClient::scheduleReconnect() {
  connected = false;
  reconnectDelay = reconnectDelay * 2;
  if (reconnectDelay > WS_RECONNECT_MAX_MS) reconnectDelay = WS_RECONNECT_MAX_MS;
  Serial.printf("[NET] Scheduled reconnect in %lu ms\n", reconnectDelay);
}

void NetClient::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      connected = true;
      reconnectDelay = WS_RECONNECT_BASE_MS;
      Serial.println("[NET] WebSocket connected successfully!");
      sendHello();
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[NET] WebSocket disconnected. Code: %d\n", length);
      Serial.printf("[NET] Current reconnect delay: %lu ms\n", reconnectDelay);
      scheduleReconnect();
      if (runner) {
        runner->onDisconnect();
      }
      break;
    case WStype_ERROR:
      Serial.printf("[NET] WebSocket error: %s\n", payload);
      scheduleReconnect();
      break;
    case WStype_TEXT: {
      String msg;
      msg.reserve(length + 1);
      for (size_t i = 0; i < length; ++i) {
        msg += static_cast<char>(payload[i]);
      }
      handleMessage(msg);
      break;
    }
    case WStype_PING:
      Serial.println("[NET] received ping");
      break;
    case WStype_PONG:
      break;
    default:
      break;
  }
}

void NetClient::handleMessage(const String& payload) {
  StaticJsonDocument<2048> doc;
  auto err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[NET] JSON parse error: %s\n", err.c_str());
    return;
  }
  const char* kind = doc["kind"];
  if (!kind) {
    Serial.println("[NET] missing kind field");
    return;
  }
  if (strcmp(kind, "hello") == 0) {
    Serial.println("[NET] server hello received");
  } else if (strcmp(kind, "task.replace") == 0) {
    JsonArrayConst arr = doc["tasks"].as<JsonArrayConst>();
    parseTasks(arr, false);
  } else if (strcmp(kind, "task.enqueue") == 0) {
    JsonArrayConst arr = doc["tasks"].as<JsonArrayConst>();
    parseTasks(arr, true);
  } else if (strcmp(kind, "task.cancel") == 0) {
    String device = doc["device"].as<String>();
    Serial.printf("[NET] cancel device=%s\n", device.c_str());
    if (runner) {
      runner->cancelDevice(device);
    }
  } else if (strcmp(kind, "ping") == 0) {
    StaticJsonDocument<128> pong;
    pong["kind"] = "pong";
    pong["t"] = doc["t"].as<uint32_t>();
    sendEnvelope(pong);
  } else {
    Serial.printf("[NET] unknown message kind=%s\n", kind);
  }
}

void NetClient::parseTasks(JsonArrayConst arr, bool enqueueMode) {
  if (!runner) {
    return;
  }
  std::vector<TaskEnvelope> tasks;
  for (JsonVariantConst item : arr) {
    if (!item.is<JsonObjectConst>()) {
      continue;
    }
    JsonObjectConst obj = item.as<JsonObjectConst>();
    TaskEnvelope env;
    env.taskId = obj["taskId"].as<String>();
    env.device = obj["device"].as<String>();
    env.type = obj["type"].as<String>();
    env.angle = obj["angle"].as<uint16_t>();
    env.left = obj["left"].as<int16_t>();
    env.right = obj["right"].as<int16_t>();
    env.durationMs = obj["durationMs"].as<uint32_t>();
    if (env.taskId.length() == 0 || env.device.length() == 0) {
      sendError("", "invalid task data");
      continue;
    }
    tasks.push_back(env);
  }
  if (tasks.empty()) {
    Serial.println("[NET] no tasks to process");
    return;
  }
  Serial.printf("[NET] received %u task(s) (%s)\n", static_cast<unsigned>(tasks.size()),
                enqueueMode ? "enqueue" : "replace");
  if (enqueueMode) {
    runner->enqueueTasks(tasks);
  } else {
    runner->replaceTasks(tasks);
  }
}

void NetClient::sendHello() {
  StaticJsonDocument<192> doc;
  doc["kind"] = "hello";
  doc["espId"] = String(ESP.getChipId(), HEX);
  doc["fw"] = "robot-max-fw/1.0";
  sendEnvelope(doc);
}

void NetClient::sendEnvelope(JsonDocument& doc) {
  if (!connected) return;
  String buffer;
  serializeJson(doc, buffer);
  ws.sendTXT(buffer);
}
