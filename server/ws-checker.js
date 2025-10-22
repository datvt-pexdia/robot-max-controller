const WebSocket = require('ws');

// WebSocket checker for robot controller
const WS_URL = 'ws://10.0.5.97:8080/robot';
const CHECK_INTERVAL = 5000; // Check every 5 seconds
const TIMEOUT = 10000; // 10 second timeout for connection

class WebSocketChecker {
    constructor() {
        this.isConnected = false;
        this.lastCheck = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
    }

    async checkConnection() {
        console.log(`[${new Date().toISOString()}] Checking WebSocket connection to ${WS_URL}`);

        return new Promise((resolve) => {
            const ws = new WebSocket(WS_URL, ['arduino']); // Use arduino subprotocol like the server expects

            const timeout = setTimeout(() => {
                console.log('❌ Connection timeout');
                ws.terminate();
                resolve(false);
            }, TIMEOUT);

            ws.on('open', () => {
                console.log('✅ WebSocket connected successfully');
                clearTimeout(timeout);

                // Send a hello message to test communication
                const helloMessage = {
                    kind: 'hello',
                    espId: 'checker-' + Date.now(),
                    fw: '1.0.0'
                };

                ws.send(JSON.stringify(helloMessage));
                console.log('📤 Sent hello message');

                this.isConnected = true;
                this.reconnectAttempts = 0;
                this.lastCheck = new Date();

                // Close connection after successful test
                setTimeout(() => {
                    ws.close();
                    resolve(true);
                }, 1000);
            });

            ws.on('message', (data) => {
                try {
                    const message = JSON.parse(data.toString());
                    console.log('📥 Received message:', JSON.stringify(message, null, 2));
                } catch (err) {
                    console.log('📥 Received raw message:', data.toString());
                }
            });

            ws.on('error', (error) => {
                console.log('❌ WebSocket error:', error.message);
                clearTimeout(timeout);
                this.isConnected = false;
                resolve(false);
            });

            ws.on('close', (code, reason) => {
                console.log(`🔌 WebSocket closed - Code: ${code}, Reason: ${reason}`);
                clearTimeout(timeout);
                if (!this.isConnected) {
                    resolve(false);
                }
            });
        });
    }

    async startContinuousCheck() {
        console.log('🚀 Starting WebSocket connection checker');
        console.log(`Target: ${WS_URL}`);
        console.log(`Check interval: ${CHECK_INTERVAL}ms`);
        console.log('Press Ctrl+C to stop\n');

        while (true) {
            try {
                const success = await this.checkConnection();

                if (!success) {
                    this.reconnectAttempts++;
                    console.log(`⚠️  Failed connection attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts}`);

                    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
                        console.log('🛑 Max reconnection attempts reached. Stopping checker.');
                        break;
                    }
                }

                // Wait before next check
                await new Promise(resolve => setTimeout(resolve, CHECK_INTERVAL));

            } catch (error) {
                console.error('💥 Unexpected error:', error);
                await new Promise(resolve => setTimeout(resolve, CHECK_INTERVAL));
            }
        }
    }

    async singleCheck() {
        console.log('🔍 Performing single WebSocket check');
        const success = await this.checkConnection();

        if (success) {
            console.log('✅ WebSocket connection is working');
            process.exit(0);
        } else {
            console.log('❌ WebSocket connection failed');
            process.exit(1);
        }
    }
}

// Command line interface
const args = process.argv.slice(2);
const checker = new WebSocketChecker();

if (args.includes('--continuous') || args.includes('-c')) {
    checker.startContinuousCheck();
} else {
    checker.singleCheck();
}

// Handle graceful shutdown
process.on('SIGINT', () => {
    console.log('\n👋 Shutting down WebSocket checker...');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\n👋 Shutting down WebSocket checker...');
    process.exit(0);
});
