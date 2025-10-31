# Changelog - Wheels Continuous Mode Implementation

## Ngày: 2025-10-25

### Tóm tắt
Đã triển khai hệ thống điều khiển bánh xe liên tục (continuous mode) cho Meccano MAX robot. Thay vì gửi lệnh theo từng task có thời gian giới hạn, hệ thống mới tạo một task liên tục không dừng, luôn gửi tín hiệu đến bánh xe theo chu kỳ 10ms.

### Động lực
Dựa trên tài liệu "Meccano MAX Control codes.txt", bánh xe Meccano MAX cần nhận tín hiệu liên tục để chuyển động đều. Hệ thống cũ gửi lệnh theo task riêng lẻ có thể gây giật cục hoặc mất tín hiệu.

### Các thay đổi chính

#### 1. WheelsDevice.h
**Thêm mới:**
- `bool continuousMode` - cờ đánh dấu continuous mode
- `bool isMoving` - trạng thái di chuyển (true) hoặc dừng (false)
- `bool needSpeedUpdate` - cờ đánh dấu cần gửi speed byte
- `void startContinuousTask(uint32_t now)` - khởi động continuous task

**Mô tả:**
```cpp
// Continuous mode state
bool continuousMode;  // true = task liên tục không dừng
bool isMoving;        // true = đang di chuyển, false = đang dừng
bool needSpeedUpdate; // true = cần gửi speed byte
```

#### 2. WheelsDevice.cpp

**Constructor:**
- Khởi tạo các biến continuous mode: `continuousMode = false`, `isMoving = false`, `needSpeedUpdate = false`

**startTask():**
- Kiểm tra `continuousMode` để đặt `durationMs = 0xFFFFFFFF` (vô hạn)
- Cập nhật `isMoving` dựa trên `left` và `right`
- Đặt `needSpeedUpdate = true` khi bắt đầu task

**tryUpdate():**
- Logic phát hiện chuyển đổi trạng thái:
  - Dừng → Di chuyển: gửi speed byte
  - Di chuyển → Di chuyển (thay đổi tốc độ): gửi speed byte mới
  - Di chuyển → Dừng: đặt speed = STOP
- Trong continuous mode, không kéo dài duration

**tick():**
- Logic gửi tín hiệu mỗi 10ms:
  - Nếu `isMoving = true`: gửi direction byte liên tục
  - Nếu `isMoving = false`: gửi STOP byte liên tục
- Trong continuous mode, task không bao giờ kết thúc tự động

**cancel():**
- Trong continuous mode: không dừng task, chỉ chuyển về trạng thái dừng
- Trong normal mode: dừng task như cũ

**isCompleted():**
- Trong continuous mode: luôn trả về `false`

**progress():**
- Trong continuous mode: luôn trả về 50% (đang chạy liên tục)

**startContinuousTask():**
- Tạo task dummy với `taskId = "wheels_continuous"`
- Đặt `continuousMode = true`
- Khởi tạo trạng thái dừng: `isMoving = false`, speed = STOP, direction = STOP
- Đặt `durationMs = 0xFFFFFFFF`

#### 3. TaskRunner.h
**Thêm mới:**
- `WheelsDevice* getWheelsDevice()` - truy cập wheels device

#### 4. TaskRunner.cpp
**acceptTasks():**
- Khi device đang chạy và `tryUpdate()` thành công, gửi `done` message ngay
- Không xếp hàng task nếu đã update thành công

#### 5. main.ino
**setup():**
- Gọi `taskRunner.getWheelsDevice()->startContinuousTask(millis())` sau khi khởi tạo

### Files mới

#### 1. firmware/main/WHEELS_CONTINUOUS_MODE.md
Tài liệu kỹ thuật chi tiết về continuous mode:
- Nguyên lý hoạt động
- Cấu trúc code
- Tham khảo tài liệu Meccano MAX
- Lợi ích
- Kiểm tra và debug
- Ví dụ sử dụng

#### 2. firmware/main/TEST_WHEELS_CONTINUOUS.md
Kế hoạch test chi tiết:
- 10 test cases
- Kết quả mong đợi
- Debug tips
- Vấn đề có thể gặp

#### 3. firmware/example/test-wheels-continuous.js
Script test tự động qua WebSocket:
- 10 test cases tự động
- Có thể chạy tất cả hoặc từng test riêng
- Logs chi tiết

### Hành vi mới

#### Trước khi thay đổi:
```
1. Nhận task wheels: left=50, right=50, duration=2000ms
2. Gửi speed + direction
3. Gửi keep-alive mỗi 10ms (cả speed và direction)
4. Sau 2000ms: dừng task, gửi STOP
5. Không có task → không gửi gì
```

#### Sau khi thay đổi:
```
1. Khởi động: bắt đầu continuous task
2. Gửi STOP liên tục mỗi 10ms
3. Nhận lệnh di chuyển: left=50, right=50
4. Gửi speed một lần
5. Gửi direction liên tục mỗi 10ms
6. Nhận lệnh thay đổi tốc độ: left=80, right=80
7. Gửi speed mới một lần
8. Tiếp tục gửi direction liên tục
9. Nhận lệnh dừng: left=0, right=0
10. Quay về gửi STOP liên tục mỗi 10ms
11. Task không bao giờ kết thúc
```

### Tương thích ngược
- **API không thay đổi**: Server vẫn gửi task như cũ
- **Normal mode vẫn hoạt động**: Nếu không gọi `startContinuousTask()`, wheels hoạt động như cũ
- **Arm và Neck không ảnh hưởng**: Chỉ wheels sử dụng continuous mode

### Testing
Để test:
1. Bật debug: `#define WHEELS_DEBUG 1` trong `WheelsDevice.cpp`
2. Upload firmware
3. Chạy test script: `node firmware/example/test-wheels-continuous.js`
4. Quan sát Serial Monitor và hành vi vật lý

### Lợi ích

1. **Chuyển động mượt mà hơn**: Gửi tín hiệu liên tục tránh giật cục
2. **An toàn hơn**: Luôn có tín hiệu điều khiển, tránh bánh xe mất kiểm soát
3. **Phản hồi nhanh hơn**: Có thể thay đổi hướng/tốc độ ngay lập tức
4. **Đơn giản hóa logic**: Không cần quản lý nhiều task ngắn hạn
5. **Tuân thủ tài liệu**: Theo đúng khuyến nghị của Meccano MAX

### Lưu ý quan trọng

1. **Timing**: Chu kỳ 10ms (100Hz) theo tài liệu Meccano MAX
2. **Wiring**: Byte[1] = RIGHT motor, Byte[2] = LEFT motor
3. **Speed mapping**: 0x40 = STOP, 0x42-0x4F = running
4. **Direction**: 0x2n/0x3n cho forward/backward
5. **Yield**: Gọi `yield()` sau mỗi lần gửi để tránh đói Wi-Fi

### Tham khảo
- Tài liệu gốc: `firmware/example/Meccano MAX Control codes.txt`
- Tài liệu kỹ thuật: `firmware/main/WHEELS_CONTINUOUS_MODE.md`
- Test plan: `firmware/main/TEST_WHEELS_CONTINUOUS.md`
- Test script: `firmware/example/test-wheels-continuous.js`

### Tác giả
- Triển khai dựa trên yêu cầu người dùng
- Tham khảo tài liệu Meccano MAX Control codes

### Phiên bản
- Firmware version: 1.1.0 (với Wheels Continuous Mode)
- Tương thích với server version: 1.0.0+

