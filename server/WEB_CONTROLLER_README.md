# Robot MAX Controller - Giao diện điều khiển Web

## Mô tả
Giao diện web điều khiển robot MAX với hai cần điều khiển song song để điều khiển bánh xe trái và phải.

## Tính năng
- ✅ **Cần điều khiển song song**: Điều khiển độc lập bánh xe trái và phải
- ✅ **Điều khiển trực quan**: Kéo thả cần điều khiển lên/xuống để điều khiển tốc độ
- ✅ **Di chuyển mượt mà**: Sử dụng task.enqueue với duration để tránh giật cục
- ✅ **Dừng ngay lập tức**: Khi nhả cần điều khiển, robot dừng ngay không đi thêm
- ✅ **Dừng khẩn cấp**: Nút dừng khẩn cấp để dừng robot ngay lập tức
- ✅ **Hiển thị trạng thái**: Theo dõi kết nối và tốc độ bánh xe
- ✅ **Nhật ký hoạt động**: Xem lịch sử các lệnh đã gửi
- ✅ **Responsive**: Hoạt động tốt trên cả desktop và mobile

## Cách sử dụng

### 1. Khởi động server
```bash
cd server
npm run dev
```

### 2. Truy cập giao diện
Mở trình duyệt và truy cập: `http://localhost:8080`

### 3. Điều khiển robot
- **Cần điều khiển trái**: Kéo lên/xuống để điều khiển bánh xe trái
- **Cần điều khiển phải**: Kéo lên/xuống để điều khiển bánh xe phải
- **Nhả cần điều khiển**: Robot sẽ tự động dừng
- **Nút dừng khẩn cấp**: Nhấn để dừng robot ngay lập tức

### 4. Cách hoạt động
- Kéo cần điều khiển **lên** = Robot tiến (tốc độ dương)
- Kéo cần điều khiển **xuống** = Robot lùi (tốc độ âm)
- **Nhả cần điều khiển** = Robot dừng (tốc độ = 0)

## Cấu trúc file
```
server/
├── public/
│   ├── index.html      # Giao diện chính
│   ├── style.css       # CSS styling
│   └── script.js       # JavaScript logic
├── src/
│   ├── index.ts        # Server chính (đã cập nhật để serve static files)
│   ├── api.ts          # API endpoints
│   └── ...
```

## API Endpoints
- `GET /` - Giao diện web
- `GET /robot/status` - Trạng thái robot
- `POST /robot/tasks/replace` - Gửi lệnh điều khiển

## Lệnh điều khiển
Giao diện gửi lệnh dạng JSON:
```json
{
  "tasks": [{
    "device": "wheels",
    "type": "drive",
    "left": 50,    // Tốc độ bánh xe trái (-100 đến 100)
    "right": -30,  // Tốc độ bánh xe phải (-100 đến 100)
    "taskId": "manual-1234567890"
  }]
}
```

## Lưu ý
- Tốc độ được giới hạn từ -100% đến +100%
- **Di chuyển mượt mà**: Sử dụng task.enqueue với duration 200ms để tránh giật cục
- **Dừng ngay lập tức**: Khi nhả cần điều khiển, robot dừng ngay không đi thêm
- **Command deduplication**: Không gửi lệnh trùng lặp để tiết kiệm băng thông
- Giao diện tự động kiểm tra kết nối với robot mỗi 5 giây
- Hỗ trợ cả điều khiển bằng chuột và cảm ứng (touch)
