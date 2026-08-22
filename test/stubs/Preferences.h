#pragma once
#include "Arduino.h"

struct Preferences {
  void begin(const char*, bool) {}
  void end() {}
  int getInt(const char* key, int def) { return def; }
  void putInt(const char*, int) {}
};
