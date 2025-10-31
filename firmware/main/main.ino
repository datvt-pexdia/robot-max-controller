#include <ESP8266WiFi.h>
#include <Scheduler.h>

#include "Config.h"
#include "NetClient.h"
#include "TaskRunner.h"

// Include device implementations to ensure they're compiled
#include "Devices/ArmDevice.cpp"
#include "Devices/NeckDevice.cpp"
#include "Devices/WheelsDevice.cpp"

TaskRunner taskRunner;
NetClient netClient;

void connectWifi() {
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  Serial.printf("[WIFI] connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint8_t retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print('.');
    retries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] connected, IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WIFI] failed to connect");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("robot-max-controller firmware boot");
  connectWifi();
  taskRunner.setNetClient(&netClient);
  netClient.begin(&taskRunner);
  
  // Khởi động continuous task cho wheels SAU KHI đã kết nối WebSocket
  // Để tránh block quá lâu trong setup()
  Serial.println("[MAIN] Continuous wheels task will start after WebSocket connects...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
    delay(100);
  }
  netClient.loop();
  taskRunner.tick(millis());
  delay(5);  // Reduced delay for smoother motor control
}
