/**
 * Robot MAX Controller Firmware - Optimized
 * 
 * Features:
 * - Non-blocking WiFi management
 * - OTA (Over-The-Air) updates
 * - Health monitoring & telemetry
 * - Proper error handling
 * - Watchdog support
 * 
 * @version 2.0.0
 * @author Optimized by AI Assistant
 */

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include "Config.h"
#include "NetClient.h"
#include "TaskRunner.h"
#include "WiFiManager.h"
#include "SystemMonitor.h"

// Forward declarations
void setupOTA();
void setupWatchdog();

// Global instances
TaskRunner taskRunner;
NetClient netClient;
WiFiManager wifiManager(WIFI_SSID, WIFI_PASS);
SystemMonitor sysMonitor;

void setup() {
  // Initialize serial with timeout
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 3000) {
    delay(10);
  }
  
  Serial.println();
  Serial.println(F("╔════════════════════════════════╗"));
  Serial.println(F("║  Robot MAX Controller v2.0     ║"));
  Serial.println(F("╚════════════════════════════════╝"));
  Serial.printf("[SYSTEM] Firmware: v%s\n", FIRMWARE_VERSION);
  Serial.printf("[SYSTEM] Chip ID: %08X\n", ESP.getChipId());
  Serial.printf("[SYSTEM] Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("[SYSTEM] Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println();
  
  // Setup watchdog
  setupWatchdog();
  
  // Connect to WiFi (non-blocking)
  Serial.println(F("[WIFI] Starting connection..."));
  wifiManager.begin();
  
  // Wait for WiFi with timeout
  uint8_t retries = 0;
  while (!wifiManager.isConnected() && retries < WIFI_CONNECT_RETRIES) {
    wifiManager.loop(millis());
    delay(100);
    retries++;
    if (retries % 5 == 0) {
      Serial.print('.');
    }
  }
  Serial.println();
  
  if (wifiManager.isConnected()) {
    Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal strength: %d dBm\n", WiFi.RSSI());
    
    // Setup OTA updates
    setupOTA();
  } else {
    Serial.println(F("[WIFI] Failed to connect, continuing in offline mode"));
  }
  
  // Initialize subsystems
  Serial.println(F("[MAIN] Initializing subsystems..."));
  
  taskRunner.setNetClient(&netClient);
  
  if (!netClient.begin(&taskRunner)) {
    Serial.println(F("[ERROR] NetClient initialization failed!"));
  }
  
  if (!taskRunner.startWheelsContinuous()) {
    Serial.println(F("[ERROR] Wheels continuous task failed to start!"));
  }
  
  // Initialize system monitor
  sysMonitor.begin();
  
  Serial.println(F("[MAIN] Setup complete!\n"));
  Serial.println(F("System ready for operation."));
  Serial.println(F("════════════════════════════════\n"));
}

void loop() {
  static unsigned long lastStatusPrint = 0;
  unsigned long now = millis();
  
  // Feed watchdog
  ESP.wdtFeed();
  
  // WiFi management (non-blocking)
  wifiManager.loop(now);
  
  // OTA updates (only when WiFi connected)
  if (wifiManager.isConnected()) {
    ArduinoOTA.handle();
  }
  
  // Network client
  netClient.loop();
  
  // Task runner
  taskRunner.tick(now);
  
  // System monitoring
  sysMonitor.tick(now);
  
  // Status printing (every STATUS_PRINT_INTERVAL)
  if (now - lastStatusPrint >= STATUS_PRINT_INTERVAL) {
    lastStatusPrint = now;
    sysMonitor.printStatus();
  }
  
  // Yield to ESP8266 background tasks (WiFi, TCP/IP stack)
  yield();
  
  // Small delay for stability (adjustable based on performance)
  delay(MAIN_LOOP_DELAY);
}

/**
 * Setup Over-The-Air (OTA) updates
 */
void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    
    // Emergency stop all devices before update
    taskRunner.emergencyStopWheels();
    taskRunner.cancelAll("OTA update starting");
    
    Serial.println("\n[OTA] Update starting: " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\n[OTA] Update complete!"));
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint8_t lastPct = 0;
    uint8_t pct = (progress / (total / 100));
    if (pct != lastPct && pct % 10 == 0) {
      Serial.printf("[OTA] Progress: %u%%\n", pct);
      lastPct = pct;
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println(F("Auth Failed"));
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println(F("Begin Failed"));
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println(F("Connect Failed"));
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println(F("Receive Failed"));
    } else if (error == OTA_END_ERROR) {
      Serial.println(F("End Failed"));
    }
  });
  
  ArduinoOTA.begin();
  Serial.println(F("[OTA] Ready for updates"));
  Serial.printf("[OTA] Hostname: %s\n", DEVICE_HOSTNAME);
}

/**
 * Setup hardware watchdog timer
 */
void setupWatchdog() {
  #ifdef WATCHDOG_ENABLED
    ESP.wdtEnable(WATCHDOG_TIMEOUT);
    Serial.printf("[WDT] Watchdog enabled: %u ms timeout\n", WATCHDOG_TIMEOUT);
  #else
    ESP.wdtDisable();
    Serial.println(F("[WDT] Watchdog disabled"));
  #endif
}
