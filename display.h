#ifndef DISPLAY_H
#define DISPLAY_H

extern bool displayAvailable;

void initDisplay();
void displayPollInit(unsigned long now);
void updateDisplay(const char* status, int speedTenths, int walkPhase, int intervalPair,
                   unsigned long now, unsigned long phaseStart, int totalSec);
void updateMenuDisplay(bool editMode, int selection, int stepVal, int baseVal, int stopVal, int coolVal, int phaseMin);

#endif
