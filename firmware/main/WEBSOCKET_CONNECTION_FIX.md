# WebSocket Connection Fix

## Vấn đề
ESP8266 không kết nối được WebSocket server mặc dù:
- WiFi đã kết nối thành công
- ESP8266 liên tục cố kết nối (log: `[NET] Connecting ws://192.168.1.6:8080/robot`)
- Server không báo có connection nào

## Nguyên nhân

### 1. **Sai IP Server** (VẤN ĐỀ CHÍNH)
- `Config.h` set `WS_HOST = "192.168.1.6"`
- Nhưng IP thực của server là `192.168.1.5`
- ESP8266 đang cố kết nối đến địa chỉ sai!

### 2. **Lỗi kiến trúc trong server** (ĐÃ SỬA)
- `index.ts` cố ghi đè `handleConnection()` của `WsHub` SAU khi constructor đã chạy
- `handleConnection` là private method và đã được bind trong constructor
- Việc ghi đè từ bên ngoài KHÔNG có tác dụng
- Dẫn đến server không xử lý connection đúng cách

### 3. **Thiếu config trong firmware** (ĐÃ SỬA)
- `Config.h` thiếu các định nghĩa `WS_HEARTBEAT_*`
- `NetClient.cpp` sử dụng các hằng số này nhưng chúng chỉ có fallback trong `NetClient.h`

## Giải pháp

### 1. Sửa IP trong Config.h ✓
```cpp
static const char* WS_HOST = "192.168.1.5";  // ← Sửa từ .6 thành .5
```

### 2. Sửa kiến trúc WsHub ✓
Thay vì monkey-patch `handleConnection`, thêm callback pattern:

**wsHub.ts:**
```typescript
private browserConnectionHandler?: (socket: WebSocket) => void;

setBrowserConnectionHandler(handler: (socket: WebSocket) => void): void {
  this.browserConnectionHandler = handler;
}

private handleConnection(socket: WebSocket, req: http.IncomingMessage): void {
  const userAgent = req.headers['user-agent'] || '';
  const isBrowser = userAgent.includes('Mozilla') || ...;
  
  if (isBrowser && this.browserConnectionHandler) {
    this.browserConnectionHandler(socket);
    return;
  }
  
  // ESP connection logic...
}
```

**index.ts:**
```typescript
wsHub.setBrowserConnectionHandler((socket) => {
  driveRelay.handleConnection(socket);
});
```

### 3. Thêm heartbeat config ✓
```cpp
// Config.h
#define WS_HEARTBEAT_INTERVAL_MS 15000
#define WS_HEARTBEAT_TIMEOUT_MS 3000
#define WS_HEARTBEAT_TRIES 2
```

## Testing

### Test với Node.js client (giả lập ESP)
```bash
cd server
node test-esp-connection.js
```

Kết quả:
```
[TEST] ✓ WebSocket CONNECTED!
[TEST] Received: { kind: 'hello', serverTime: 1761461965915 }
```

### Kiểm tra IP server
```bash
ipconfig | findstr "IPv4"
# Output: IPv4 Address. . . . . . . . . . . : 192.168.1.5
```

## Các bước tiếp theo

1. **Upload firmware mới lên ESP8266** với IP đã sửa
2. **Kiểm tra log ESP8266** - phải thấy:
   ```
   [NET] WebSocket CONNECTED
   [NET] Starting continuous wheels task...
   ```
3. **Kiểm tra log server** - phải thấy:
   ```
   [WS] Connection from 192.168.1.8, User-Agent: ...
   [WS] Detected as: ESP (Device)
   [WS] ESP connected from 192.168.1.8
   ```

## Lưu ý

- IP có thể thay đổi nếu router assign DHCP mới
- Nên dùng IP tĩnh cho server hoặc dùng mDNS/hostname
- Firewall Windows có thể chặn connection - cần allow port 8080
- Test script `test-esp-connection.js` có thể dùng để debug server

