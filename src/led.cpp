#include "led.h"

void initLED() {
    pinMode(LED_PIN, OUTPUT);
    ledOff(); // Początkowy stan - dioda wyłączona
    Serial.println("LED zainicjalizowana na GPIO32");
}

void ledOn() {
    digitalWrite(LED_PIN, HIGH);
}

void ledOff() {
    digitalWrite(LED_PIN, LOW);
}

void ledBlink(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        ledOn();
        delay(delayMs);
        ledOff();
        if (i < times - 1) { // Nie dodawaj opóźnienia po ostatnim miganiu
            delay(delayMs);
        }
    }
}
