#ifndef LED_H
#define LED_H

#include <Arduino.h>

// GPIO pin dla diody LED
#define LED_PIN 32

// Funkcje do obsługi diody LED
void initLED();
void ledOn();
void ledOff();
void ledBlink(int times, int delayMs = 250);

#endif
