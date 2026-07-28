#include <Preferences.h>
#include "settings.h"
#include "display.h"
#include "buttons.h"

int stepSizeSignals = 35;
int baseTenths = 40;
int startMode = 0;
int cooldownEnabled = 1;
int phaseDurationMinutes = 3;

static Preferences prefs;

static bool menuActive = false;
static bool menuEditMode = false;
static int menuSelection = 0;

static const int MENU_ITEM_COUNT = 5;
static const char* const MENU_NAMES[MENU_ITEM_COUNT] = {"Step", "Base", "Mode", "Cool", "Time"};
static int* const MENU_VALUES[MENU_ITEM_COUNT] = {&stepSizeSignals, &baseTenths, &startMode, &cooldownEnabled, &phaseDurationMinutes};
static const int MENU_MIN[MENU_ITEM_COUNT] = {1, 10, 0, 0, 1};
static const int MENU_MAX[MENU_ITEM_COUNT] = {80, 100, 2, 1, 10};
static const int MENU_STEP[MENU_ITEM_COUNT] = {1, 1, 1, 1, 1};

static unsigned long menuOpenTime = 0;
static unsigned long menuExitTime = 0;

bool menuCanOpen(unsigned long now) {
  return now - menuExitTime > 1000;
}

void settingsInit() {
  prefs.begin("treadmill", true);
  stepSizeSignals     = prefs.getInt("stepSig", 35);
  baseTenths          = prefs.getInt("baseSpd", 40);
  startMode           = prefs.getInt("startM", 0);
  cooldownEnabled     = prefs.getInt("coolEn", 1);
  phaseDurationMinutes = prefs.getInt("phaseMin", 3);
  prefs.end();
}

void settingsSave() {
  prefs.begin("treadmill", false);
  prefs.putInt("stepSig", stepSizeSignals);
  prefs.putInt("baseSpd", baseTenths);
  prefs.putInt("startM", startMode);
  prefs.putInt("coolEn", cooldownEnabled);
  prefs.putInt("phaseMin", phaseDurationMinutes);
  prefs.end();
}

bool menuIsActive() {
  return menuActive;
}

void menuOpen() {
  menuActive = true;
  menuEditMode = false;
  menuSelection = 0;
  menuOpenTime = millis();
}

static void menuBrowseUp() {
  menuSelection = (menuSelection + 1) % MENU_ITEM_COUNT;
}

static void menuBrowseDown() {
  menuSelection = (menuSelection - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
}

static void menuEditUp() {
  int* v = MENU_VALUES[menuSelection];
  if (MENU_MIN[menuSelection] == 0 && MENU_MAX[menuSelection] == 1) {
    *v = (*v == 0) ? 1 : 0;
  } else {
    *v += MENU_STEP[menuSelection];
    if (*v > MENU_MAX[menuSelection]) *v = MENU_MAX[menuSelection];
  }
}

static void menuEditDown() {
  int* v = MENU_VALUES[menuSelection];
  if (MENU_MIN[menuSelection] == 0 && MENU_MAX[menuSelection] == 1) {
    *v = (*v == 0) ? 1 : 0;
  } else {
    *v -= MENU_STEP[menuSelection];
    if (*v < MENU_MIN[menuSelection]) *v = MENU_MIN[menuSelection];
  }
}

void menuProcess(unsigned long now) {
  if (!menuActive) return;

  ButtonEvent evUp    = readButton(BTN_UP);
  ButtonEvent evDown  = readButton(BTN_DOWN);
  ButtonEvent evStart = readButton(BTN_START);

  if (evStart == LONG_PRESS) {
    settingsSave();
    menuActive = false;
    menuExitTime = now;
    return;
  }

  if (evStart == SHORT_PRESS && now - menuOpenTime > 500) {
    if (menuEditMode) {
      menuEditMode = false;
      settingsSave();
    } else {
      menuEditMode = true;
    }
  } else if (menuEditMode) {
    if (evUp == SHORT_PRESS) menuEditUp();
    if (evDown == SHORT_PRESS) menuEditDown();
  } else if (evUp == SHORT_PRESS || evDown == SHORT_PRESS) {
    if (evUp == SHORT_PRESS) menuBrowseUp();
    if (evDown == SHORT_PRESS) menuBrowseDown();
  }

  updateMenuDisplay(menuEditMode, menuSelection, stepSizeSignals, baseTenths, startMode, cooldownEnabled, phaseDurationMinutes);
}
