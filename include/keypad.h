#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Funkcje klawiatury
void initKeypad();
void drawKeypad();
void handleKeypad(bool pressed, uint16_t x, uint16_t y);
void executeKeyAction(int buttonIndex);
void updatePESELDisplay();
void showKeypadStatus(const char* msg);
String getKeypadInput(uint16_t x, uint16_t y);

#endif
