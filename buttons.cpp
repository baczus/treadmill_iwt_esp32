#include "buttons.h"

static const int PINS[] = {33, 26, 25};
static const unsigned long DEBOUNCE_MS = 30;
static const unsigned long LONG_PRESS_MS = 3000;

static int lastRaw[3];
static unsigned long lastTrans[3];
static bool active[3];
static unsigned long pressStart[3];
static bool longFired[3];

void initButtons() {
  for (int i = 0; i < 3; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    lastRaw[i] = HIGH;
    lastTrans[i] = 0;
    active[i] = false;
    pressStart[i] = 0;
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
