#include "buzzer.h"

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Buzzer zainicjalizowany na GPIO26");
}

// {"beep1"} - ciągły sygnał 0,5 s
void buzzerBeep1() {
  Serial.println("[BUZZER] Beep1 - ciągły sygnał 0.5s");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
}

// {"beep2"} - dwa piknięcia po 0,2s, 0,5s przerwy, kolejne dwa piknięcia po 0,2s
void buzzerBeep2() {
  Serial.println("[BUZZER] Beep2 - 2x(2 piknięcia)");
  
  // Pierwsza seria - 2 piknięcia
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < 1) delay(100); // Krótka przerwa między piknięciami
  }
  
  delay(500); // Przerwa 0,5s
  
  // Druga seria - 2 piknięcia
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < 1) delay(100); // Krótka przerwa między piknięciami
  }
}

// {"beep3"} - 3x(3 sygnały po 0,3s, odstęp 1s)
void buzzerBeep3() {
  Serial.println("[BUZZER] Beep3 - 3x(3 sygnały)");
  
  // Powtórz 3 razy
  for (int cycle = 0; cycle < 3; cycle++) {
    
    // 3 sygnały po 0,3s w każdym cyklu
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(300);
      digitalWrite(BUZZER_PIN, LOW);
      if (i < 2) delay(100); // Krótka przerwa między sygnałami
    }
    
    // Odstęp 1s między cyklami (oprócz ostatniego)
    if (cycle < 2) {
      delay(1000);
    }
  }
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, LOW);
}

void processBuzzerCommand(String command) {
  command.trim();
  
  Serial.print("[BUZZER] Otrzymano komendę: ");
  Serial.println(command);
  
  if (command == "{\"beep1\"}" || command.indexOf("beep1") >= 0) {
    buzzerBeep1();
  } 
  else if (command == "{\"beep2\"}" || command.indexOf("beep2") >= 0) {
    buzzerBeep2();
  } 
  else if (command == "{\"beep3\"}" || command.indexOf("beep3") >= 0) {
    buzzerBeep3();
  } 
  else {
    Serial.print("[BUZZER] Nieznana komenda: ");
    Serial.println(command);
  }
}
