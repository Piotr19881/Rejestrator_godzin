#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TFT_eSPI.h>
#include "screens.h"
#include "keypad.h"
#include "local_communication.h"
#include "cards_mapping.h"
#include "buzzer.h"
#include "led.h"

// TFT_eSPI: piny VSPI ustawione w User_Setup.h
TFT_eSPI tft = TFT_eSPI();

// Piny RFID RC522
#define SS_PIN      22
#define RST_PIN     5

// Konfiguracja RC522
MFRC522 rfid(SS_PIN, RST_PIN);

// Zmienne dla obsługi aplikacji
String cardID = "";
String enteredPESEL = "";
bool waitingForCard = true;
bool keypadActive = false;

// Funkcje do przełączania SPI
void enableTouchSPI() {
  SPI.end();
  SPI.begin(); // Domyślne piny TFT
}

void enableRFIDSPI() {
  SPI.end();
  SPI.begin(14, 12, 13, 22); // Twoje piny RFID
}

// Funkcja konwersji RFID hex na różne formaty
String parseCardID(String rawID) {
  rawID.trim();
  
  // Usuń spacje z hex ID
  String hexID = rawID;
  hexID.replace(" ", "");
  hexID.toUpperCase();
  
  Serial.println("=== PARSOWANIE KARTY ===");
  Serial.print("Raw ID: ");
  Serial.println(rawID);
  Serial.print("Hex bez spacji: ");
  Serial.println(hexID);
  
  // Spróbuj mapować przez tablicę
  String mappedPESEL = mapRFIDtoPESEL(hexID);
  
  // Sprawdź czy mapowanie się udało (czy zwrócono PESEL)
  if (mappedPESEL != hexID && mappedPESEL.length() == 11) {
    Serial.print("Zmapowano na PESEL: ");
    Serial.println(mappedPESEL);
    Serial.println("========================");
    return mappedPESEL;
  }
  
  // Konwersja hex na decimal (alternatywna metoda)
  String decimalID = "";
  if (hexID.length() >= 8 && hexID.length() <= 14) { 
    // Konwertuj hex na liczbę całkowitą, potem na string
    unsigned long hexValue = 0;
    for (int i = 0; i < hexID.length() && i < 8; i++) { // Max 8 znaków hex = 32 bity
      char c = hexID.charAt(i);
      if (c >= '0' && c <= '9') {
        hexValue = (hexValue << 4) + (c - '0');
      } else if (c >= 'A' && c <= 'F') {
        hexValue = (hexValue << 4) + (c - 'A' + 10);
      }
    }
    decimalID = String(hexValue);
  }
  
  Serial.print("Decimal: ");
  Serial.println(decimalID);
  
  // Sprawdź czy decimal ma 11 cyfr (jak PESEL)
  if (decimalID.length() == 11) {
    Serial.println("Format: PESEL-like (11 cyfr)");
    Serial.println("========================");
    return decimalID;
  }
  
  // Jeśli nic nie pasuje, zwróć hex bez spacji
  Serial.println("Format: HEX (bez mapowania)");
  Serial.println("========================");
  return hexID;
}

// Ulepszona funkcja odczytu karty RFID
String readRFIDCard() {
  String cardData = "";
  
  // Sprawdź czy jest nowa karta
  if (!rfid.PICC_IsNewCardPresent()) {
    return "";
  }
  
  // Wybierz kartę
  if (!rfid.PICC_ReadCardSerial()) {
    return "";
  }
  
  // Odczytaj UID karty
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (i > 0) cardData += " ";
    if (rfid.uid.uidByte[i] < 0x10) cardData += "0";
    cardData += String(rfid.uid.uidByte[i], HEX);
  }
  
  cardData.toUpperCase();
  
  // Zatrzymaj komunikację z kartą
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  Serial.print("Karta wykryta (raw): ");
  Serial.println(cardData);
  
  // Parsuj ID do właściwego formatu
  String parsedID = parseCardID(cardData);
  
  Serial.print("ID do autoryzacji: ");
  Serial.println(parsedID);
  
  return parsedID;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Czekaj na połączenie z portem szeregowym
  }
  Serial.println("=== Rozpoczynam konfiguracje ===");

  // --- INICJALIZACJA LED ---
  Serial.println("0.0. Inicjalizacja LED GPIO32...");
  initLED();
  
  // Test LED
  Serial.println("Test LED...");
  ledBlink(3, 200);
  delay(500);

  // --- INICJALIZACJA BUZZER ---
  Serial.println("0.1. Inicjalizacja buzzer GPIO26...");
  initBuzzer();
  
  // Buzzer gotowy do odbioru komend z ESP32-CAM
  Serial.println("Buzzer gotowy - czeka na komendy z ESP32-CAM");
  delay(500);

  // --- INICJALIZACJA KOMUNIKACJI Z ESP-CAM ---
  Serial.println("0. Inicjalizacja komunikacji z ESP-CAM...");
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 
  Serial.println("Serial2 zainicjalizowany (RX:16, TX:17, 115200 baud)");
  delay(500);
  
  // Użyj nowego systemu oczekiwania na gotowość ESP32-CAM
  Serial.println("Czekam na sygnał gotowości ESP32-CAM (max 90s)...");
  bool espCamReady = waitForESPCamReady(90000); // 90 sekund timeout
  
  if (espCamReady) {
    Serial.println("✓ ESP32-CAM gotowy do komunikacji!");
    Serial.println("✓ Można wysyłać zapytania o autoryzację");
  } else {
    Serial.println("⚠ ESP32-CAM nie wysłał sygnału gotowości. Sprawdź:");
    Serial.println("  - Połączenia: ESP32-CAM TX->WROOM RX(16), ESP32-CAM RX->WROOM TX(17)");
    Serial.println("  - Zasilanie ESP32-CAM");
    Serial.println("  - Kod ESP32-CAM uruchomiony");
    Serial.println("  - Połączenie WiFi ESP32-CAM");
    Serial.println("  - Połączenie z Google Sheets");
    Serial.println("Kontynuuję konfigurację bez ESP32-CAM...");
    setESPCamReady(false);
  }
  
  Serial.println("--------------------------");

  // --- KROK 1: Inicjalizacja i diagnostyka RFID ---
  Serial.println("1. Konfiguracja RFID...");
  enableRFIDSPI();   // Ustaw SPI dla RFID
  rfid.PCD_Init();   // Inicjalizacja RC522
  delay(10);         // Krótka pauza dla stabilności

  Serial.println("Sprawdzanie wersji MFRC522...");
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print("Firmware Version: 0x");
  Serial.print(version, HEX);
  if (version == 0x91) Serial.println(" = MFRC522 clone");
  else if (version == 0x92) Serial.println(" = MFRC522 v2");  
  else if (version == 0x12) Serial.println(" = MFRC522 counterfeit");
  else Serial.println(" = (unknown)");
  
  if (version == 0x00 || version == 0xFF) {
    Serial.println("BŁĄD: Brak komunikacji z MFRC522!");
    Serial.println("Sprawdź połączenia SPI:");
    Serial.println("MISO -> GPIO19, MOSI -> GPIO23, SCK -> GPIO18");
    Serial.println("SS -> GPIO22, RST -> GPIO5");
    // Na tym etapie można by wyświetlić błąd na ekranie, ale bez SPI dla TFT to niemożliwe
    while(true); // Zatrzymaj program
  } else {
    Serial.println("Komunikacja z RC522 OK.");
  }
  Serial.println("--------------------------");

  // --- KROK 2: Inicjalizacja TFT i Kalibracja ---
  Serial.println("2. Konfiguracja TFT...");
  enableTouchSPI(); // Przełącz SPI na tryb dla TFT
  tft.init();
  tft.setRotation(0);
  
  // Kalibracja dotyku - na podstawie działającego przykładu
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 0);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Uruchamiam kalibracje dotyku...");
  delay(1000);

  uint16_t calData[5];
  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
  tft.setTouch(calData);
  Serial.println("Kalibracja zakonczona.");
  
  // Inicjalizacja systemu ekranów
  initScreens(); // To wyświetli pierwszy ekran motywacyjny
  
  // Inicjalizacja klawiatury
  initKeypad();
  
  Serial.println("System gotowy - oczekiwanie na karte...");
}

void loop() {
  // ===========================================
  // KOD TESTOWY - NASŁUCH NA SERIAL2 (RX/TX)
  // ===========================================
  
  static int rxCounter = 0;
  static unsigned long lastHeartbeat = 0;
  
  // Co 30 sekund wyślij sygnał życia do ESP-CAM
  if (millis() - lastHeartbeat > 30000) {
    Serial.println("[HEARTBEAT] Wysyłam sygnał życia do ESP-CAM");
    Serial2.println("{\"heartbeat\":\"alive\"}");
    lastHeartbeat = millis();
  }
  
  // Sprawdź, czy są dane przychodzące z ESP32-CAM
  if (Serial2.available()) {
    char c = Serial2.read();
    static String receivedData = "";
    
    // Pokaż surowe bajty dla pierwszych 20 znaków każdej sesji
    if (rxCounter < 20) {
      Serial.print("[RX ");
      Serial.print(rxCounter);
      Serial.print("]: '");
      if (c >= 32 && c <= 126) { // Drukowalne znaki ASCII
        Serial.print(c);
      } else {
        Serial.print("\\x");
        Serial.print(c, HEX);
      }
      Serial.print("' (0x");
      Serial.print(c, HEX);
      Serial.println(")");
    }
    rxCounter++;
    
    if (c == '\n' || c == '\r') {
      if (receivedData.length() > 0) {
        receivedData.trim();
        
        // Zawsze pokazuj odebrane linie z czasem
        Serial.print("[");
        Serial.print(millis()/1000);
        Serial.print("s] RX LINE: '");
        Serial.print(receivedData);
        Serial.println("'");
        
        // Analiza typu wiadomości
        if (receivedData.startsWith("{confirm") || receivedData.startsWith("{denide")) {
          Serial.println("*** ODPOWIEDŹ AUTORYZACYJNA ***");
        } else if (receivedData.indexOf("pong") >= 0 || receivedData.indexOf("ready") >= 0) {
          Serial.println("*** SYGNAŁ GOTOWOŚCI ESP-CAM ***");
        } else if (receivedData.indexOf("beep1") >= 0 || receivedData.indexOf("beep2") >= 0 || receivedData.indexOf("beep3") >= 0) {
          Serial.println("*** KOMENDA BUZZER ***");
          processBuzzerCommand(receivedData);
        } else if (receivedData.startsWith("ets Jul") || receivedData.startsWith("rst:")) {
          Serial.println("(ESP-CAM restart - ignoruję)");
        } else if (receivedData.startsWith("WiFi") || receivedData.indexOf("connected") >= 0) {
          Serial.println("(ESP-CAM łączy się z WiFi)");
        } else if (receivedData.length() > 50) {
          Serial.println("(Długi komunikat - prawdopodobnie log inicjalizacji)");
        }
        
        receivedData = "";
      }
    } else {
      receivedData += c;
      
      // Zabezpieczenie przed zbyt długimi liniami
      if (receivedData.length() > 500) {
        Serial.println("[UWAGA] Linia zbyt długa, resetuję bufor");
        receivedData = "";
      }
    }
  }
  
  // Sprawdź, czy są dane do wysłania z Serial Monitor
  if (Serial.available()) {
    String dataToSend = Serial.readStringUntil('\n');
    dataToSend.trim();
    
    if (dataToSend.length() > 0) {
      Serial.println("=== WYSYŁANIE RĘCZNE ===");
      Serial.print("Tekst: '");
      Serial.print(dataToSend);
      Serial.println("'");
      Serial.print("Długość: ");
      Serial.print(dataToSend.length());
      Serial.println(" bajtów");
      
      // Sprawdź czy to komenda buzzer do testowania
      if (dataToSend.indexOf("beep") >= 0) {
        Serial.println("*** TEST BUZZER LOKALNIE ***");
        processBuzzerCommand(dataToSend);
      } else if (dataToSend.indexOf("led") >= 0) {
        Serial.println("*** TEST LED LOKALNIE ***");
        if (dataToSend == "led_on") {
          Serial.println("LED ON");
          ledOn();
        } else if (dataToSend == "led_off") {
          Serial.println("LED OFF");
          ledOff();
        } else if (dataToSend == "led_blink") {
          Serial.println("LED BLINK x5");
          ledBlink(5, 300);
        } else {
          Serial.println("Komendy LED: led_on, led_off, led_blink");
        }
      } else {
        // Wyślij do ESP32-CAM
        Serial2.println(dataToSend);
        Serial2.flush();
        Serial.println("Wysłano! Resetuję licznik RX...");
        rxCounter = 0; // Reset licznika przy nowej wiadomości
      }
    }
  }
  
  // WAŻNE: Aktualizuj ekrany oczekiwania, żeby nie było czarnego ekranu
  enableTouchSPI();
  
  // Obsługa RFID i dotyku podczas testów
  bool cardReadSuccess = false;

  // --- ODCZYT RFID ---
  enableRFIDSPI();
  
  if (waitingForCard && !keypadActive) {
    // Używaj nowej funkcji odczytu z parserem
    String detectedCard = readRFIDCard();
    
    if (detectedCard.length() > 0) {
      Serial.print("Wykryto karte: ");
      Serial.println(detectedCard);

      cardReadSuccess = true;
      cardID = detectedCard;
    }
  }

  // --- OBSŁUGA EKRANU I DOTYKU ---
  enableTouchSPI();

  if (cardReadSuccess) {
    handleAuthorization(cardID, tft);
    // Po zakończeniu autoryzacji, zresetuj stan
    waitingForCard = true;
    resetTextCycle();
    cardID = "";
  } else if (waitingForCard && !keypadActive) {
    // Aktualizuj ekran oczekiwania
    updateWaitingScreen();
  }

  // Sprawdź dotyk
  uint16_t x, y;
  bool is_pressed = tft.getTouch(&x, &y);
  if (is_pressed) {
    if (keypadActive) {
      handleKeypad(is_pressed, x, y);
    } else if (waitingForCard) {
      // Dotknięcie w trybie oczekiwania przełącza na klawiaturę
      keypadActive = true;
      waitingForCard = false;
      drawKeypad();
      delay(200); // prosty debounce
    }
  } else {
    if(keypadActive) {
      handleKeypad(is_pressed, 0, 0);
    }
  }
  
  delay(10); // Krótkie opóźnienie dla stabilności pętli
}

void handleKeypadInput(String input) {
  if (input == "OK") {
    Serial.print("Otrzymano PESEL: ");
    Serial.println(enteredPESEL);
    
    enableTouchSPI(); // Upewnij się, że SPI jest dla TFT
    handleAuthorization(enteredPESEL, tft);

    // Po obsłudze, wróć do ekranu oczekiwania
    keypadActive = false;
    waitingForCard = true;
    resetTextCycle();
    enteredPESEL = ""; // Wyczyść PESEL
  } else if (input == "CANCEL") {
    keypadActive = false;
    waitingForCard = true;
    resetTextCycle();
    enteredPESEL = ""; // Wyczyść PESEL
  }
}
