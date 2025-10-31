/**
 * SystemMonitor implementation
 */

#include "SystemMonitor.h"
#include <ESP8266WiFi.h>

SystemMonitor::SystemMonitor()
  : bootTime(0),
    lastTickTime(0),
    minHeap(0xFFFFFFFF),
    avgLoopTime(0),
    loopCount(0),
    slowLoopCount(0) {
}

void SystemMonitor::begin() {
  bootTime = millis();
  lastTickTime = micros();
  minHeap = ESP.getFreeHeap();
  
  Serial.println(F("[MONITOR] System monitor initialized"));
}

void SystemMonitor::tick(unsigned long now) {
  // Calculate loop time
  uint32_t currentTime = micros();
  uint32_t loopTime = currentTime - lastTickTime;
  lastTickTime = currentTime;
  
  // Update statistics
  updateLoopStats(loopTime);
  checkHeap();
  
  loopCount++;
}

void SystemMonitor::printStatus() const {
  Serial.println(F("════════════════════════════════"));
  Serial.println(F("    SYSTEM STATUS REPORT"));
  Serial.println(F("════════════════════════════════"));
  
  // Uptime
  uint32_t uptimeSec = getUptimeSeconds();
  uint32_t days = uptimeSec / 86400;
  uint32_t hours = (uptimeSec % 86400) / 3600;
  uint32_t minutes = (uptimeSec % 3600) / 60;
  uint32_t seconds = uptimeSec % 60;
  
  Serial.printf("Uptime:        %ud %02u:%02u:%02u\n", days, hours, minutes, seconds);
  
  // Memory
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t heapFragmentation = ESP.getHeapFragmentation();
  Serial.printf("Free Heap:     %u bytes\n", freeHeap);
  Serial.printf("Min Heap:      %u bytes\n", minHeap);
  Serial.printf("Fragmentation: %u%%\n", heapFragmentation);
  
  if (freeHeap < HEAP_WARNING_THRESHOLD) {
    Serial.println(F("⚠ WARNING: Low heap memory!"));
  }
  
  // Performance
  Serial.printf("Loop Count:    %u\n", loopCount);
  Serial.printf("Avg Loop Time: %.2f ms\n", avgLoopTime / 1000.0);
  Serial.printf("Slow Loops:    %u (%.2f%%)\n", 
                slowLoopCount, 
                loopCount > 0 ? (slowLoopCount * 100.0 / loopCount) : 0);
  
  // WiFi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi RSSI:     %d dBm\n", WiFi.RSSI());
    Serial.printf("WiFi IP:       %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F("WiFi Status:   Disconnected"));
  }
  
  // Chip info
  Serial.printf("Chip ID:       %08X\n", ESP.getChipId());
  Serial.printf("Flash Size:    %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("CPU Freq:      %u MHz\n", ESP.getCpuFreqMHz());
  
  Serial.println(F("════════════════════════════════\n"));
}

uint32_t SystemMonitor::getFreeHeap() const {
  return ESP.getFreeHeap();
}

uint32_t SystemMonitor::getMinHeap() const {
  return minHeap;
}

uint32_t SystemMonitor::getAvgLoopTime() const {
  return (uint32_t)avgLoopTime;
}

uint32_t SystemMonitor::getUptimeSeconds() const {
  return (millis() - bootTime) / 1000;
}

void SystemMonitor::checkHeap() {
  uint32_t currentHeap = ESP.getFreeHeap();
  
  // Update minimum heap
  if (currentHeap < minHeap) {
    minHeap = currentHeap;
  }
  
  // Warn if heap is critically low
  if (currentHeap < HEAP_WARNING_THRESHOLD) {
    static unsigned long lastWarning = 0;
    unsigned long now = millis();
    
    // Only warn once per minute to avoid spam
    if (now - lastWarning >= 60000) {
      lastWarning = now;
      WARN_LOGF("MONITOR", "Low heap memory: %u bytes", currentHeap);
    }
  }
}

void SystemMonitor::updateLoopStats(uint32_t loopTime) {
  // Exponential moving average
  // EMA formula: EMA_new = alpha * value + (1 - alpha) * EMA_old
  // Using alpha = 0.1 for smooth averaging
  const float alpha = 0.1;
  avgLoopTime = alpha * loopTime + (1.0 - alpha) * avgLoopTime;
  
  // Track slow loops
  if (loopTime > LOOP_TIME_WARNING * 1000) {  // Convert ms to us
    slowLoopCount++;
    
    // Warn about very slow loops
    if (loopTime > LOOP_TIME_WARNING * 2000) {
      WARN_LOGF("MONITOR", "Very slow loop: %.2f ms", loopTime / 1000.0);
    }
  }
}
