// firmware/src/Devices/WheelsDevice.cpp
#include "WheelsDevice.h"
#include "../Config.h"
#include <math.h>

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void WheelsDevice::begin() {
  curL_ = curR_ = 0.0f;
  tgtL_ = tgtR_ = 0.0f;
  lastCmdAt_ = millis();
  deadlineAt_ = 0;

  lastDirL_ = lastDirR_ = 0xFF;
  lastSpeedL_ = lastSpeedR_ = 0xFF;
  lastFrameAt_ = 0;

#if !SIMULATION
  if (!maxBus_)  maxBus_ = new MeccaChannel(MAX_DATA_PIN);
  if (!motorL_)  motorL_ = new MeccaMaxMotorDevice(*maxBus_, MAX_LEFT_POS);
  if (!motorR_)  motorR_ = new MeccaMaxMotorDevice(*maxBus_, MAX_RIGHT_POS);

  // Gửi một lượt communicate để kích hoạt thiết bị trên bus (giống DriveTest.ino)
  if (maxBus_) {
    maxBus_->communicate();
  }
#endif
}

void WheelsDevice::setTarget(int8_t leftPct, int8_t rightPct, uint32_t durationMs) {
  // map -100..100 → -1..1
  tgtL_ = clampf(leftPct / 100.0f, -1.0f, 1.0f);
  tgtR_ = clampf(rightPct / 100.0f, -1.0f, 1.0f);
  lastCmdAt_ = millis();
  deadlineAt_ = durationMs ? (lastCmdAt_ + durationMs) : 0;
}

<<<<<<< HEAD
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
#if WHEELS_DEBUG
    Serial.printf("[WHEELS] duration reached; stopping (elapsed=%lu, id=%s)\n",
                  (now - startMs), current.taskId.c_str());
#endif
    stopMotors();
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
}
=======
void WheelsDevice::emergencyStop() {
  curL_ = curR_ = 0.0f;
  tgtL_ = tgtR_ = 0.0f;
  driveMotors(0, 0);
}

void WheelsDevice::applySlewToward(float &cur, float tgt) {
  const float SLEW = 0.08f; // mỗi tick đổi tối đa 0.08
  float d = tgt - cur;
  if (fabs(d) <= SLEW) cur = tgt;
  else cur += (d > 0 ? SLEW : -SLEW);
}

void WheelsDevice::tick() {
  const uint32_t now = millis();

  // Timeout mềm: không có lệnh mới trong một thời gian ngắn ⇒ tự giảm về 0
  if ((now - lastCmdAt_) > SOFT_STOP_TIMEOUT_MS) { tgtL_ = 0.0f; tgtR_ = 0.0f; }
  // Deadline tác vụ (nếu có)
  if (deadlineAt_ && now >= deadlineAt_) { tgtL_ = 0.0f; tgtR_ = 0.0f; }

  // Timeout cứng: mất kết nối lâu ⇒ STOP ngay
  if ((now - lastCmdAt_) > HARD_STOP_TIMEOUT_MS) { emergencyStop(); return; }

  // Slew-rate để mượt
  applySlewToward(curL_, tgtL_);
  applySlewToward(curR_, tgtR_);

  // Gửi lệnh xuống hardware (hoặc log nếu SIMULATION)
  driveMotors(curL_, curR_);
}

// Map [-1..1] → speed code {0x40 (stop)} U {0x42..0x4F}
// 0x41 không dùng; ngưỡng |v|<0.02 coi như 0 → STOP
uint8_t WheelsDevice::speedCodeFromNorm(float v) const {
  float a = fabs(v);
  if (a < 0.02f) return 0x40; // STOP
  int step = (int)roundf(a * 14.0f); // 1..14
  if (step < 1) step = 1;
  if (step > 14) step = 14;
  return (uint8_t)(0x41 + step); // 0x42..0x4F
}

// Trả về 0x2n(CW) / 0x3n(CCW) theo hướng "tiến" cấu hình cho từng bánh
uint8_t WheelsDevice::dirCodeForWheel(bool isLeft, float v) const {
  const uint8_t DIR_CW  = 0x20;
  const uint8_t DIR_CCW = 0x30;
  const bool forwardIsCCW = isLeft ? (LEFT_FORWARD_IS_CCW != 0) : (RIGHT_FORWARD_IS_CCW != 0);
  const bool forwardCmd   = (v >= 0.0f);
  bool wantCCW = forwardCmd ? forwardIsCCW : !forwardIsCCW;
  return wantCCW ? DIR_CCW : DIR_CW;
}

void WheelsDevice::driveMotors(float normL, float normR) {
#if SIMULATION
  static uint32_t lastLog = 0;
  const uint32_t now = millis();
  if (now - lastLog >= 100) {
    Serial.printf("[WHEELS] L=%.2f R=%.2f\n", normL, normR);
    lastLog = now;
  }
#else
  if (!maxBus_) return;
  const uint32_t now = millis();
>>>>>>> a6f534eb9db26fd35b6623fcd6d33d67f795d0f4

  // Frame hiện tại
  const uint8_t spL = speedCodeFromNorm(normL);
  const uint8_t spR = speedCodeFromNorm(normR);
  const uint8_t dirL = (spL == 0x40) ? dirCodeForWheel(true,  0.0f) : dirCodeForWheel(true,  normL);
  const uint8_t dirR = (spR == 0x40) ? dirCodeForWheel(false, 0.0f) : dirCodeForWheel(false, normR);

  const bool chgL = (dirL != lastDirL_) || (spL != lastSpeedL_);
  const bool chgR = (dirR != lastDirR_) || (spR != lastSpeedR_);
  const bool needKeepAlive = (now - lastFrameAt_) >= MAX_KEEPALIVE_MS;

  if (!(chgL || chgR || needKeepAlive)) {
    return;
  }

  // Theo mapping community: Byte[0] = Right dir, Byte[1] = Left dir, Byte[2] = Right speed, Byte[3] = Left speed
  // Sử dụng communicateAllByte() như trong DriveTest.ino
  maxBus_->communicateAllByte(dirR, dirL, spR, spL);

<<<<<<< HEAD
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
=======
  lastDirL_ = dirL;
  lastSpeedL_ = spL;
  lastDirR_ = dirR;
  lastSpeedR_ = spR;
  lastFrameAt_ = now;
#endif
>>>>>>> a6f534eb9db26fd35b6623fcd6d33d67f795d0f4
}
