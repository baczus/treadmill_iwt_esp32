#pragma once
#include "Arduino.h"
#include "Adafruit_GFX.h"
#include "Wire.h"

struct Adafruit_SSD1306 : Adafruit_GFX {
  Adafruit_SSD1306(int w, int h, TwoWire*, int) : Adafruit_GFX(w, h) {}
  bool begin(int, int) { return true; }
  void clearDisplay() {}
  void drawRect(int, int, int, int, int) {}
  void fillRect(int, int, int, int, int) {}
  void display() {}
};
