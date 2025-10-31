# ✅ Quick Fix Checklist

## Vấn đề
ESP8266 không kết nối được WebSocket server vì **SAI IP**.

## Giải pháp nhanh (3 bước)

### ☑️ Bước 1: Verify IP đã được sửa
```bash
node firmware/update-server-ip.js
```
Kết quả mong đợi:
```
📡 Đã detect IP: 192.168.1.5
✓ IP đã đúng: 192.168.1.5
```

### ☑️ Bước 2: Upload firmware lên ESP8266
1. Mở Arduino IDE
2. Mở file: `firmware/main/main.ino`
3. Click **Upload** (Ctrl+U)
4. Đợi upload hoàn tất

### ☑️ Bước 3: Kiểm tra kết nối
Mở Serial Monitor (Ctrl+Shift+M), bạn sẽ thấy:
```
[NET] Connecting ws://192.168.1.5:8080/robot  (IP=192.168.1.8)
[NET] WebSocket CONNECTED                      ← ✅ THÀNH CÔNG!
[NET] Starting continuous wheels task...
```

## Nếu vẫn không kết nối được

### 1. Kiểm tra IP có thay đổi không
```bash
ipconfig | findstr "IPv4"
```
Nếu IP khác 192.168.1.5:
```bash
node firmware/update-server-ip.js [ip-mới]
```
Rồi upload lại firmware.

### 2. Kiểm tra Firewall
```powershell
# PowerShell (Run as Admin)
netsh advfirewall firewall add rule name="Robot Server" dir=in action=allow protocol=TCP localport=8080
```

### 3. Test server
```bash
cd server
node test-esp-connection.js
```
Nếu test script kết nối được → vấn đề ở ESP/Firewall
Nếu test script không kết nối được → vấn đề ở server

## Files đã được sửa
- ✅ `firmware/main/Config.h` - IP + heartbeat config
- ✅ `server/src/wsHub.ts` - Fix connection handling
- ✅ `server/src/index.ts` - Fix callback pattern

## Chi tiết kỹ thuật
Xem: `WEBSOCKET_DEBUG_SUMMARY.md`

