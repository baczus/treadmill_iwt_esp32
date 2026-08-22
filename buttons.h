#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

enum ButtonEvent { NONE, SHORT_PRESS, LONG_PRESS, REPEAT };
enum ButtonIndex { BTN_UP, BTN_DOWN, BTN_START };

void initButtons();
ButtonEvent readButton(ButtonIndex idx);
ButtonEvent readButtonRepeat(ButtonIndex idx);
bool isButtonPressed(ButtonIndex idx);

#endif
