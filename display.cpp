#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

void updateDisplay(const char* status, int speedTenths, int seqPhase, int seqCycle) {
  if (!displayAvailable) return;

  display.clearDisplay();

  display.setCursor(32, 0);
  display.print("TREADMILL");

  display.setCursor(0, 8);
  display.print(status);

  if (seqPhase >= 6 && seqPhase <= 7) {
    display.setCursor(78, 8);
    display.print("C ");
    display.print(seqCycle + 1);
    display.print("/");
    display.print(5);
  }

  display.setCursor(0, 16);
  display.print("Speed: ");
  display.print(speedTenths / 10);
  display.print(".");
  display.print(speedTenths % 10);
  display.print(" km/h");

  if (seqPhase >= 6 && seqPhase <= 7) {
    display.setCursor(0, 24);
    display.print("Cycle: ");
    display.print(seqCycle + 1);
    display.print("/");
    display.print(5);
  } else if (seqPhase == 8) {
    display.setCursor(0, 24);
    display.print("Cooldown...");
  } else if (seqPhase == 9) {
    display.setCursor(0, 24);
    display.print("Complete!");
  }

  display.display();
}
