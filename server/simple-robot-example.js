#!/usr/bin/env node

/**
 * Simple Robot Control Example
 * 
 * This is a simplified example showing basic robot control
 * Run with: node simple-robot-example.js
 */

const axios = require('axios');

const SERVER_HOST = '10.0.5.97';
const SERVER_PORT = 8080;
const BASE_URL = `http://${SERVER_HOST}:${SERVER_PORT}`;

// Simple function to send HTTP request using axios
async function sendRequest(method, path, data = null) {
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

// Send tasks to robot
async function sendTasks(tasks) {
    try {
        const response = await sendRequest('POST', '/robot/tasks/replace', { tasks });

        if (response.status === 202) {
            console.log('✅ Tasks sent successfully');
            return true;
        } else {
            console.log('❌ Failed to send tasks:', response.status);
            return false;
        }
    } catch (error) {
        console.log('❌ Error sending tasks:', error.message);
        return false;
    }
}

// Get robot status
async function getStatus() {
    try {
        const response = await sendRequest('GET', '/robot/status');

        if (response.status === 200) {
            return response.data;
        } else {
            console.log('❌ Failed to get status:', response.status);
            return null;
        }
    } catch (error) {
        console.log('❌ Error getting status:', error.message);
        return null;
    }
}

// Sleep function
function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

// Main example
async function main() {
    console.log('🤖 Simple Robot Control Example');
    console.log('===============================');

    // Check if robot is connected
    const status = await getStatus();
    if (!status) {
        console.log('❌ Cannot connect to robot server');
        console.log('Make sure the server is running: npm run dev');
        return;
    }

    console.log('✅ Connected to robot server');
    console.log('Robot connected:', status.connected ? 'Yes' : 'No');

    if (!status.connected) {
        console.log('⚠️  Robot is not connected. Make sure ESP8266 is running and connected to WiFi');
        return;
    }

    // Example 1: Move forward
    console.log('\n📱 Example 1: Moving forward...');
    await sendTasks([{
        taskId: 'example-forward',
        device: 'wheels',
        type: 'drive',
        left: 60,
        right: 60,
        durationMs: 2000
    }]);

    await sleep(2500);

    // Example 2: Turn left
    console.log('📱 Example 2: Turning left...');
    await sendTasks([{
        taskId: 'example-turn-left',
        device: 'wheels',
        type: 'drive',
        left: 20,
        right: 80,
        durationMs: 1500
    }]);

    await sleep(2000);

    // Example 3: Move arm
    console.log('📱 Example 3: Moving arm...');
    await sendTasks([{
        taskId: 'example-arm',
        device: 'arm',
        type: 'moveAngle',
        angle: 120
    }]);

    await sleep(1500);

    // Example 4: Move neck
    console.log('📱 Example 4: Moving neck...');
    await sendTasks([{
        taskId: 'example-neck',
        device: 'neck',
        type: 'moveAngle',
        angle: 45
    }]);

    await sleep(1500);

    // Example 5: Stop everything
    console.log('📱 Example 5: Stopping...');
    await sendTasks([{
        taskId: 'example-stop',
        device: 'wheels',
        type: 'drive',
        left: 0,
        right: 0,
        durationMs: 500
    }]);

    await sleep(1000);

    // Final status
    console.log('\n📊 Final Status:');
    const finalStatus = await getStatus();
    if (finalStatus) {
        console.log('Connected:', finalStatus.connected ? 'Yes' : 'No');
        console.log('Device Status:');
        Object.entries(finalStatus.devices).forEach(([device, info]) => {
            const running = info.runningTaskId ? `Running: ${info.runningTaskId}` : 'Idle';
            console.log(`  ${device}: ${running}, Queue: ${info.queueSize}`);
        });
    }

    console.log('\n✅ Example completed!');
}

// Run the example
main().catch(error => {
    console.log('❌ Error:', error.message);
});
