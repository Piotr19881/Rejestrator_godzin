#ifndef SCREENS_H
#define SCREENS_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Funkcje inicjalizacji i zarządzania ekranami
void initScreens();
void updateWaitingScreen();
void displayMotivationalText();
void drawProgressIndicator();
void forceScreenUpdate();
void resetTextCycle();
void stopScreenUpdates();
bool isScreenTouched(uint16_t x, uint16_t y);

// Nowe funkcje ekranów
void showLoadingScreen(TFT_eSPI& display, String message);
void showWelcomeScreen(TFT_eSPI& display, String userName);
void showErrorScreen(TFT_eSPI& display, String title, String message);
void showMainScreen(TFT_eSPI& display);

#endif
