# Test Plan - Wheels Continuous Mode

## Kịch bản test

### Test 1: Khởi động và STOP signal
**Mục đích**: Kiểm tra task khởi động và gửi STOP liên tục

**Các bước**:
1. Upload firmware lên ESP8266
2. Mở Serial Monitor (115200 baud)
3. Quan sát logs

**Kết quả mong đợi**:
```
robot-max-controller firmware boot
[WIFI] connecting to ...
[WIFI] connected, IP=...
[MAIN] Starting continuous wheels task...
[WHEELS] Continuous task started - will send STOP signals continuously
[WHEELS] Initializing Meccano MAX motors...
```

**Với WHEELS_DEBUG=1**:
```
[WHEELS] sendRL: [00 00 FE FE] (R,L)  // Gửi STOP liên tục mỗi 10ms
```

**Kiểm tra vật lý**: Bánh xe không quay

---

### Test 2: Di chuyển tiến
**Mục đích**: Kiểm tra chuyển từ STOP sang di chuyển

**Các bước**:
1. Gửi WebSocket message:
```json
{
  "type": "tasks",
  "mode": "enqueue",
  "tasks": [{
    "taskId": "test_forward_1",
    "device": "wheels",
    "left": 50,
    "right": 50,
    "durationMs": 2000
  }]
}
```

**Kết quả mong đợi**:
```
[WHEELS] tryUpdate: start moving L=50 R=50
[WHEELS] setSpeed L=49 R=49
[WHEELS] sendRL: [49 49 FE FE] (R,L)  // Speed
[WHEELS] sendRL: [24 34 FE FE] (R,L)  // Direction: Forward
```

Sau đó mỗi 10ms:
```
[WHEELS] sendRL: [24 34 FE FE] (R,L)  // Chỉ gửi direction
```

**Kiểm tra vật lý**: Bánh xe quay tiến với tốc độ trung bình

---

### Test 3: Thay đổi tốc độ
**Mục đích**: Kiểm tra thay đổi tốc độ trong khi di chuyển

**Các bước**:
1. Trong khi bánh xe đang chạy (Test 2), gửi:
```json
{
  "type": "tasks",
  "mode": "enqueue",
  "tasks": [{
    "taskId": "test_speedup_1",
    "device": "wheels",
    "left": 80,
    "right": 80,
    "durationMs": 2000
  }]
}
```

**Kết quả mong đợi**:
```
[WHEELS] tryUpdate: speed changed L=49->4C R=49->4C
[WHEELS] setSpeed L=4C R=4C
[WHEELS] sendRL: [4C 4C FE FE] (R,L)  // Speed mới
[WHEELS] sendRL: [24 34 FE FE] (R,L)  // Direction tiếp tục
```

**Kiểm tra vật lý**: Bánh xe tăng tốc mượt mà

---

### Test 4: Rẽ trái
**Mục đích**: Kiểm tra điều khiển độc lập từng bánh

**Các bước**:
1. Gửi:
```json
{
  "type": "tasks",
  "mode": "enqueue",
  "tasks": [{
    "taskId": "test_turn_left_1",
    "device": "wheels",
    "left": 30,
    "right": 70,
    "durationMs": 1000
  }]
}
```

**Kết quả mong đợi**:
```
[WHEELS] tryUpdate: speed changed L=4C->46 R=4C->4B
[WHEELS] setSpeed L=46 R=4B
[WHEELS] sendRL: [4B 46 FE FE] (R,L)  // Speed khác nhau
[WHEELS] sendRL: [24 34 FE FE] (R,L)  // Direction
```

**Kiểm tra vật lý**: Robot rẽ trái

---

### Test 5: Dừng lại
**Mục đích**: Kiểm tra chuyển từ di chuyển về STOP

**Các bước**:
1. Gửi:
```json
{
  "type": "tasks",
  "mode": "enqueue",
  "tasks": [{
    "taskId": "test_stop_1",
    "device": "wheels",
    "left": 0,
    "right": 0,
    "durationMs": 100
  }]
}
```

**Kết quả mong đợi**:
```
[WHEELS] tryUpdate: stopping
```

Sau đó mỗi 10ms:
```
[WHEELS] sendRL: [00 00 FE FE] (R,L)  // STOP liên tục
```

**Kiểm tra vật lý**: Bánh xe dừng ngay lập tức

---

### Test 6: Di chuyển lùi
**Mục đích**: Kiểm tra hướng ngược

**Các bước**:
1. Gửi:
```json
{
  "type": "tasks",
  "mode": "enqueue",
  "tasks": [{
    "taskId": "test_backward_1",
    "device": "wheels",
    "left": -50,
    "right": -50,
    "durationMs": 2000
  }]
}
```

**Kết quả mong đợi**:
```
[WHEELS] tryUpdate: start moving L=-50 R=-50
[WHEELS] setSpeed L=49 R=49
[WHEELS] sendRL: [49 49 FE FE] (R,L)  // Speed
[WHEELS] sendRL: [34 24 FE FE] (R,L)  // Direction: Backward (đảo ngược)
```

**Kiểm tra vật lý**: Bánh xe quay lùi

---

### Test 7: Xoay tại chỗ
**Mục đích**: Kiểm tra xoay (một bánh tiến, một bánh lùi)

**Các bước**:
1. Gửi:
```json
{
  "type": "tasks",
  "mode": "enqueue",
  "tasks": [{
    "taskId": "test_spin_1",
    "device": "wheels",
    "left": -50,
    "right": 50,
    "durationMs": 1000
  }]
}
```

**Kết quả mong đợi**:
```
[WHEELS] setSpeed L=49 R=49
[WHEELS] sendRL: [49 49 FE FE] (R,L)
[WHEELS] sendRL: [24 24 FE FE] (R,L)  // Left: backward, Right: forward
```

**Kiểm tra vật lý**: Robot xoay tại chỗ

---

### Test 8: Liên tục nhiều lệnh
**Mục đích**: Kiểm tra xử lý nhiều lệnh liên tiếp

**Các bước**:
1. Gửi liên tục các lệnh:
   - Forward 50%
   - Forward 80%
   - Turn left
   - Stop
   - Backward 50%
   - Stop

**Kết quả mong đợi**:
- Mỗi lệnh được xử lý ngay lập tức qua `tryUpdate()`
- Không có delay giữa các lệnh
- Chuyển động mượt mà

---

### Test 9: Cancel
**Mục đích**: Kiểm tra hành vi cancel

**Các bước**:
1. Cho bánh xe chạy
2. Gửi cancel:
```json
{
  "type": "cancel",
  "device": "wheels"
}
```

**Kết quả mong đợi**:
```
[WHEELS] cancel at ... ms (id=...) continuousMode=1
```

Sau đó:
```
[WHEELS] sendRL: [00 00 FE FE] (R,L)  // Quay về STOP
```

**Kiểm tra vật lý**: Bánh xe dừng, task vẫn chạy (gửi STOP)

---

### Test 10: Timing
**Mục đích**: Kiểm tra chu kỳ gửi 10ms

**Các bước**:
1. Bật WHEELS_DEBUG=1
2. Cho bánh xe chạy
3. Đo thời gian giữa các lần gửi

**Kết quả mong đợi**:
- Khoảng cách giữa các lần gửi: ~10ms ± 2ms
- Không có gap lớn (>20ms)

---

## Checklist tổng hợp

- [ ] Test 1: Khởi động và STOP signal
- [ ] Test 2: Di chuyển tiến
- [ ] Test 3: Thay đổi tốc độ
- [ ] Test 4: Rẽ trái
- [ ] Test 5: Dừng lại
- [ ] Test 6: Di chuyển lùi
- [ ] Test 7: Xoay tại chỗ
- [ ] Test 8: Liên tục nhiều lệnh
- [ ] Test 9: Cancel
- [ ] Test 10: Timing

## Debug tips

### Bật debug logs
Trong `WheelsDevice.cpp`:
```cpp
#define WHEELS_DEBUG 1
```

### Kiểm tra wiring
- Byte[1] = RIGHT motor (phải)
- Byte[2] = LEFT motor (trái)

### Speed mapping
- 0x40 = STOP
- 0x42 = 6 rpm (min)
- 0x4F = 21 rpm (max)
- Percent to byte: map(0-100, 0x42, 0x4F)

### Direction
- Forward: Left=0x34 (anticlockwise), Right=0x24 (clockwise)
- Backward: Left=0x24 (clockwise), Right=0x34 (anticlockwise)

## Vấn đề có thể gặp

### 1. Bánh xe không quay
- Kiểm tra motor đã kết nối đúng channel
- Kiểm tra `motorInitialized = true`
- Kiểm tra logs có thấy "Motor device connected successfully"

### 2. Bánh xe giật cục
- Kiểm tra timing: phải ~10ms
- Kiểm tra Wi-Fi không bị delay
- Kiểm tra có gọi `yield()` sau mỗi lần gửi

### 3. Bánh xe không dừng
- Kiểm tra có gửi STOP signal (0x00)
- Kiểm tra `isMoving = false` khi dừng
- Kiểm tra logic trong `tick()`

### 4. Thay đổi tốc độ không mượt
- Kiểm tra có gửi speed byte mới
- Kiểm tra `speedChanged` logic
- Kiểm tra không gửi speed quá nhiều lần

### 5. Task không nhận lệnh mới
- Kiểm tra `tryUpdate()` trả về `true`
- Kiểm tra `continuousMode = true`
- Kiểm tra `running = true`

