#ifndef DISPLAY_H
#define DISPLAY_H

extern bool displayAvailable;
extern unsigned long phaseTimer;

void initDisplay();
void updateDisplay(const char* status, int speedTenths, int walkPhase, int intervalPair, unsigned long now);
void updateMenuDisplay(bool editMode, int selection, int stepVal, int baseVal, int stopVal, int coolVal);

#endif
