# Fix: WebSocket Connection Issue

## Vấn đề

Sau khi triển khai Wheels Continuous Mode, ESP8266 không thể kết nối WebSocket:
- Server báo "Rejecting additional ESP connection"
- ESP liên tục reconnect
- Logs: `code=1006` (abnormal closure)

```
2025-10-25T16:12:03.870Z [WS] Rejecting additional ESP connection from 192.168.1.9
2025-10-25T16:12:04.945Z [WS] Connection from 192.168.1.9, User-Agent: arduino-WebSocket-Client...
2025-10-25T16:12:04.946Z [WS] Detected as: ESP (Device)
2025-10-25T16:12:04.946Z [WS] Rejecting additional ESP connection from 192.168.1.9
```

## Nguyên nhân

**`initializeMotors()` block quá lâu trong `setup()`**:
- Vòng lặp `while` với 50 retries × 100ms delay = **5 giây**
- Trong thời gian này, ESP không xử lý WebSocket
- Server nghĩ ESP đã kết nối nhưng không nhận được `hello` message
- ESP cố reconnect nhưng server từ chối vì "đã có kết nối"

```cpp
// Code cũ - BLOCK 5 giây!
void WheelsDevice::initializeMotors() {
  uint8_t retries = 0;
  while (channelMOTOR->getDeviceName(1) == NULL && retries < 50) {
    channelMOTOR->communicate();
    delay(100);  // ← BLOCK Ở ĐÂY!
    retries++;
  }
}
```

## Giải pháp

### 1. Khởi động continuous task SAU KHI WebSocket kết nối

**Trước:**
```cpp
void setup() {
  // ...
  netClient.begin(&taskRunner);
  taskRunner.getWheelsDevice()->startContinuousTask(millis()); // ← Block ở đây!
}
```

**Sau:**
```cpp
void setup() {
  // ...
  netClient.begin(&taskRunner);
  // Không gọi startContinuousTask() ở đây nữa
}

// Trong NetClient.cpp - WStype_CONNECTED event:
void NetClient::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      // ...
      sendHello();
      
      // Khởi động continuous task SAU KHI đã kết nối
      if (runner) {
        runner->getWheelsDevice()->startContinuousTask(millis());
      }
      break;
    }
  }
}
```

### 2. Làm `initializeMotors()` không đồng bộ (non-blocking)

**Trước:**
```cpp
void WheelsDevice::initializeMotors() {
  // Block 5 giây để chờ motor
  uint8_t retries = 0;
  while (channelMOTOR->getDeviceName(1) == NULL && retries < 50) {
    channelMOTOR->communicate();
    delay(100);
    retries++;
  }
}
```

**Sau:**
```cpp
void WheelsDevice::initializeMotors() {
  // Chỉ allocate channel, KHÔNG block
  if (!channelMOTOR) channelMOTOR = new MeccaChannel(pinMOTOR);
  if (!drive) drive = new MeccaMaxDrive(channelMOTOR);
  
  // Thử detect một lần, không block
  channelMOTOR->communicate();
  if (channelMOTOR->getDeviceName(1) != NULL) {
    motorInitialized = true;
  } else {
    motorInitialized = false;
    // Sẽ retry trong tick()
  }
}
```

### 3. Retry detect motor trong `tick()` (không block)

```cpp
void WheelsDevice::tick(uint32_t now) {
  if (!running) return;

#if !SIMULATION
  // Nếu motor chưa initialized, thử detect lại
  if (!motorInitialized && channelMOTOR) {
    static uint32_t lastRetry = 0;
    if (now - lastRetry >= 1000) { // retry mỗi 1 giây
      lastRetry = now;
      channelMOTOR->communicate();
      if (channelMOTOR->getDeviceName(1) != NULL) {
        motorInitialized = true;
        Serial.println("[WHEELS] Motor device detected!");
      }
    }
  }
  
  // Tiếp tục gửi tín hiệu nếu đã initialized...
#endif
}
```

## Files đã sửa

1. **firmware/main/main.ino**
   - Xóa `startContinuousTask()` khỏi `setup()`

2. **firmware/main/NetClient.cpp**
   - Thêm `startContinuousTask()` vào `WStype_CONNECTED` event

3. **firmware/main/Devices/WheelsDevice.cpp**
   - Sửa `initializeMotors()`: không block, chỉ allocate và thử detect một lần
   - Thêm retry logic trong `tick()`: detect motor mỗi 1 giây nếu chưa initialized

## Kết quả

### Trước khi fix:
```
[MAIN] Starting continuous wheels task...
[WHEELS] Initializing Meccano MAX motors...
[WHEELS] Waiting for motor device to connect...
Device status: Not Connected
Device status: Not Connected
... (block 5 giây) ...
[WHEELS] Failed to connect to motor device!
← WebSocket timeout, reconnect loop
```

### Sau khi fix:
```
[NET] WebSocket CONNECTED
[NET] Starting continuous wheels task...
[WHEELS] Initializing Meccano MAX motors (non-blocking)...
[WHEELS] Motor device not detected yet, will retry in tick()...
[WHEELS] Continuous task started - will send STOP signals continuously
← WebSocket hoạt động bình thường
← Motor sẽ được detect dần dần trong background
```

## Lợi ích

1. **WebSocket kết nối ngay lập tức** - không bị block
2. **Không ảnh hưởng đến Wi-Fi** - vòng lặp chính chạy mượt
3. **Motor detect không đồng bộ** - retry trong background
4. **Graceful degradation** - nếu không có motor, vẫn hoạt động (simulation mode)

## Testing

### Test 1: Không có motor (simulation)
```
✓ WebSocket kết nối OK
✓ Continuous task chạy
✓ Không có lỗi
```

### Test 2: Có motor
```
✓ WebSocket kết nối OK
✓ Continuous task chạy
✓ Motor được detect sau 1-2 giây
✓ Bánh xe hoạt động bình thường
```

### Test 3: Motor kết nối sau
```
✓ WebSocket kết nối OK
✓ Continuous task chạy (gửi STOP)
✓ Cắm motor vào
✓ Motor được detect tự động
✓ Bánh xe bắt đầu hoạt động
```

## Lưu ý

- **Simulation mode**: Motor luôn initialized ngay lập tức
- **Real hardware**: Motor được detect dần dần, không block
- **Retry interval**: 1 giây (có thể điều chỉnh nếu cần)
- **Timeout**: Không có timeout, sẽ retry mãi mãi cho đến khi detect được

## Tham khảo

- Issue: WebSocket connection rejected
- Root cause: Blocking I/O in setup()
- Solution: Non-blocking initialization + lazy detection

