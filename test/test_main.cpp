// Host-side logic tests for the treadmill sketch (no hardware needed).
#include "Arduino.h"
#include "RCSwitch.h"
#include <cstdio>
#include <string>

extern std::vector<RFEvent> rf_log;
#include "buttons.h"
#include "settings.h"

void setup();
void loop();
int getDisplaySpeedTenths();
bool menuIsActive();

static int failures = 0;
static int checks = 0;
#define CHECK(cond) do { \
  ++checks; \
  if (!(cond)) { ++failures; printf("  FAIL line %d: %s\n", __LINE__, #cond); } \
} while (0)

static const unsigned long RF_UP    = 16776972UL;
static const unsigned long RF_DOWN  = 16776970UL;
static const unsigned long RF_STOP  = 16776971UL;
static const unsigned long RF_START = 16776974UL;

static int count_code(unsigned long code) {
  int n = 0;
  for (auto& e : rf_log) if (e.code == code) ++n;
  return n;
}

// Simulate a quick tap on a button through loop()
static void tap(int idx) {
  set_button(idx, true);
  for (int i = 0; i < 5; i++) { advance_ms(10); loop(); }
  set_button(idx, false);
  for (int i = 0; i < 5; i++) { advance_ms(10); loop(); }
}

static void hold(int idx, unsigned long ms) {
  set_button(idx, true);
  unsigned long end = millis() + ms;
  while (millis() < end) { advance_ms(10); loop(); }
  set_button(idx, false);
  for (int i = 0; i < 5; i++) { advance_ms(10); loop(); }
}

extern bool walkActive;

// Advance simulating loop() every 10ms until predicate or timeout
template <typename P>
static bool run_until(P pred, unsigned long max_ms) {
  unsigned long end = millis() + max_ms;
  while (!pred()) {
    if (millis() > end) return false;
    advance_ms(10);
    loop();
  }
  return true;
}

// ---- test cases -----------------------------------------------------------

static void test_manual_buttons_send_rf() {
  reset_fakes();
  setup();

  CHECK(rf_log.empty());
  tap(0); // UP
  CHECK(count_code(RF_UP) == 1);
  CHECK(getDisplaySpeedTenths() == 1);

  // DOWN below the 1.0 km/h floor still transmits but keeps display at 1
  int downsBefore = count_code(RF_DOWN);
  tap(1);
  CHECK(count_code(RF_DOWN) == downsBefore + 1);
  CHECK(getDisplaySpeedTenths() == 1);
  tap(1);
  CHECK(count_code(RF_DOWN) == downsBefore + 2);
  CHECK(getDisplaySpeedTenths() == 1);
}

static void test_button_debounce_and_long_press() {
  reset_fakes();
  initButtons();

  // Bounce within debounce window: only one press
  set_button(BTN_UP, true);
  advance_ms(5);  readButton(BTN_UP);
  set_button(BTN_UP, false);
  advance_ms(5);  readButton(BTN_UP);
  set_button(BTN_UP, true);
  advance_ms(40); 
  CHECK(readButton(BTN_UP) == NONE);            // still held, no repeat from plain readButton
  set_button(BTN_UP, false);
  advance_ms(40);
  CHECK(readButton(BTN_UP) == SHORT_PRESS);     // released quickly -> short press
  CHECK(readButton(BTN_UP) == NONE);

  // Long press fires once while held (poll like the real loop does)
  set_button(BTN_START, true);
  ButtonEvent seen = NONE;
  for (int i = 0; i < 320 && seen == NONE; i++) {
    advance_ms(10);
    ButtonEvent ev = readButton(BTN_START);
    if (ev != NONE) seen = ev;
  }
  CHECK(seen == LONG_PRESS);
  CHECK(readButton(BTN_START) == NONE);
  set_button(BTN_START, false);
  advance_ms(40); readButton(BTN_START);
  CHECK(readButton(BTN_START) == NONE);         // no duplicate short-press after long
}

static void test_hold_repeat() {
  reset_fakes();
  initButtons();

  set_button(BTN_UP, true);
  int repeats = 0;
  for (int i = 0; i < 500; i++) {               // up to 5s held
    advance_ms(10);
    if (readButtonRepeat(BTN_UP) == REPEAT) ++repeats;
  }
  // first repeat ~400ms, then every 200ms -> about (5000-400)/200 + 1 = 24
  CHECK(repeats > 18 && repeats < 30);
  set_button(BTN_UP, false);
  advance_ms(40); readButtonRepeat(BTN_UP);
  CHECK(readButtonRepeat(BTN_UP) == NONE);      // no short-press after repeats
}

static void test_full_walk_sequence() {
  reset_fakes();
  setup();

  const int baseTenths_ = 40;   // defaults (NVS stubbed to defaults)
  const int step_ = 35;

  tap(2); // START -> begin walk

  // Phase 1: STOP sent immediately
  CHECK(count_code(RF_STOP) == 1);

  // Phase 2: START after 20s
  bool ok = run_until([]{ return count_code(RF_START) == 1; }, 25000);
  CHECK(ok);

  // Phase 3: ramp 1.0 -> base after 10s: base-10 UP signals at 200ms spacing
  ok = run_until([&]{ return count_code(RF_UP) >= baseTenths_ - 10; }, 20000);
  CHECK(ok);
  CHECK(getDisplaySpeedTenths() == baseTenths_);

  // Warm-up hold: 3 min without any RF, then first fast ramp (+35)
  run_until([&]{ return count_code(RF_UP) >= baseTenths_ - 10 + step_; },
            3 * 60000UL + 30000UL);
  CHECK(count_code(RF_UP) == baseTenths_ - 10 + step_);

  // Full interval cycles: 4 more fast->slow transitions after the first pair
  // Total expected: UP: (base-10) + step*5, DOWN: step*5 ... plus cooldown 40 DOWN
  ok = run_until([]{ return !walkActive; }, 40 * 60000UL);
  CHECK(ok);

  int up_total   = count_code(RF_UP);
  int down_total = count_code(RF_DOWN);
  printf("  up=%d down=%d\n", up_total, down_total);
  CHECK(up_total == (baseTenths_ - 10) + step_ * 5);
  CHECK(down_total == step_ * 5 + 40);
  CHECK(getDisplaySpeedTenths() == 0);
}

static void test_stop_mid_walk_sends_stop_and_resets() {
  reset_fakes();
  setup();

  tap(2);                                        // start walk
  run_until([]{ return count_code(RF_START) == 1; }, 25000);
  advance_ms(12000);
  loop();                                        // into ramp/warmup area

  tap(2);                                        // second START press = stop request
  bool stopped = run_until([]{ return getDisplaySpeedTenths() == 0; }, 5000);
  CHECK(stopped);
  CHECK(count_code(RF_STOP) >= 2);               // initial + stop

  // Restarting works cleanly
  tap(2);
  CHECK(count_code(RF_STOP) >= 3);
}

static void test_menu_opens_on_start_long_press() {
  reset_fakes();
  setup();

  hold(2, 3100);
  CHECK(menuIsActive());

  hold(2, 3100);   // long-press inside menu: save & exit
  CHECK(!menuIsActive());
}

int main() {
  // Menu test must run while idle; later tests leave walks running/stopped.
  test_menu_opens_on_start_long_press();
  test_manual_buttons_send_rf();
  test_button_debounce_and_long_press();
  test_hold_repeat();
  test_full_walk_sequence();
  test_stop_mid_walk_sends_stop_and_resets();

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
