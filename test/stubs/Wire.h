#pragma once
#include "Arduino.h"

struct TwoWire {
  void begin(int, int) {}
  void setTimeOut(int) {}
  void beginTransmission(int) {}
  size_t write(int) { return 1; }
  int endTransmission() { return 0; }
};
extern TwoWire Wire;
