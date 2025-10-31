/**
 * WiFiManager implementation
 */

#include "WiFiManager.h"

WiFiManager::WiFiManager(const char* ssid, const char* password)
  : ssid(ssid),
    password(password),
    state(STATE_DISCONNECTED),
    lastCheckTime(0),
    lastConnectAttempt(0),
    reconnectAttempts(0) {
}

void WiFiManager::begin() {
  // Configure WiFi mode and settings
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);  // Don't write to flash on every connection
  WiFi.setAutoReconnect(false);  // We handle reconnection ourselves
  WiFi.setSleep(false);  // Disable sleep mode for better responsiveness
  
  Serial.printf("[WIFI_MGR] Initializing WiFi for SSID: %s\n", ssid);
  
  // Start initial connection
  startConnection();
}

void WiFiManager::loop(unsigned long now) {
  // Check WiFi status periodically
  if (now - lastCheckTime >= WIFI_CHECK_INTERVAL) {
    lastCheckTime = now;
    checkConnection(now);
  }
}

bool WiFiManager::isConnected() const {
  return state == STATE_CONNECTED && WiFi.status() == WL_CONNECTED;
}

int WiFiManager::getRSSI() const {
  if (!isConnected()) {
    return 0;
  }
  return WiFi.RSSI();
}

String WiFiManager::getLocalIP() const {
  if (!isConnected()) {
    return "0.0.0.0";
  }
  return WiFi.localIP().toString();
}

void WiFiManager::reconnect() {
  Serial.println(F("[WIFI_MGR] Manual reconnection requested"));
  state = STATE_RECONNECTING;
  reconnectAttempts = 0;
  startConnection();
}

void WiFiManager::disconnect() {
  Serial.println(F("[WIFI_MGR] Disconnecting WiFi"));
  WiFi.disconnect();
  state = STATE_DISCONNECTED;
  reconnectAttempts = 0;
}

void WiFiManager::startConnection() {
  if (state == STATE_CONNECTING) {
    DEBUG_LOG("WIFI_MGR", "Already connecting, skipping");
    return;
  }
  
  lastConnectAttempt = millis();
  state = STATE_CONNECTING;
  
  Serial.printf("[WIFI_MGR] Connecting to %s...\n", ssid);
  WiFi.begin(ssid, password);
}

void WiFiManager::checkConnection(unsigned long now) {
  wl_status_t status = WiFi.status();
  
  switch (state) {
    case STATE_DISCONNECTED:
      // Shouldn't happen if begin() was called
      startConnection();
      break;
      
    case STATE_CONNECTING:
      if (status == WL_CONNECTED) {
        state = STATE_CONNECTED;
        reconnectAttempts = 0;
        Serial.println(F("[WIFI_MGR] ✓ Connected successfully!"));
        Serial.printf("[WIFI_MGR]   IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI_MGR]   Signal: %d dBm\n", WiFi.RSSI());
      } else if (now - lastConnectAttempt >= WIFI_RECONNECT_DELAY * WIFI_CONNECT_RETRIES) {
        // Connection timeout
        Serial.println(F("[WIFI_MGR] ✗ Connection timeout"));
        state = STATE_RECONNECTING;
        reconnectAttempts++;
        
        // Calculate backoff delay
        unsigned long delay = getReconnectDelay();
        Serial.printf("[WIFI_MGR] Retrying in %lu ms (attempt %u)\n", delay, reconnectAttempts);
        lastConnectAttempt = now + delay - WIFI_RECONNECT_DELAY;
      }
      break;
      
    case STATE_CONNECTED:
      if (status != WL_CONNECTED) {
        Serial.println(F("[WIFI_MGR] ✗ Connection lost!"));
        state = STATE_RECONNECTING;
        reconnectAttempts = 0;
        startConnection();
      }
      break;
      
    case STATE_RECONNECTING:
      if (status == WL_CONNECTED) {
        state = STATE_CONNECTED;
        reconnectAttempts = 0;
        Serial.println(F("[WIFI_MGR] ✓ Reconnected successfully!"));
      } else if (now - lastConnectAttempt >= getReconnectDelay()) {
        reconnectAttempts++;
        startConnection();
      }
      break;
  }
}

unsigned long WiFiManager::getReconnectDelay() const {
  // Exponential backoff: 500ms, 1s, 2s, 4s, 8s, max 10s
  unsigned long delay = WIFI_RECONNECT_DELAY * (1 << reconnectAttempts);
  if (delay > 10000) {
    delay = 10000;
  }
  return delay;
}
