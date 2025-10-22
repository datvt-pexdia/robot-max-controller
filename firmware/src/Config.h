#pragma once

#define SIMULATION true

static const char* WIFI_SSID = "YOUR_SSID";
static const char* WIFI_PASS = "YOUR_PASSWORD";
static const char* WS_HOST = "192.168.1.100";
static const uint16_t WS_PORT = 8080;
static const char* WS_PATH = "/robot";
static const uint32_t WS_RECONNECT_BASE_MS = 1000;
static const uint32_t WS_RECONNECT_MAX_MS = 5000;
