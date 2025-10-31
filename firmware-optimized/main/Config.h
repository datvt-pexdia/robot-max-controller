/**
 * Configuration file for Robot MAX Controller
 * 
 * Centralized configuration for all firmware settings
 */

#pragma once

// ════════════════════════════════════════════════════════════════
// FIRMWARE INFO
// ════════════════════════════════════════════════════════════════
#define FIRMWARE_VERSION "2.0.0"
#define DEVICE_HOSTNAME "robot-max"

// ════════════════════════════════════════════════════════════════
// SERIAL COMMUNICATION
// ════════════════════════════════════════════════════════════════
#define SERIAL_BAUD 115200
#define DEBUG_ENABLED true

// ════════════════════════════════════════════════════════════════
// WIFI CONFIGURATION
// ════════════════════════════════════════════════════════════════
#ifndef WIFI_SSID
  #define WIFI_SSID "Huy Minh"
#endif

#ifndef WIFI_PASS
  #define WIFI_PASS "23052004"
#endif

// WiFi connection parameters
#define WIFI_CONNECT_RETRIES 30        // Maximum connection attempts
#define WIFI_CHECK_INTERVAL 5000       // WiFi status check interval (ms)
#define WIFI_RECONNECT_DELAY 500       // Delay between reconnection attempts (ms)

// ════════════════════════════════════════════════════════════════
// WEBSOCKET CONFIGURATION
// ════════════════════════════════════════════════════════════════
#define WS_HOST "192.168.1.6"
#define WS_PORT 8080
#define WS_PATH "/robot"

// WebSocket reconnection parameters
#define WS_RECONNECT_BASE_MS 1000      // Base reconnect delay
#define WS_RECONNECT_MAX_MS 10000      // Maximum backoff delay

// WebSocket heartbeat parameters
#define WS_HEARTBEAT_INTERVAL_MS 15000 // Ping interval
#define WS_HEARTBEAT_TIMEOUT_MS 3000   // Pong timeout
#define WS_HEARTBEAT_TRIES 2           // Retries before disconnect

// ════════════════════════════════════════════════════════════════
// OTA (OVER-THE-AIR) UPDATE
// ════════════════════════════════════════════════════════════════
#define OTA_ENABLED true
#define OTA_PASSWORD "robot-max-2024"
#define OTA_PORT 8266

// ════════════════════════════════════════════════════════════════
// WATCHDOG TIMER
// ════════════════════════════════════════════════════════════════
#define WATCHDOG_ENABLED true
#define WATCHDOG_TIMEOUT 8000          // Watchdog timeout (ms)

// ════════════════════════════════════════════════════════════════
// TASK RUNNER CONFIGURATION
// ════════════════════════════════════════════════════════════════
#define PROGRESS_REPORT_INTERVAL 200   // Progress update interval (ms)
#define MAX_TASK_QUEUE_SIZE 10         // Maximum tasks per device

// ════════════════════════════════════════════════════════════════
// WHEELS CONTINUOUS TASK CONFIGURATION
// ════════════════════════════════════════════════════════════════
// Signal timing (based on Meccano MAX protocol documentation)
#define WHEELS_SIGNAL_INTERVAL 30      // Signal interval (ms) - 30ms = 33Hz
#define WHEELS_COMMAND_TIMEOUT 1000    // Command timeout (ms)

// Acceleration/deceleration parameters
#define WHEELS_ACCEL_ENABLED true      // Enable smooth acceleration
#define WHEELS_ACCEL_STEP 10           // Speed change per tick (%)
#define WHEELS_ACCEL_INTERVAL 50       // Acceleration tick interval (ms)

// Motor initialization
#define MOTOR_INIT_RETRIES 50          // Max retries for motor connection
#define MOTOR_INIT_INTERVAL 100        // Retry interval (ms)
#define MOTOR_PIN D4                   // ESP8266 GPIO2 (D4)

// Debug flags
#define WHEELS_DEBUG false             // Enable wheels debug logging
#define MOTOR_DEBUG false              // Enable motor debug logging

// ════════════════════════════════════════════════════════════════
// MECCANO MAX PROTOCOL CONSTANTS
// ════════════════════════════════════════════════════════════════
// Speed range: 0x42-0x4F (66-79 decimal)
#define MECCANO_SPEED_STOP 0x40        // Stop command
#define MECCANO_SPEED_MIN 0x42         // Minimum running speed
#define MECCANO_SPEED_MAX 0x4F         // Maximum speed

// Direction constants (from protocol documentation)
#define MECCANO_DIR_STOP 0x00          // No rotation

// Left wheel directions
#define MECCANO_DIR_L_FWD 0x34         // Left anti-clockwise (forward)
#define MECCANO_DIR_L_REV 0x24         // Left clockwise (reverse)

// Right wheel directions  
#define MECCANO_DIR_R_FWD 0x24         // Right clockwise (forward)
#define MECCANO_DIR_R_REV 0x34         // Right anti-clockwise (reverse)

// Placeholder bytes for unused channels
#define MECCANO_PLACEHOLDER 0xFE       // Unused channel marker

// ════════════════════════════════════════════════════════════════
// SYSTEM MONITORING
// ════════════════════════════════════════════════════════════════
#define SYSTEM_MONITOR_ENABLED true
#define STATUS_PRINT_INTERVAL 60000    // Status print interval (ms) - 1 minute
#define HEAP_WARNING_THRESHOLD 5000    // Low heap warning threshold (bytes)
#define LOOP_TIME_WARNING 50           // Slow loop warning threshold (ms)

// ════════════════════════════════════════════════════════════════
// MAIN LOOP TIMING
// ════════════════════════════════════════════════════════════════
#define MAIN_LOOP_DELAY 5              // Main loop delay (ms)

// ════════════════════════════════════════════════════════════════
// SIMULATION MODE
// ════════════════════════════════════════════════════════════════
#define SIMULATION false               // Enable simulation mode (no real hardware)

// ════════════════════════════════════════════════════════════════
// VALIDATION MACROS
// ════════════════════════════════════════════════════════════════
// Constrain value to range
#define CONSTRAIN_SPEED(x) constrain(x, 0, 100)
#define CONSTRAIN_DIRECTION(x) constrain(x, -100, 100)
#define CONSTRAIN_ANGLE(x) constrain(x, 0, 180)

// ════════════════════════════════════════════════════════════════
// LOGGING MACROS
// ════════════════════════════════════════════════════════════════
#if DEBUG_ENABLED
  #define DEBUG_LOG(tag, msg) Serial.printf("[%s] %s\n", tag, msg)
  #define DEBUG_LOGF(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, __VA_ARGS__)
#else
  #define DEBUG_LOG(tag, msg)
  #define DEBUG_LOGF(tag, fmt, ...)
#endif

// Error logging (always enabled)
#define ERROR_LOG(tag, msg) Serial.printf("[ERROR:%s] %s\n", tag, msg)
#define ERROR_LOGF(tag, fmt, ...) Serial.printf("[ERROR:%s] " fmt "\n", tag, __VA_ARGS__)

// Warning logging (always enabled)
#define WARN_LOG(tag, msg) Serial.printf("[WARN:%s] %s\n", tag, msg)
#define WARN_LOGF(tag, fmt, ...) Serial.printf("[WARN:%s] " fmt "\n", tag, __VA_ARGS__)
