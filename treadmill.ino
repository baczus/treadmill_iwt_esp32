#include <RCSwitch.h>

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
const int MIN_TREADMILL_SPEED = 1; // km/h
const int BASELINE_SPEED = 3; // km/h added on top of minimal
const int INCREASE_SPEED = 3; // km/h added on top of baseline
const int CYCLE_COUNT = 5;
const unsigned long INTERVAL_3MIN = 180000; // 3 minutes in ms

int seqPhase = 0;
int seqCycle = 0;
unsigned long seqTimer = 0;
bool running = false;

void setup() {
  Serial.begin(115200);
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
  }
  lastUp = state;

  int state2 = digitalRead(PIN_SPEED_DOWN);
  if (lastDown == HIGH && state2 == LOW && now - lastDownPress > 100) {
    lastDownPress = now;
    Serial.println("speed down");
    tx.send(SPEED_DOWN, 24);
  }
  lastDown = state2;

  int stateWalk = digitalRead(PIN_INTERNAL_WALKING);
  if (!running && lastWalk == HIGH && stateWalk == LOW && now - lastWalkPress > 100) {
    lastWalkPress = now;
    running = true;
    seqPhase = 1;
    seqTimer = now;
    Serial.println("=== internal walking started ===");
  }
  lastWalk = stateWalk;

  if (running) tickSequence(now);

  delay(10);
}

void tickSequence(unsigned long now) {
  switch (seqPhase) {
    // === SEND STOP ===
    case 1:
      tx.setProtocol(1);
      tx.setPulseLength(422);
      tx.send(16776971UL, 24);
      Serial.println("stop sent");
      seqTimer = now;
      seqPhase = 2;
      break;

    // === WAIT 20s THEN SEND START ===
    case 2:
      if (now - seqTimer >= 20000) {
        tx.setProtocol(1);
        tx.setPulseLength(424);
        tx.send(16776974UL, 24);
        Serial.println("start sent");
        seqTimer = now;
        seqPhase = 3;
      }
      break;

    // === WAIT 10s THEN SEND BASELINE (40x speed up) ===
    case 3:
      if (now - seqTimer >= 10000) {
        tx.setProtocol(1);
        tx.setPulseLength(425);
        sendCode(SPEED_UP, BASELINE_SPEED * SIGNALS_PER_KMH, 200);
        Serial.println("baseline speed up done");
        seqTimer = now;
        seqPhase = 4;
      }
      break;

    // === WAIT 3min THEN SEND WALK UP (30x speed up) ===
    case 4:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_UP, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        Serial.println("walking speed up done");
        seqTimer = now;
        seqPhase = 5;
      }
      break;

    // === WAIT 3min THEN SEND WALK DOWN (30x speed down) ===
    case 5:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_DOWN, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        Serial.println("walking speed down done");
        seqCycle = 0;
        seqTimer = now;
        seqPhase = 6;
      }
      break;

    // === CYCLE: WAIT 3min THEN SEND CYCLE UP (30x speed up) ===
    case 6:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_UP, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
        Serial.print("cycle "); Serial.print(seqCycle + 1); Serial.println("/5 walking speed up");
        seqTimer = now;
        seqPhase = 7;
      }
      break;

    // === CYCLE: WAIT 3min THEN SEND CYCLE DOWN (30x speed down) ===
    case 7:
      if (now - seqTimer >= INTERVAL_3MIN) {
        sendCode(SPEED_DOWN, INCREASE_SPEED * SIGNALS_PER_KMH, 200);
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

    // === SEND COOLDOWN (40x speed down) ===
    case 8:
      sendCode(SPEED_DOWN, 40, 200);
      Serial.println("cooldown done");
      seqPhase = 9;
      break;

    // === DONE ===
    case 9:
      Serial.println("=== internal walking done ===");
      running = false;
      seqPhase = 0;
      break;
  }
}
