#pragma once
#include "Arduino.h"

struct Adafruit_GFX {
  Adafruit_GFX(int, int) {}
  void setRotation(int) {}
  void setTextSize(int) {}
  void setTextColor(int) {}
  void setCursor(int, int) {}
  void print(const char*) {}
  void print(int) {}
};
