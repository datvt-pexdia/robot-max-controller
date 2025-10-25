/**
 * Tank Drive Controller - Dual Joystick Control
 * Implements the logic spec for UI → Server → Device flow
 */

class TankDriveController {
    constructor() {
        // DOM elements
        this.leftJoystick = document.getElementById('leftJoystick');
        this.rightJoystick = document.getElementById('rightJoystick');
        this.leftKnob = document.getElementById('leftKnob');
        this.rightKnob = document.getElementById('rightKnob');
        this.leftValue = document.getElementById('leftValue');
        this.rightValue = document.getElementById('rightValue');
        this.leftSpeed = document.getElementById('leftSpeed');
        this.rightSpeed = document.getElementById('rightSpeed');
        this.connectionStatus = document.getElementById('connectionStatus');
        this.statusDot = document.querySelector('.status-dot');
        this.statusText = document.querySelector('.status-text');
        this.logContainer = document.getElementById('logContainer');
        this.emergencyStop = document.getElementById('emergencyStop');

        // Normalization parameters (configurable)
        this.DZ = 0.06;          // Dead-zone threshold
        this.SR_UP = 3.0;        // Slew rate up (per second)
        this.SR_DOWN = 6.0;      // Slew rate down (per second)
        this.QUANTIZE_LEVELS = 12; // Discrete levels

        // State
        this.rawLeft = 0.0;
        this.rawRight = 0.0;
        this.normLeft = 0.0;
        this.normRight = 0.0;
        this.prevLeft = 0.0;
        this.prevRight = 0.0;
        this.seq = 0;
        this.sessionStart = performance.now();
        this.lastSendTime = 0;
        this.lastKeepaliveTime = 0;

        // Connection
        this.ws = null;
        this.connected = false;
        this.reconnectDelay = 500;
        this.maxReconnectDelay = 8000;

        // Input state
        this.isDraggingLeft = false;
        this.isDraggingRight = false;

        // Timers
        this.tickTimer = null;
        this.keepaliveTimer = null;

        this.init();
    }

    init() {
        this.setupJoysticks();
        this.setupEmergencyStop();
        this.setupPageUnload();
        this.connectWebSocket();
        this.startTickLoop();
        this.log('Hệ thống đã khởi tạo');
    }

    // ========================================
    // NORMALIZATION PIPELINE
    // ========================================

    normalize(vRaw, prevNorm, dt) {
        // 1. Clamp to [-1, 1]
        let v1 = Math.max(-1.0, Math.min(1.0, vRaw));

        // 2. Dead-zone
        let v2;
        if (Math.abs(v1) <= this.DZ) {
            v2 = 0.0;
        } else {
            const sign = v1 > 0 ? 1 : -1;
            v2 = sign * ((Math.abs(v1) - this.DZ) / (1.0 - this.DZ));
        }

        // 3. Slew-rate limiting
        const delta = v2 - prevNorm;
        const isIncreasing = Math.abs(v2) > Math.abs(prevNorm);
        const maxChange = (isIncreasing ? this.SR_UP : this.SR_DOWN) * dt;
        
        let v3;
        if (Math.abs(delta) <= maxChange) {
            v3 = v2;
        } else {
            v3 = prevNorm + Math.sign(delta) * maxChange;
        }

        // 4. Quantization (optional)
        if (this.QUANTIZE_LEVELS > 1) {
            const level = Math.round(v3 * this.QUANTIZE_LEVELS);
            v3 = level / this.QUANTIZE_LEVELS;
        }

        return v3;
    }

    // ========================================
    // TICK LOOP (40 Hz = 25ms)
    // ========================================

    startTickLoop() {
        const TICK_INTERVAL = 25; // 25ms = 40 Hz
        const KEEPALIVE_INTERVAL = 150; // 150ms keepalive

        this.tickTimer = setInterval(() => {
            const now = performance.now();
            const dt = 0.025; // 25ms in seconds

            // Normalize inputs
            this.normLeft = this.normalize(this.rawLeft, this.prevLeft, dt);
            this.normRight = this.normalize(this.rawRight, this.prevRight, dt);

            // Check if changed significantly or keepalive due
            const changed = Math.abs(this.normLeft - this.prevLeft) > 0.01 ||
                          Math.abs(this.normRight - this.prevRight) > 0.01;
            const keepaliveDue = (now - this.lastKeepaliveTime) >= KEEPALIVE_INTERVAL;

            if (changed || keepaliveDue || (now - this.lastSendTime) >= TICK_INTERVAL) {
                this.sendDriveIntent();
                this.prevLeft = this.normLeft;
                this.prevRight = this.normRight;
            }

            // Update UI
            this.updateDisplay();
        }, TICK_INTERVAL);
    }

    // ========================================
    // WEBSOCKET COMMUNICATION
    // ========================================

    connectWebSocket() {
        const wsUrl = `ws://${window.location.host}/robot`;
        this.log(`Connecting to ${wsUrl}...`);

        try {
            this.ws = new WebSocket(wsUrl);
            
            this.ws.onopen = () => {
                this.connected = true;
                this.reconnectDelay = 500;
                this.statusDot.classList.add('connected');
                this.statusText.textContent = 'Đã kết nối';
                this.log('WebSocket connected', 'success');

                // Send hello
                this.ws.send(JSON.stringify({
                    type: 'hello',
                    role: 'ui',
                    version: '1.0'
                }));
            };

            this.ws.onclose = () => {
                this.connected = false;
                this.statusDot.classList.remove('connected');
                this.statusText.textContent = 'Đang kết nối lại...';
                this.log('WebSocket disconnected', 'warning');
                this.scheduleReconnect();
            };

            this.ws.onerror = (err) => {
                this.log(`WebSocket error: ${err}`, 'error');
            };

            this.ws.onmessage = (event) => {
                try {
                    const msg = JSON.parse(event.data);
                    this.handleServerMessage(msg);
                } catch (e) {
                    this.log(`Invalid message: ${e}`, 'error');
                }
            };
        } catch (e) {
            this.log(`Failed to connect: ${e}`, 'error');
            this.scheduleReconnect();
        }
    }

    scheduleReconnect() {
        setTimeout(() => {
            this.connectWebSocket();
        }, this.reconnectDelay);
        
        // Exponential backoff
        this.reconnectDelay = Math.min(this.reconnectDelay * 2, this.maxReconnectDelay);
    }

    handleServerMessage(msg) {
        switch (msg.type) {
            case 'controller_taken':
                this.log('Another controller took over', 'warning');
                break;
            case 'pong':
                // RTT telemetry
                break;
            default:
                this.log(`Unknown message type: ${msg.type}`);
        }
    }

    sendDriveIntent() {
        if (!this.connected || !this.ws) {
            return;
        }

        const now = performance.now();
        const intent = {
            type: 'drive',
            left: this.normLeft,
            right: this.normRight,
            seq: ++this.seq,
            ts: Math.floor(now - this.sessionStart)
        };

        try {
            this.ws.send(JSON.stringify(intent));
            this.lastSendTime = now;
            this.lastKeepaliveTime = now;
            this.log(`→ L=${intent.left.toFixed(2)} R=${intent.right.toFixed(2)} seq=${intent.seq}`, 'success');
        } catch (e) {
            this.log(`Send failed: ${e}`, 'error');
        }
    }

    sendStop() {
        this.normLeft = 0.0;
        this.normRight = 0.0;
        this.prevLeft = 0.0;
        this.prevRight = 0.0;
        this.sendDriveIntent();
        this.log('🛑 STOP sent', 'warning');
    }

    // ========================================
    // JOYSTICK INPUT
    // ========================================

    setupJoysticks() {
        this.setupJoystick(this.leftJoystick, this.leftKnob, 'left');
        this.setupJoystick(this.rightJoystick, this.rightKnob, 'right');
    }

    setupJoystick(joystick, knob, side) {
        let startY = 0;
        let startValue = 0;

        const handleStart = (y) => {
            if (side === 'left') {
                this.isDraggingLeft = true;
                startValue = this.rawLeft;
            } else {
                this.isDraggingRight = true;
                startValue = this.rawRight;
            }
            startY = y;
            knob.classList.add('active');
        };

        const handleMove = (y) => {
            const rect = joystick.getBoundingClientRect();
            const deltaY = startY - y;
            const maxDelta = rect.height / 2;
            
            let newValue = startValue + (deltaY / maxDelta);
            newValue = Math.max(-1.0, Math.min(1.0, newValue));
            
            if (side === 'left') {
                this.rawLeft = newValue;
            } else {
                this.rawRight = newValue;
            }

            // Update visual position immediately
            const position = (newValue + 1.0) / 2.0; // Convert -1..1 to 0..1
            knob.style.top = `${(1.0 - position) * 100}%`;
        };

        const handleEnd = () => {
            if (side === 'left') {
                this.isDraggingLeft = false;
                this.rawLeft = 0.0;
            } else {
                this.isDraggingRight = false;
                this.rawRight = 0.0;
            }
            knob.classList.remove('active');
            knob.style.top = '50%';
        };

        // Mouse events
        joystick.addEventListener('mousedown', (e) => {
            handleStart(e.clientY);
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => {
            if ((side === 'left' && this.isDraggingLeft) || 
                (side === 'right' && this.isDraggingRight)) {
                handleMove(e.clientY);
            }
        });

        document.addEventListener('mouseup', () => {
            if ((side === 'left' && this.isDraggingLeft) || 
                (side === 'right' && this.isDraggingRight)) {
                handleEnd();
            }
        });

        // Touch events
        joystick.addEventListener('touchstart', (e) => {
            handleStart(e.touches[0].clientY);
            e.preventDefault();
        });

        joystick.addEventListener('touchmove', (e) => {
            if ((side === 'left' && this.isDraggingLeft) || 
                (side === 'right' && this.isDraggingRight)) {
                handleMove(e.touches[0].clientY);
                e.preventDefault();
            }
        });

        joystick.addEventListener('touchend', () => {
            if ((side === 'left' && this.isDraggingLeft) || 
                (side === 'right' && this.isDraggingRight)) {
                handleEnd();
            }
        });
    }

    // ========================================
    // UI UPDATES
    // ========================================

    updateDisplay() {
        // Convert normalized values to percentages for display
        const leftPct = Math.round(this.normLeft * 100);
        const rightPct = Math.round(this.normRight * 100);

        this.leftValue.textContent = `${leftPct}%`;
        this.rightValue.textContent = `${rightPct}%`;
        this.leftSpeed.textContent = `${leftPct}%`;
        this.rightSpeed.textContent = `${rightPct}%`;
    }

    // ========================================
    // STOP & SAFETY
    // ========================================

    setupEmergencyStop() {
        this.emergencyStop.addEventListener('click', () => {
            this.sendStop();
        });
    }

    setupPageUnload() {
        // Flush STOP on page unload/blur
        window.addEventListener('beforeunload', () => {
            this.sendStop();
        });

        window.addEventListener('pagehide', () => {
            this.sendStop();
        });

        window.addEventListener('blur', () => {
            this.sendStop();
        });
    }

    // ========================================
    // LOGGING
    // ========================================

    log(message, type = 'info') {
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = document.createElement('div');
        logEntry.className = `log-entry ${type}`;
        logEntry.textContent = `[${timestamp}] ${message}`;
        
        this.logContainer.appendChild(logEntry);
        this.logContainer.scrollTop = this.logContainer.scrollHeight;
        
        // Keep only last 100 log entries
        while (this.logContainer.children.length > 100) {
            this.logContainer.removeChild(this.logContainer.firstChild);
        }
    }
}

// Initialize the controller when the page loads
document.addEventListener('DOMContentLoaded', () => {
    window.controller = new TankDriveController();
});

