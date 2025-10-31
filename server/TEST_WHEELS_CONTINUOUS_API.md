# Test Wheels Continuous Mode - REST API

Script test cho Wheels Continuous Mode sử dụng REST API thay vì WebSocket trực tiếp.

## Yêu cầu

1. **Server đang chạy**:
```bash
cd server
npm start
```

2. **ESP8266 đã kết nối** với server

3. **Node.js packages**:
```bash
npm install  # axios đã có trong package.json
```

## Sử dụng

### 1. Chạy tất cả tests (12 tests)

```bash
node test-wheels-continuous-api.js
```

Hoặc với custom server:
```bash
SERVER_HOST=192.168.1.6 SERVER_PORT=8080 node test-wheels-continuous-api.js
```

### 2. Chạy một test cụ thể

```bash
node test-wheels-continuous-api.js --test 2
```

Ví dụ:
- `--test 1`: STOP Signal
- `--test 2`: Di chuyển tiến
- `--test 3`: Thay đổi tốc độ
- `--test 8`: Chuỗi lệnh liên tục

### 3. Liệt kê các tests

```bash
node test-wheels-continuous-api.js --list
```

## Danh sách Tests

### Test 1: STOP Signal
Kiểm tra gửi STOP liên tục
- Gửi: `left=0, right=0`
- Kỳ vọng: Bánh xe không quay

### Test 2: Di chuyển tiến
Kiểm tra chuyển từ STOP sang di chuyển
- Gửi: `left=50, right=50`
- Kỳ vọng: Bánh xe quay tiến với tốc độ 50%

### Test 3: Thay đổi tốc độ
Kiểm tra thay đổi tốc độ trong khi di chuyển
- Gửi: 40% → 80% → 30%
- Kỳ vọng: Tốc độ thay đổi mượt mà

### Test 4: Rẽ trái
Kiểm tra điều khiển độc lập từng bánh
- Gửi: `left=30, right=70`
- Kỳ vọng: Robot rẽ trái

### Test 5: Rẽ phải
Kiểm tra rẽ phải
- Gửi: `left=70, right=30`
- Kỳ vọng: Robot rẽ phải

### Test 6: Di chuyển lùi
Kiểm tra hướng ngược
- Gửi: `left=-50, right=-50`
- Kỳ vọng: Bánh xe quay lùi

### Test 7: Xoay tại chỗ
Kiểm tra xoay (một bánh tiến, một bánh lùi)
- Gửi: `left=-50, right=50` (xoay trái)
- Gửi: `left=50, right=-50` (xoay phải)
- Kỳ vọng: Robot xoay tại chỗ

### Test 8: Chuỗi lệnh liên tục
Kiểm tra xử lý nhiều lệnh liên tiếp
- Gửi: Forward 50% → 80% → Turn left → Turn right → Backward → Stop
- Kỳ vọng: Chuyển động mượt mà, không delay

### Test 9: Stress Test
Kiểm tra xử lý lệnh thay đổi rất nhanh
- Gửi: 10 lệnh ngẫu nhiên liên tục (mỗi 300ms)
- Kỳ vọng: Hệ thống không treo, chuyển động theo lệnh cuối

### Test 10: Edge Cases
Kiểm tra các trường hợp đặc biệt
- Tốc độ rất thấp (10%)
- Tốc độ tối đa (100%)
- Một bánh dừng, một bánh chạy
- Giá trị âm tối đa (-100%)
- Kỳ vọng: Xử lý đúng tất cả trường hợp

### Test 11: Cancel Test
Kiểm tra hành vi cancel
- Bắt đầu di chuyển → Cancel → Di chuyển lại
- Kỳ vọng: Cancel dừng bánh xe, có thể di chuyển lại

### Test 12: Status Check
Kiểm tra status API
- Lấy status trước/trong/sau khi di chuyển
- Kỳ vọng: Status phản ánh đúng trạng thái

## Output mẫu

```
[2025-10-25T16:30:00.000Z] ╔════════════════════════════════════════╗
[2025-10-25T16:30:00.000Z] ║  WHEELS CONTINUOUS MODE - TEST SUITE  ║
[2025-10-25T16:30:00.000Z] ║           (REST API)                   ║
[2025-10-25T16:30:00.000Z] ╚════════════════════════════════════════╝

[2025-10-25T16:30:00.100Z] → Kiểm tra kết nối server...
[2025-10-25T16:30:00.150Z] ✓ ESP đã kết nối

[2025-10-25T16:30:01.000Z] ╔════════════════════════════════════════╗
[2025-10-25T16:30:01.000Z] ║  TEST 1: STOP Signal                  ║
[2025-10-25T16:30:01.000Z] ╚════════════════════════════════════════╝
[2025-10-25T16:30:01.000Z] Mục đích: Kiểm tra gửi STOP liên tục
[2025-10-25T16:30:01.000Z] Kỳ vọng: Bánh xe không quay, logs hiển thị [00 00 FE FE]
[2025-10-25T16:30:01.100Z] → Gửi task: L=0 R=0 duration=1000ms
[2025-10-25T16:30:01.200Z] ✓ Response: {"accepted":1}
```

## API Endpoints được sử dụng

### POST /robot/tasks/enqueue
Gửi task mới cho wheels (enqueue mode - không cancel task hiện tại)

Request:
```json
{
  "tasks": [{
    "taskId": "test_forward_1234567890_0",
    "device": "wheels",
    "type": "drive",
    "left": 50,
    "right": 50,
    "durationMs": 2000
  }]
}
```

Response:
```json
{
  "accepted": 1
}
```

### POST /robot/tasks/cancel
Cancel task đang chạy của wheels

Request:
```json
{
  "device": "wheels"
}
```

Response:
```json
{
  "ok": true
}
```

### GET /robot/status
Lấy trạng thái hiện tại của robot

Response:
```json
{
  "connected": true,
  "lastHello": "2025-10-25T16:30:00.000Z",
  "devices": {
    "wheels": {
      "runningTaskId": "test_forward_1234567890_0",
      "queueSize": 0,
      "lastUpdated": "2025-10-25T16:30:01.000Z"
    }
  }
}
```

## Continuous Mode Behavior

### Khi gửi task mới trong continuous mode:

1. **Task đầu tiên** (từ STOP):
   - ESP gửi speed byte một lần
   - ESP bắt đầu gửi direction byte liên tục (mỗi 10ms)

2. **Task tiếp theo** (thay đổi tốc độ):
   - ESP gửi speed byte mới một lần
   - Tiếp tục gửi direction byte liên tục

3. **Task STOP** (left=0, right=0):
   - ESP quay về gửi STOP byte liên tục (mỗi 10ms)

4. **Cancel**:
   - Trong continuous mode: chuyển về trạng thái STOP
   - Task vẫn chạy, tiếp tục gửi STOP signal

## Debugging

### Bật debug logs trên ESP

Trong `firmware/main/Devices/WheelsDevice.cpp`:
```cpp
#define WHEELS_DEBUG 1
```

Upload lại firmware và mở Serial Monitor để xem logs chi tiết.

### Kiểm tra server logs

Server sẽ log tất cả requests và WebSocket messages:
```
2025-10-25T16:30:01.000Z [HTTP] POST /robot/tasks/enqueue {"tasks":[...]}
2025-10-25T16:30:01.100Z [WS] → ESP: {"type":"tasks","mode":"enqueue","tasks":[...]}
2025-10-25T16:30:01.200Z [WS] ← ESP: {"kind":"ack","taskId":"test_forward_1234567890_0"}
```

### Kiểm tra ESP logs

ESP sẽ log chi tiết về wheels:
```
[WHEELS] tryUpdate: start moving L=50 R=50
[WHEELS] setSpeed L=49 R=49
[WHEELS] sendRL: [49 49 FE FE] (R,L)  // Speed
[WHEELS] sendRL: [24 34 FE FE] (R,L)  // Direction
```

## Troubleshooting

### Server không phản hồi
```bash
# Kiểm tra server đang chạy
curl http://localhost:8080/robot/status

# Nếu không chạy, start server
cd server && npm start
```

### ESP chưa kết nối
```bash
# Kiểm tra status
curl http://localhost:8080/robot/status

# Nếu connected: false, kiểm tra:
# 1. ESP đã upload firmware mới chưa?
# 2. Config.h có đúng WS_HOST không?
# 3. Serial Monitor có log gì không?
```

### Test timeout
```bash
# Tăng delay giữa các tests
DELAY_BETWEEN_TESTS=5000 node test-wheels-continuous-api.js
```

### Bánh xe không quay
```bash
# Kiểm tra:
# 1. Motor đã kết nối đúng channel chưa?
# 2. Serial Monitor có log "Motor device detected!" không?
# 3. SIMULATION mode có đang bật không? (trong Config.h)
```

## So sánh với WebSocket test

### WebSocket test (`firmware/example/test-wheels-continuous.js`)
- Kết nối trực tiếp đến WebSocket
- Gửi message theo protocol WebSocket
- Nhận events realtime (ack, progress, done, error)
- Phù hợp cho: Test low-level, debug protocol

### REST API test (`server/test-wheels-continuous-api.js`)
- Sử dụng REST API endpoints
- Đơn giản hơn, dễ hiểu hơn
- Không nhận events realtime (phải poll status)
- Phù hợp cho: Test high-level, integration test

## Kết luận

Script này cung cấp một cách đơn giản để test Wheels Continuous Mode qua REST API. Sử dụng script này để:

1. ✅ Kiểm tra continuous mode hoạt động đúng
2. ✅ Test các trường hợp edge cases
3. ✅ Stress test hệ thống
4. ✅ Verify API endpoints
5. ✅ Debug issues

Để có logs chi tiết hơn, sử dụng WebSocket test hoặc bật WHEELS_DEBUG trên ESP.

