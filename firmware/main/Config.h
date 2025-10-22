#pragma once

#define SIMULATION true

static const char* WIFI_SSID = "Pex Tower 5 Floor";
static const char* WIFI_PASS = "P07052018";
static const char* WS_HOST = "10.0.5.97";
static const uint16_t WS_PORT = 8080;
static const char* WS_PATH = "/robot";
static const uint32_t WS_RECONNECT_BASE_MS = 1000;
static const uint32_t WS_RECONNECT_MAX_MS = 5000;
