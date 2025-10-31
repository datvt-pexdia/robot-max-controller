# WebSocket Connection Debug Summary

## 🔍 Vấn đề
ESP8266 không kết nối được WebSocket server:
```
ESP Log:
06:54:01.504 -> [NET] Connecting ws://192.168.1.6:8080/robot  (IP=192.168.1.8)
06:54:06.528 -> [NET] Connecting ws://192.168.1.6:8080/robot  (IP=192.168.1.8)
...

Server Log:
(không có log connection nào)
```

## ✅ Nguyên nhân đã tìm ra

### 1. **SAI IP SERVER** ⚠️ (Vấn đề chính)
- ESP8266 đang kết nối đến: `192.168.1.6`
- IP thực của server: `192.168.1.5`
- **ESP đang gửi request đến địa chỉ sai!**

### 2. **Lỗi kiến trúc server** (Đã sửa)
File `server/src/index.ts` cố ghi đè method `handleConnection` của `WsHub` sau khi constructor đã chạy:
```typescript
// ❌ Code cũ - KHÔNG HOẠT ĐỘNG
const originalWsHubHandleConnection = (wsHub as any).handleConnection.bind(wsHub);
(wsHub as any).handleConnection = (socket: any, req: any) => { ... }
```

Vấn đề:
- `handleConnection` là private method
- Đã được bind trong constructor: `this.wss.on('connection', (socket, req) => this.handleConnection(socket, req))`
- Ghi đè từ bên ngoài không có tác dụng

### 3. **Thiếu config heartbeat** (Đã sửa)
File `firmware/main/Config.h` thiếu các định nghĩa mà `NetClient.cpp` cần.

## 🔧 Các sửa đổi đã thực hiện

### 1. Sửa IP trong firmware ✓
**File:** `firmware/main/Config.h`
```cpp
// Trước:
static const char* WS_HOST = "192.168.1.6";

// Sau:
static const char* WS_HOST = "192.168.1.5";
```

### 2. Refactor WsHub để hỗ trợ callback ✓
**File:** `server/src/wsHub.ts`
```typescript
export class WsHub {
  private browserConnectionHandler?: (socket: WebSocket) => void;

  setBrowserConnectionHandler(handler: (socket: WebSocket) => void): void {
    this.browserConnectionHandler = handler;
  }

  private handleConnection(socket: WebSocket, req: http.IncomingMessage): void {
    const userAgent = req.headers['user-agent'] || '';
    const isBrowser = userAgent.includes('Mozilla') || ...;
    
    wsLog(`Connection from ${req.socket.remoteAddress}, User-Agent: ${userAgent.substring(0, 50)}...`);
    wsLog(`Detected as: ${isBrowser ? 'BROWSER (UI)' : 'ESP (Device)'}`);

    if (isBrowser && this.browserConnectionHandler) {
      wsLog('Delegating to browser connection handler');
      this.browserConnectionHandler(socket);
      return;
    }

    // ESP connection logic...
  }
}
```

**File:** `server/src/index.ts`
```typescript
// ✓ Code mới - HOẠT ĐỘNG ĐÚNG
wsHub.setBrowserConnectionHandler((socket) => {
  driveRelay.handleConnection(socket);
});
```

### 3. Thêm heartbeat config ✓
**File:** `firmware/main/Config.h`
```cpp
// WebSocket heartbeat settings (phải match với server)
#define WS_HEARTBEAT_INTERVAL_MS 15000  // ping mỗi 15s
#define WS_HEARTBEAT_TIMEOUT_MS 3000    // đợi pong 3s
#define WS_HEARTBEAT_TRIES 2            // fail sau 2 lần miss
```

## 🧪 Testing

### Test 1: Node.js client giả lập ESP ✓
```bash
cd server
node test-esp-connection.js
```

**Kết quả:**
```
[TEST] Connecting to ws://192.168.1.6:8080/robot...
[TEST] ✓ WebSocket CONNECTED!
[TEST] Sending hello: { kind: 'hello', espId: 'TEST123', ... }
[TEST] Received: { kind: 'hello', serverTime: 1761461965915 }
```
✅ Server hoạt động đúng!

### Test 2: Kiểm tra IP ✓
```bash
ipconfig | findstr "IPv4"
```
**Kết quả:**
```
IPv4 Address. . . . . . . . . . . : 192.168.1.5
```

### Test 3: Auto-update IP script ✓
```bash
node firmware/update-server-ip.js
```
**Kết quả:**
```
📡 Đã detect IP: 192.168.1.5
✓ IP đã đúng: 192.168.1.5
```

## 📋 Các bước tiếp theo

### 1. Upload firmware mới lên ESP8266
- Mở Arduino IDE
- Mở `firmware/main/main.ino`
- Verify rằng `Config.h` có IP đúng: `192.168.1.5`
- Upload lên ESP8266

### 2. Kiểm tra log ESP8266
Sau khi upload, bạn sẽ thấy:
```
[NET] Connecting ws://192.168.1.5:8080/robot  (IP=192.168.1.8)
[NET] WebSocket CONNECTED
[NET] Starting continuous wheels task...
```

### 3. Kiểm tra log server
Server sẽ log:
```
[WS] Connection from 192.168.1.8, User-Agent: ...
[WS] Detected as: ESP (Device)
[WS] ESP connected from 192.168.1.8
[WS] ESP hello id=... fw=robot-max-fw/1.0
```

## 🛠️ Tools mới

### 1. `firmware/update-server-ip.js`
Script tự động detect IP và update `Config.h`:
```bash
# Auto-detect IP
node firmware/update-server-ip.js

# Hoặc set IP thủ công
node firmware/update-server-ip.js 192.168.1.5
```

### 2. `server/test-esp-connection.js`
Script test WebSocket connection giả lập ESP8266:
```bash
cd server
node test-esp-connection.js
```

## ⚠️ Lưu ý quan trọng

### IP động
- Router có thể assign IP mới cho máy server sau mỗi lần khởi động
- **Giải pháp:**
  1. Set IP tĩnh cho máy server trong router
  2. Hoặc chạy `node firmware/update-server-ip.js` trước mỗi lần upload firmware

### Firewall
- Windows Firewall có thể chặn port 8080
- Nếu vẫn không kết nối được, check:
  ```bash
  # PowerShell (Run as Admin)
  netsh advfirewall firewall add rule name="Robot Server" dir=in action=allow protocol=TCP localport=8080
  ```

### Network
- ESP8266 và server phải cùng mạng WiFi
- Kiểm tra ESP có IP đúng subnet không (192.168.1.x)

## 📊 Tóm tắt thay đổi

### Files đã sửa:
1. ✅ `firmware/main/Config.h` - Sửa IP + thêm heartbeat config
2. ✅ `server/src/wsHub.ts` - Thêm callback pattern cho browser connections
3. ✅ `server/src/index.ts` - Sử dụng callback thay vì monkey-patch

### Files mới:
1. ✅ `firmware/update-server-ip.js` - Auto-update IP tool
2. ✅ `server/test-esp-connection.js` - WebSocket test client
3. ✅ `firmware/main/WEBSOCKET_CONNECTION_FIX.md` - Chi tiết kỹ thuật
4. ✅ `WEBSOCKET_DEBUG_SUMMARY.md` - Document này

### Build artifacts:
1. ✅ `server/dist/` - Đã rebuild với code mới

## 🎯 Kết luận

Vấn đề chính là **SAI IP SERVER** trong Config.h. Các vấn đề khác (kiến trúc server, thiếu config) cũng đã được sửa để đảm bảo hệ thống hoạt động ổn định.

**Bước quan trọng nhất:** Upload firmware mới với IP đã sửa lên ESP8266!

