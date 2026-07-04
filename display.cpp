#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display.h"
#include "settings.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayAvailable = false;

void initDisplay() {
  Wire.begin(21, 22);
  delay(100);

  Wire.beginTransmission(OLED_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("OLED not found");
    return;
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED init failed");
    return;
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  displayAvailable = true;
}

void updateDisplay(const char* status, int speedTenths, int walkPhase, int intervalPair, unsigned long now) {
  if (!displayAvailable) return;

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(31, 0);
  display.print("~~~ IWT ~~~");

  int totalSec = phaseTotalSec;
  if (totalSec == 0) {
    switch (walkPhase) {
      case 2: totalSec = 20; break;
      case 8: totalSec = 3; break;
    }
  }

  if (totalSec > 0) {
    int elapsed = (now - phaseTimer) / 1000;
    if (elapsed > totalSec) elapsed = totalSec;
    int filled = (SCREEN_WIDTH - 2) * elapsed / totalSec;
    display.drawRect(0, 10, SCREEN_WIDTH, 5, SSD1306_WHITE);
    display.fillRect(1, 11, filled, 3, SSD1306_WHITE);
  }

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(status);

  display.setCursor(0, 36);
  display.print(speedTenths / 10);
  display.print(".");
  display.print(speedTenths % 10);
  display.print(" km/h");

  display.setTextSize(1);
  if (walkPhase >= 5 && walkPhase <= 6) {
    display.setCursor(0, 56);
    display.print("Cycle: ");
    display.print(intervalPair + 1);
    display.print("/");
    display.print(5);
  } else if (walkPhase == 7) {
    display.setCursor(0, 56);
    display.print("Cooldown...");
  } else if (walkPhase == 8) {
    display.setCursor(0, 56);
    display.print("Complete!");
  }

  display.display();
}

void updateMenuDisplay(bool editMode, int selection, int stepVal, int baseVal, int stopVal, int coolVal, int phaseMin) {
  if (!displayAvailable) return;

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(22, 0);
  display.print("~~ Settings ~~");

  struct Item { const char* name; int val; int fmt; };
  Item items[5] = {{"Step", stepVal, 0}, {"Base", baseVal, 0}, {"Stop", stopVal, 1}, {"Cool", coolVal, 1}, {"Time", phaseMin, 2}};

  int page = selection / 2;
  int startIdx = page * 2;

  display.setTextSize(2);
  for (int i = 0; i < 2 && startIdx + i < 5; i++) {
    int idx = startIdx + i;
    int y = 12 + i * 18;
    display.setCursor(0, y);

    if (idx == selection) {
      display.print(editMode ? "*" : ">");
    } else {
      display.print(" ");
    }

    display.print(items[idx].name);
    display.print(" ");
    if (items[idx].fmt == 0) {
      display.print(items[idx].val / 10);
      display.print(".");
      display.print(items[idx].val % 10);
    } else if (items[idx].fmt == 1) {
      display.print(items[idx].val ? "On" : "Off");
    } else {
      display.print(items[idx].val);
      display.print("m");
    }

    if (idx == selection && editMode) {
      display.print("*");
    }
  }

  display.setTextSize(1);
  display.setCursor(0, 50);
  if (editMode) {
    display.print("[OK=save]");
  } else {
    display.print("[OK] [Hold=Exit]");
  }

  display.display();
}
