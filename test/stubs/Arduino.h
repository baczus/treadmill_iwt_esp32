// Host-side Arduino stub for running treadmill logic tests on PC.
#pragma once
#include <cstdint>
#include <cstddef>

#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
#define SSD1306_SWITCHCAPVCC 2
#define SSD1306_WHITE 1

unsigned long millis();
void delay(unsigned long);
void pinMode(int, int);
int digitalRead(int);

// Test hooks implemented in fake.cpp
void advance_ms(unsigned long ms);
void set_button(int index, bool pressed);   // index: 0=UP,1=DOWN,2=START
void reset_fakes();

struct SerialStub {
  void begin(unsigned long) {}
  template <typename T> void print(T) {}
  template <typename T> void println(T) {}
  operator bool() const { return true; }
};
extern SerialStub Serial;
