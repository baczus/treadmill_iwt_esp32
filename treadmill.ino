#include <RCSwitch.h>
#include "display.h"
#include "settings.h"
#include "buttons.h"

RCSwitch tx;

const unsigned long RF_UP    = 16776972UL;
const unsigned long RF_DOWN  = 16776970UL;
const unsigned long RF_STOP  = 16776971UL;
const unsigned long RF_START = 16776974UL;

const int SIGNALS_PER_KMH       = 10;
const int START_SPEED_TENTHS    = 10;
const int INTERVAL_PAIRS        = 5;
const unsigned long PHASE_DURATION_MS = 180000;

int walkPhase = 0;
int intervalPair = 0;
unsigned long phaseTimer = 0;
bool walkActive = false;
int speedTenths = 0;

char statusMsg[16] = "Ready";

// Non-blocking RF sender
static unsigned long rfCode;
static int rfCount;
static int rfInterval;
static unsigned long rfLastTime;
static bool rfBusy = false;
static int rfNextPhase;

static void rfStart(unsigned long code, int count, int interval, int nextPhase) {
  rfCode = code;
  rfCount = count;
  rfInterval = interval;
  rfLastTime = 0;
  rfBusy = true;
  rfNextPhase = nextPhase;
}

static void rfProcess() {
  if (!rfBusy) return;
  unsigned long now = millis();
  if (now - rfLastTime >= (unsigned long)rfInterval) {
    tx.send(rfCode, 24);
    rfCount--;
    rfLastTime = now;
    if (rfCount <= 0) {
      rfBusy = false;
      walkPhase = rfNextPhase;
      phaseTimer = now;
    }
  }
}

int getDisplaySpeedTenths() {
  return speedTenths;
}

void setup() {
  Serial.begin(115200);

  settingsInit();
  initDisplay();
  initButtons();
  updateDisplay(statusMsg, getDisplaySpeedTenths(), walkPhase, intervalPair, 0);

  tx.enableTransmit(14);
  tx.setProtocol(1);
  tx.setPulseLength(425);
  tx.setRepeatTransmit(3);

  Serial.println("ready");
}

void loop() {
  unsigned long now = millis();

  rfProcess();

  if (menuIsActive()) {
    menuProcess(now);
    delay(10);
    return;
  }

  ButtonEvent evUp    = readButton(BTN_UP);
  ButtonEvent evDown  = readButton(BTN_DOWN);
  ButtonEvent evStart = readButton(BTN_START);

  if (evStart == LONG_PRESS && !walkActive && menuCanOpen(now)) {
    menuOpen();
  } else if (evStart == SHORT_PRESS && !walkActive) {
    walkActive = true;
    walkPhase = stopBeforeStart ? 1 : 2;
    phaseTimer = now;
    speedTenths = 0;
    strcpy(statusMsg, "Init");
    Serial.println("=== internal walking started ===");
  }

  if (evUp == SHORT_PRESS) {
    speedTenths++;
    Serial.println("speed up");
    tx.send(RF_UP, 24);
  }
  if (evDown == SHORT_PRESS) {
    if (speedTenths > 0) speedTenths--;
    Serial.println("speed down");
    tx.send(RF_DOWN, 24);
  }

  if (walkActive) runWalkSequence(now);

  updateDisplay(statusMsg, getDisplaySpeedTenths(), walkPhase, intervalPair, now);
  delay(10);
}

void runWalkSequence(unsigned long now) {
  switch (walkPhase) {
    case 1:
      tx.send(RF_STOP, 24);
      Serial.println("stop sent");
      strcpy(statusMsg, "Init");
      phaseTimer = now;
      walkPhase = 2;
      break;

    case 2:
      if (now - phaseTimer >= 20000) {
        tx.send(RF_START, 24);
        Serial.println("start sent");
        strcpy(statusMsg, "Starting");
        speedTenths = START_SPEED_TENTHS;
        phaseTimer = now;
        walkPhase = 3;
      }
      break;

    case 3:
      if (now - phaseTimer >= 10000) {
        int rampSignals = baseTenths - START_SPEED_TENTHS;
        if (rampSignals > 0) {
          rfStart(RF_UP, rampSignals, 200, 4);
          phaseTimer = now;
          speedTenths += rampSignals;
          intervalPair = 0;
          strcpy(statusMsg, "Ramping");
        } else {
          intervalPair = 0;
          speedTenths = baseTenths;
          strcpy(statusMsg, "Warm-up");
          phaseTimer = now;
          walkPhase = 4;
        }
      }
      break;

    case 4:
      if (now - phaseTimer >= PHASE_DURATION_MS) {
        rfStart(RF_UP, stepSizeSignals, 200, 5);
        phaseTimer = now;
        speedTenths += stepSizeSignals;
        Serial.println("warm-up done, entering fast interval");
        strcpy(statusMsg, "Fast");
      }
      break;

    case 5:
      if (now - phaseTimer >= PHASE_DURATION_MS) {
        rfStart(RF_DOWN, stepSizeSignals, 200, 6);
        phaseTimer = now;
        if (speedTenths >= stepSizeSignals)
          speedTenths -= stepSizeSignals;
        else
          speedTenths = 0;
        Serial.println("fast interval done, entering slow interval");
        strcpy(statusMsg, "Slow");
      }
      break;

    case 6:
      if (now - phaseTimer >= PHASE_DURATION_MS) {
        if (intervalPair < INTERVAL_PAIRS - 1) {
          rfStart(RF_UP, stepSizeSignals, 200, 5);
          phaseTimer = now;
          speedTenths += stepSizeSignals;
          intervalPair++;
          Serial.print("slow interval done, cycle ");
          Serial.print(intervalPair);
          Serial.println("/5 entering fast interval");
          strcpy(statusMsg, "Fast");
        } else {
          if (cooldownEnabled) {
            rfStart(RF_DOWN, 40, 200, 8);
            if (speedTenths >= 40)
              speedTenths -= 40;
            else
              speedTenths = 0;
          } else {
            speedTenths = 0;
          }
          strcpy(statusMsg, "Cooling");
          walkPhase = 7;
          phaseTimer = now;
          Serial.println("slow interval done, entering cooldown");
        }
      }
      break;

    case 7:
      // Display shows "Cooldown..." (via display.cpp walkPhase==7 check)
      // rfProcess sends the 40 RF_DOWN signals in loop()
      // When done, rfProcess sets walkPhase = rfNextPhase (8)
      if (!rfBusy) {
        walkPhase = 8;
        phaseTimer = now;
      }
      break;

    case 8:
      if (now - phaseTimer >= 3000) {
        Serial.println("=== internal walking done ===");
        walkActive = false;
        walkPhase = 0;
        speedTenths = 0;
        strcpy(statusMsg, "Ready");
      }
      break;
  }
}
