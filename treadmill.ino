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

int walkPhase = 0;
int intervalPair = 0;
unsigned long phaseTimer = 0;
int phaseTotalSec = 0;
bool walkActive = false;
bool stopRequested = false;
int speedTenths = 0;

char statusMsg[16] = "Ready";

// Non-blocking RF sender. Each call queues one command (one transmission per
// signal); rfProcess() emits them from loop() spaced by `interval` ms.
// rfLastTime is stamped AFTER tx.send() returns so the gap between consecutive
// bursts is exactly `interval` (matching backup/v1). Stamping it before the
// send shortened the gap and made the treadmill over-count speed steps.
static unsigned long rfCode;
static int rfCount;
static int rfInterval;
static unsigned long rfLastTime;
static bool rfBusy = false;
static int rfNextPhase;
static int rfDelta;   // speedTenths change applied per transmitted signal (+1 up, -1 down)
static int rfTotal;   // total signals queued for the active ramp

// Progress of the active RF ramp, expressed as signals sent (0..100), so the
// progress bar tracks the real ramp instead of an assumed wall-clock duration.
int rampProgress = 0;
int rampActive = 0;

static void rfStart(unsigned long code, int count, int interval, int nextPhase) {
  tx.setPulseLength(425);
  rfCode = code;
  rfCount = count;
  rfInterval = interval;
  rfLastTime = millis();
  rfBusy = true;
  rfNextPhase = nextPhase;
  rfDelta = (code == RF_UP) ? 1 : (code == RF_DOWN) ? -1 : 0;
  rfTotal = count;
  rampActive = 1;
  rampProgress = 0;
}

static void rfProcess(unsigned long now) {
  if (!rfBusy) return;
  if (stopRequested) {
    rfBusy = false;
    rampActive = 0;
    stopWalk();
    return;
  }
  if (now - rfLastTime >= (unsigned long)rfInterval) {
    tx.send(rfCode, 24);
    rfCount--;
    // Advance the displayed speed one step per actual transmitted signal so the
    // number stays in lockstep with the progress bar and the treadmill.
    speedTenths += rfDelta;
    if (speedTenths < 0) speedTenths = 0;
    rfLastTime = millis();
    rampProgress = (rfTotal - rfCount) * 100 / rfTotal;
    if (rfCount <= 0) {
      rfBusy = false;
      rampActive = 0;
      walkPhase = rfNextPhase;
      // Use the loop's `now` (already captured before rfProcess) so phaseTimer
      // is never newer than the `now` runWalkSequence compares against — an
      // unsigned underflow there would make every timed wait fire instantly.
      phaseTimer = now;
    }
  }
}

// Abort the walking sequence: stop the treadmill and reset all state.
static void stopWalk() {
  if (!walkActive && !stopRequested) return;
  tx.setPulseLength(422);
  tx.send(RF_STOP, 24);
  Serial.println("=== walking stopped by user ===");
  walkActive = false;
  walkPhase = 0;
  intervalPair = 0;
  speedTenths = 0;
  stopRequested = false;
  strcpy(statusMsg, "Ready");
  phaseTimer = millis();
  phaseTotalSec = 0;
}

int getDisplaySpeedTenths() {
  return speedTenths;
}

// Test mode: send a RF_DOWN signal every 5 seconds to verify the transmitter.
// Uncomment the line below to enable, comment out to disable.
// #define TEST_SPEED_DOWN

#ifdef TEST_SPEED_DOWN
static unsigned long testDownTimer = 0;
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("boot");
#ifdef TEST_SPEED_DOWN
  while (!Serial) delay(50);
#endif

  settingsInit();
  initDisplay();
  initButtons();
  updateDisplay(statusMsg, getDisplaySpeedTenths(), walkPhase, intervalPair, 0);

  tx.enableTransmit(1);
  tx.setProtocol(1);
  tx.setPulseLength(425);
  tx.setRepeatTransmit(3);

  Serial.println("ready");
}

void loop() {
  unsigned long now = millis();

#ifdef TEST_SPEED_DOWN
  static unsigned long hb = 0;
  if (now - hb >= 1000) {
    Serial.println("loop alive");
    hb = now;
  }
#endif

  rfProcess(now);

#ifdef TEST_SPEED_DOWN
  if (now - testDownTimer >= 5000) {
    tx.setPulseLength(425);
    tx.send(RF_DOWN, 24);
    Serial.println("TEST: speed down");
    testDownTimer = now;
  }
#endif

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
  } else if (evStart == SHORT_PRESS) {
    if (!walkActive) {
      walkActive = true;
      walkPhase = stopBeforeStart ? 1 : 2;
      phaseTimer = now;
      speedTenths = 0;
      strcpy(statusMsg, "Init");
      Serial.println("=== internal walking started ===");
    } else {
      stopRequested = true;
    }
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
  if (stopRequested) {
    stopWalk();
    return;
  }
  switch (walkPhase) {
    case 1:
      tx.setPulseLength(422);
      tx.send(RF_STOP, 24);
      Serial.println("stop sent");
      strcpy(statusMsg, "Init");
      phaseTimer = now;
      walkPhase = 2;
      phaseTotalSec = 20;
      break;

    case 2:
      if (now - phaseTimer >= 20000) {
        tx.setPulseLength(424);
        tx.send(RF_START, 24);
        Serial.println("start sent");
        strcpy(statusMsg, "Starting");
        speedTenths = START_SPEED_TENTHS;
        phaseTimer = now;
        walkPhase = 3;
        phaseTotalSec = 10;
      }
      break;

    case 3:
      if (now - phaseTimer >= 10000 && !rfBusy) {
        int rampSignals = baseTenths - START_SPEED_TENTHS;
        if (rampSignals > 0) {
          strcpy(statusMsg, "Ramping");
          phaseTimer = now;
          phaseTotalSec = rampSignals * 200 / 1000;
          if (phaseTotalSec < 1) phaseTotalSec = 1;
          rfStart(RF_UP, rampSignals, 200, 4);
          intervalPair = 0;
        } else {
          intervalPair = 0;
          speedTenths = baseTenths;
          strcpy(statusMsg, "Warm-up");
          phaseTimer = now;
          walkPhase = 4;
          phaseTotalSec = phaseDurationMinutes * 60;
        }
      }
      break;

    case 4:
      if (!rfBusy) {
        phaseTotalSec = phaseDurationMinutes * 60;
        strcpy(statusMsg, "Warm-up");
        if (now - phaseTimer >= (phaseDurationMinutes * 60000UL)) {
          rfStart(RF_UP, stepSizeSignals, 200, 5);
          phaseTimer = now;
          phaseTotalSec = stepSizeSignals * 200 / 1000;
          if (phaseTotalSec < 1) phaseTotalSec = 1;
          Serial.println("warm-up done, entering fast interval");
          strcpy(statusMsg, "Speeding");
        }
      }
      break;

    case 5:
      if (!rfBusy) {
        phaseTotalSec = phaseDurationMinutes * 60;
        strcpy(statusMsg, "Fast");
        if (now - phaseTimer >= (phaseDurationMinutes * 60000UL)) {
          rfStart(RF_DOWN, stepSizeSignals, 200, 6);
          phaseTimer = now;
          phaseTotalSec = stepSizeSignals * 200 / 1000;
          if (phaseTotalSec < 1) phaseTotalSec = 1;
          Serial.println("fast interval done, entering slow interval");
          strcpy(statusMsg, "Slowing");
        }
      }
      break;

    case 6:
      if (!rfBusy) {
        phaseTotalSec = phaseDurationMinutes * 60;
        strcpy(statusMsg, "Slow");
      }
      if (!rfBusy && now - phaseTimer >= (phaseDurationMinutes * 60000UL)) {
        if (intervalPair < INTERVAL_PAIRS - 1) {
          rfStart(RF_UP, stepSizeSignals, 200, 5);
          phaseTimer = now;
          phaseTotalSec = stepSizeSignals * 200 / 1000;
          if (phaseTotalSec < 1) phaseTotalSec = 1;
          intervalPair++;
          Serial.print("slow interval done, cycle ");
          Serial.print(intervalPair);
          Serial.println("/5 entering fast interval");
          strcpy(statusMsg, "Speeding");
        } else {
          if (cooldownEnabled) {
            rfStart(RF_DOWN, 40, 200, 8);
            phaseTimer = now;
          } else {
            speedTenths = 0;
          }
          Serial.println("slow interval done, entering cooldown");
          strcpy(statusMsg, "Cooling");
          walkPhase = 7;
          phaseTimer = now;
          phaseTotalSec = 0;
        }
      }
      break;

    case 7:
      phaseTotalSec = 0;
      // rfProcess emits the cooldown signals, then sets walkPhase = 8
      if (!rfBusy) {
        walkPhase = 8;
        phaseTimer = now;
        phaseTotalSec = 3;
      }
      break;

    case 8:
      phaseTotalSec = 3;
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
