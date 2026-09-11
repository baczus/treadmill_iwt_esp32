#include "Wire.h"
#include "Adafruit_SSD1306.h"
TwoWire Wire;

static int g_nack_remaining = 0;
static bool g_down = false;

int TwoWire::endTransmission() {
  if (g_down) return 1;  // panel absent / bus stuck: permanent NACK
  if (g_nack_remaining > 0) { --g_nack_remaining; return 1; }
  return 0;
}

void wire_fail_next(int n) { g_nack_remaining = n; }
void wire_set_down(bool down) { g_down = down; g_nack_remaining = 0; }

static int g_begin_failures = 0;
bool Adafruit_SSD1306::begin(int, int) {
  if (g_begin_failures > 0) { --g_begin_failures; return false; }
  return true;
}
void ssd1306_fail_next(int n) { g_begin_failures = n; }
