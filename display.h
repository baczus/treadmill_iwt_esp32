#ifndef DISPLAY_H
#define DISPLAY_H

extern bool displayAvailable;

void initDisplay();
void updateDisplay(const char* status, int speedTenths, int seqPhase, int seqCycle);

#endif
