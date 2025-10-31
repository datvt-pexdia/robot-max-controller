#!/usr/bin/env node

/**
 * Wheels Continuous Mode Test Script (REST API)
 * 
 * Test script cho Wheels Continuous Mode sử dụng REST API
 * 
 * Usage:
 *   node test-wheels-continuous-api.js              # Chạy tất cả tests
 *   node test-wheels-continuous-api.js --test N     # Chạy test số N (1-10)
 *   node test-wheels-continuous-api.js --list       # Liệt kê các tests
 * 
 * Requirements:
 *   - Server đang chạy (npm start)
 *   - ESP8266 đã kết nối với server
 */

const axios = require('axios');

// Configuration
const SERVER_HOST = process.env.SERVER_HOST || 'localhost';
const SERVER_PORT = process.env.SERVER_PORT || 8080;
const BASE_URL = `http://${SERVER_HOST}:${SERVER_PORT}`;
const DELAY_BETWEEN_TESTS = 3000; // 3 giây

// Colors for console output
const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    magenta: '\x1b[35m',
    cyan: '\x1b[36m'
};

// Utility functions
function log(message, color = 'reset') {
    const timestamp = new Date().toISOString();
    console.log(`${colors[color]}[${timestamp}] ${message}${colors.reset}`);
}

function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

let taskCounter = 0;
function generateTaskId(prefix) {
    return `${prefix}_${Date.now()}_${taskCounter++}`;
}

// API functions
async function sendWheelsTask(left, right, durationMs, taskIdPrefix = 'test') {
    const task = {
        taskId: generateTaskId(taskIdPrefix),
        device: 'wheels',
        type: 'drive',
        left: left,
        right: right,
        durationMs: durationMs
    };

    try {
        log(`→ Gửi task: L=${left} R=${right} duration=${durationMs}ms`, 'cyan');
        const response = await axios.post(`${BASE_URL}/robot/tasks/enqueue`, {
            tasks: [task]
        });
        log(`✓ Response: ${JSON.stringify(response.data)}`, 'green');
        return response.data;
    } catch (error) {
        log(`✗ Error: ${error.message}`, 'red');
        if (error.response) {
            log(`  Status: ${error.response.status}`, 'red');
            log(`  Data: ${JSON.stringify(error.response.data)}`, 'red');
        }
        throw error;
    }
}

async function getStatus() {
    try {
        const response = await axios.get(`${BASE_URL}/robot/status`);
        return response.data;
    } catch (error) {
        log(`✗ Error getting status: ${error.message}`, 'red');
        return null;
    }
}

async function cancelWheels() {
    try {
        log('→ Gửi cancel wheels', 'yellow');
        const response = await axios.post(`${BASE_URL}/robot/tasks/cancel`, {
            device: 'wheels'
        });
        log(`✓ Canceled: ${JSON.stringify(response.data)}`, 'green');
        return response.data;
    } catch (error) {
        log(`✗ Error: ${error.message}`, 'red');
        throw error;
    }
}

// Helper function: Chạy mãi mãi
async function runForever(left, right) {
    log('\n╔════════════════════════════════════════╗', 'bright');
    log('║  CHẠY MÃI MÃI (CONTINUOUS MODE)       ║', 'bright');
    log('╚════════════════════════════════════════╝\n', 'bright');
    
    log(`→ Bắt đầu chạy L=${left} R=${right} (duration = 1 giờ)`, 'cyan');
    log('⚠ Bánh xe sẽ chạy liên tục cho đến khi bạn:', 'yellow');
    log('  1. Gửi lệnh dừng: L=0 R=0', 'yellow');
    log('  2. Cancel: node test-wheels-continuous-api.js --cancel', 'yellow');
    log('  3. Ctrl+C (bánh xe vẫn chạy trên ESP)', 'yellow');
    log('', 'reset');
    
    // Gửi với duration rất lớn (1 giờ)
    await sendWheelsTask(left, right, 3600000, 'run_forever');
    
    log('\n✓ Lệnh đã gửi. Bánh xe đang chạy...', 'green');
    log('  Trong continuous mode, ESP sẽ liên tục gửi tín hiệu đến motor', 'blue');
    log('  Task không tự động dừng cho đến khi hết duration hoặc nhận lệnh mới\n', 'blue');
}

// Test cases
class WheelsContinuousTest {
    async test1_Stop() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 1: STOP Signal                  ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra gửi STOP liên tục', 'yellow');
        log('Kỳ vọng: Bánh xe không quay, logs hiển thị [00 00 FE FE]', 'yellow');
        
        await sendWheelsTask(0, 0, 1000, 'test_stop');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test2_Forward() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 2: Di chuyển tiến                ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra chuyển từ STOP sang di chuyển', 'yellow');
        log('Kỳ vọng: Bánh xe quay tiến, logs hiển thị speed rồi direction', 'yellow');
        
        await sendWheelsTask(50, 50, 3000, 'test_forward');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test3_SpeedChange() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 3: Thay đổi tốc độ               ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra thay đổi tốc độ trong khi di chuyển', 'yellow');
        log('Kỳ vọng: Bánh xe tăng/giảm tốc mượt mà', 'yellow');
        
        log('→ Bắt đầu với 40%...', 'cyan');
        await sendWheelsTask(40, 40, 2000, 'test_speed_1');
        await delay(1000);
        
        log('→ Tăng tốc lên 80%...', 'cyan');
        await sendWheelsTask(80, 80, 2000, 'test_speed_2');
        await delay(1500);
        
        log('→ Giảm tốc xuống 30%...', 'cyan');
        await sendWheelsTask(30, 30, 2000, 'test_speed_3');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test4_TurnLeft() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 4: Rẽ trái                       ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra điều khiển độc lập từng bánh', 'yellow');
        log('Kỳ vọng: Robot rẽ trái (bánh phải nhanh hơn bánh trái)', 'yellow');
        
        await sendWheelsTask(30, 70, 2000, 'test_turn_left');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test5_TurnRight() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 5: Rẽ phải                       ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra rẽ phải', 'yellow');
        log('Kỳ vọng: Robot rẽ phải (bánh trái nhanh hơn bánh phải)', 'yellow');
        
        await sendWheelsTask(70, 30, 2000, 'test_turn_right');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test6_Backward() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 6: Di chuyển lùi                 ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra hướng ngược', 'yellow');
        log('Kỳ vọng: Bánh xe quay lùi', 'yellow');
        
        await sendWheelsTask(-50, -50, 3000, 'test_backward');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test7_Spin() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 7: Xoay tại chỗ                  ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra xoay (một bánh tiến, một bánh lùi)', 'yellow');
        log('Kỳ vọng: Robot xoay tại chỗ', 'yellow');
        
        log('→ Xoay trái (left backward, right forward)...', 'cyan');
        await sendWheelsTask(-50, 50, 2000, 'test_spin_left');
        await delay(2500);
        
        log('→ Xoay phải (left forward, right backward)...', 'cyan');
        await sendWheelsTask(50, -50, 2000, 'test_spin_right');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test8_Sequence() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 8: Chuỗi lệnh liên tục           ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra xử lý nhiều lệnh liên tiếp', 'yellow');
        log('Kỳ vọng: Chuyển động mượt mà, không có delay', 'yellow');
        
        const sequence = [
            { left: 50, right: 50, duration: 1000, name: 'Forward 50%' },
            { left: 80, right: 80, duration: 1000, name: 'Forward 80%' },
            { left: 30, right: 70, duration: 1000, name: 'Turn left' },
            { left: 70, right: 30, duration: 1000, name: 'Turn right' },
            { left: -50, right: -50, duration: 1000, name: 'Backward' },
            { left: 0, right: 0, duration: 500, name: 'Stop' }
        ];

        for (const cmd of sequence) {
            log(`→ ${cmd.name}...`, 'cyan');
            await sendWheelsTask(cmd.left, cmd.right, cmd.duration, 'test_seq');
            await delay(cmd.duration + 200);
        }

        await delay(DELAY_BETWEEN_TESTS);
    }

    async test9_StressTest() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 9: Stress Test - Thay đổi nhanh ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra xử lý lệnh thay đổi rất nhanh', 'yellow');
        log('Kỳ vọng: Hệ thống không bị treo, chuyển động theo lệnh cuối', 'yellow');
        
        for (let i = 0; i < 10; i++) {
            const left = Math.floor(Math.random() * 100) - 50;
            const right = Math.floor(Math.random() * 100) - 50;
            log(`→ Lệnh ${i + 1}/10: L=${left} R=${right}`, 'cyan');
            await sendWheelsTask(left, right, 300, 'test_stress');
            await delay(300);
        }

        log('→ Dừng lại...', 'cyan');
        await sendWheelsTask(0, 0, 500, 'test_stress_stop');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test10_EdgeCases() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 10: Edge Cases                   ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra các trường hợp đặc biệt', 'yellow');
        
        log('→ Tốc độ rất thấp (10%)...', 'cyan');
        await sendWheelsTask(10, 10, 2000, 'test_edge_low');
        await delay(2500);
        
        log('→ Tốc độ tối đa (100%)...', 'cyan');
        await sendWheelsTask(100, 100, 2000, 'test_edge_max');
        await delay(2500);
        
        log('→ Một bánh dừng, một bánh chạy...', 'cyan');
        await sendWheelsTask(0, 50, 2000, 'test_edge_one');
        await delay(2500);
        
        log('→ Giá trị âm tối đa (-100%)...', 'cyan');
        await sendWheelsTask(-100, -100, 2000, 'test_edge_min');
        await delay(2500);
        
        log('→ Dừng lại...', 'cyan');
        await sendWheelsTask(0, 0, 500, 'test_edge_stop');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test11_CancelTest() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 11: Cancel Test                  ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra hành vi cancel', 'yellow');
        log('Kỳ vọng: Bánh xe dừng, task vẫn chạy (gửi STOP)', 'yellow');
        
        log('→ Bắt đầu di chuyển...', 'cyan');
        await sendWheelsTask(70, 70, 5000, 'test_cancel');
        await delay(2000);
        
        log('→ Cancel wheels...', 'yellow');
        await cancelWheels();
        await delay(2000);
        
        log('→ Kiểm tra có thể di chuyển lại...', 'cyan');
        await sendWheelsTask(50, 50, 2000, 'test_after_cancel');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test12_StatusCheck() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 12: Status Check                 ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra status API', 'yellow');
        
        log('→ Lấy status ban đầu...', 'cyan');
        let status = await getStatus();
        log(`Status: ${JSON.stringify(status, null, 2)}`, 'blue');
        await delay(1000);
        
        log('→ Bắt đầu di chuyển...', 'cyan');
        await sendWheelsTask(60, 60, 3000, 'test_status');
        await delay(500);
        
        log('→ Lấy status trong khi di chuyển...', 'cyan');
        status = await getStatus();
        log(`Status: ${JSON.stringify(status, null, 2)}`, 'blue');
        await delay(3000);
        
        log('→ Lấy status sau khi hoàn thành...', 'cyan');
        status = await getStatus();
        log(`Status: ${JSON.stringify(status, null, 2)}`, 'blue');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async test13_ContinuousRun() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  TEST 13: Continuous Run (Chạy mãi)   ║', 'bright');
        log('╚════════════════════════════════════════╝', 'bright');
        log('Mục đích: Kiểm tra chạy liên tục không dừng', 'yellow');
        log('Kỳ vọng: Bánh xe chạy mãi cho đến khi cancel', 'yellow');
        
        log('→ Bắt đầu chạy với duration rất lớn (1 giờ = 3600000ms)...', 'cyan');
        await sendWheelsTask(50, 50, 3600000, 'test_continuous');
        
        log('⚠ Bánh xe đang chạy liên tục...', 'yellow');
        log('  Để dừng, gọi: node test-wheels-continuous-api.js --cancel', 'yellow');
        log('  Hoặc Ctrl+C để thoát (bánh xe vẫn chạy)', 'yellow');
        
        // Chờ 5 giây để demo
        await delay(5000);
        
        log('→ Demo: Thay đổi tốc độ trong khi chạy...', 'cyan');
        await sendWheelsTask(80, 80, 3600000, 'test_continuous_2');
        await delay(3000);
        
        log('→ Demo: Rẽ trái...', 'cyan');
        await sendWheelsTask(30, 70, 3600000, 'test_continuous_3');
        await delay(3000);
        
        log('→ Dừng lại (để tiếp tục test khác)...', 'cyan');
        await sendWheelsTask(0, 0, 1000, 'test_continuous_stop');
        await delay(DELAY_BETWEEN_TESTS);
    }

    async runAllTests() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║  WHEELS CONTINUOUS MODE - TEST SUITE  ║', 'bright');
        log('║           (REST API)                   ║', 'bright');
        log('╚════════════════════════════════════════╝\n', 'bright');
        
        // Kiểm tra kết nối
        log('→ Kiểm tra kết nối server...', 'yellow');
        const status = await getStatus();
        if (!status) {
            log('✗ Không thể kết nối đến server!', 'red');
            log(`  Đảm bảo server đang chạy tại ${BASE_URL}`, 'red');
            process.exit(1);
        }
        
        if (!status.connected) {
            log('⚠ ESP chưa kết nối!', 'yellow');
            log('  Đảm bảo ESP8266 đã kết nối với server', 'yellow');
            log('  Tiếp tục test (có thể không có phản hồi từ ESP)...', 'yellow');
        } else {
            log('✓ ESP đã kết nối', 'green');
        }
        
        await delay(1000);

        try {
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
            await this.test11_CancelTest();
            await this.test12_StatusCheck();
            await this.test13_ContinuousRun();
            
            log('\n╔════════════════════════════════════════╗', 'bright');
            log('║       TẤT CẢ TEST ĐÃ HOÀN THÀNH       ║', 'bright');
            log('╚════════════════════════════════════════╝\n', 'bright');
            
            // Dừng lại cuối cùng
            log('→ Gửi lệnh STOP cuối cùng...', 'cyan');
            await sendWheelsTask(0, 0, 1000, 'test_final_stop');
            
        } catch (error) {
            log(`\n✗ Test failed: ${error.message}`, 'red');
            process.exit(1);
        }
    }

    async runSingleTest(testNumber) {
        const tests = [
            { fn: this.test1_Stop, name: 'STOP Signal' },
            { fn: this.test2_Forward, name: 'Di chuyển tiến' },
            { fn: this.test3_SpeedChange, name: 'Thay đổi tốc độ' },
            { fn: this.test4_TurnLeft, name: 'Rẽ trái' },
            { fn: this.test5_TurnRight, name: 'Rẽ phải' },
            { fn: this.test6_Backward, name: 'Di chuyển lùi' },
            { fn: this.test7_Spin, name: 'Xoay tại chỗ' },
            { fn: this.test8_Sequence, name: 'Chuỗi lệnh liên tục' },
            { fn: this.test9_StressTest, name: 'Stress Test' },
            { fn: this.test10_EdgeCases, name: 'Edge Cases' },
            { fn: this.test11_CancelTest, name: 'Cancel Test' },
            { fn: this.test12_StatusCheck, name: 'Status Check' },
            { fn: this.test13_ContinuousRun, name: 'Continuous Run (Chạy mãi)' }
        ];

        if (testNumber < 1 || testNumber > tests.length) {
            log(`✗ Test number phải từ 1 đến ${tests.length}`, 'red');
            process.exit(1);
        }

        const test = tests[testNumber - 1];
        log(`\n→ Chạy test ${testNumber}: ${test.name}`, 'bright');
        
        try {
            await test.fn.call(this);
            log(`\n✓ Test ${testNumber} hoàn thành`, 'green');
        } catch (error) {
            log(`\n✗ Test ${testNumber} failed: ${error.message}`, 'red');
            process.exit(1);
        }
    }

    listTests() {
        log('\n╔════════════════════════════════════════╗', 'bright');
        log('║    DANH SÁCH TESTS                     ║', 'bright');
        log('╚════════════════════════════════════════╝\n', 'bright');
        
        const tests = [
            'STOP Signal',
            'Di chuyển tiến',
            'Thay đổi tốc độ',
            'Rẽ trái',
            'Rẽ phải',
            'Di chuyển lùi',
            'Xoay tại chỗ',
            'Chuỗi lệnh liên tục',
            'Stress Test',
            'Edge Cases',
            'Cancel Test',
            'Status Check',
            'Continuous Run (Chạy mãi)'
        ];

        tests.forEach((test, index) => {
            log(`  ${index + 1}. ${test}`, 'cyan');
        });
        
        log('\nSử dụng:', 'yellow');
        log('  node test-wheels-continuous-api.js              # Chạy tất cả tests', 'yellow');
        log('  node test-wheels-continuous-api.js --test N     # Chạy test số N', 'yellow');
        log('  node test-wheels-continuous-api.js --list       # Hiển thị danh sách', 'yellow');
        log('  node test-wheels-continuous-api.js --run L R    # Chạy mãi với L, R', 'yellow');
        log('  node test-wheels-continuous-api.js --cancel     # Dừng wheels\n', 'yellow');
    }
}

// Main
async function main() {
    const args = process.argv.slice(2);
    const tester = new WheelsContinuousTest();

    if (args.length === 0) {
        // Chạy tất cả tests
        await tester.runAllTests();
    } else if (args[0] === '--test' && args[1]) {
        // Chạy một test cụ thể
        const testNumber = parseInt(args[1]);
        if (isNaN(testNumber)) {
            log('✗ Test number phải là số', 'red');
            process.exit(1);
        }
        await tester.runSingleTest(testNumber);
    } else if (args[0] === '--list') {
        // Liệt kê tests
        tester.listTests();
    } else if (args[0] === '--run' && args[1] && args[2]) {
        // Chạy mãi với left, right
        const left = parseInt(args[1]);
        const right = parseInt(args[2]);
        if (isNaN(left) || isNaN(right)) {
            log('✗ Left và Right phải là số', 'red');
            log('  Ví dụ: node test-wheels-continuous-api.js --run 50 50', 'yellow');
            process.exit(1);
        }
        await runForever(left, right);
    } else if (args[0] === '--cancel') {
        // Cancel wheels
        log('\n→ Đang cancel wheels...', 'yellow');
        await cancelWheels();
        log('✓ Đã gửi lệnh cancel', 'green');
    } else {
        log('✗ Tham số không hợp lệ', 'red');
        tester.listTests();
        process.exit(1);
    }
}

main().catch(error => {
    log(`\n✗ Fatal error: ${error.message}`, 'red');
    console.error(error);
    process.exit(1);
});

