/**
 * Test script cho Wheels Continuous Mode
 * 
 * Sử dụng:
 * 1. Đảm bảo server đang chạy: cd server && npm start
 * 2. Đảm bảo ESP8266 đã kết nối với server
 * 3. Chạy script: node test-wheels-continuous.js
 */

const WebSocket = require('ws');

// Cấu hình
const SERVER_URL = 'ws://localhost:3000';
const DELAY_BETWEEN_TESTS = 3000; // 3 giây giữa các test

// Utility function để delay
const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// Tạo taskId unique
let taskCounter = 0;
const generateTaskId = (prefix) => `${prefix}_${Date.now()}_${taskCounter++}`;

class WheelsTest {
  constructor() {
    this.ws = null;
  }

  connect() {
    return new Promise((resolve, reject) => {
      console.log(`Đang kết nối đến ${SERVER_URL}...`);
      this.ws = new WebSocket(SERVER_URL);

      this.ws.on('open', () => {
        console.log('✓ Đã kết nối WebSocket\n');
        resolve();
      });

      this.ws.on('error', (error) => {
        console.error('✗ Lỗi WebSocket:', error.message);
        reject(error);
      });

      this.ws.on('message', (data) => {
        try {
          const msg = JSON.parse(data.toString());
          console.log('← Nhận:', msg);
        } catch (e) {
          console.log('← Nhận (raw):', data.toString());
        }
      });

      this.ws.on('close', () => {
        console.log('WebSocket đã đóng');
      });
    });
  }

  sendTask(device, left, right, durationMs, taskIdPrefix) {
    const task = {
      type: 'tasks',
      mode: 'enqueue',
      tasks: [{
        taskId: generateTaskId(taskIdPrefix),
        device: device,
        left: left,
        right: right,
        durationMs: durationMs
      }]
    };
    console.log('→ Gửi:', task);
    this.ws.send(JSON.stringify(task));
  }

  async test1_Stop() {
    console.log('\n=== TEST 1: STOP Signal ===');
    console.log('Mục đích: Kiểm tra gửi STOP liên tục');
    console.log('Kỳ vọng: Bánh xe không quay, logs hiển thị [00 00 FE FE]');
    this.sendTask('wheels', 0, 0, 1000, 'test_stop');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test2_Forward() {
    console.log('\n=== TEST 2: Di chuyển tiến ===');
    console.log('Mục đích: Kiểm tra chuyển từ STOP sang di chuyển');
    console.log('Kỳ vọng: Bánh xe quay tiến, logs hiển thị speed rồi direction');
    this.sendTask('wheels', 50, 50, 3000, 'test_forward');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test3_SpeedChange() {
    console.log('\n=== TEST 3: Thay đổi tốc độ ===');
    console.log('Mục đích: Kiểm tra thay đổi tốc độ trong khi di chuyển');
    console.log('Kỳ vọng: Bánh xe tăng tốc mượt mà');
    
    this.sendTask('wheels', 40, 40, 2000, 'test_speed_1');
    await delay(1000);
    
    console.log('→ Tăng tốc lên 80%...');
    this.sendTask('wheels', 80, 80, 2000, 'test_speed_2');
    await delay(1500);
    
    console.log('→ Giảm tốc xuống 30%...');
    this.sendTask('wheels', 30, 30, 2000, 'test_speed_3');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test4_TurnLeft() {
    console.log('\n=== TEST 4: Rẽ trái ===');
    console.log('Mục đích: Kiểm tra điều khiển độc lập từng bánh');
    console.log('Kỳ vọng: Robot rẽ trái (bánh phải nhanh hơn bánh trái)');
    this.sendTask('wheels', 30, 70, 2000, 'test_turn_left');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test5_TurnRight() {
    console.log('\n=== TEST 5: Rẽ phải ===');
    console.log('Mục đích: Kiểm tra rẽ phải');
    console.log('Kỳ vọng: Robot rẽ phải (bánh trái nhanh hơn bánh phải)');
    this.sendTask('wheels', 70, 30, 2000, 'test_turn_right');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test6_Backward() {
    console.log('\n=== TEST 6: Di chuyển lùi ===');
    console.log('Mục đích: Kiểm tra hướng ngược');
    console.log('Kỳ vọng: Bánh xe quay lùi');
    this.sendTask('wheels', -50, -50, 3000, 'test_backward');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test7_Spin() {
    console.log('\n=== TEST 7: Xoay tại chỗ ===');
    console.log('Mục đích: Kiểm tra xoay (một bánh tiến, một bánh lùi)');
    console.log('Kỳ vọng: Robot xoay tại chỗ');
    
    console.log('→ Xoay trái (left backward, right forward)...');
    this.sendTask('wheels', -50, 50, 2000, 'test_spin_left');
    await delay(2500);
    
    console.log('→ Xoay phải (left forward, right backward)...');
    this.sendTask('wheels', 50, -50, 2000, 'test_spin_right');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test8_Sequence() {
    console.log('\n=== TEST 8: Chuỗi lệnh liên tục ===');
    console.log('Mục đích: Kiểm tra xử lý nhiều lệnh liên tiếp');
    console.log('Kỳ vọng: Chuyển động mượt mà, không có delay');
    
    const sequence = [
      { left: 50, right: 50, duration: 1000, name: 'Forward 50%' },
      { left: 80, right: 80, duration: 1000, name: 'Forward 80%' },
      { left: 30, right: 70, duration: 1000, name: 'Turn left' },
      { left: 70, right: 30, duration: 1000, name: 'Turn right' },
      { left: -50, right: -50, duration: 1000, name: 'Backward' },
      { left: 0, right: 0, duration: 500, name: 'Stop' }
    ];

    for (const cmd of sequence) {
      console.log(`→ ${cmd.name}...`);
      this.sendTask('wheels', cmd.left, cmd.right, cmd.duration, 'test_seq');
      await delay(cmd.duration + 200);
    }

    await delay(DELAY_BETWEEN_TESTS);
  }

  async test9_StressTest() {
    console.log('\n=== TEST 9: Stress Test - Thay đổi nhanh ===');
    console.log('Mục đích: Kiểm tra xử lý lệnh thay đổi rất nhanh');
    console.log('Kỳ vọng: Hệ thống không bị treo, chuyển động theo lệnh cuối');
    
    for (let i = 0; i < 10; i++) {
      const left = Math.floor(Math.random() * 100) - 50;
      const right = Math.floor(Math.random() * 100) - 50;
      console.log(`→ Lệnh ${i + 1}/10: L=${left} R=${right}`);
      this.sendTask('wheels', left, right, 300, 'test_stress');
      await delay(300);
    }

    console.log('→ Dừng lại...');
    this.sendTask('wheels', 0, 0, 500, 'test_stress_stop');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async test10_EdgeCases() {
    console.log('\n=== TEST 10: Edge Cases ===');
    console.log('Mục đích: Kiểm tra các trường hợp đặc biệt');
    
    console.log('→ Tốc độ rất thấp (10%)...');
    this.sendTask('wheels', 10, 10, 2000, 'test_edge_low');
    await delay(2500);
    
    console.log('→ Tốc độ tối đa (100%)...');
    this.sendTask('wheels', 100, 100, 2000, 'test_edge_max');
    await delay(2500);
    
    console.log('→ Một bánh dừng, một bánh chạy...');
    this.sendTask('wheels', 0, 50, 2000, 'test_edge_one');
    await delay(2500);
    
    console.log('→ Dừng lại...');
    this.sendTask('wheels', 0, 0, 500, 'test_edge_stop');
    await delay(DELAY_BETWEEN_TESTS);
  }

  async runAllTests() {
    try {
      await this.connect();
      
      console.log('\n╔════════════════════════════════════════╗');
      console.log('║  WHEELS CONTINUOUS MODE - TEST SUITE  ║');
      console.log('╚════════════════════════════════════════╝\n');
      
      await this.test1_Stop();
      await this.test2_Forward();
      await this.test3_SpeedChange();
      await this.test4_TurnLeft();
      await this.test5_TurnRight();
      await this.test6_Backward();
      await this.test7_Spin();
      await this.test8_Sequence();
      await this.test9_StressTest();
      await this.test10_EdgeCases();
      
      console.log('\n╔════════════════════════════════════════╗');
      console.log('║       TẤT CẢ TEST ĐÃ HOÀN THÀNH       ║');
      console.log('╚════════════════════════════════════════╝\n');
      
      // Dừng lại cuối cùng
      console.log('→ Gửi lệnh STOP cuối cùng...');
      this.sendTask('wheels', 0, 0, 1000, 'test_final_stop');
      
      await delay(2000);
      this.ws.close();
      
    } catch (error) {
      console.error('✗ Lỗi khi chạy test:', error);
      if (this.ws) {
        this.ws.close();
      }
    }
  }

  async runSingleTest(testNumber) {
    try {
      await this.connect();
      
      const tests = [
        this.test1_Stop,
        this.test2_Forward,
        this.test3_SpeedChange,
        this.test4_TurnLeft,
        this.test5_TurnRight,
        this.test6_Backward,
        this.test7_Spin,
        this.test8_Sequence,
        this.test9_StressTest,
        this.test10_EdgeCases
      ];

      if (testNumber < 1 || testNumber > tests.length) {
        console.error(`✗ Test number phải từ 1 đến ${tests.length}`);
        this.ws.close();
        return;
      }

      await tests[testNumber - 1].call(this);
      
      console.log('\n→ Test hoàn thành');
      await delay(2000);
      this.ws.close();
      
    } catch (error) {
      console.error('✗ Lỗi khi chạy test:', error);
      if (this.ws) {
        this.ws.close();
      }
    }
  }

  disconnect() {
    if (this.ws) {
      this.ws.close();
    }
  }
}

// Main
async function main() {
  const args = process.argv.slice(2);
  const tester = new WheelsTest();

  if (args.length === 0) {
    // Chạy tất cả tests
    await tester.runAllTests();
  } else if (args[0] === '--test' && args[1]) {
    // Chạy một test cụ thể
    const testNumber = parseInt(args[1]);
    await tester.runSingleTest(testNumber);
  } else {
    console.log('Sử dụng:');
    console.log('  node test-wheels-continuous.js           # Chạy tất cả tests');
    console.log('  node test-wheels-continuous.js --test N  # Chạy test số N (1-10)');
    console.log('\nDanh sách tests:');
    console.log('  1. STOP Signal');
    console.log('  2. Di chuyển tiến');
    console.log('  3. Thay đổi tốc độ');
    console.log('  4. Rẽ trái');
    console.log('  5. Rẽ phải');
    console.log('  6. Di chuyển lùi');
    console.log('  7. Xoay tại chỗ');
    console.log('  8. Chuỗi lệnh liên tục');
    console.log('  9. Stress Test');
    console.log(' 10. Edge Cases');
    process.exit(1);
  }
}

main().catch(console.error);

