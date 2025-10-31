/**
 * SystemMonitor - System health monitoring and telemetry
 * 
 * Monitors:
 * - Heap memory usage
 * - Loop execution time
 * - WiFi signal strength
 * - Uptime tracking
 */

#pragma once

#include <Arduino.h>
#include "Config.h"

class SystemMonitor {
public:
  SystemMonitor();
  
  /**
   * Initialize monitor
   */
  void begin();
  
  /**
   * Main loop - must be called regularly
   */
  void tick(unsigned long now);
  
  /**
   * Print system status to Serial
   */
  void printStatus() const;
  
  /**
   * Get current free heap
   */
  uint32_t getFreeHeap() const;
  
  /**
   * Get minimum free heap since boot
   */
  uint32_t getMinHeap() const;
  
  /**
   * Get average loop time in microseconds
   */
  uint32_t getAvgLoopTime() const;
  
  /**
   * Get uptime in seconds
   */
  uint32_t getUptimeSeconds() const;

private:
  uint32_t bootTime;
  uint32_t lastTickTime;
  uint32_t minHeap;
  float avgLoopTime;
  uint32_t loopCount;
  uint32_t slowLoopCount;
  
  /**
   * Check heap status and warn if low
   */
  void checkHeap();
  
  /**
   * Update loop time statistics
   */
  void updateLoopStats(uint32_t loopTime);
};
