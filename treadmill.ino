#include <RCSwitch.h>
#include "display.h"

RCSwitch tx;

const unsigned long SPEED_UP   = 16776972UL;
const unsigned long SPEED_DOWN = 16776970UL;
const unsigned long CODE_STOP  = 16776971UL;
const unsigned long CODE_START = 16776974UL;

const int PIN_SPEED_DOWN = 26;
const int PIN_SPEED_UP   = 33;
const int PIN_START_STOP = 25;

int lastUp = HIGH;
int lastDown = HIGH;
int lastStart = HIGH;
unsigned long lastUpPress = 0;
unsigned long lastDownPress = 0;
unsigned long lastStartPress = 0;

const int SIGNALS_PER_KMH = 10;
const int MIN_TREADMILL_SPEED = 1;
const int WARMUP_SPEED = 3;
const int SPEED_STEP = 3;
const int CYCLE_COUNT = 5;
const unsigned long INTERVAL_3MIN = 180000;

// Walking sequence state machine.
//   1 = STOP & set Starting
//   2 = wait 20s, send START, set speed 1 km/h
//   3 = wait 10s, add WARMUP_SPEED km/h (ramp to 4 km/h)
//   4 = wait 3min at 4 km/h (Warm-up)
//   5 = wait 3min, add SPEED_STEP km/h (Fast)
//   6 = wait 3min, sub SPEED_STEP km/h (Slow), then loop to 5 or go to 7
//   7 = rapid slowdown (Cooling)
//   8 = wait 3s, reset to idle
int seqPhase = 0;
int seqCycle = 0;
unsigned long seqTimer = 0;
bool running = false;

int speedTenths = 0;
char statusText[16] = "Ready";

void setup() {
  Serial.begin(115200);

  initDisplay();
  updateDisplay(statusText, speedTenths, seqPhase, seqCycle, 0);

  pinMode(PIN_SPEED_UP, INPUT_PULLUP);
  pinMode(PIN_SPEED_DOWN, INPUT_PULLUP);
  pinMode(PIN_START_STOP, INPUT_PULLUP);
  tx.enableTransmit(14);

  tx.setProtocol(1);
  tx.setPulseLength(425);
  tx.setRepeatTransmit(3);

  Serial.println("ready");
}

void sendCode(unsigned long code, int times, int intervalMs) {
  for (int i = 0; i < times; i++) {
    tx.send(code, 24);
    delay(intervalMs);
  }
}

void loop() {
  unsigned long now = millis();

  int state = digitalRead(PIN_SPEED_UP);
  if (lastUp == HIGH && state == LOW && now - lastUpPress > 100) {
    lastUpPress = now;
    Serial.println("speed up");
    tx.send(SPEED_UP, 24);
    speedTenths++;
  }
  lastUp = state;

  int state2 = digitalRead(PIN_SPEED_DOWN);
  if (lastDown == HIGH && state2 == LOW && now - lastDownPress > 100) {
    lastDownPress = now;
    Serial.println("speed down");
    tx.send(SPEED_DOWN, 24);
    if (speedTenths > 0) speedTenths--;
  }
  lastDown = state2;

  int stateStart = digitalRead(PIN_START_STOP);
  if (!running && lastStart == HIGH && stateStart == LOW && now - lastStartPress > 100) {
    lastStartPress = now;
    running = true;
    seqPhase = 1;
    seqTimer = now;
    speedTenths = 0;
    strcpy(statusText, "Init");
    Serial.println("=== internal walking started ===");
  }
  lastStart = stateStart;

  if (running) intervalWalkingTraining(now);

  updateDisplay(statusText, speedTenths, seqPhase, seqCycle, now);
  delay(10);
}

void intervalWalkingTraining(unsigned long now) {
  switch (seqPhase) {
    // 1: STOP – send stop signal, arm for starting
    case 1:
      tx.setProtocol(1);
      tx.setPulseLength(422);
      tx.send(CODE_STOP, 24);
      Serial.println("stop sent");
      speedTenths = 0;
      strcpy(statusText, "Init");
      seqTimer = now;
      seqPhase = 2;
      break;

    // 2: wait 20s, send START, set speed to 1 km/h
    case 2:
      if (now - seqTimer >= 20000) {
        tx.setProtocol(1);
        tx.setPulseLength(424);
        tx.send(CODE_START, 24);
        Serial.println("start sent");
        strcpy(statusText, "Starting");
        speedTenths = MIN_TREADMILL_SPEED * SIGNALS_PER_KMH;
        seqTimer = now;
        seqPhase = 3;
      }
      break;

    // 3: RAMP – wait 10s, add WARMUP_SPEED km/h (1→4 km/h)
    case 3:
      if (now - seqTimer >= 10000) {
        tx.setProtocol(1);
        tx.setPulseLength(425);
        sendCode(SPEED_UP, WARMUP_SPEED * SIGNALS_PER_KMH, 200);
        speedTenths += WARMUP_SPEED * SIGNALS_PER_KMH;
        seqCycle = 0;
        Serial.println("ramp done, entering warm-up");
        strcpy(statusText, "Warm-up");
        seqTimer = now;
        seqPhase = 4;
      }
      break;

    // 4: WARMUP – wait 3min at 4 km/h, then +3 km/h (4→7) for Fast
    case 4:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_UP, SPEED_STEP * SIGNALS_PER_KMH, 200);
        speedTenths += SPEED_STEP * SIGNALS_PER_KMH;
        Serial.println("warm-up done, entering fast interval");
        strcpy(statusText, "Fast");
        seqTimer = now;
        seqPhase = 5;
      }
      break;

    // 5: FAST – wait 3min at 7 km/h, then -3 km/h (7→4) for Slow
    case 5:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_DOWN, SPEED_STEP * SIGNALS_PER_KMH, 200);
        if (speedTenths >= SPEED_STEP * SIGNALS_PER_KMH)
          speedTenths -= SPEED_STEP * SIGNALS_PER_KMH;
        else
          speedTenths = 0;
        Serial.println("fast interval done, entering slow interval");
        strcpy(statusText, "Slow");
        seqTimer = now;
        seqPhase = 6;
      }
      break;

    // 6: SLOW INTERVAL – wait 3min at 4 km/h
    //    loops back to phase 5 (+3 km/h) for CYCLE_COUNT pairs, then exits to cooldown
    case 6:
      if (now - seqTimer >= INTERVAL_3MIN) {
        if (seqCycle < CYCLE_COUNT - 1) {
          sendCode(SPEED_UP, SPEED_STEP * SIGNALS_PER_KMH, 200);
          speedTenths += SPEED_STEP * SIGNALS_PER_KMH;
          seqCycle++;
          Serial.print("slow interval done, cycle "); Serial.print(seqCycle); Serial.println("/5 entering fast interval");
          strcpy(statusText, "Fast");
          seqTimer = now;
          seqPhase = 5;
        } else {
          Serial.println("slow interval done, entering cooldown");
          strcpy(statusText, "Cooling");
          seqPhase = 7;
        }
      }
      break;

    // 7: COOLDOWN – rapid slowdown to stop
    case 7:
      sendCode(SPEED_DOWN, 40, 200);
      if (speedTenths >= 40)
        speedTenths -= 40;
      else
        speedTenths = 0;
      seqTimer = now;
      seqPhase = 8;
      Serial.println("cooldown done");
      break;

    // 8: show Complete! for 3s, then reset
    case 8:
      if (now - seqTimer >= 3000) {
        Serial.println("=== internal walking done ===");
        running = false;
        seqPhase = 0;
        strcpy(statusText, "Ready");
      }
      break;
  }
}
