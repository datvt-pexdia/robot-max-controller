 #include "WheelsDevice.h"
#include "../Config.h"
#include <Arduino.h>

// ==========================================================
// Build-time toggles
// ==========================================================
#ifndef WHEELS_DEBUG
#define WHEELS_DEBUG 0           // 0: quiet, 1: verbose logs
#endif

#ifndef COMM_PERIOD_MS
#define COMM_PERIOD_MS 10        // 10ms cadence theo tài liệu để chuyển động đều
#endif

// ==========================================================
// Motor control constants (Meccano MAX protocol)
// ==========================================================
// Speed bytes
static constexpr uint8_t SPEED_STOP   = 0x40;  // valid STOP speed
static constexpr uint8_t SPEED_MINRUN = 0x42;  // minimal running speed
static constexpr uint8_t SPEED_MAX    = 0x4F;  // maximal speed

// Direction bytes
// Note: trên MAX, "robot forward" = Left anticlockwise (0x34), Right clockwise (0x24)
static constexpr uint8_t DIR_STOP     = 0x00;
static constexpr uint8_t DIR_L_FWD    = 0x34;  // Left forward  (anti-clockwise)
static constexpr uint8_t DIR_L_REV    = 0x24;  // Left reverse  (clockwise)
static constexpr uint8_t DIR_R_FWD    = 0x24;  // Right forward (clockwise)
static constexpr uint8_t DIR_R_REV    = 0x34;  // Right reverse (anti-clockwise)

// Wiring note: Byte[1] -> RIGHT motor, Byte[2] -> LEFT motor
static constexpr int pinMOTOR = D4;   // ESP8266 GPIO2

// ==========================================================
// Small helpers (internal linkage)
// ==========================================================
static inline uint8_t mapSpeedPercentToByte(int pctAbs /*0..100*/) {
  if (pctAbs <= 0) return SPEED_STOP;  // allow true stop for that wheel
  return (uint8_t)map(pctAbs, 0, 100, SPEED_MINRUN, SPEED_MAX);
}

static inline uint8_t clampSpeedAllowStop(uint8_t s) {
  if (s == SPEED_STOP) return SPEED_STOP;  // keep STOP as is
  if (s < SPEED_MINRUN) return SPEED_MINRUN;
  if (s > SPEED_MAX)    return SPEED_MAX;
  return s;
}

static inline uint8_t leftDirFromSigned(int v) {
  if (v == 0) return DIR_STOP;
  return (v > 0) ? DIR_L_FWD : DIR_L_REV;
}

static inline uint8_t rightDirFromSigned(int v) {
  if (v == 0) return DIR_STOP;
  return (v > 0) ? DIR_R_FWD : DIR_R_REV;
}

// ==========================================================
// WheelsDevice
// ==========================================================
WheelsDevice::WheelsDevice()
    : running(false),
      hasTask(false),
      startMs(0),
      durationMs(0),
      lastLoggedProgress(0),
      lastCommandMs(0),
      channelMOTOR(nullptr),
      drive(nullptr),
      motorInitialized(false),
      continuousMode(true),  // MẶC ĐỊNH: true = continuous mode luôn bật
      isMoving(false),
      needSpeedUpdate(false) {
  current.taskId = "";
#if defined(__has_include)
  // no-op
#endif
  // Nếu TaskEnvelope có trường leftCmd/rightCmd thì đảm bảo init stop
  current.leftCmd  = DIR_STOP;
  current.rightCmd = DIR_STOP;
  leftSpeedByte  = SPEED_STOP;
  rightSpeedByte = SPEED_STOP;
}

const char* WheelsDevice::deviceName() const { return "wheels"; }

// ----------------------------------------------------------
// Internal: unified send respecting wiring (R,L)
// ----------------------------------------------------------
void WheelsDevice::sendRL_(uint8_t rightByte, uint8_t leftByte) {
  // NOTE: Byte[1] -> RIGHT, Byte[2] -> LEFT
  channelMOTOR->communicateAllByte(rightByte, leftByte, 0xFE, 0xFE);
  
  // ALWAYS log hex để debug (không cần WHEELS_DEBUG)
  Serial.printf("[WHEELS] sendRL: [FF %02X %02X FE FE] → Right=0x%02X Left=0x%02X\n", 
                rightByte, leftByte, rightByte, leftByte);
}

void WheelsDevice::startTask(const TaskEnvelope& task, uint32_t now) {
  current = task;
  hasTask = true;
  running = true;
  startMs = now;
  // Trong continuous mode, task không bao giờ kết thúc (durationMs rất lớn)
  // Trong normal mode, dùng duration từ task
  if (continuousMode) {
    durationMs = 0xFFFFFFFF; // vô hạn
  } else {
    durationMs = (task.durationMs >= 100) ? task.durationMs : 100;
  }
  lastLoggedProgress = 0;
  lastCommandMs = now;

  if (!motorInitialized) {
    initializeMotors();
  }

#if SIMULATION
  Serial.printf("[WHEELS] drive L=%d R=%d duration=%lu ms (taskId=%s) continuousMode=%d\n",
                task.left, task.right, durationMs, task.taskId.c_str(), continuousMode);
#else
  const bool isStopBoth = (task.left == 0 && task.right == 0);

  // ---- Speed per wheel
  uint8_t leftSpeed  = isStopBoth ? SPEED_STOP : mapSpeedPercentToByte(abs(task.left));
  uint8_t rightSpeed = isStopBoth ? SPEED_STOP : mapSpeedPercentToByte(abs(task.right));
  leftSpeed  = clampSpeedAllowStop(leftSpeed);
  rightSpeed = clampSpeedAllowStop(rightSpeed);
  leftSpeedByte  = leftSpeed;
  rightSpeedByte = rightSpeed;

  // ---- Direction per wheel
  uint8_t leftCmd  = isStopBoth ? DIR_STOP : leftDirFromSigned(task.left);
  uint8_t rightCmd = isStopBoth ? DIR_STOP : rightDirFromSigned(task.right);

  // Cập nhật trạng thái di chuyển
  isMoving = !isStopBoth;
  needSpeedUpdate = true; // đánh dấu cần gửi speed

  // ALWAYS log để debug
  Serial.printf("[WHEELS] startTask L=%d R=%d -> speed L=0x%02X R=0x%02X dir L=0x%02X R=0x%02X dur=%lu isMoving=%d (id=%s)\n",
                task.left, task.right, leftSpeed, rightSpeed, leftCmd, rightCmd,
                durationMs, isMoving, task.taskId.c_str());

  if (motorInitialized && channelMOTOR) {
    // Nếu đã có task cũ đang chạy và đổi hướng, gửi STOP trước
    if (hasTask && running) {
      uint8_t oldLeftCmd = current.leftCmd;
      uint8_t oldRightCmd = current.rightCmd;
      if (oldLeftCmd != leftCmd || oldRightCmd != rightCmd) {
        Serial.println("[WHEELS] startTask: DIRECTION CHANGE DETECTED - sending STOP first");
        sendRL_(SPEED_STOP, SPEED_STOP);  // dừng tốc độ
        delay(2);
        sendRL_(DIR_STOP, DIR_STOP);      // dừng hướng
        delay(2);
        Serial.println("[WHEELS] startTask: STOP sent, now setting new speed and direction");
      }
    }
    
    // 1) Set speed first
    setMotorSpeed(leftSpeed, rightSpeed);
    delay(1);
    needSpeedUpdate = false; // đã gửi speed

    // 2) Save & send initial direction (kept in tick)
    current.leftCmd  = leftCmd;
    current.rightCmd = rightCmd;
    sendRL_(current.rightCmd, current.leftCmd);
  } else {
#if WHEELS_DEBUG
    Serial.println("[WHEELS] startTask: motor not initialized or channel null");
#endif
  }
#endif
}

bool WheelsDevice::tryUpdate(const TaskEnvelope& task, uint32_t now) {
  if (!running || !motorInitialized || !channelMOTOR) return false;

  // Cập nhật tham số lái: tốc độ/hướng mỗi bánh
  const bool isStopBoth = (task.left == 0 && task.right == 0);
  const bool wasMoving = isMoving;

  uint8_t leftSpeed  = isStopBoth ? SPEED_STOP : mapSpeedPercentToByte(abs(task.left));
  uint8_t rightSpeed = isStopBoth ? SPEED_STOP : mapSpeedPercentToByte(abs(task.right));
  leftSpeed  = clampSpeedAllowStop(leftSpeed);
  rightSpeed = clampSpeedAllowStop(rightSpeed);

  uint8_t leftCmdUpd  = isStopBoth ? DIR_STOP : leftDirFromSigned(task.left);
  uint8_t rightCmdUpd = isStopBoth ? DIR_STOP : rightDirFromSigned(task.right);

  // Kiểm tra xem có thay đổi tốc độ hoặc hướng không
  const bool speedChanged = (leftSpeed != leftSpeedByte) || (rightSpeed != rightSpeedByte);
  const bool directionChanged = (leftCmdUpd != current.leftCmd) || (rightCmdUpd != current.rightCmd);
  
  // Cập nhật trạng thái
  isMoving = !isStopBoth;
  
  // ALWAYS log để debug
  Serial.printf("[WHEELS] tryUpdate: L=%d R=%d -> speed L=0x%02X R=0x%02X dir L=0x%02X R=0x%02X wasMoving=%d isMoving=%d speedChanged=%d dirChanged=%d\n",
                task.left, task.right, leftSpeed, rightSpeed, leftCmdUpd, rightCmdUpd,
                wasMoving, isMoving, speedChanged, directionChanged);
  
  // Trong continuous mode:
  // - Nếu thay đổi từ dừng -> di chuyển: gửi speed rồi direction
  // - Nếu thay đổi tốc độ HOẶC hướng: gửi speed mới (vì motor cần reset khi đổi hướng)
  // - Nếu thay đổi từ di chuyển -> dừng: gửi stop
  if (continuousMode) {
    if (!wasMoving && isMoving) {
      // Bắt đầu di chuyển: gửi speed
      Serial.printf("[WHEELS] tryUpdate: START MOVING L=%d R=%d\n", task.left, task.right);
      leftSpeedByte  = leftSpeed;
      rightSpeedByte = rightSpeed;
      setMotorSpeed(leftSpeed, rightSpeed);
      delay(1);
      needSpeedUpdate = false;
    } else if (speedChanged || directionChanged) {
      // Thay đổi tốc độ HOẶC hướng: gửi speed mới (không cần kiểm tra isMoving)
      if (directionChanged) {
        Serial.printf("[WHEELS] tryUpdate: DIRECTION CHANGED L=0x%02X->0x%02X R=0x%02X->0x%02X\n", 
                      current.leftCmd, leftCmdUpd, current.rightCmd, rightCmdUpd);
        
        // QUAN TRỌNG: Gửi lệnh dừng trước khi đổi hướng để motor không bị "ráng"
        Serial.println("[WHEELS] tryUpdate: SENDING STOP BEFORE DIRECTION CHANGE");
        sendRL_(SPEED_STOP, SPEED_STOP);  // dừng tốc độ
        delay(2);
        sendRL_(DIR_STOP, DIR_STOP);      // dừng hướng
        delay(2);
        
        Serial.println("[WHEELS] tryUpdate: STOP sent, now setting new speed and direction");
      }
      if (speedChanged) {
        Serial.printf("[WHEELS] tryUpdate: SPEED CHANGED L=0x%02X->0x%02X R=0x%02X->0x%02X\n", 
                      leftSpeedByte, leftSpeed, rightSpeedByte, rightSpeed);
      }
      leftSpeedByte  = leftSpeed;
      rightSpeedByte = rightSpeed;
      setMotorSpeed(leftSpeed, rightSpeed);
      delay(1);
      needSpeedUpdate = false;
    } else if (wasMoving && !isMoving) {
      // Dừng lại
      Serial.println("[WHEELS] tryUpdate: STOPPING");
      leftSpeedByte  = SPEED_STOP;
      rightSpeedByte = SPEED_STOP;
      needSpeedUpdate = false;
      // Gửi stop signal ngay
      stopMotors();
      delay(1);
    }
  } else {
    // Normal mode: LUÔN cập nhật speed và direction (giống backup version)
    leftSpeedByte  = leftSpeed;
    rightSpeedByte = rightSpeed;
    setMotorSpeed(leftSpeed, rightSpeed);
    delay(1);
  }

  // Cập nhật hướng (keep-alive tiếp tục dùng các byte này)
  current.leftCmd  = leftCmdUpd;
  current.rightCmd = rightCmdUpd;
  sendRL_(current.rightCmd, current.leftCmd);

  // Cập nhật tham số runtime cho task hiện tại
  current.left  = task.left;
  current.right = task.right;

  // Kéo dài thời lượng (chỉ trong normal mode)
  if (!continuousMode) {
    const uint32_t minDur = (task.durationMs >= 100) ? task.durationMs : 300;
    const uint32_t elapsed = now - startMs;
    if (elapsed + 50 >= durationMs) {
      durationMs = elapsed + minDur;
    } else {
      durationMs = max(durationMs, elapsed + minDur);
    }
  }

  lastCommandMs = now;
  return true;
}

void WheelsDevice::tick(uint32_t now) {
  if (!running) return;

#if !SIMULATION
  // Nếu motor chưa initialized, thử detect lại (không block)
  if (!motorInitialized && channelMOTOR) {
    static uint32_t lastRetry = 0;
    if (now - lastRetry >= 1000) { // retry mỗi 1 giây
      lastRetry = now;
      channelMOTOR->communicate();
      if (channelMOTOR->getDeviceName(1) != NULL) {
        motorInitialized = true;
        Serial.println("[WHEELS] Motor device detected!");
      }
    }
  }

  if (motorInitialized && channelMOTOR && (now - lastCommandMs) >= COMM_PERIOD_MS) {
    // Continuous mode: gửi tín hiệu liên tục theo trạng thái
    if (continuousMode) {
      if (isMoving) {
        // Đang di chuyển: gửi tín hiệu direction liên tục
        // (speed đã được gửi khi bắt đầu hoặc khi thay đổi)
        if (needSpeedUpdate) {
          // Nếu cần cập nhật speed (trường hợp đặc biệt)
          sendRL_(rightSpeedByte, leftSpeedByte);
          delay(1);
          needSpeedUpdate = false;
        }
        // Gửi direction liên tục
        sendRL_(current.rightCmd, current.leftCmd);
      } else {
        // Đang dừng: gửi tín hiệu STOP liên tục
        sendRL_(DIR_STOP, DIR_STOP);
      }
    } else {
      // Normal mode: keep-alive như cũ (gửi cả speed và direction)
      sendRL_(rightSpeedByte, leftSpeedByte); // speed (R,L)
      delay(1);
      sendRL_(current.rightCmd, current.leftCmd); // direction (R,L)
    }
    lastCommandMs = now;
    // tránh đói Wi-Fi
    yield();
  }
#endif

  const uint8_t pct = progress(now);
#if WHEELS_DEBUG
  if (pct >= lastLoggedProgress + 25 && pct < 100) {
    Serial.printf("[WHEELS] progress %u%% (id=%s)\n", pct, current.taskId.c_str());
    lastLoggedProgress = pct;
  }
#endif

  // Trong continuous mode, task không bao giờ kết thúc tự động
  if (!continuousMode && (now - startMs >= durationMs)) {
    running = false;
#if SIMULATION
    Serial.printf("[WHEELS] completed (id=%s)\n", current.taskId.c_str());
#else
#if WHEELS_DEBUG
    Serial.printf("[WHEELS] duration reached; stopping (elapsed=%lu, id=%s)\n",
                  (now - startMs), current.taskId.c_str());
#endif
    stopMotors();
#endif
  }
}

void WheelsDevice::cancel(uint32_t now) {
  if (!hasTask) {
#if WHEELS_DEBUG
    Serial.println("[WHEELS] cancel() called but no task is running");
#endif
    return;
  }
#if WHEELS_DEBUG
  Serial.printf("[WHEELS] cancel at %lu ms (id=%s) continuousMode=%d\n", now, current.taskId.c_str(), continuousMode);
#endif
#if !SIMULATION
  // Trong continuous mode, không dừng task, chỉ chuyển về trạng thái dừng
  if (continuousMode) {
    isMoving = false;
    leftSpeedByte = SPEED_STOP;
    rightSpeedByte = SPEED_STOP;
    current.leftCmd = DIR_STOP;
    current.rightCmd = DIR_STOP;
    current.left = 0;
    current.right = 0;
    // Task vẫn chạy, tiếp tục gửi STOP signal
  } else {
    stopMotors();
    running = false;
    hasTask = false;
    current.taskId = "";
  }
#else
  running = false;
  hasTask = false;
  current.taskId = "";
#endif
}

void WheelsDevice::finish() {
  hasTask = false;
  current.taskId = "";
}

bool WheelsDevice::isRunning() const { return running; }

bool WheelsDevice::isCompleted(uint32_t now) const {
  (void)now;
  // Trong continuous mode, task không bao giờ completed
  if (continuousMode) return false;
  return hasTask && !running && current.taskId.length() > 0;
}

uint8_t WheelsDevice::progress(uint32_t now) const {
  // Trong continuous mode, luôn trả về 50% (đang chạy liên tục)
  if (continuousMode) return running ? 50 : 0;
  
  if (!running) return hasTask ? 100 : 0;
  const uint32_t elapsed = now - startMs;
  if (durationMs == 0) return 0;
  uint32_t pct = (elapsed * 100) / durationMs;
  if (pct > 100) pct = 100;
  return (uint8_t)pct;
}

String WheelsDevice::currentTaskId() const { return current.taskId; }

// ==========================================================
// Motor I/O
// ==========================================================
void WheelsDevice::initializeMotors() {
  // Chỉ allocate channel và drive, KHÔNG block để chờ motor
  // Motor sẽ được phát hiện dần dần trong tick()
  if (channelMOTOR && drive) {
    Serial.println("[WHEELS] Motors already initialized");
    return;
  }

  Serial.println("[WHEELS] Initializing Meccano MAX motors (non-blocking)...");
  
  // Allocate once
  if (!channelMOTOR) channelMOTOR = new MeccaChannel(pinMOTOR);
  if (!drive)        drive        = new MeccaMaxDrive(channelMOTOR);

#if !SIMULATION
  // Thử communicate một lần để bắt đầu discovery
  channelMOTOR->communicate();
  
  // Kiểm tra xem motor đã sẵn sàng chưa (không block)
  if (channelMOTOR->getDeviceName(1) != NULL) {
    motorInitialized = true;
    Serial.println("[WHEELS] Motor device detected immediately!");
  } else {
    motorInitialized = false;
    Serial.println("[WHEELS] Motor device not detected yet, will retry in tick()...");
  }
#else
  motorInitialized = true; // Simulation mode
  Serial.println("[WHEELS] Motors initialized (simulation mode)");
#endif
}

void WheelsDevice::setMotorSpeed(uint8_t leftSpeed, uint8_t rightSpeed) {
  if (!motorInitialized || !channelMOTOR) {
    Serial.println("[WHEELS] setMotorSpeed: not initialized or channel is null");
    return;
  }

  leftSpeed  = clampSpeedAllowStop(leftSpeed);
  rightSpeed = clampSpeedAllowStop(rightSpeed);

  Serial.printf("[WHEELS] setMotorSpeed: L=0x%02X R=0x%02X\n", leftSpeed, rightSpeed);
  // Send as (R,L)
  sendRL_(rightSpeed, leftSpeed);
}

void WheelsDevice::stopMotors() {
  if (!motorInitialized || !channelMOTOR) {
#if WHEELS_DEBUG
    Serial.println("[WHEELS] stopMotors: not initialized or channel is null");
#endif
    return;
  }

#if WHEELS_DEBUG
  Serial.println("[WHEELS] Stopping motors...");
#endif
  // 1) Speed stop for both wheels
  sendRL_(SPEED_STOP, SPEED_STOP);
  delay(2);
  // 2) Direction stop for both wheels
  sendRL_(DIR_STOP, DIR_STOP);

  // Update current cached bytes (avoid ghost motion in tick)
  current.leftCmd  = DIR_STOP;
  current.rightCmd = DIR_STOP;
  leftSpeedByte  = SPEED_STOP;
  rightSpeedByte = SPEED_STOP;
}

void WheelsDevice::startContinuousTask(uint32_t now) {
  if (!motorInitialized) {
    initializeMotors();
  }

  continuousMode = true;
  isMoving = false; // ban đầu dừng
  needSpeedUpdate = false;
  
  // Tạo một task dummy với taskId đặc biệt
  TaskEnvelope continuousTask;
  continuousTask.taskId = "wheels_continuous";
  continuousTask.device = "wheels";
  continuousTask.left = 0;
  continuousTask.right = 0;
  continuousTask.durationMs = 0xFFFFFFFF; // vô hạn
  
  current = continuousTask;
  hasTask = true;
  running = true;
  startMs = now;
  durationMs = 0xFFFFFFFF;
  lastLoggedProgress = 0;
  lastCommandMs = now;
  
  leftSpeedByte = SPEED_STOP;
  rightSpeedByte = SPEED_STOP;
  current.leftCmd = DIR_STOP;
  current.rightCmd = DIR_STOP;

#if WHEELS_DEBUG
  Serial.println("[WHEELS] Continuous task started - will send STOP signals continuously");
#endif
}

uint8_t WheelsDevice::clampSpeed(uint8_t speed) {
  // Kept for header/API compatibility; we allow STOP across
  return clampSpeedAllowStop(speed);
}
