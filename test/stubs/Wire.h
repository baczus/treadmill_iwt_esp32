#pragma once
#include "Arduino.h"

struct TwoWire {
  void begin(int, int) {}
  void setTimeOut(int) {}
  void beginTransmission(int) {}
  size_t write(int) { return 1; }
  int endTransmission();
};
extern TwoWire Wire;

// Fault injection for display boot tests: next N transmissions NACK.
void wire_fail_next(int n);
void wire_set_down(bool down);
