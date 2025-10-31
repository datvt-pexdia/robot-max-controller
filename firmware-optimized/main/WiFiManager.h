/**
 * WiFiManager - Non-blocking WiFi connection management
 * 
 * Features:
 * - Non-blocking connection
 * - Automatic reconnection with exponential backoff
 * - Connection status monitoring
 * - Signal strength tracking
 */

#pragma once

#include <ESP8266WiFi.h>
#include "Config.h"

class WiFiManager {
public:
  WiFiManager(const char* ssid, const char* password);
  
  /**
   * Initialize WiFi connection (non-blocking)
   */
  void begin();
  
  /**
   * Main loop - must be called regularly
   * Handles connection monitoring and auto-reconnection
   */
  void loop(unsigned long now);
  
  /**
   * Check if WiFi is connected
   */
  bool isConnected() const;
  
  /**
   * Get signal strength in dBm
   */
  int getRSSI() const;
  
  /**
   * Get local IP address
   */
  String getLocalIP() const;
  
  /**
   * Force reconnection
   */
  void reconnect();
  
  /**
   * Disconnect from WiFi
   */
  void disconnect();

private:
  enum State {
    STATE_DISCONNECTED,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_RECONNECTING
  };
  
  const char* ssid;
  const char* password;
  State state;
  unsigned long lastCheckTime;
  unsigned long lastConnectAttempt;
  uint8_t reconnectAttempts;
  
  /**
   * Start connection attempt
   */
  void startConnection();
  
  /**
   * Check connection status
   */
  void checkConnection(unsigned long now);
  
  /**
   * Calculate reconnect delay with exponential backoff
   */
  unsigned long getReconnectDelay() const;
};
