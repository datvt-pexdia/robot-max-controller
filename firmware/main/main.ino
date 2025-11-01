// firmware/src/main.ino
#include <Arduino.h>
#include "Config.h"
#include "TaskRunner.h"
#include "NetClient.h"

TaskRunner RUNNER;
NetClient   NET;

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("\n[BOOT] robot-max-controller (optimized)");

  RUNNER.begin();
  NET.begin(&RUNNER);
}

void loop() {
  NET.loop();
  RUNNER.loop();
}
