#include "buttons.h"

static const int PINS[] = {7, 5, 6};
static const unsigned long DEBOUNCE_MS = 30;
static const unsigned long LONG_PRESS_MS = 3000;

static int lastRaw[3];
static unsigned long lastTrans[3];
static bool active[3];
static unsigned long pressStart[3];
static bool longFired[3];
static unsigned long lastRepeat[3];

static const unsigned long REPEAT_DELAY_MS = 400;
static const unsigned long REPEAT_RATE_MS = 150;

void initButtons() {
  for (int i = 0; i < 3; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    lastRaw[i] = HIGH;
    lastTrans[i] = 0;
    active[i] = false;
    pressStart[i] = 0;
    lastRepeat[i] = 0;
    longFired[i] = false;
  }
}

bool isButtonPressed(ButtonIndex idx) {
  return active[idx];
}

ButtonEvent readButton(ButtonIndex idx) {
  int raw = digitalRead(PINS[idx]);
  unsigned long now = millis();

  if (raw != lastRaw[idx]) {
    // Pin transitioned – only accept if stable for the debounce period
    if (now - lastTrans[idx] >= DEBOUNCE_MS) {
      lastTrans[idx] = now;
      lastRaw[idx] = raw;

      if (raw == LOW) {
        // Pressed (active low)
        pressStart[idx] = now;
        lastRepeat[idx] = now;
        active[idx] = true;
        longFired[idx] = false;
      } else {
        // Released
        active[idx] = false;
        unsigned long duration = now - pressStart[idx];
        if (!longFired[idx]) {
          if (duration >= LONG_PRESS_MS) return LONG_PRESS;
          return SHORT_PRESS;
        }
      }
    }
    // Bounce rejected: don't update lastRaw – keeps detecting original edge
  }

  // Long-press while held (fires once)
  if (active[idx] && !longFired[idx] && now - pressStart[idx] >= LONG_PRESS_MS) {
    longFired[idx] = true;
    return LONG_PRESS;
  }

  return NONE;
}

// Like readButton, but keeps emitting REPEAT while the button is held
// (after REPEAT_DELAY_MS, every REPEAT_RATE_MS). Use for UP/DOWN so a held
// button ramps speed continuously. The initial press still comes through
// as SHORT_PRESS on release only if no repeat fired... it fires immediately,
// so callers should treat SHORT_PRESS + REPEAT uniformly.
ButtonEvent readButtonRepeat(ButtonIndex idx) {
  int raw = digitalRead(PINS[idx]);
  unsigned long now = millis();

  if (raw != lastRaw[idx]) {
    if (now - lastTrans[idx] >= DEBOUNCE_MS) {
      lastTrans[idx] = now;
      lastRaw[idx] = raw;

      if (raw == LOW) {
        pressStart[idx] = now;
        lastRepeat[idx] = now;
        active[idx] = true;
        longFired[idx] = false;
      } else {
        active[idx] = false;
        unsigned long duration = now - pressStart[idx];
        if (!longFired[idx] && duration < REPEAT_DELAY_MS) {
          return SHORT_PRESS;   // quick tap
        }
      }
    }
  }

  // Auto-repeat while held: first after REPEAT_DELAY_MS, then every REPEAT_RATE_MS
  if (active[idx] && !longFired[idx] && now - pressStart[idx] >= REPEAT_DELAY_MS) {
    if (now - lastRepeat[idx] >= REPEAT_RATE_MS) {
      lastRepeat[idx] = now;
      return REPEAT;
    }
  }

  return NONE;
}