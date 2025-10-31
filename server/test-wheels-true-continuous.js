#!/usr/bin/env node

/**
 * Test Wheels với continuousMode = true (mặc định)
 * 
 * Khi continuousMode = true:
 * - Task KHÔNG BAO GIỜ tự động dừng
 * - Duration bị BỎ QUA
 * - Chỉ dừng khi: cancel hoặc gửi L=0 R=0
 * 
 * Usage:
 *   node test-wheels-true-continuous.js              # Demo continuous mode
 *   node test-wheels-true-continuous.js --forward    # Chạy tiến mãi
 *   node test-wheels-true-continuous.js --stop       # Dừng lại
 */

const axios = require('axios');

// Configuration
const SERVER_HOST = process.env.SERVER_HOST || 'localhost';
const SERVER_PORT = process.env.SERVER_PORT || 8080;
const BASE_URL = `http://${SERVER_HOST}:${SERVER_PORT}`;

// Colors
const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    cyan: '\x1b[36m'
};

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
        log(`→ Gửi: L=${left} R=${right} duration=${durationMs}ms`, 'cyan');
        const response = await axios.post(`${BASE_URL}/robot/tasks/enqueue`, {
            tasks: [task]
        });
        log(`✓ Response: ${JSON.stringify(response.data)}`, 'green');
        return response.data;
    } catch (error) {
        log(`✗ Error: ${error.message}`, 'red');
        throw error;
    }
}

async function getStatus() {
    try {
        const response = await axios.get(`${BASE_URL}/robot/status`);
        return response.data;
    } catch (error) {
        log(`✗ Error: ${error.message}`, 'red');
        return null;
    }
}

async function demoTest() {
    log('\n╔════════════════════════════════════════════════════════╗', 'bright');
    log('║  TEST: continuousMode = true (MẶC ĐỊNH)              ║', 'bright');
    log('╚════════════════════════════════════════════════════════╝\n', 'bright');
    
    log('📌 Trong continuous mode:', 'yellow');
    log('  • Task KHÔNG BAO GIỜ tự động dừng', 'yellow');
    log('  • Duration bị BỎ QUA (có thể gửi bất kỳ giá trị nào)', 'yellow');
    log('  • ESP liên tục gửi tín hiệu mỗi 10ms', 'yellow');
    log('  • Chỉ dừng khi: cancel hoặc gửi L=0 R=0\n', 'yellow');
    
    // Kiểm tra kết nối
    log('→ Kiểm tra kết nối...', 'cyan');
    const status = await getStatus();
    if (!status) {
        log('✗ Server không phản hồi!', 'red');
        process.exit(1);
    }
    if (!status.connected) {
        log('⚠ ESP chưa kết nối!', 'yellow');
    } else {
        log('✓ ESP đã kết nối\n', 'green');
    }
    
    // Test 1: Duration ngắn (100ms) - KHÔNG TỰ ĐỘNG DỪNG
    // log('\n═══ TEST 1: Duration ngắn (100ms) ═══', 'bright');
    // log('Kỳ vọng: Bánh xe chạy MÃI dù duration chỉ 100ms', 'yellow');
    // await sendWheelsTask(50, 50, 100, 'test_short_duration');
    // log('⏱ Chờ 5 giây...', 'blue');
    // await delay(5000);
    // log('✓ Bánh xe VẪN ĐANG CHẠY sau 5 giây (duration đã bị bỏ qua)', 'green');
    
    // // Test 2: Thay đổi tốc độ - KHÔNG CẦN DỪNG
    // log('\n═══ TEST 2: Thay đổi tốc độ ═══', 'bright');
    // log('Kỳ vọng: Thay đổi tốc độ ngay lập tức, không cần dừng', 'yellow');
    // await sendWheelsTask(80, 80, 100, 'test_speed_change');
    // log('⏱ Chờ 3 giây...', 'blue');
    // await delay(3000);
    // log('✓ Tốc độ đã thay đổi, bánh xe vẫn chạy', 'green');
    
    // // Test 3: Rẽ trái
    // log('\n═══ TEST 3: Rẽ trái ═══', 'bright');
    // await sendWheelsTask(-70, 70, 100, 'test_turn_left');
    // log('⏱ Chờ 3 giây...', 'blue');
    // await delay(3000);
    // log('✓ Đang rẽ trái, bánh xe vẫn chạy', 'green');
    
    // // Test 4: Rẽ phải
    // log('\n═══ TEST 4: Rẽ phải ═══', 'bright');
    // await sendWheelsTask(70, -70, 100, 'test_turn_right');
    // log('⏱ Chờ 3 giây...', 'blue');
    // await delay(3000);
    // log('✓ Đang rẽ phải, bánh xe vẫn chạy', 'green');
    
    // // Test 5: Lùi
    // log('\n═══ TEST 5: Di chuyển lùi ═══', 'bright');
    // await sendWheelsTask(-50, -50, 100, 'test_backward');
    // log('⏱ Chờ 3 giây...', 'blue');
    // await delay(3000);
    // log('✓ Đang lùi, bánh xe vẫn chạy', 'green');
    
    // // Test 6: Dừng bằng L=0 R=0
    // log('\n═══ TEST 6: Dừng bằng L=0 R=0 ═══', 'bright');
    // log('Kỳ vọng: Bánh xe dừng, nhưng task vẫn chạy (gửi STOP signal)', 'yellow');
    // await sendWheelsTask(0, 0, 100, 'test_stop');
    // log('⏱ Chờ 3 giây...', 'blue');
    // await delay(3000);
    // log('✓ Bánh xe đã dừng, ESP vẫn gửi STOP signal liên tục', 'green');
    
    // // Test 7: Chạy lại sau khi dừng
    // log('\n═══ TEST 7: Chạy lại sau khi dừng ═══', 'bright');
    // await sendWheelsTask(60, 60, 100, 'test_restart');
    // log('⏱ Chờ 3 giây...', 'blue');
    // await delay(3000);
    // log('✓ Bánh xe chạy lại ngay lập tức', 'green');
    
    // Test 8: Nhiều lệnh liên tiếp
    log('\n═══ TEST 8: Nhiều lệnh liên tiếp ═══', 'bright');
    log('Kỳ vọng: Mỗi lệnh thực hiện ngay, không chờ duration', 'yellow');
    const commands = [
        { left: 100, right: 100, name: 'Forward' },
        { left: 100, right: -100, name: 'Right' },
        { left: -100, right: -100, name: 'Backward' },
        { left: -100, right: 100, name: 'Left' }
    ];
    
    for (const cmd of commands) {
        log(`→ ${cmd.name}`, 'cyan');
        await sendWheelsTask(cmd.left, cmd.right, 100, 'test_sequence');
        await delay(5000); // Chờ ngắn để thấy sự thay đổi
    }
    log('✓ Tất cả lệnh thực hiện mượt mà', 'green');
    
    // Dừng cuối cùng
    log('\n═══ Dừng cuối cùng ═══', 'bright');
    await sendWheelsTask(0, 0, 100, 'test_final_stop');
    
    log('\n╔════════════════════════════════════════════════════════╗', 'bright');
    log('║  KẾT LUẬN: continuousMode = true                      ║', 'bright');
    log('╚════════════════════════════════════════════════════════╝', 'bright');
    log('✓ Duration BỊ BỎ QUA hoàn toàn', 'green');
    log('✓ Task chạy MÃI cho đến khi nhận lệnh mới', 'green');
    log('✓ Thay đổi tốc độ/hướng NGAY LẬP TỨC', 'green');
    log('✓ Phù hợp cho điều khiển realtime\n', 'green');
}

async function runForward() {
    log('\n╔════════════════════════════════════════════════════════╗', 'bright');
    log('║  CHẠY TIẾN MÃI MÃI                                    ║', 'bright');
    log('╚════════════════════════════════════════════════════════╝\n', 'bright');
    
    log('→ Gửi lệnh chạy tiến với duration = 100ms', 'cyan');
    log('  (Nhưng sẽ chạy MÃI vì continuousMode = true)', 'yellow');
    await sendWheelsTask(50, 50, 100, 'forward_forever');
    
    log('\n✓ Bánh xe đang chạy tiến...', 'green');
    log('  Để dừng, chạy: node test-wheels-true-continuous.js --stop\n', 'blue');
}

async function runStop() {
    log('\n╔════════════════════════════════════════════════════════╗', 'bright');
    log('║  DỪNG BÁNH XE                                         ║', 'bright');
    log('╚════════════════════════════════════════════════════════╝\n', 'bright');
    
    await sendWheelsTask(0, 0, 100, 'stop');
    log('\n✓ Đã gửi lệnh dừng', 'green');
    log('  ESP vẫn gửi STOP signal liên tục (continuous mode)\n', 'blue');
}

async function showInfo() {
    log('\n╔════════════════════════════════════════════════════════╗', 'bright');
    log('║  CONTINUOUS MODE = TRUE (MẶC ĐỊNH)                    ║', 'bright');
    log('╚════════════════════════════════════════════════════════╝\n', 'bright');
    
    log('📌 Đặc điểm:', 'yellow');
    log('  • continuousMode = true (mặc định trong WheelsDevice)', 'cyan');
    log('  • Task KHÔNG BAO GIỜ tự động dừng', 'cyan');
    log('  • Duration bị BỎ QUA (có thể gửi bất kỳ giá trị nào)', 'cyan');
    log('  • ESP liên tục gửi tín hiệu mỗi 10ms', 'cyan');
    log('  • Chỉ dừng khi: gửi L=0 R=0 hoặc cancel', 'cyan');
    log('', 'reset');
    
    log('💡 Lợi ích:', 'yellow');
    log('  • Điều khiển realtime, phản hồi ngay lập tức', 'green');
    log('  • Không cần tính toán duration chính xác', 'green');
    log('  • Chuyển động mượt mà, không giật cục', 'green');
    log('  • An toàn: luôn có tín hiệu điều khiển', 'green');
    log('', 'reset');
    
    log('🎯 Cách sử dụng:', 'yellow');
    log('  node test-wheels-true-continuous.js              # Demo đầy đủ', 'cyan');
    log('  node test-wheels-true-continuous.js --forward    # Chạy tiến mãi', 'cyan');
    log('  node test-wheels-true-continuous.js --stop       # Dừng lại', 'cyan');
    log('  node test-wheels-true-continuous.js --info       # Hiển thị info', 'cyan');
    log('', 'reset');
    
    log('📝 Ví dụ:', 'yellow');
    log('  # Chạy tiến', 'cyan');
    log('  curl -X POST http://localhost:8080/robot/tasks/enqueue \\', 'blue');
    log('    -H "Content-Type: application/json" \\', 'blue');
    log('    -d \'{"tasks":[{"taskId":"t1","device":"wheels","type":"drive","left":50,"right":50,"durationMs":100}]}\'', 'blue');
    log('  # → Bánh xe chạy MÃI dù duration chỉ 100ms', 'green');
    log('', 'reset');
    
    log('  # Thay đổi tốc độ', 'cyan');
    log('  curl -X POST http://localhost:8080/robot/tasks/enqueue \\', 'blue');
    log('    -H "Content-Type: application/json" \\', 'blue');
    log('    -d \'{"tasks":[{"taskId":"t2","device":"wheels","type":"drive","left":80,"right":80,"durationMs":100}]}\'', 'blue');
    log('  # → Tốc độ thay đổi NGAY LẬP TỨC', 'green');
    log('', 'reset');
    
    log('  # Dừng', 'cyan');
    log('  curl -X POST http://localhost:8080/robot/tasks/enqueue \\', 'blue');
    log('    -H "Content-Type: application/json" \\', 'blue');
    log('    -d \'{"tasks":[{"taskId":"t3","device":"wheels","type":"drive","left":0,"right":0,"durationMs":100}]}\'', 'blue');
    log('  # → Bánh xe dừng, ESP vẫn gửi STOP signal\n', 'green');
}

// Main
async function main() {
    const args = process.argv.slice(2);
    
    if (args.length === 0) {
        // Demo đầy đủ
        await demoTest();
    } else if (args[0] === '--forward') {
        await runForward();
    } else if (args[0] === '--stop') {
        await runStop();
    } else if (args[0] === '--info') {
        await showInfo();
    } else {
        log('✗ Tham số không hợp lệ', 'red');
        log('Sử dụng:', 'yellow');
        log('  node test-wheels-true-continuous.js              # Demo đầy đủ', 'cyan');
        log('  node test-wheels-true-continuous.js --forward    # Chạy tiến mãi', 'cyan');
        log('  node test-wheels-true-continuous.js --stop       # Dừng lại', 'cyan');
        log('  node test-wheels-true-continuous.js --info       # Hiển thị info', 'cyan');
        process.exit(1);
    }
}

main().catch(error => {
    log(`\n✗ Fatal error: ${error.message}`, 'red');
    console.error(error);
    process.exit(1);
});

