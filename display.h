#ifndef DISPLAY_H
#define DISPLAY_H

extern bool displayAvailable;
extern unsigned long seqTimer;

void initDisplay();
void updateDisplay(const char* status, int speedTenths, int seqPhase, int seqCycle, unsigned long now);

#endif
