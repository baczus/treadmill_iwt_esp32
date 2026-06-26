#include "buttons.h"

static const int PIN_BTN_DOWN  = 26;
static const int PIN_BTN_UP    = 33;
static const int PIN_BTN_START = 25;

Button2 btnUp(PIN_BTN_UP, INPUT_PULLUP, true);
Button2 btnDown(PIN_BTN_DOWN, INPUT_PULLUP, true);
Button2 btnStart(PIN_BTN_START, INPUT_PULLUP, true);

void initButtons() {
  btnUp.setDebounceTime(30);
  btnUp.setLongClickTime(3000);

  btnDown.setDebounceTime(30);
  btnDown.setLongClickTime(3000);

  btnStart.setDebounceTime(30);
  btnStart.setLongClickTime(3000);
}

bool isButtonPressed(const Button2& b) {
  return b.isPressed();
}

ButtonEvent readButton(Button2& b) {
  b.loop();

  clickType ct = b.read();

  if (ct == single_click) {
    return SHORT_PRESS;
  }
  if (ct == long_click) {
    return LONG_PRESS;
  }
  return NONE;
}
