// Demo script để test WheelsContinuousTask qua Node.js Server
// Kết nối đến server thay vì trực tiếp đến ESP8266

const WebSocket = require('ws');

// Cấu hình kết nối - kết nối đến Node.js server
const WS_URL = 'ws://localhost:8080/ws'; // Kết nối đến server, không phải ESP8266
let ws = null;

// Kết nối WebSocket
function connect() {
  console.log('Connecting to Node.js server at', WS_URL);
  ws = new WebSocket(WS_URL);
  
  ws.on('open', () => {
    console.log('✅ Connected to Node.js server');
    console.log('Starting wheels continuous demo...');
    
    // Bắt đầu demo sau 2 giây
    setTimeout(startDemo, 2000);
  });
  
  ws.on('message', (data) => {
    const message = JSON.parse(data.toString());
    console.log('📨 Received:', message);
  });
  
  ws.on('error', (error) => {
    console.error('❌ WebSocket error:', error.message);
  });
  
  ws.on('close', () => {
    console.log('🔌 Connection closed');
    // Tự động kết nối lại sau 3 giây
    setTimeout(connect, 3000);
  });
}

// Gửi message đến server (sẽ forward đến ESP8266)
function sendMessage(message) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(message));
    console.log('📤 Sent:', message);
  } else {
    console.log('❌ WebSocket not connected, cannot send:', message);
  }
}

// Demo sequence
function startDemo() {
  console.log('\n🚀 Starting Wheels Continuous Demo via Server...\n');
  
  // 1. Khởi động wheels continuous task (đã được khởi động trong setup())
  console.log('1. Wheels continuous task should already be running on ESP8266');
  
  // 2. Bắt đầu di chuyển tiến với tốc độ 50% trong 3 giây
  setTimeout(() => {
    console.log('2. Starting forward movement at 50% speed for 3 seconds');
    sendMessage({
      kind: 'wheels.move',
      move: true,
      leftSpeed: 50,
      rightSpeed: 50,
      durationMs: 3000
    });
  }, 1000);
  
  // 3. Tăng tốc độ lên 80%
  setTimeout(() => {
    console.log('3. Increasing speed to 80%');
    sendMessage({
      kind: 'wheels.speed',
      leftSpeed: 80,
      rightSpeed: 80
    });
  }, 5000);
  
  // 4. Rẽ trái (giảm tốc độ bánh trái)
  setTimeout(() => {
    console.log('4. Turning left (reducing left wheel speed)');
    sendMessage({
      kind: 'wheels.speed',
      leftSpeed: 30,
      rightSpeed: 80
    });
  }, 8000);
  
  // 5. Rẽ phải (giảm tốc độ bánh phải)
  setTimeout(() => {
    console.log('5. Turning right (reducing right wheel speed)');
    sendMessage({
      kind: 'wheels.speed',
      leftSpeed: 80,
      rightSpeed: 30
    });
  }, 11000);
  
  // 6. Đi lùi
  setTimeout(() => {
    console.log('6. Moving backward');
    sendMessage({
      kind: 'wheels.direction',
      leftDirection: -100,  // Left wheel reverse
      rightDirection: -100  // Right wheel reverse
    });
  }, 14000);
  
  // 7. Dừng khẩn cấp
  setTimeout(() => {
    console.log('7. Emergency stop!');
    sendMessage({
      kind: 'wheels.stop'
    });
  }, 17000);
  
  // 8. Bắt đầu lại di chuyển tiến với thời gian giới hạn
  setTimeout(() => {
    console.log('8. Starting forward movement again for 4 seconds');
    sendMessage({
      kind: 'wheels.move',
      move: true,
      leftSpeed: 60,
      rightSpeed: 60,
      durationMs: 4000
    });
  }, 20000);
  
  // 9. Dừng bình thường
  setTimeout(() => {
    console.log('9. Normal stop');
    sendMessage({
      kind: 'wheels.move',
      move: false,
      leftSpeed: 0,
      rightSpeed: 0
    });
  }, 25000);
  
  // 10. Chạy liên tục không giới hạn thời gian
  setTimeout(() => {
    console.log('10. Starting continuous movement (no time limit)');
    sendMessage({
      kind: 'wheels.move',
      move: true,
      leftSpeed: 40,
      rightSpeed: 40
      // Không có durationMs = chạy liên tục
    });
  }, 28000);
  
  // 11. Kết thúc demo
  setTimeout(() => {
    console.log('\n✅ Demo completed!');
    console.log('The robot should be moving continuously now.');
    console.log('Press Ctrl+C to emergency stop and exit.\n');
  }, 32000);
}

// Xử lý tín hiệu thoát
process.on('SIGINT', () => {
  console.log('\n🛑 Shutting down...');
  
  // Gửi lệnh dừng khẩn cấp trước khi thoát
  if (ws && ws.readyState === WebSocket.OPEN) {
    sendMessage({
      kind: 'wheels.stop'
    });
    
    setTimeout(() => {
      ws.close();
      process.exit(0);
    }, 1000);
  } else {
    process.exit(0);
  }
});

// Bắt đầu kết nối
connect();

console.log('🤖 Wheels Continuous Demo Script (via Server)');
console.log('===============================================');
console.log('This script will test the continuous wheels control system.');
console.log('Make sure your Node.js server is running and ESP8266 is connected.');
console.log('The demo will:');
console.log('- Connect to Node.js server (which forwards to ESP8266)');
console.log('- Move forward at different speeds (speed sent once, direction continuously)');
console.log('- Test duration-based movement (auto-stop after specified time)');
console.log('- Test continuous movement (no time limit)');
console.log('- Turn left and right');
console.log('- Move backward');
console.log('- Test emergency stop');
console.log('- Demonstrate optimized signal sending (speed once, direction continuous)');
console.log('- Show progress tracking for timed movements');
console.log('');
console.log('Press Ctrl+C to stop the demo and emergency stop the robot.');
console.log('');