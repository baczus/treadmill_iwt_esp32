#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

extern int stepSizeSignals;
extern int baseTenths;
extern int stopBeforeStart;
extern int cooldownEnabled;
extern int phaseDurationMinutes;

void settingsInit();
void settingsSave();
bool menuIsActive();
bool menuCanOpen(unsigned long now);
void menuOpen();
void menuProcess(unsigned long now);

#endif
