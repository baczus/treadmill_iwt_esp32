#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
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

  display.setCursor(31, 0);
  display.print("~~~ IWT ~~~");

  display.setCursor(0, 8);
  display.print(status);

  int barX = 50;
  int barY = 8;
  int barH = 8;

  int totalSec = 0;
  switch (walkPhase) {
    case 2: totalSec = 20; break;
    case 3: totalSec = 10; break;
    case 4: case 5: case 6: totalSec = 180; break;
    case 8: totalSec = 3; break;
  }

  if (totalSec > 0) {
    display.drawFastVLine(barX, barY, barH, SSD1306_WHITE);
    display.drawFastVLine(SCREEN_WIDTH - 1, barY, barH, SSD1306_WHITE);

    int elapsed = (now - phaseTimer) / 1000;
    if (elapsed > totalSec) elapsed = totalSec;
    int fillable = SCREEN_WIDTH - barX - 2;
    int filled = fillable * elapsed / totalSec;
    display.fillRect(barX + 1, barY, filled, barH, SSD1306_WHITE);
  }

  display.setCursor(0, 16);
  display.print("Speed: ");
  display.print(speedTenths / 10);
  display.print(".");
  display.print(speedTenths % 10);
  display.print(" km/h");

  if (walkPhase >= 5 && walkPhase <= 6) {
    display.setCursor(0, 24);
    display.print("Cycle: ");
    display.print(intervalPair + 1);
    display.print("/");
    display.print(5);
  } else if (walkPhase == 7) {
    display.setCursor(0, 24);
    display.print("Cooldown...");
  } else if (walkPhase == 8) {
    display.setCursor(0, 24);
    display.print("Complete!");
  }

  display.display();
}

void updateMenuDisplay(bool editMode, int selection, int stepVal, int baseVal, int stopVal, int coolVal) {
  if (!displayAvailable) return;

  display.clearDisplay();
  display.setCursor(22, 0);
  display.print("~~ Settings ~~");

  struct Item { const char* name; int val; int fmt; };
  Item items[4] = {{"Step", stepVal, 0}, {"Base", baseVal, 0}, {"Stop", stopVal, 1}, {"Cool", coolVal, 1}};

  // Scroll window: show 3 items centered on selection
  int startIdx = selection - 1;
  if (startIdx < 0) startIdx = 0;
  if (startIdx > 1) startIdx = 1;  // max start index for 4 items with 3 visible

  for (int i = 0; i < 3; i++) {
    int idx = startIdx + i;
    if (idx >= 4) break;

    display.setCursor(0, 8 + i * 8);

    if (idx == selection) {
      display.print(editMode ? "*" : ">");
    } else {
      display.print(" ");
    }

    display.print(items[idx].name);
    display.print(": ");
    if (items[idx].fmt == 0) {
      display.print(items[idx].val / 10);
      display.print(".");
      display.print(items[idx].val % 10);
      display.print(" km/h");
    } else {
      display.print(items[idx].val ? "On " : "Off");
    }

    if (idx == selection && editMode) {
      display.print("*");
    }
  }

  // Scroll indicators
  if (startIdx > 0) {
    display.setCursor(120, 8);
    display.print("^");
  }
  if (startIdx < 1) {
    display.setCursor(120, 24);
    display.print("v");
  }

  display.setCursor(0, 24);
  if (editMode) {
    display.print("[OK=save]");
  } else {
    display.print("[OK] [Hold=Exit]");
  }

  display.display();
}
