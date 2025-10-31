# Testing Tools

## 1. test-esp-connection.js

Giả lập ESP8266 WebSocket client để test server.

### Sử dụng:
```bash
cd server
node test-esp-connection.js
```

### Kết quả mong đợi:
```
[TEST] Connecting to ws://192.168.1.5:8080/robot...
[TEST] User-Agent: (none - simulating ESP8266)
[TEST] ✓ WebSocket CONNECTED!
[TEST] Sending hello: { kind: 'hello', espId: 'TEST123', ... }
[TEST] Received: { kind: 'hello', serverTime: 1761461965915 }
[TEST] Test complete, closing connection...
[TEST] Connection closed: code=1005 reason=
```

### Nếu thất bại:
- **Connection refused**: Server chưa chạy hoặc sai IP/port
- **Timeout**: Firewall chặn hoặc server không lắng nghe
- **No response**: Server không xử lý connection đúng

### Sửa IP:
Nếu server IP khác 192.168.1.5, sửa trong file:
```javascript
const WS_URL = 'ws://YOUR_IP:8080/robot';
```

## 2. test-wheels-continuous-api.js

Test wheels continuous mode API.

### Sử dụng:
```bash
cd server
node test-wheels-continuous-api.js
```

### Chức năng:
- Test các API endpoints
- Test wheels continuous messages
- Test tank drive controls

## 3. test-wheels-true-continuous.js

Test wheels true continuous mode với WebSocket.

### Sử dụng:
```bash
cd server
node test-wheels-true-continuous.js
```

### Chức năng:
- Kết nối WebSocket như browser
- Gửi wheels continuous commands
- Test real-time control

## 4. ws-checker.js

Kiểm tra WebSocket connection và messages.

### Sử dụng:
```bash
cd server
node ws-checker.js
```

### Chức năng:
- Monitor WebSocket connections
- Log tất cả messages
- Debug connection issues

## Tips

### Debug server
```bash
# Terminal 1: Chạy server với log chi tiết
cd server
npm run dev

# Terminal 2: Chạy test
node test-esp-connection.js
```

### Check port đang sử dụng
```powershell
# Windows
netstat -ano | findstr :8080

# Nếu port bị chiếm, kill process:
taskkill /PID [PID] /F
```

### Test từ browser
Mở DevTools Console và chạy:
```javascript
const ws = new WebSocket('ws://192.168.1.5:8080/robot');
ws.onopen = () => console.log('Connected!');
ws.onmessage = (e) => console.log('Received:', e.data);
ws.onerror = (e) => console.error('Error:', e);
```

### Test với curl (HTTP API)
```bash
# Get status
curl http://192.168.1.5:8080/api/status

# Send command
curl -X POST http://192.168.1.5:8080/api/wheels/move \
  -H "Content-Type: application/json" \
  -d '{"left": 50, "right": 50, "durationMs": 1000}'
```

