#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

// Pin boozera
#define BUZZER_PIN 26

// Funkcje obsługi boozera
void initBuzzer();
void buzzerBeep1();
void buzzerBeep2();
void buzzerBeep3();
void buzzerOff();
void processBuzzerCommand(String command);

#endif // BUZZER_H
