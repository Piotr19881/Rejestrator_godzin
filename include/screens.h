#ifndef SCREENS_H
#define SCREENS_H

#include <Arduino.h>

// Funkcje inicjalizacji i zarządzania ekranami
void initScreens();
void updateWaitingScreen();
void displayMotivationalText();
void drawProgressIndicator();
void forceScreenUpdate();
void resetTextCycle();
void stopScreenUpdates();
bool isScreenTouched(uint16_t x, uint16_t y);

#endif
