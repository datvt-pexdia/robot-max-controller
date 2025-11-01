# robot-max-controller

A minimal yet production-ready system to orchestrate a three-device robot (arm, neck, wheels) using a Node.js command server and an ESP8266 firmware client. The firmware runs in REAL mode only.

## Project structure

```
project/
  server/            # Node.js command and WebSocket server (TypeScript)
    package.json
    tsconfig.json
    src/
      api.ts         # REST API routes
      config.ts      # Core configuration values
      demoData.ts    # Helpers to generate sample tasks
      index.ts       # Entry point
      logger.ts      # Timestamped console logger
      models.ts      # Shared schemas/types
      taskQueue.ts   # In-memory task bookkeeping for status reporting
      wsHub.ts       # ESP WebSocket connection manager
  firmware/          # ESP8266 Arduino firmware (REAL mode default)
    main/
      Config.h       # Wi-Fi + WebSocket + Meccano M.A.X configuration
      NetClient.*    # WebSocket client with reconnect logic
      TaskRunner.*   # Cooperative scheduler + device queues
      TaskTypes.h    # Shared task envelope struct
      main.ino       # Arduino setup/loop
      Devices/       # Device-specific controllers (REAL-only)
        ArmDevice.*
        NeckDevice.*
        WheelsDevice.*
```

## Prerequisites

- Node.js 20+
- npm 9+
- `arduino-cli` (for firmware compilation)

## Server setup

```bash
cd server
npm install
```

### Development server

```bash
npm run dev
```

The server starts on <http://localhost:8080> and exposes a WebSocket endpoint at `ws://localhost:8080/robot`. Startup logs include ready-made `curl` commands for quick testing.

### Build & production run

```bash
npm run build
npm start
```

## REST API

All endpoints accept/return JSON.

### Replace tasks immediately

Cancels any running task per device and starts the provided tasks right away (first item per device becomes active, additional tasks queued).

```bash
curl -X POST http://localhost:8080/robot/tasks/replace \
  -H "Content-Type: application/json" \
  -d '{
    "tasks":[
      {"taskId":"t1","device":"wheels","type":"drive","left":60,"right":60,"durationMs":1500},
      {"taskId":"t2","device":"arm","type":"moveAngle","angle":120}
    ]
  }'
```

### Enqueue tasks (preserve currently running task)

```bash
curl -X POST http://localhost:8080/robot/tasks/enqueue \
  -H "Content-Type: application/json" \
  -d '{
    "tasks":[
      {"taskId":"t3","device":"wheels","type":"drive","left":0,"right":80,"durationMs":800,"enqueue":true},
      {"taskId":"t4","device":"wheels","type":"drive","left":80,"right":0,"durationMs":800,"enqueue":true}
    ]
  }'
```

### Cancel a device

```bash
curl -X POST http://localhost:8080/robot/tasks/cancel \
  -H "Content-Type: application/json" \
  -d '{"device":"wheels"}'
```

### Server + queue status

```bash
curl http://localhost:8080/robot/status
```

Example response:

```json
{
  "connected": false,
  "lastHello": null,
  "devices": {
    "arm": { "runningTaskId": null, "queueSize": 0, "lastUpdated": null },
    "neck": { "runningTaskId": null, "queueSize": 0, "lastUpdated": null },
    "wheels": { "runningTaskId": null, "queueSize": 0, "lastUpdated": null }
  },
  "queueSizes": { "arm": 0, "neck": 0, "wheels": 0 }
}
```

## ESP8266 firmware

The firmware is designed for NodeMCU-style ESP8266 boards and runs in REAL mode only.

**Configure Wi-Fi credentials:** Create `firmware/main/Config.local.h` with your credentials:
```c
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASS "YourPassword"
#define WS_HOST "192.168.1.5"  // Optional: override server IP
```

This file is gitignored and will not be committed. Defaults are provided in `Config.h` for initial testing.

**Required libraries:**
- `arduinoWebSockets >= 2.3.6`
- `ArduinoJson v6`

### Meccano M.A.X Configuration

Configure the MAX bus wiring in `firmware/main/Config.h`:

- `MAX_DATA_PIN`: ESP8266 pin connected to MAX bus data wire (default: `D4`)
- `MAX_LEFT_POS`: Device position of left motor on MAX chain (default: `0`)
- `MAX_RIGHT_POS`: Device position of right motor on MAX chain (default: `1`)
- `LEFT_FORWARD_IS_CCW`: `1` if left wheel forward = CCW, `0` if forward = CW (default: `1`)
- `RIGHT_FORWARD_IS_CCW`: `1` if right wheel forward = CCW, `0` if forward = CW (default: `0`)

If the robot moves backward when commanded to go forward, swap the `LEFT_FORWARD_IS_CCW`/`RIGHT_FORWARD_IS_CCW` values.

### Arduino CLI setup

```bash
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli lib install "arduinoWebSockets@>=2.3.6" "ArduinoJson@6"
arduino-cli lib install "MeccaChannel" "MeccaMaxDrive"
```

### Compile

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 ./firmware/main
```

### Upload (adjust serial port)

```bash
arduino-cli upload -p COM13 --fqbn esp8266:esp8266:nodemcuv2 ./firmware/main
```

### Serial monitor

```bash
arduino-cli monitor -p COM13 -c baudrate=115200
```

## Firmware behaviour highlights

- **Single active client:** when the ESP connects, the server buffers any pending tasks and flushes them after the handshake.
- **Replace vs enqueue:** replace cancels current + future tasks for each device; enqueue preserves the current task and appends to the queue.
- **Task lifecycle callbacks:** the ESP responds with `ack`, `progress`, `done`, or `error` messages for each task so the server always knows the state.
- **Robust reconnects:** Wi-Fi and WebSocket connections auto-retry with exponential backoff (1s → 5s). When the socket drops, all in-flight tasks are canceled to prevent desynchronisation.
- **Heartbeats:** the server pings every **15s**; if no `pong` within **30s** the socket is dropped.
- **Wheels Continuous Mode:** refresh **~30 Hz (every 33 ms)**; includes slew-rate smoothing, soft/hard stop safety, and keep-alive to prevent devices from dozing.

## Troubleshooting

- **ESP never connects:** confirm `WS_HOST` points to your server's LAN IP and that the port (8080) is reachable. Check the serial monitor for `[WIFI]` and `[NET]` logs.
- **Server status shows `connected:false`:** ensure the firmware is running and Wi-Fi is stable. The ESP sends a `hello` packet on connect which updates `lastHello`.
- **Tasks not executing:** watch serial logs for cancellation messages. Replace requests preempt running tasks per device.

## Meccano M.A.X (REAL-only)

The firmware controls Meccano M.A.X hardware using the MAX bus protocol.

**Mapping (community-verified):**
- Direction nybble: **0x2n = CW**, **0x3n = CCW**
- Speed codes: **0x40 = STOP**, **0x42..0x4F** (14 steps; **0x41 unused**)

**Configuration constants** (see `firmware/main/Config.h`):
```c
static constexpr uint8_t MAX_DATA_PIN = D4;
static constexpr uint8_t MAX_LEFT_POS = 0;
static constexpr uint8_t MAX_RIGHT_POS = 1;
static constexpr bool LEFT_FORWARD_IS_CCW = true;
static constexpr bool RIGHT_FORWARD_IS_CCW = false;
static constexpr uint32_t WHEELS_TICK_MS = 33;
static constexpr uint32_t SOFT_STOP_TIMEOUT_MS = 150;
static constexpr uint32_t HARD_STOP_TIMEOUT_MS = 400;
static constexpr uint32_t MAX_KEEPALIVE_MS = 250;
```

**MAX Protocol Constants** (see `firmware/main/Config.h`):
- Speed commands: `MAXProtocol::CMD_STOP` (0x40), `CMD_SPEED_MIN` (0x42), `CMD_SPEED_MAX` (0x4F)
- Direction helpers: `MAXProtocol::dirByte(pos, ccw)` creates direction bytes (0x2n for CW, 0x3n for CCW)

**Notes:** Commands are emitted at **~30 Hz**; writes to bus happen on change or every `MAX_KEEPALIVE_MS` to prevent devices from dozing. Soft stop activates after **150ms** of no commands; hard stop triggers after **400ms**.

## Quick tests

Test wheels control with these commands (replace `<SERVER_IP>` with your server's IP address):

```bash
# Straight forward for 1.5s
curl -X POST http://<SERVER_IP>:8080/robot/tasks/replace \
  -H "Content-Type: application/json" \
  -d '{"tasks":[{"taskId":"t1","device":"wheels","type":"drive","left":60,"right":60,"durationMs":1500}]}'

# In-place left turn for 0.8s
curl -X POST http://<SERVER_IP>:8080/robot/tasks/replace \
  -H "Content-Type: application/json" \
  -d '{"tasks":[{"taskId":"t2","device":"wheels","type":"drive","left":-50,"right":50,"durationMs":800}]}'
```

## Recent Refactoring Changes

The codebase has been refactored for better maintainability and efficiency:

- **Configuration Management:** Credentials moved to `Config.local.h` (gitignored) for security
- **Type Safety:** Replaced `#define` macros with `constexpr` constants where possible
- **Protocol Constants:** Created `Protocol.h` and `MAXProtocol` namespace for consistent command strings and MAX bus commands
- **State Machines:** Replaced boolean flags with `DeviceState` enum for Arm/Neck devices
- **Non-blocking Wi-Fi:** Wi-Fi connection is now non-blocking, checked periodically in `loop()`
- **Memory Optimization:** JSON serialization uses pre-allocated char buffers instead of String objects
- **Error Handling:** Added MAX bus communication error tracking and validation in command handlers
- **Code Cleanup:** Removed unused functions and improved inline documentation

## Next steps

- Integrate actual Servo hardware for Arm/Neck devices (currently TODO placeholders).
- Extend `NetClient` to persist outbound telemetry if desired (e.g., local buffering when offline).
- Add authentication by replacing the `JWT_DISABLED` placeholder once security requirements are defined.
