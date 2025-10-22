#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "Config.h"
#include "TaskTypes.h"

class TaskRunner;

class NetClient {
 public:
  NetClient();
  void begin(TaskRunner* runner);
  void loop();

  void sendAck(const String& taskId);
  void sendProgress(const String& taskId, uint8_t pct, const String& note);
  void sendDone(const String& taskId);
  void sendError(const String& taskId, const String& message);

 private:
  WebSocketsClient ws;
  TaskRunner* runner;
  bool connected;
  uint32_t lastConnectAttempt;
  uint32_t reconnectDelay;

  void connect();
  void scheduleReconnect();
  void handleEvent(WStype_t type, uint8_t* payload, size_t length);
  void handleMessage(const String& payload);
  void parseTasks(JsonArrayConst arr, bool enqueueMode);
  void sendHello();
  void sendEnvelope(JsonDocument& doc);
};
