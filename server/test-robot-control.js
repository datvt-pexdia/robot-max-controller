#!/usr/bin/env node

/**
 * Robot Control Test Script
 * 
 * This script provides various test scenarios to control the robot
 * Usage: node test-robot-control.js [scenario]
 * 
 * Available scenarios:
 * - basic: Basic forward/backward movement
 * - turn: Left/right turning movements  
 * - arm: Arm movement tests
 * - neck: Neck movement tests
 * - dance: Complex coordinated movement sequence
 * - emergency: Emergency stop test
 * - status: Check robot status
 * - interactive: Interactive mode for manual control
 */

const axios = require('axios');
const readline = require('readline');

// Configuration
const SERVER_HOST = '10.0.5.97';
const SERVER_PORT = 8080;
const BASE_URL = `http://${SERVER_HOST}:${SERVER_PORT}`;

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
    console.log(`${colors[color]}${message}${colors.reset}`);
}

async function makeRequest(method, path, data = null) {
    try {
        const config = {
            method: method.toLowerCase(),
            url: `${BASE_URL}${path}`,
            headers: {
                'Content-Type': 'application/json'
            }
        };

        if (data) {
            config.data = data;
        }

        const response = await axios(config);
        return {
            status: response.status,
            data: response.data
        };
    } catch (error) {
        if (error.response) {
            return {
                status: error.response.status,
                data: error.response.data
            };
        }
        throw error;
    }
}

async function sendTasks(tasks, mode = 'replace') {
    try {
        const endpoint = mode === 'replace' ? '/robot/tasks/replace' : '/robot/tasks/enqueue';
        const response = await makeRequest('POST', endpoint, { tasks });

        if (response.status === 202) {
            log(`✓ Tasks sent successfully (${mode} mode)`, 'green');
            return true;
        } else {
            log(`✗ Failed to send tasks: ${response.status}`, 'red');
            return false;
        }
    } catch (error) {
        log(`✗ Error sending tasks: ${error.message}`, 'red');
        return false;
    }
}

async function cancelDevice(device) {
    try {
        const response = await makeRequest('POST', '/robot/tasks/cancel', { device });

        if (response.status === 202) {
            log(`✓ Cancelled ${device} device`, 'green');
            return true;
        } else {
            log(`✗ Failed to cancel ${device}: ${response.status}`, 'red');
            return false;
        }
    } catch (error) {
        log(`✗ Error cancelling ${device}: ${error.message}`, 'red');
        return false;
    }
}

async function getStatus() {
    try {
        const response = await makeRequest('GET', '/robot/status');

        if (response.status === 200) {
            return response.data;
        } else {
            log(`✗ Failed to get status: ${response.status}`, 'red');
            return null;
        }
    } catch (error) {
        log(`✗ Error getting status: ${error.message}`, 'red');
        return null;
    }
}

function displayStatus(status) {
    if (!status) return;

    log('\n=== Robot Status ===', 'cyan');
    log(`Connected: ${status.connected ? '✓' : '✗'}`, status.connected ? 'green' : 'red');
    log(`Last Hello: ${status.lastHello || 'Never'}`);

    log('\nDevice Status:', 'yellow');
    Object.entries(status.devices).forEach(([device, info]) => {
        const running = info.runningTaskId ? `Task: ${info.runningTaskId}` : 'Idle';
        const queue = info.queueSize > 0 ? `Queue: ${info.queueSize}` : 'No queue';
        log(`  ${device.toUpperCase()}: ${running}, ${queue}`);
    });

    log('\nQueue Sizes:', 'yellow');
    Object.entries(status.queueSizes).forEach(([device, size]) => {
        log(`  ${device}: ${size}`);
    });
}

// Test Scenarios
async function testBasicMovement() {
    log('\n=== Basic Movement Test ===', 'magenta');

    // Forward movement
    log('Moving forward...', 'blue');
    await sendTasks([{
        taskId: 'forward-1',
        device: 'wheels',
        type: 'drive',
        left: 60,
        right: 60,
        durationMs: 2000
    }]);

    await sleep(2500);

    // Backward movement
    log('Moving backward...', 'blue');
    await sendTasks([{
        taskId: 'backward-1',
        device: 'wheels',
        type: 'drive',
        left: 40,
        right: 40,
        durationMs: 2000
    }]);

    await sleep(2500);

    // Stop
    log('Stopping...', 'blue');
    await sendTasks([{
        taskId: 'stop-1',
        device: 'wheels',
        type: 'drive',
        left: 0,
        right: 0,
        durationMs: 500
    }]);
}

async function testTurning() {
    log('\n=== Turning Test ===', 'magenta');

    // Turn left
    log('Turning left...', 'blue');
    await sendTasks([{
        taskId: 'turn-left-1',
        device: 'wheels',
        type: 'drive',
        left: 20,
        right: 80,
        durationMs: 1500
    }]);

    await sleep(2000);

    // Turn right
    log('Turning right...', 'blue');
    await sendTasks([{
        taskId: 'turn-right-1',
        device: 'wheels',
        type: 'drive',
        left: 80,
        right: 20,
        durationMs: 1500
    }]);

    await sleep(2000);

    // Stop
    await sendTasks([{
        taskId: 'stop-turn',
        device: 'wheels',
        type: 'drive',
        left: 0,
        right: 0,
        durationMs: 500
    }]);
}

async function testArmMovement() {
    log('\n=== Arm Movement Test ===', 'magenta');

    // Move arm to different positions
    const positions = [0, 45, 90, 135, 180];

    for (let i = 0; i < positions.length; i++) {
        const angle = positions[i];
        log(`Moving arm to ${angle}°...`, 'blue');

        await sendTasks([{
            taskId: `arm-${i}`,
            device: 'arm',
            type: 'moveAngle',
            angle: angle
        }]);

        await sleep(1000);
    }

    // Return to center
    log('Returning arm to center (90°)...', 'blue');
    await sendTasks([{
        taskId: 'arm-center',
        device: 'arm',
        type: 'moveAngle',
        angle: 90
    }]);
}

async function testNeckMovement() {
    log('\n=== Neck Movement Test ===', 'magenta');

    // Move neck to different positions
    const positions = [0, 30, 60, 90, 120, 150, 180];

    for (let i = 0; i < positions.length; i++) {
        const angle = positions[i];
        log(`Moving neck to ${angle}°...`, 'blue');

        await sendTasks([{
            taskId: `neck-${i}`,
            device: 'neck',
            type: 'moveAngle',
            angle: angle
        }]);

        await sleep(800);
    }

    // Return to center
    log('Returning neck to center (90°)...', 'blue');
    await sendTasks([{
        taskId: 'neck-center',
        device: 'neck',
        type: 'moveAngle',
        angle: 90
    }]);
}

async function testDanceSequence() {
    log('\n=== Dance Sequence Test ===', 'magenta');

    // Complex coordinated movement
    const danceSteps = [
        // Step 1: Look around
        { taskId: 'dance-1', device: 'neck', type: 'moveAngle', angle: 0 },
        { taskId: 'dance-2', device: 'neck', type: 'moveAngle', angle: 180 },
        { taskId: 'dance-3', device: 'neck', type: 'moveAngle', angle: 90 },

        // Step 2: Arm wave
        { taskId: 'dance-4', device: 'arm', type: 'moveAngle', angle: 0 },
        { taskId: 'dance-5', device: 'arm', type: 'moveAngle', angle: 180 },
        { taskId: 'dance-6', device: 'arm', type: 'moveAngle', angle: 90 },

        // Step 3: Spin
        { taskId: 'dance-7', device: 'wheels', type: 'drive', left: 100, right: 0, durationMs: 2000 },
        { taskId: 'dance-8', device: 'wheels', type: 'drive', left: 0, right: 100, durationMs: 2000 },

        // Step 4: Final pose
        { taskId: 'dance-9', device: 'wheels', type: 'drive', left: 0, right: 0, durationMs: 500 },
        { taskId: 'dance-10', device: 'arm', type: 'moveAngle', angle: 45 },
        { taskId: 'dance-11', device: 'neck', type: 'moveAngle', angle: 135 }
    ];

    log('Starting dance sequence...', 'blue');

    // Send all tasks at once (they will be queued)
    await sendTasks(danceSteps, 'replace');

    // Wait for completion
    await sleep(8000);
}

async function testEmergencyStop() {
    log('\n=== Emergency Stop Test ===', 'magenta');

    // Start some movement
    log('Starting movement...', 'blue');
    await sendTasks([
        { taskId: 'emergency-1', device: 'wheels', type: 'drive', left: 80, right: 80, durationMs: 5000 },
        { taskId: 'emergency-2', device: 'arm', type: 'moveAngle', angle: 180 }
    ]);

    await sleep(1000);

    // Emergency stop all devices
    log('EMERGENCY STOP!', 'red');
    await cancelDevice('wheels');
    await cancelDevice('arm');
    await cancelDevice('neck');

    await sleep(1000);
}

async function testStatusMonitoring() {
    log('\n=== Status Monitoring Test ===', 'magenta');

    // Start a task
    await sendTasks([{
        taskId: 'status-test',
        device: 'wheels',
        type: 'drive',
        left: 50,
        right: 50,
        durationMs: 3000
    }]);

    // Monitor status every second
    for (let i = 0; i < 5; i++) {
        await sleep(1000);
        const status = await getStatus();
        displayStatus(status);
    }
}

async function interactiveMode() {
    log('\n=== Interactive Mode ===', 'magenta');
    log('Type commands to control the robot:', 'yellow');
    log('Commands: forward, backward, left, right, stop, arm <angle>, neck <angle>, status, quit', 'yellow');

    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });

    const askCommand = () => {
        rl.question('\n> ', async (input) => {
            const parts = input.trim().split(' ');
            const command = parts[0].toLowerCase();

            switch (command) {
                case 'forward':
                    await sendTasks([{
                        taskId: `interactive-${Date.now()}`,
                        device: 'wheels',
                        type: 'drive',
                        left: 60,
                        right: 60,
                        durationMs: 2000
                    }]);
                    break;

                case 'backward':
                    await sendTasks([{
                        taskId: `interactive-${Date.now()}`,
                        device: 'wheels',
                        type: 'drive',
                        left: 40,
                        right: 40,
                        durationMs: 2000
                    }]);
                    break;

                case 'left':
                    await sendTasks([{
                        taskId: `interactive-${Date.now()}`,
                        device: 'wheels',
                        type: 'drive',
                        left: 20,
                        right: 80,
                        durationMs: 1500
                    }]);
                    break;

                case 'right':
                    await sendTasks([{
                        taskId: `interactive-${Date.now()}`,
                        device: 'wheels',
                        type: 'drive',
                        left: 80,
                        right: 20,
                        durationMs: 1500
                    }]);
                    break;

                case 'stop':
                    await sendTasks([{
                        taskId: `interactive-${Date.now()}`,
                        device: 'wheels',
                        type: 'drive',
                        left: 0,
                        right: 0,
                        durationMs: 500
                    }]);
                    break;

                case 'arm':
                    const armAngle = parseInt(parts[1]) || 90;
                    if (armAngle >= 0 && armAngle <= 180) {
                        await sendTasks([{
                            taskId: `interactive-${Date.now()}`,
                            device: 'arm',
                            type: 'moveAngle',
                            angle: armAngle
                        }]);
                    } else {
                        log('Arm angle must be between 0 and 180', 'red');
                    }
                    break;

                case 'neck':
                    const neckAngle = parseInt(parts[1]) || 90;
                    if (neckAngle >= 0 && neckAngle <= 180) {
                        await sendTasks([{
                            taskId: `interactive-${Date.now()}`,
                            device: 'neck',
                            type: 'moveAngle',
                            angle: neckAngle
                        }]);
                    } else {
                        log('Neck angle must be between 0 and 180', 'red');
                    }
                    break;

                case 'status':
                    const status = await getStatus();
                    displayStatus(status);
                    break;

                case 'quit':
                case 'exit':
                    log('Goodbye!', 'green');
                    rl.close();
                    return;

                default:
                    log('Unknown command. Type "quit" to exit.', 'red');
            }

            askCommand();
        });
    };

    askCommand();
}

// Utility function
function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

// Main function
async function main() {
    const scenario = process.argv[2] || 'help';

    log('🤖 Robot Control Test Script', 'bright');
    log('============================', 'bright');

    // Check server connection first
    const status = await getStatus();
    if (!status) {
        log('❌ Cannot connect to robot server. Make sure the server is running on port 8080.', 'red');
        process.exit(1);
    }

    log(`✅ Connected to robot server`, 'green');
    displayStatus(status);

    switch (scenario) {
        case 'basic':
            await testBasicMovement();
            break;
        case 'turn':
            await testTurning();
            break;
        case 'arm':
            await testArmMovement();
            break;
        case 'neck':
            await testNeckMovement();
            break;
        case 'dance':
            await testDanceSequence();
            break;
        case 'emergency':
            await testEmergencyStop();
            break;
        case 'status':
            await testStatusMonitoring();
            break;
        case 'interactive':
            await interactiveMode();
            break;
        case 'help':
        default:
            log('\nAvailable test scenarios:', 'yellow');
            log('  basic      - Basic forward/backward movement', 'cyan');
            log('  turn       - Left/right turning movements', 'cyan');
            log('  arm        - Arm movement tests', 'cyan');
            log('  neck       - Neck movement tests', 'cyan');
            log('  dance      - Complex coordinated movement sequence', 'cyan');
            log('  emergency  - Emergency stop test', 'cyan');
            log('  status     - Status monitoring test', 'cyan');
            log('  interactive- Interactive mode for manual control', 'cyan');
            log('\nUsage: node test-robot-control.js [scenario]', 'yellow');
            break;
    }

    // Final status check
    if (scenario !== 'interactive') {
        log('\n=== Final Status ===', 'cyan');
        const finalStatus = await getStatus();
        displayStatus(finalStatus);
    }
}

// Handle errors
process.on('unhandledRejection', (error) => {
    log(`❌ Unhandled error: ${error.message}`, 'red');
    process.exit(1);
});

// Run the script
if (require.main === module) {
    main().catch(error => {
        log(`❌ Script error: ${error.message}`, 'red');
        process.exit(1);
    });
}

module.exports = {
    sendTasks,
    cancelDevice,
    getStatus,
    displayStatus
};
