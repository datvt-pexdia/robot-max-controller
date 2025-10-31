# robot-max-controller

A minimal yet production-ready system to orchestrate a three-device robot (arm, neck, wheels) using a Node.js command server and an ESP8266 firmware client. The firmware runs in simulation mode by default so you can exercise the full stack without motors attached.

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
  firmware/          # ESP8266 Arduino firmware (simulation-first)
    src/
      Config.h       # Wi-Fi + WebSocket configuration
      NetClient.*    # WebSocket client with reconnect logic
      TaskRunner.*   # Cooperative scheduler + device queues
      TaskTypes.h    # Shared task envelope struct
      main.ino       # Arduino setup/loop
      Devices/       # Device-specific controllers (simulation aware)
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

The firmware is designed for NodeMCU-style ESP8266 boards. It defaults to **REAL mode** (`SIMULATION false`) which controls actual Meccano M.A.X hardware. Set `SIMULATION true` to log actions to serial monitor instead.

Update `firmware/main/Config.h` with your Wi-Fi credentials and server IP before flashing.

### Meccano M.A.X Configuration

When running in REAL mode (`SIMULATION false`), configure the MAX bus wiring in `Config.h`:

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
arduino-cli lib install "arduinoWebSockets" "ArduinoJson"
```

### Compile

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 ./firmware
```

### Upload (adjust serial port)

```bash
arduino-cli upload -p COM13 --fqbn esp8266:esp8266:nodemcuv2 ./firmware
```

### Serial monitor

```bash
arduino-cli monitor -p COM13 -c baudrate=115200
```

## Firmware behaviour highlights

- **Simulation friendly:** every device action (`[ARM]`, `[NECK]`, `[WHEELS]`) logs to serial with angles, speeds, durations, and completion notices.
- **Single active client:** when the ESP connects, the server buffers any pending tasks and flushes them after the handshake.
- **Replace vs enqueue:** replace cancels current + future tasks for each device; enqueue preserves the current task and appends to the queue.
- **Task lifecycle callbacks:** the ESP responds with `ack`, `progress`, `done`, or `error` messages for each task so the server always knows the state.
- **Robust reconnects:** Wi-Fi and WebSocket connections auto-retry with exponential backoff (1s → 5s). When the socket drops, all in-flight tasks are canceled to prevent desynchronisation.
- **Heartbeats:** the server pings the ESP every 15s (`WS_HEARTBEAT_INTERVAL_MS`) and treats missing pongs after 30s as a disconnect. The ESP responds with `pong` to keep the connection alive.
- **Wheels Continuous Mode:** the wheels device runs a continuous task that sends motor commands at **~30 Hz (33ms intervals)** to keep the MAX modules responsive. Speed is mapped to 14 levels (0x42-0x4F), with 0x40 for STOP. Direction is encoded as 0x2n (CW) or 0x3n (CCW). Features slew-rate limiting and soft/hard stop timeouts (150ms/400ms). See `firmware/main/WHEELS_CONTINUOUS_MODE.md` for details.

## Troubleshooting

- **ESP never connects:** confirm `WS_HOST` points to your server's LAN IP and that the port (8080) is reachable. Check the serial monitor for `[WIFI]` and `[NET]` logs.
- **Server status shows `connected:false`:** ensure the firmware is running and Wi-Fi is stable. The ESP sends a `hello` packet on connect which updates `lastHello`.
- **Tasks not executing:** watch serial logs for cancellation messages. Replace requests preempt running tasks per device.

## Hardware Integration

The firmware now supports **real Meccano M.A.X motor control** when `SIMULATION` is set to `false`:

- Motor commands are sent via the MAX bus at 30 Hz for responsive control
- Speed mapping: normalized values [-1..1] → 14 speed levels (0x42-0x4F) or STOP (0x40)
- Direction: CW (0x2n) or CCW (0x3n) based on configured forward direction per wheel
- Automatic stop: soft stop after 150ms without commands, hard stop after 400ms

Ensure you have the Meccano M.A.X libraries installed:
```bash
arduino-cli lib install "MeccaChannel" "MeccaMaxDrive"
```

## Next steps

- Extend `NetClient` to persist outbound telemetry if desired (e.g., local buffering when offline).
- Add authentication by replacing the `JWT_DISABLED` placeholder once security requirements are defined.
