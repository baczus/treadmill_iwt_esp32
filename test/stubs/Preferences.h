#pragma once
#include "Arduino.h"

struct Preferences {
  void begin(const char*, bool) {}
  void end() {}
  int getInt(const char* key, int def) { return def; }
  bool isKey(const char*) { return false; }
  void putInt(const char*, int) {}
};
