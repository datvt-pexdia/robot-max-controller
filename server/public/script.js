class RobotController {
    constructor() {
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

        this.leftValuePercent = 0;
        this.rightValuePercent = 0;
        this.isLeftActive = false;
        this.isRightActive = false;
        this.isConnected = false;
        this.sendTimeout = null;
        this.continuousSendInterval = null;
        this.lastCommandSent = null;

        this.init();
    }

    init() {
        this.setupJoysticks();
        this.setupEmergencyStop();
        this.checkConnection();
        this.startConnectionCheck();
        this.log('Hệ thống đã khởi tạo');
    }

    setupJoysticks() {
        // Left joystick
        this.setupJoystick(this.leftJoystick, this.leftKnob, 'left');
        
        // Right joystick
        this.setupJoystick(this.rightJoystick, this.rightKnob, 'right');
    }

    setupJoystick(joystick, knob, side) {
        let isDragging = false;
        let startY = 0;
        let startPercent = 0;

        // Mouse events
        joystick.addEventListener('mousedown', (e) => {
            isDragging = true;
            startY = e.clientY;
            startPercent = side === 'left' ? this.leftValuePercent : this.rightValuePercent;
            knob.classList.add('active');
            document.addEventListener('mousemove', handleMove);
            document.addEventListener('mouseup', handleEnd);
            e.preventDefault();
        });

        // Touch events
        joystick.addEventListener('touchstart', (e) => {
            isDragging = true;
            startY = e.touches[0].clientY;
            startPercent = side === 'left' ? this.leftValuePercent : this.rightValuePercent;
            knob.classList.add('active');
            e.preventDefault();
        });

        joystick.addEventListener('touchmove', (e) => {
            if (!isDragging) return;
            handleMove(e.touches[0]);
            e.preventDefault();
        });

        joystick.addEventListener('touchend', (e) => {
            if (isDragging) {
                handleEnd();
            }
            e.preventDefault();
        });

        const handleMove = (e) => {
            if (!isDragging) return;
            
            const rect = joystick.getBoundingClientRect();
            const centerY = rect.top + rect.height / 2;
            const deltaY = startY - e.clientY;
            const maxDelta = rect.height / 2;
            
            let newPercent = startPercent + (deltaY / maxDelta) * 100;
            newPercent = Math.max(-100, Math.min(100, newPercent));
            
            this.updateJoystick(side, newPercent);
        };

        const handleEnd = () => {
            isDragging = false;
            knob.classList.remove('active');
            this.stopJoystick(side);
            document.removeEventListener('mousemove', handleMove);
            document.removeEventListener('mouseup', handleEnd);
        };
    }

    updateJoystick(side, percent) {
        const knob = side === 'left' ? this.leftKnob : this.rightKnob;
        const valueDisplay = side === 'left' ? this.leftValue : this.rightValue;
        const speedDisplay = side === 'left' ? this.leftSpeed : this.rightSpeed;
        
        // Update visual position
        const position = (percent + 100) / 2; // Convert -100..100 to 0..100
        knob.style.top = `${100 - position}%`;
        
        // Update values
        if (side === 'left') {
            this.leftValuePercent = percent;
            this.isLeftActive = true;
        } else {
            this.rightValuePercent = percent;
            this.isRightActive = true;
        }
        
        valueDisplay.textContent = `${Math.round(percent)}%`;
        speedDisplay.textContent = `${Math.round(percent)}%`;
        
        // Send command to robot
        this.sendWheelCommand();
    }

    stopJoystick(side) {
        const knob = side === 'left' ? this.leftKnob : this.rightKnob;
        const valueDisplay = side === 'left' ? this.leftValue : this.rightValue;
        const speedDisplay = side === 'left' ? this.leftSpeed : this.rightSpeed;
        
        // Reset to center
        knob.style.top = '50%';
        
        if (side === 'left') {
            this.leftValuePercent = 0;
            this.isLeftActive = false;
        } else {
            this.rightValuePercent = 0;
            this.isRightActive = false;
        }
        
        valueDisplay.textContent = '0%';
        speedDisplay.textContent = '0%';
        
        // Dừng gửi lệnh liên tục ngay lập tức
        this.stopContinuousSending();
        
        // Reset command tracking để đảm bảo gửi lệnh dừng
        this.lastCommandSent = null;
        
        // Send stop command ngay lập tức
        this.sendWheelCommand();
    }

    async sendWheelCommand() {
        if (!this.isConnected) {
            this.log('Không thể gửi lệnh - chưa kết nối với server', 'error');
            return;
        }

        // Clear previous timeout
        if (this.sendTimeout) {
            clearTimeout(this.sendTimeout);
        }

        // Debounce sending commands
        this.sendTimeout = setTimeout(async () => {
            await this.executeWheelCommand();
        }, 50); // 50ms debounce
    }

    async executeWheelCommand() {
        try {
            const leftSpeed = this.isLeftActive ? this.leftValuePercent : 0;
            const rightSpeed = this.isRightActive ? this.rightValuePercent : 0;
            
            // Không dedupe khi đang giữ để firmware có thể gia hạn/tryUpdate mượt mà
            // Chỉ dedupe với trường hợp dừng (0,0) thông qua nhánh dưới

            // Nếu cả hai bánh đều 0, gửi lệnh dừng ngay lập tức (cancel + replace 0,0)
            if (leftSpeed === 0 && rightSpeed === 0) {
                // Thử cancel trước để cắt mọi tác vụ còn đệm
                try { await fetch('/robot/tasks/cancel', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ device: 'wheels' }) }); } catch {}
                const stopCommand = {
                    tasks: [{
                        device: 'wheels',
                        type: 'drive',
                        left: 0,
                        right: 0,
                        taskId: `stop-${Date.now()}`
                    }]
                };

                const response = await fetch('/robot/tasks/replace', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(stopCommand)
                });

                if (response.ok) {
                    this.log('🛑 Robot đã dừng', 'success');
                } else {
                    this.log(`Lỗi dừng robot: ${response.status}`, 'error');
                }
                return;
            }

            // Nếu có tốc độ khác 0, gửi lệnh di chuyển bằng enqueue với duration ngắn
            const moveCommand = {
                tasks: [{
                    device: 'wheels',
                    type: 'drive',
                    left: leftSpeed,
                    right: rightSpeed,
                    durationMs: 300, // 300ms để firmware có đủ thời gian gia hạn
                    taskId: `move-${Date.now()}`
                }]
            };

            const response = await fetch('/robot/tasks/replace', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(moveCommand)
            });

            if (response.ok) {
                this.log(`Đã gửi lệnh: Trái=${leftSpeed}%, Phải=${rightSpeed}%`, 'success');
                
                // Nếu có cần điều khiển đang hoạt động, tiếp tục gửi lệnh
                if (this.isLeftActive || this.isRightActive) {
                    this.startContinuousSending();
                } else {
                    this.stopContinuousSending();
                }
            } else {
                this.log(`Lỗi gửi lệnh: ${response.status}`, 'error');
            }
        } catch (error) {
            this.log(`Lỗi kết nối: ${error.message}`, 'error');
        }
    }

    startContinuousSending() {
        // Nếu đã có interval đang chạy thì không tạo mới
        if (this.continuousSendInterval) {
            return;
        }

        // Gửi lệnh liên tục mỗi 100ms khi có cần điều khiển hoạt động (bội số của 10ms tick ESP)
        // Duration 300ms + interval 100ms = overlap tốt, khớp nhịp 10ms trên ESP
        this.continuousSendInterval = setInterval(async () => {
            if (this.isLeftActive || this.isRightActive) {
                await this.executeWheelCommand();
            } else {
                this.stopContinuousSending();
            }
        }, 100);
    }

    stopContinuousSending() {
        if (this.continuousSendInterval) {
            clearInterval(this.continuousSendInterval);
            this.continuousSendInterval = null;
        }
    }

    setupEmergencyStop() {
        this.emergencyStop.addEventListener('click', async () => {
            await this.emergencyStopRobot();
        });
    }

    async emergencyStopRobot() {
        if (!this.isConnected) {
            this.log('Không thể dừng khẩn cấp - chưa kết nối với server', 'error');
            return;
        }

        try {
            // Dừng gửi lệnh liên tục ngay lập tức
            this.stopContinuousSending();
            
            // Reset command tracking
            this.lastCommandSent = null;
            
            // Stop all joysticks
            this.stopJoystick('left');
            this.stopJoystick('right');

            // Send stop command
            const stopCommand = {
                tasks: [{
                    device: 'wheels',
                    type: 'drive',
                    left: 0,
                    right: 0,
                    taskId: `emergency-stop-${Date.now()}`
                }]
            };

            const response = await fetch('/robot/tasks/replace', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(stopCommand)
            });

            if (response.ok) {
                this.log('🛑 DỪNG KHẨN CẤP - Robot đã dừng', 'warning');
            } else {
                this.log(`Lỗi dừng khẩn cấp: ${response.status}`, 'error');
            }
        } catch (error) {
            this.log(`Lỗi dừng khẩn cấp: ${error.message}`, 'error');
        }
    }

    async checkConnection() {
        try {
            const response = await fetch('/robot/status');
            if (response.ok) {
                const status = await response.json();
                this.isConnected = status.connected;
                
                if (this.isConnected) {
                    this.statusDot.classList.add('connected');
                    this.statusText.textContent = 'Đã kết nối';
                    this.log('Đã kết nối với robot', 'success');
                } else {
                    this.statusDot.classList.remove('connected');
                    this.statusText.textContent = 'Robot offline';
                    this.log('Robot đang offline', 'warning');
                }
            } else {
                this.isConnected = false;
                this.statusDot.classList.remove('connected');
                this.statusText.textContent = 'Lỗi kết nối';
                this.log('Lỗi kiểm tra kết nối', 'error');
            }
        } catch (error) {
            this.isConnected = false;
            this.statusDot.classList.remove('connected');
            this.statusText.textContent = 'Lỗi kết nối';
            this.log(`Lỗi kết nối: ${error.message}`, 'error');
        }
    }

    startConnectionCheck() {
        // Check connection every 5 seconds
        setInterval(() => {
            this.checkConnection();
        }, 5000);
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
    new RobotController();
});
