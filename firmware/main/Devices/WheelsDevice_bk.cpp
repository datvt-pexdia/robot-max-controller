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
      motorInitialized(false) {
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
#if WHEELS_DEBUG
  Serial.printf("[WHEELS] sendRL: [%02X %02X FE FE] (R,L)\n", rightByte, leftByte);
#endif
}

void WheelsDevice::startTask(const TaskEnvelope& task, uint32_t now) {
  current = task;
  hasTask = true;
  running = true;
  startMs = now;
  // tránh lệnh quá ngắn gây race
  durationMs = (task.durationMs >= 100) ? task.durationMs : 100;
  lastLoggedProgress = 0;
  lastCommandMs = now;

  if (!motorInitialized) {
    initializeMotors();
  }

#if SIMULATION
  Serial.printf("[WHEELS] drive L=%d R=%d duration=%lu ms (taskId=%s)\n",
                task.left, task.right, durationMs, task.taskId.c_str());
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

#if WHEELS_DEBUG
  Serial.printf("[WHEELS] startTask L=%d R=%d -> speed L=%02X R=%02X dir L=%02X R=%02X dur=%lu (id=%s)\n",
                task.left, task.right, leftSpeed, rightSpeed, leftCmd, rightCmd,
                durationMs, task.taskId.c_str());
#endif

  if (motorInitialized && channelMOTOR) {
    // 1) Set speed first
    setMotorSpeed(leftSpeed, rightSpeed);
    delay(1);

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

  // Cập nhật tham số lái: tốc độ/hướng mỗi bánh và kéo dài thời gian
  const bool isStopBoth = (task.left == 0 && task.right == 0);

  uint8_t leftSpeed  = isStopBoth ? SPEED_STOP : mapSpeedPercentToByte(abs(task.left));
  uint8_t rightSpeed = isStopBoth ? SPEED_STOP : mapSpeedPercentToByte(abs(task.right));
  leftSpeed  = clampSpeedAllowStop(leftSpeed);
  rightSpeed = clampSpeedAllowStop(rightSpeed);
  leftSpeedByte  = leftSpeed;
  rightSpeedByte = rightSpeed;

  uint8_t leftCmdUpd  = isStopBoth ? DIR_STOP : leftDirFromSigned(task.left);
  uint8_t rightCmdUpd = isStopBoth ? DIR_STOP : rightDirFromSigned(task.right);

  // 1) Cập nhật speed nếu thay đổi
  setMotorSpeed(leftSpeed, rightSpeed);
  delay(1);

  // 2) Cập nhật hướng (keep-alive tiếp tục dùng các byte này)
  current.leftCmd  = leftCmdUpd;
  current.rightCmd = rightCmdUpd;
  sendRL_(current.rightCmd, current.leftCmd);

  // 3) Cập nhật tham số runtime cho task hiện tại
  current.left  = task.left;
  current.right = task.right;

  // 4) Kéo dài thời lượng: nếu có duration mới thì dùng, nếu không thì "gia hạn mềm"
  const uint32_t minDur = (task.durationMs >= 100) ? task.durationMs : 300; // mặc định 300ms cho giữ liên tục
  const uint32_t elapsed = now - startMs;
  if (elapsed + 50 >= durationMs) {
    // nếu gần chạm đích, đẩy đích ra thêm
    durationMs = elapsed + minDur;
  } else {
    // còn xa đích: cũng gia hạn nhẹ để tránh cắt sớm
    durationMs = max(durationMs, elapsed + minDur);
  }

  lastCommandMs = now;
  return true;
}

void WheelsDevice::tick(uint32_t now) {
  if (!running) return;

#if !SIMULATION
  if (motorInitialized && channelMOTOR && (now - lastCommandMs) >= COMM_PERIOD_MS) {
    // keep-alive: resend speed then direction to avoid stutter on bus
    sendRL_(rightSpeedByte, leftSpeedByte); // speed (R,L)
    delay(1);
    sendRL_(current.rightCmd, current.leftCmd); // direction (R,L)
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

  if (now - startMs >= durationMs) {
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
  Serial.printf("[WHEELS] cancel at %lu ms (id=%s)\n", now, current.taskId.c_str());
#endif
#if !SIMULATION
  stopMotors();
#endif
  running = false;
  hasTask = false;
  current.taskId = "";
}

void WheelsDevice::finish() {
  hasTask = false;
  current.taskId = "";
}

bool WheelsDevice::isRunning() const { return running; }

bool WheelsDevice::isCompleted(uint32_t now) const {
  (void)now;
  return hasTask && !running && current.taskId.length() > 0;
}

uint8_t WheelsDevice::progress(uint32_t now) const {
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
  Serial.println("[WHEELS] Initializing Meccano MAX motors...");

  // Allocate once (kept for API compatibility)
  if (!channelMOTOR) channelMOTOR = new MeccaChannel(pinMOTOR);
  if (!drive)        drive        = new MeccaMaxDrive(channelMOTOR);

  Serial.println("[WHEELS] Waiting for motor device to connect...");
  uint8_t retries = 0;
  while (channelMOTOR->getDeviceName(1) == NULL && retries < 50) {
    channelMOTOR->communicate();
    Serial.print("Device status: ");
    Serial.println((channelMOTOR->getDeviceName(1) == NULL) ? "Not Connected" : "Connected");
    delay(100);
    retries++;
  }

  if (channelMOTOR->getDeviceName(1) != NULL) {
    motorInitialized = true;
    Serial.println("[WHEELS] Motor device connected successfully!");
  } else {
    motorInitialized = false;
    Serial.println("[WHEELS] Failed to connect to motor device!");
  }
}

void WheelsDevice::setMotorSpeed(uint8_t leftSpeed, uint8_t rightSpeed) {
  if (!motorInitialized || !channelMOTOR) {
#if WHEELS_DEBUG
    Serial.println("[WHEELS] setMotorSpeed: not initialized or channel is null");
#endif
    return;
  }

  leftSpeed  = clampSpeedAllowStop(leftSpeed);
  rightSpeed = clampSpeedAllowStop(rightSpeed);

#if WHEELS_DEBUG
  Serial.printf("[WHEELS] setSpeed L=%02X R=%02X\n", leftSpeed, rightSpeed);
#endif
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

uint8_t WheelsDevice::clampSpeed(uint8_t speed) {
  // Kept for header/API compatibility; we allow STOP across
  return clampSpeedAllowStop(speed);
}
