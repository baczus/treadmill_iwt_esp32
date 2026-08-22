#include "Arduino.h"
#include "RCSwitch.h"
#include <vector>

SerialStub Serial;
std::vector<RFEvent> rf_log;

static unsigned long fake_ms = 0;
static int pin_state[3] = {HIGH, HIGH, HIGH};   // matches PINS[] order {7,5,6} = UP,DOWN,START

unsigned long millis() { return fake_ms; }
void delay(unsigned long ms) { fake_ms += ms; }
void pinMode(int, int) {}
int digitalRead(int pin) {
  // pins 7,5,6 -> index 0,1,2
  int idx = (pin == 7) ? 0 : (pin == 5) ? 1 : 2;
  return pin_state[idx];
}

void advance_ms(unsigned long ms) { fake_ms += ms; }
void set_button(int idx, bool pressed) { pin_state[idx] = pressed ? LOW : HIGH; }

void reset_fakes() {
  fake_ms = 0;
  for (auto& p : pin_state) p = HIGH;
  rf_log.clear();
}
