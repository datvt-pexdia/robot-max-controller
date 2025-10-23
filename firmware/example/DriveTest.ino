#include <MeccaChannel.h>
#include <MeccaMaxDrive.h>
#include <Scheduler.h>

// Motor command states for FSM
enum MotorStates {
  FORWARD,
  BACKWARD,
  TURN_LEFT,
  TURN_RIGHT,
  STOP
};

// ===== Speed constants & helpers =====
static const uint8_t SPEED_STOP = 0x40;    // stop
static const uint8_t SPEED_MINRUN = 0x42;  // bắt đầu quay
static const uint8_t SPEED_MAX = 0x4F;     // max speed
static const uint16_t RAMP_STEP_MS = 10;   // delay giữa các nấc tăng tốc

// Globals
static MeccaChannel *channelMOTOR;
static Scheduler mainScheduler = Scheduler();
static MotorStates motorState = FORWARD;
static int motor_update = 50;  // ms
static int pinMOTOR = D4;      // GPIO2 on ESP8266
static MeccaMaxDrive *drive;

// --- Utils (giữ như base, rút gọn log) ---
void printByteHex2(uint8_t b) {
  if (b < 0x10) Serial.print("0");
  Serial.print(b, HEX);
}
void logMotorCommand(uint8_t, uint8_t, uint8_t, uint8_t, const char *action) {
  Serial.print("Motor Command - ");
  Serial.println(action);
}

// --- NEW: speed frames ---
inline void setSpeed(uint8_t leftSpeed, uint8_t rightSpeed) {
  channelMOTOR->communicateAllByte(leftSpeed, rightSpeed, 0xFE, 0xFE);
}

static inline uint8_t clampSpeed(uint8_t v) {
  if (v < 0x40) return 0x40;
  if (v > 0x4F) return 0x4F;
  return v;
}

void rampSpeedTo(uint8_t targetLeft, uint8_t targetRight) {
  setSpeed(clampSpeed(targetLeft), clampSpeed(targetRight));
}
// ============ Actions (giữ base, chỉ thêm speed control) ============
void motorForward() {
  Serial.println("Starting FORWARD (ramp to MAX)...");
  // Chỉ FORWARD: ramp lên max để mượt & tốc độ cao
  rampSpeedTo(SPEED_MAX, SPEED_MAX);

  for (int i = 0; i < 10; i++) {
    // Base forward direction bytes (giữ nguyên như bạn đang dùng)
    channelMOTOR->communicateAllByte(0x24, 0x34, 0xFE, 0xFE);
    logMotorCommand(0x24, 0x34, 0xFE, 0xFE, "FORWARD");
    delay(1);
  }
}

void motorBackward() {
  Serial.println("Starting BACKWARD (set MIN speed)...");
  // Các action khác: đặt speed MIN để phân biệt rõ
  rampSpeedTo(SPEED_MAX, SPEED_MAX);
  for (int i = 0; i < 10; i++) {
    channelMOTOR->communicateAllByte(0x34, 0x24, 0xFE, 0xFE);
    logMotorCommand(0x34, 0x24, 0xFE, 0xFE, "BACKWARD");
    delay(1);
  }
}

void motorTurnLeft() {
  Serial.println("Starting TURN LEFT (set MIN speed)...");
  rampSpeedTo(SPEED_MAX, SPEED_MAX);
  for (int i = 0; i < 10; i++) {
    channelMOTOR->communicateAllByte(0x24, 0x24, 0xFE, 0xFE);
    logMotorCommand(0x24, 0x24, 0xFE, 0xFE, "TURN LEFT");
    delay(1);
  }
}

void motorTurnRight() {
  Serial.println("Starting TURN RIGHT (set MIN speed)...");
  rampSpeedTo(SPEED_MAX, SPEED_MAX);
  for (int i = 0; i < 10; i++) {
    channelMOTOR->communicateAllByte(0x34, 0x34, 0xFE, 0xFE);
    logMotorCommand(0x34, 0x34, 0xFE, 0xFE, "TURN RIGHT");
    delay(1);
  }
}

void motorStop() {
  Serial.println("Starting STOP...");
  // Có thể gửi stop speed, nhưng giữ base như yêu cầu
  for (int i = 0; i < 1; i++) {
    channelMOTOR->communicateAllByte(0x00, 0x00, 0x00, 0x00);
    logMotorCommand(0x00, 0x00, 0x00, 0x00, "STOP");
    delay(1);
  }
}

void stopBetweenMovements() {
  Serial.println("Stopping between movements...");
  for (int i = 0; i < 1; i++) {
    channelMOTOR->communicateAllByte(0x00, 0x00, 0xFE, 0xFE);
    logMotorCommand(0x00, 0x00, 0xFE, 0xFE, "STOP");
    delay(1);
  }
}

// FSM (giữ base)
void testMotorCommands() {
  Serial.println("=== MOTOR COMMAND TEST ===");
  switch (motorState) {
    case FORWARD:
      motorForward();
      stopBetweenMovements();
      motorState = BACKWARD;
      break;
    case BACKWARD:
      motorBackward();
      stopBetweenMovements();
      motorState = TURN_LEFT;
      break;
    case TURN_LEFT:
      motorTurnLeft();
      stopBetweenMovements();
      motorState = TURN_RIGHT;
      break;
    case TURN_RIGHT:
      motorTurnRight();
      stopBetweenMovements();
      motorState = FORWARD;
      break;
  }
  mainScheduler.schedule(testMotorCommands, motor_update);
}

void setup() {
  Serial.begin(57600);
  delay(1000);

  Serial.println("\n\n=== MOTOR COMMAND TEST STARTED ===");
  Serial.println("FORWARD: ramp->MAX | Others: MIN speed");
  Serial.println("=====================================\n");

  channelMOTOR = new MeccaChannel(pinMOTOR);
  drive = new MeccaMaxDrive(channelMOTOR);

  Serial.println("Waiting for motor device to connect...");
  while (channelMOTOR->getDeviceName(1) == NULL) {
    channelMOTOR->communicate();
    Serial.print("Device status: ");
    Serial.println((channelMOTOR->getDeviceName(1) == NULL) ? "Not Connected" : "Connected");
    delay(100);
  }

  Serial.println("Motor device connected! Starting test...");
  mainScheduler.schedule(testMotorCommands, 200);
}

void loop() {
  mainScheduler.update();
}
