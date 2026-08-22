// Host-side test wrapper: pulls the sketch in like the Arduino builder does
// (auto-generated prototypes) and exposes setup()/loop() to the tests.
#include "Arduino.h"
static void stopWalk();
void runWalkSequence(unsigned long now);
#include "../treadmill.ino"
