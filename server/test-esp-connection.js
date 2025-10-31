/**
 * Test script để giả lập ESP8266 WebSocket client
 * Kiểm tra xem server có nhận connection không
 */

const WebSocket = require('ws');

const WS_URL = 'ws://192.168.1.5:8080/robot';

console.log(`[TEST] Connecting to ${WS_URL}...`);
console.log(`[TEST] User-Agent: (none - simulating ESP8266)`);

const ws = new WebSocket(WS_URL, {
  // ESP8266 không gửi User-Agent header chuẩn
  headers: {
    // Không set User-Agent để giống ESP8266
  }
});

ws.on('open', () => {
  console.log('[TEST] ✓ WebSocket CONNECTED!');
  
  // Gửi hello message giống firmware
  const hello = {
    kind: 'hello',
    espId: 'TEST123',
    fw: 'test-client/1.0',
    rssi: -50,
    ip: '192.168.1.100'
  };
  
  console.log('[TEST] Sending hello:', hello);
  ws.send(JSON.stringify(hello));
});

ws.on('message', (data) => {
  try {
    const msg = JSON.parse(data.toString());
    console.log('[TEST] Received:', msg);
  } catch (e) {
    console.log('[TEST] Received (raw):', data.toString());
  }
});

ws.on('close', (code, reason) => {
  console.log(`[TEST] Connection closed: code=${code} reason=${reason}`);
  process.exit(0);
});

ws.on('error', (err) => {
  console.error('[TEST] ✗ WebSocket ERROR:', err.message);
  process.exit(1);
});

ws.on('ping', () => {
  console.log('[TEST] Received PING from server');
});

ws.on('pong', () => {
  console.log('[TEST] Received PONG from server');
});

// Đợi 5 giây rồi đóng
setTimeout(() => {
  console.log('[TEST] Test complete, closing connection...');
  ws.close();
}, 5000);

