#include <RCSwitch.h>
#include "display.h"

RCSwitch tx;

const unsigned long SPEED_UP   = 16776972UL;
const unsigned long SPEED_DOWN = 16776970UL;

const int PIN_SPEED_DOWN = 26;
const int PIN_SPEED_UP   = 33;
const int PIN_INTERNAL_WALKING = 25;

int lastUp = HIGH;
int lastDown = HIGH;
int lastWalk = HIGH;
unsigned long lastUpPress = 0;
unsigned long lastDownPress = 0;
unsigned long lastWalkPress = 0;

const int SIGNALS_PER_KMH = 10;
const int MIN_TREADMILL_SPEED = 1;
const int BASELINE_SPEED = 3;
const int INCREASE_SPEED = 3;
const int CYCLE_COUNT = 5;
const unsigned long INTERVAL_3MIN = 180000;

int seqPhase = 0;
int seqCycle = 0;
unsigned long seqTimer = 0;
bool running = false;

int speedTenths = 0;
char statusText[16] = "Ready";

void setup() {
  Serial.begin(115200);

  initDisplay();
  updateDisplay(statusText, speedTenths, seqPhase, seqCycle);

  pinMode(PIN_SPEED_UP, INPUT_PULLUP);
  pinMode(PIN_SPEED_DOWN, INPUT_PULLUP);
  pinMode(PIN_INTERNAL_WALKING, INPUT_PULLUP);
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

  int stateWalk = digitalRead(PIN_INTERNAL_WALKING);
  if (!running && lastWalk == HIGH && stateWalk == LOW && now - lastWalkPress > 100) {
    lastWalkPress = now;
    running = true;
    seqPhase = 1;
    seqTimer = now;
    speedTenths = 0;
    strcpy(statusText, "Stopping");
    Serial.println("=== internal walking started ===");
  }
  lastWalk = stateWalk;

  if (running) tickSequence(now);

  updateDisplay(statusText, speedTenths, seqPhase, seqCycle);
  delay(10);
}

void tickSequence(unsigned long now) {
  switch (seqPhase) {
    case 1:
      tx.setProtocol(1);
      tx.setPulseLength(422);
      tx.send(16776971UL, 24);
      Serial.println("stop sent");
      speedTenths = 0;
      strcpy(statusText, "Starting");
      seqTimer = now;
      seqPhase = 2;
      break;

    case 2:
      if (now - seqTimer >= 20000) {
        tx.setProtocol(1);
        tx.setPulseLength(424);
        tx.send(16776974UL, 24);
        Serial.println("start sent");
        speedTenths = MIN_TREADMILL_SPEED * SIGNALS_PER_KMH;
        strcpy(statusText, "Warm-up");
        seqTimer = now;
        seqPhase = 3;
      }
      break;

    case 3:
      if (now - seqTimer >= 10000) {
        tx.setProtocol(1);
        tx.setPulseLength(425);
        sendCode(SPEED_UP, BASELINE_SPEED * SIGNALS_PER_KMH, 200);
        speedTenths += BASELINE_SPEED * SIGNALS_PER_KMH;
        strcpy(statusText, "Warm-up");
        Serial.println("baseline speed up done");
        seqTimer = now;
        seqPhase = 4;
      }
      break;

    case 4:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_UP, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        speedTenths += INCREASE_SPEED * SIGNALS_PER_KMH;
        strcpy(statusText, "Fast Interval");
        Serial.println("walking speed up done");
        seqTimer = now;
        seqPhase = 5;
      }
      break;

    case 5:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_DOWN, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        if (speedTenths >= INCREASE_SPEED * SIGNALS_PER_KMH)
          speedTenths -= INCREASE_SPEED * SIGNALS_PER_KMH;
        else
          speedTenths = 0;
        seqCycle = 0;
        seqTimer = now;
        seqPhase = 6;
        strcpy(statusText, "Slow Interval");
        Serial.println("walking speed down done");
      }
      break;

    case 6:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_UP, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        speedTenths += INCREASE_SPEED * SIGNALS_PER_KMH;
        strcpy(statusText, "Fast Interval");
        Serial.print("cycle "); Serial.print(seqCycle + 1); Serial.println("/5 walking speed up");
        seqTimer = now;
        seqPhase = 7;
      }
      break;

    case 7:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_DOWN, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        if (speedTenths >= INCREASE_SPEED * SIGNALS_PER_KMH)
          speedTenths -= INCREASE_SPEED * SIGNALS_PER_KMH;
        else
          speedTenths = 0;
        strcpy(statusText, "Slow Interval");
        Serial.print("cycle "); Serial.print(seqCycle + 1); Serial.println("/5 walking speed down");
        seqCycle++;
        if (seqCycle < CYCLE_COUNT) {
          seqTimer = now;
          seqPhase = 6;
        } else {
          seqPhase = 8;
        }
      }
      break;

    case 8:
      sendCode(SPEED_DOWN, 40, 200);
      if (speedTenths >= 40)
        speedTenths -= 40;
      else
        speedTenths = 0;
      strcpy(statusText, "Cool-down");
      seqTimer = now;
      seqPhase = 9;
      Serial.println("cooldown done");
      break;

    case 9:
      if (now - seqTimer >= 3000) {
        Serial.println("=== internal walking done ===");
        running = false;
        seqPhase = 0;
        strcpy(statusText, "Ready");
      }
      break;
  }
}
