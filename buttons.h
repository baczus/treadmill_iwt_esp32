#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include <Button2.h>

enum ButtonEvent { NONE, SHORT_PRESS, LONG_PRESS };

extern Button2 btnUp;
extern Button2 btnDown;
extern Button2 btnStart;

void initButtons();
ButtonEvent readButton(Button2& b);
bool isButtonPressed(const Button2& b);

#endif
