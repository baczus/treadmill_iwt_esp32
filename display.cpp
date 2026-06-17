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

void updateDisplay(const char* status, int speedTenths, int seqPhase, int seqCycle, unsigned long now) {
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
  switch (seqPhase) {
    case 2: totalSec = 20; break;
    case 3: totalSec = 10; break;
    case 4: case 5: case 6: totalSec = 180; break;
    case 8: totalSec = 3; break;
  }

  if (totalSec > 0) {
    display.drawFastVLine(barX, barY, barH, SSD1306_WHITE);
    display.drawFastVLine(SCREEN_WIDTH - 1, barY, barH, SSD1306_WHITE);

    int elapsed = (now - seqTimer) / 1000;
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

  if (seqPhase >= 5 && seqPhase <= 6) {
    display.setCursor(0, 24);
    display.print("Cycle: ");
    display.print(seqCycle + 1);
    display.print("/");
    display.print(5);
  } else if (seqPhase == 7) {
    display.setCursor(0, 24);
    display.print("Cooldown...");
  } else if (seqPhase == 8) {
    display.setCursor(0, 24);
    display.print("Complete!");
  }

  display.display();
}
