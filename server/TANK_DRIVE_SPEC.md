# Tank Drive Control System - Implementation Status

## ✅ Đã triển khai (UI + Server)

### UI Layer (`server/public/controller.js`)
- ✅ **Normalization Pipeline**
  - Dead-zone: 0.06 (configurable)
  - Slew-rate: SR_UP=3.0/s, SR_DOWN=6.0/s
  - Quantization: 12 levels
- ✅ **Timing**: 40Hz tick (25ms) với keepalive 150ms
- ✅ **STOP Semantics**: Flush STOP on pagehide/unload/blur
- ✅ **Sequence numbering**: Monotonic seq per session
- ✅ **Monotonic timestamp**: performance.now() based

### Server Layer (`server/src/driveRelay.ts`)
- ✅ **Watchdog**: 300ms UI timeout → synthesize STOP
- ✅ **Validation**: Shape, ranges, clamp [-1..1]
- ✅ **Role Arbitration**: Last UI wins, devices subscribe
- ✅ **Seq forwarding**: Maintains monotonicity

### Bridge Layer (`server/src/deviceAdapter.ts`)
- ✅ **Protocol conversion**: DriveIntent → Legacy WsHub format
- ✅ **Deduplication**: Skip if same as last applied
- ✅ **Seq checking**: Drop out-of-order packets

## ⏳ Cần triển khai (Firmware ESP)

### Device Layer (Cần update firmware)
- ⏳ **Watchdog**: 350ms timeout → stopAll()
- ⏳ **Seq ordering**: Reject seq <= lastSeq
- ⏳ **Mapping**: Intent [-1..1] → Motor (dir, level)
- ⏳ **Ramp logic**: Smooth level transitions
- ⏳ **Immediate brake**: Cancel ramps on STOP

## Protocol Format

### UI → Server
```json
{
  "type": "drive",
  "left": -0.75,      // [-1..1]
  "right": 0.50,      // [-1..1]
  "seq": 42,          // uint32
  "ts": 173           // ms since UI load
}
```

### Server → Device (via DeviceAdapter → WsHub)
```json
{
  "device": "wheels",
  "type": "drive",
  "left": -75,        // [-100..100]
  "right": 50,        // [-100..100]
  "taskId": "drive-seq42"
}
```

## Safety Layers

1. **UI Safety**
   - ✅ Keepalive every ≤150ms
   - ✅ STOP on pagehide/unload
   - ✅ STOP on blur

2. **Server Safety**
   - ✅ 300ms UI silence → synth STOP
   - ✅ Validation & clamping
   - ✅ Seq monotonicity

3. **Device Safety** (Cần firmware update)
   - ⏳ 350ms silence → stopAll()
   - ⏳ Lower seq → drop
   - ⏳ Malformed repeatedly → stopAll() + backoff

## Cách sử dụng

### 1. Khởi động server
```bash
cd server
npm run dev
```

### 2. Truy cập giao diện
Mở browser: `http://localhost:8080`

### 3. Điều khiển
- Kéo cần điều khiển lên/xuống
- Nhả về 0 = STOP
- Nút STOP khẩn cấp

## Next Steps để hoàn thiện

1. **Update ESP Firmware** để implement:
   - Device watchdog 350ms
   - Seq ordering check
   - Smooth mapping với ramp logic
   - Immediate brake on STOP

2. **Tuning Parameters**:
   - `DZ`: dead-zone threshold
   - `SR_UP/SR_DOWN`: slew rates
   - `QUANTIZE_LEVELS`: số levels
   - Motor level table: mapping |v| → physical levels

## Lợi ích của kiến trúc mới

✅ **Mượt mà**: Normalization pipeline + slew-rate
✅ **An toàn**: Triple-layer watchdog
✅ **Đúng thứ tự**: Seq monotonicity
✅ **Deterministic**: Quantization + level mapping
✅ **Responsive**: 40Hz UI, 10ms motor tick

