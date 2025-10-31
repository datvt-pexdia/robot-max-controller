# Wheels Continuous Mode - Tài liệu kỹ thuật

## Tổng quan

Đã triển khai một hệ thống điều khiển bánh xe liên tục (continuous mode) cho Meccano MAX robot. Thay vì gửi lệnh theo từng task có thời gian giới hạn, hệ thống mới tạo một task liên tục không dừng, luôn gửi tín hiệu đến bánh xe theo chu kỳ 10ms.

## Nguyên lý hoạt động

### 1. Continuous Task
- Khi khởi động firmware, một task đặc biệt với `taskId = "wheels_continuous"` được tạo
- Task này có `durationMs = 0xFFFFFFFF` (vô hạn) và không bao giờ kết thúc
- Task luôn chạy và gửi tín hiệu mỗi 10ms (theo tài liệu Meccano MAX)

### 2. Trạng thái ban đầu: STOP
- Khi khởi động, `isMoving = false`
- Task liên tục gửi tín hiệu STOP (`DIR_STOP = 0x00`) đến cả hai bánh
- Đảm bảo bánh xe luôn ở trạng thái dừng an toàn

### 3. Khi yêu cầu di chuyển
Khi nhận lệnh di chuyển (left ≠ 0 hoặc right ≠ 0):
1. **Gửi tốc độ một lần**: Gửi speed byte (0x42-0x4F) cho mỗi bánh
2. **Chuyển sang gửi hướng liên tục**: Sau đó chỉ gửi direction byte (0x2n/0x3n) mỗi 10ms
3. Cập nhật `isMoving = true`

### 4. Khi thay đổi tốc độ
Khi tốc độ thay đổi trong khi đang di chuyển:
1. **Gửi tốc độ mới**: Gửi speed byte mới
2. **Tiếp tục gửi hướng**: Sau đó tiếp tục gửi direction byte liên tục

### 5. Khi yêu cầu dừng
Khi nhận lệnh dừng (left = 0 và right = 0):
1. Cập nhật `isMoving = false`
2. **Quay về gửi STOP liên tục**: Task tiếp tục chạy nhưng gửi `DIR_STOP`

## Cấu trúc code

### WheelsDevice.h
```cpp
// Continuous mode state
bool continuousMode;  // true = task liên tục không dừng
bool isMoving;        // true = đang di chuyển, false = đang dừng
bool needSpeedUpdate; // true = cần gửi speed byte
```

### Các phương thức chính

#### `startContinuousTask(uint32_t now)`
- Khởi động continuous mode
- Tạo task với taskId đặc biệt
- Đặt trạng thái ban đầu: dừng, gửi STOP

#### `tick(uint32_t now)`
Logic gửi tín hiệu mỗi 10ms:
```cpp
if (continuousMode) {
  if (isMoving) {
    // Gửi direction liên tục (speed đã gửi khi bắt đầu)
    sendRL_(current.rightCmd, current.leftCmd);
  } else {
    // Gửi STOP liên tục
    sendRL_(DIR_STOP, DIR_STOP);
  }
}
```

#### `tryUpdate(const TaskEnvelope& task, uint32_t now)`
Xử lý cập nhật trạng thái:
- Phát hiện chuyển đổi: dừng → di chuyển, di chuyển → dừng
- Phát hiện thay đổi tốc độ
- Gửi speed byte khi cần thiết
- Luôn gửi direction byte sau mỗi lần cập nhật

### TaskRunner
- Khi nhận task mới cho wheels trong continuous mode, gọi `tryUpdate()` thay vì tạo task mới
- Gửi `done` message ngay sau khi update thành công

### main.ino
```cpp
void setup() {
  // ...
  taskRunner.getWheelsDevice()->startContinuousTask(millis());
}
```

## Tham khảo tài liệu Meccano MAX

Từ file "Meccano MAX Control codes.txt":

### Smart Motors (trang 213-238)
- **Speed byte**: 0x42-0x4F (0x40 = stop)
  - 0x42: ~6 rpm (tốc độ tối thiểu)
  - 0x4F: ~21 rpm (tốc độ tối đa)
  
- **Direction byte**: 0x2n hoặc 0x3n
  - 0x2n: quay theo chiều kim đồng hồ (nhìn từ bên ngoài bánh)
  - 0x3n: quay ngược chiều kim đồng hồ
  - 0x20 hoặc 0x30: dừng

### Robot movement
- **Di chuyển tiến**: `0xFF 0x34 0x24 0xFE 0xFE checksum`
  - Left wheel: 0x34 (anticlockwise)
  - Right wheel: 0x24 (clockwise)
  
- **Di chuyển lùi**: `0xFF 0x24 0x34 0xFE 0xFE checksum`
  - Left wheel: 0x24 (clockwise)
  - Right wheel: 0x34 (anticlockwise)

### Timing
- Giao tiếp serial: 2400 baud 8N2
- **Chu kỳ gửi lệnh**: Cần gửi liên tục để duy trì chuyển động đều
- Khuyến nghị: 10ms giữa các lần gửi (100Hz)

## Lợi ích của Continuous Mode

1. **Chuyển động mượt mà**: Gửi tín hiệu liên tục tránh giật cục
2. **An toàn**: Luôn có tín hiệu điều khiển, tránh bánh xe mất kiểm soát
3. **Phản hồi nhanh**: Có thể thay đổi hướng/tốc độ ngay lập tức
4. **Đơn giản hóa logic**: Không cần quản lý nhiều task ngắn hạn

## Kiểm tra và Debug

Để bật debug logs:
```cpp
#define WHEELS_DEBUG 1
```

Debug logs sẽ hiển thị:
- Khi bắt đầu continuous task
- Khi chuyển từ dừng → di chuyển
- Khi thay đổi tốc độ
- Khi dừng lại
- Các byte được gửi đi (speed và direction)

## Ví dụ sử dụng

### 1. Khởi động (tự động)
```cpp
// Trong setup()
taskRunner.getWheelsDevice()->startContinuousTask(millis());
// → Bắt đầu gửi STOP signal liên tục
```

### 2. Di chuyển tiến
```json
{
  "taskId": "move_forward_1",
  "device": "wheels",
  "left": 50,
  "right": 50,
  "durationMs": 1000
}
```
→ Gửi speed 50%, sau đó gửi direction FORWARD liên tục

### 3. Thay đổi tốc độ
```json
{
  "taskId": "speed_up_1",
  "device": "wheels",
  "left": 80,
  "right": 80,
  "durationMs": 1000
}
```
→ Gửi speed 80% mới, tiếp tục gửi direction FORWARD

### 4. Rẽ trái
```json
{
  "taskId": "turn_left_1",
  "device": "wheels",
  "left": 30,
  "right": 70,
  "durationMs": 500
}
```
→ Gửi speed khác nhau cho mỗi bánh, gửi direction tương ứng

### 5. Dừng lại
```json
{
  "taskId": "stop_1",
  "device": "wheels",
  "left": 0,
  "right": 0,
  "durationMs": 100
}
```
→ Quay về gửi STOP signal liên tục

## Lưu ý quan trọng

1. **Không dừng task**: Trong continuous mode, task không bao giờ kết thúc tự động
2. **Cancel behavior**: Khi cancel, task không dừng mà chỉ chuyển về trạng thái STOP
3. **Progress**: Luôn trả về 50% (đang chạy liên tục)
4. **Wiring**: Byte[1] = RIGHT motor, Byte[2] = LEFT motor
5. **Yield**: Gọi `yield()` sau mỗi lần gửi để tránh đói Wi-Fi

