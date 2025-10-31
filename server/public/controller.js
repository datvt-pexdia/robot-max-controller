/**
 * Simple Cross Control - 4 buttons in cross layout
 * Forward, Backward, Turn Left, Turn Right
 */

class SimpleButtonController {
    constructor() {
        // DOM elements
        this.connectionStatus = document.getElementById('connectionStatus');
        this.statusDot = document.querySelector('.status-dot');
        this.statusText = document.querySelector('.status-text');
        this.logContainer = document.getElementById('logContainer');
        this.emergencyStop = document.getElementById('emergencyStop');

        // Button states
        this.btnForwardPressed = false;
        this.btnBackwardPressed = false;
        this.btnTurnLeftPressed = false;
        this.btnTurnRightPressed = false;

        // Connection
        this.isConnected = false;
        this.apiBaseUrl = window.location.origin;

        this.init();
    }

    init() {
        this.setupButtons();
        this.setupEmergencyStop();
        this.setupPageUnload();
        this.checkConnection();
        this.startConnectionCheck();
        this.log('Hệ thống đã khởi tạo');
    }

    setupButtons() {
        // Forward button
        document.getElementById('btnForward').addEventListener('mousedown', () => this.onButtonDown('forward'));
        document.getElementById('btnForward').addEventListener('mouseup', () => this.onButtonUp('forward'));
        document.getElementById('btnForward').addEventListener('mouseleave', () => this.onButtonUp('forward'));
        document.getElementById('btnForward').addEventListener('touchstart', (e) => { e.preventDefault(); this.onButtonDown('forward'); });
        document.getElementById('btnForward').addEventListener('touchend', (e) => { e.preventDefault(); this.onButtonUp('forward'); });

        // Backward button
        document.getElementById('btnBackward').addEventListener('mousedown', () => this.onButtonDown('backward'));
        document.getElementById('btnBackward').addEventListener('mouseup', () => this.onButtonUp('backward'));
        document.getElementById('btnBackward').addEventListener('mouseleave', () => this.onButtonUp('backward'));
        document.getElementById('btnBackward').addEventListener('touchstart', (e) => { e.preventDefault(); this.onButtonDown('backward'); });
        document.getElementById('btnBackward').addEventListener('touchend', (e) => { e.preventDefault(); this.onButtonUp('backward'); });

        // Turn left button
        document.getElementById('btnTurnLeft').addEventListener('mousedown', () => this.onButtonDown('turnLeft'));
        document.getElementById('btnTurnLeft').addEventListener('mouseup', () => this.onButtonUp('turnLeft'));
        document.getElementById('btnTurnLeft').addEventListener('mouseleave', () => this.onButtonUp('turnLeft'));
        document.getElementById('btnTurnLeft').addEventListener('touchstart', (e) => { e.preventDefault(); this.onButtonDown('turnLeft'); });
        document.getElementById('btnTurnLeft').addEventListener('touchend', (e) => { e.preventDefault(); this.onButtonUp('turnLeft'); });

        // Turn right button
        document.getElementById('btnTurnRight').addEventListener('mousedown', () => this.onButtonDown('turnRight'));
        document.getElementById('btnTurnRight').addEventListener('mouseup', () => this.onButtonUp('turnRight'));
        document.getElementById('btnTurnRight').addEventListener('mouseleave', () => this.onButtonUp('turnRight'));
        document.getElementById('btnTurnRight').addEventListener('touchstart', (e) => { e.preventDefault(); this.onButtonDown('turnRight'); });
        document.getElementById('btnTurnRight').addEventListener('touchend', (e) => { e.preventDefault(); this.onButtonUp('turnRight'); });
    }

    onButtonDown(button) {
        // Reset all buttons
        this.btnForwardPressed = false;
        this.btnBackwardPressed = false;
        this.btnTurnLeftPressed = false;
        this.btnTurnRightPressed = false;
        
        // Set pressed button
        if (button === 'forward') this.btnForwardPressed = true;
        else if (button === 'backward') this.btnBackwardPressed = true;
        else if (button === 'turnLeft') this.btnTurnLeftPressed = true;
        else if (button === 'turnRight') this.btnTurnRightPressed = true;
        
        this.updateButtonVisuals();
        
        // Gửi lệnh ngay khi bấm
        const { left, right } = this.getCurrentValues();
        this.sendCommand(left, right);
        
        this.log(`${this.getButtonName(button)} - Bấm`, 'info');
    }

    onButtonUp(button) {
        // Reset all buttons
        this.btnForwardPressed = false;
        this.btnBackwardPressed = false;
        this.btnTurnLeftPressed = false;
        this.btnTurnRightPressed = false;
        
        this.updateButtonVisuals();
        
        // Gửi lệnh stop
        this.sendCommand(0, 0);
        
        this.log('Nhả nút - Dừng', 'info');
    }

    getButtonName(button) {
        const names = {
            'forward': 'Tiến',
            'backward': 'Lùi',
            'turnLeft': 'Quay trái',
            'turnRight': 'Quay phải'
        };
        return names[button] || button;
    }

    updateButtonVisuals() {
        document.getElementById('btnForward').classList.toggle('pressed', this.btnForwardPressed);
        document.getElementById('btnBackward').classList.toggle('pressed', this.btnBackwardPressed);
        document.getElementById('btnTurnLeft').classList.toggle('pressed', this.btnTurnLeftPressed);
        document.getElementById('btnTurnRight').classList.toggle('pressed', this.btnTurnRightPressed);
    }

    getCurrentValues() {
        // Forward: left=100, right=100
        if (this.btnForwardPressed) {
            return { left: 100, right: 100 };
        }
        
        // Backward: left=-100, right=-100
        if (this.btnBackwardPressed) {
            return { left: -100, right: -100 };
        }
        
        // Turn Left: left=-100, right=100 (phải tiến, trái lùi)
        if (this.btnTurnLeftPressed) {
            return { left: -100, right: 100 };
        }
        
        // Turn Right: left=100, right=-100 (trái tiến, phải lùi)
        if (this.btnTurnRightPressed) {
            return { left: 100, right: -100 };
        }
        
        // Stop
        return { left: 0, right: 0 };
    }

    async sendCommand(left, right) {
        if (!this.isConnected) {
            return;
        }

        const taskId = `control_${Date.now()}`;
        const task = {
            taskId: taskId,
            device: 'wheels',
            type: 'drive',
            left: left,
            right: right,
            durationMs: 300
        };

        try {
            const response = await fetch(`${this.apiBaseUrl}/robot/tasks/enqueue`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ tasks: [task] })
            });

            if (response.ok) {
                if (left === 0 && right === 0) {
                    this.log('🛑 Dừng', 'warning');
                } else {
                    let actionName = '';
                    if (left === 100 && right === 100) actionName = 'Tiến';
                    else if (left === -100 && right === -100) actionName = 'Lùi';
                    else if (left === -100 && right === 100) actionName = 'Quay trái';
                    else if (left === 100 && right === -100) actionName = 'Quay phải';
                    else actionName = `L=${left} R=${right}`;
                    
                    this.log(`${actionName}`, 'success');
                }
            } else {
                this.log(`Lỗi: ${response.status}`, 'error');
            }
        } catch (error) {
            this.log(`Lỗi: ${error.message}`, 'error');
        }
    }

    async checkConnection() {
        try {
            const response = await fetch(`${this.apiBaseUrl}/robot/status`);
            if (response.ok) {
                const status = await response.json();
                this.isConnected = status.connected;
                
                if (this.isConnected) {
                    this.statusDot.classList.add('connected');
                    this.statusText.textContent = 'Đã kết nối';
                    this.log('Kết nối thành công', 'success');
                } else {
                    this.statusDot.classList.remove('connected');
                    this.statusText.textContent = 'Robot offline';
                    this.log('Robot offline', 'warning');
                }
            } else {
                this.isConnected = false;
                this.statusDot.classList.remove('connected');
                this.statusText.textContent = 'Lỗi kết nối';
                this.log('Lỗi kết nối', 'error');
            }
        } catch (error) {
            this.isConnected = false;
            this.statusDot.classList.remove('connected');
            this.statusText.textContent = 'Lỗi kết nối';
            this.log(`Lỗi: ${error.message}`, 'error');
        }
    }

    startConnectionCheck() {
        setInterval(() => {
            this.checkConnection();
        }, 5000);
    }

    setupEmergencyStop() {
        this.emergencyStop.addEventListener('click', async () => {
            this.btnForwardPressed = false;
            this.btnBackwardPressed = false;
            this.btnTurnLeftPressed = false;
            this.btnTurnRightPressed = false;
            this.updateButtonVisuals();
            await this.sendCommand(0, 0);
            this.log('🛑 DỪNG KHẨN CẤP', 'warning');
        });
    }

    setupPageUnload() {
        window.addEventListener('beforeunload', () => {
            this.sendCommand(0, 0);
        });

        window.addEventListener('pagehide', () => {
            this.sendCommand(0, 0);
        });

        window.addEventListener('blur', () => {
            this.sendCommand(0, 0);
        });
    }

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
    window.controller = new SimpleButtonController();
});
