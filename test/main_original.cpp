#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TFT_eSPI.h>
#include "screens.h"
#include "keypad.h"
#include "local_communication.h"

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

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Czekaj na połączenie z portem szeregowym
  }
  Serial.println("=== Rozpoczynam konfiguracje ===");

  // --- TEST KOMUNIKACJI Z ESP-CAM ---
  Serial.println("0. Test komunikacji z ESP-CAM...");
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 
  Serial.println("Serial2 zainicjalizowany (RX:16, TX:17, 115200 baud)");
  delay(500);
  
  // Wyczyść bufor
  int cleared = 0;
  while(Serial2.available()) {
    char c = Serial2.read();
    cleared++;
  }
  if (cleared > 0) {
    Serial.print("Wyczyszczono ");
    Serial.print(cleared);
    Serial.println(" znaków z bufora");
  }
  
  // ESP-CAM potrzebuje około 1 minuty na inicjalizację
  Serial.println("ESP-CAM potrzebuje ~60s na inicjalizację WiFi i urządzeń...");
  Serial.println("Wysyłam sygnały testowe co 10s przez 90s:");
  
  bool espCamReady = false;
  for (int attempt = 1; attempt <= 9; attempt++) {
    Serial.print("Test ");
    Serial.print(attempt);
    Serial.print("/9: Wysyłam {\"test\":\"ping\"}... ");
    
    Serial2.println("{\"test\":\"ping\"}");
    Serial2.flush();
    
    // Czekaj na odpowiedź przez 10 sekund
    unsigned long testStart = millis();
    String testResponse = "";
    bool gotResponse = false;
    
    while (millis() - testStart < 10000) {
      if (Serial2.available()) {
        char c = Serial2.read();
        
        if (c == '\n' || c == '\r') {
          if (testResponse.length() > 0) {
            testResponse.trim();
            Serial.print("RX: '");
            Serial.print(testResponse);
            Serial.println("'");
            
            // Sprawdź czy to odpowiedź na test lub gotowość ESP-CAM
            if (testResponse.indexOf("pong") >= 0 || 
                testResponse.indexOf("ready") >= 0 || 
                testResponse.indexOf("test") >= 0 ||
                testResponse.startsWith("{confirm") ||
                testResponse.startsWith("{denide")) {
              Serial.println("*** ESP-CAM GOTOWY DO KOMUNIKACJI! ***");
              espCamReady = true;
              gotResponse = true;
              break;
            }
            testResponse = "";
          }
        } else {
          testResponse += c;
        }
      }
      delay(100);
    }
    
    if (gotResponse) break;
    
    Serial.println("Brak odpowiedzi. Czekam 10s...");
    delay(10000); // Czekaj 10 sekund przed następnym testem
  }
  
  if (espCamReady) {
    Serial.println("✓ ESP-CAM zainicjalizowany pomyślnie!");
  } else {
    Serial.println("⚠ ESP-CAM nie odpowiada. Sprawdź:");
    Serial.println("  - Połączenia: ESP-CAM TX->WROOM RX(16), ESP-CAM RX->WROOM TX(17)");
    Serial.println("  - Zasilanie ESP-CAM");
    Serial.println("  - Kod ESP-CAM uruchomiony");
    Serial.println("Kontynuuję konfigurację...");
  }
  
  Serial.println("--------------------------");

  // --- KROK 1: Inicjalizacja i diagnostyka RFID ---
  Serial.println("1. Konfiguracja RFID...");
  enableRFIDSPI();   // Ustaw SPI dla RFID
  rfid.PCD_Init();   // Inicjalizacja RC522
  delay(10);         // Krótka pauza dla stabilności

  Serial.println("Sprawdzanie wersji MFRC522...");
  rfid.PCD_DumpVersionToSerial(); // Wyświetl szczegółowe informacje o wersji
  
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("BŁĄD KRYTYCZNY: Nie można nawiązać komunikacji z RC522.");
    Serial.println("Sprawdź dokładnie połączenia pinów (SCK, MISO, MOSI, SS, RST)!");
    // Na tym etapie można by wyświetlić błąd na ekranie, ale bez SPI dla TFT to niemożliwe
    while(true); // Zatrzymaj program
  }
  Serial.println("Komunikacja z RC522 OK.");
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
      
      Serial2.println(dataToSend);
      Serial2.flush();
      Serial.println("Wysłano! Resetuję licznik RX...");
      rxCounter = 0; // Reset licznika przy nowej wiadomości
    }
  }
  
  // WAŻNE: Aktualizuj ekrany oczekiwania, żeby nie było czarnego ekranu
  enableTouchSPI();
  
  // Obsługa RFID i dotyku podczas testów
  bool cardReadSuccess = false;

  // --- ODCZYT RFID ---
  enableRFIDSPI();
  
  if (waitingForCard && !keypadActive) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      cardID = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        cardID += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        cardID += String(rfid.uid.uidByte[i], HEX);
      }
      cardID.toUpperCase();
      cardID.trim();
      
      Serial.print("Wykryto karte: ");
      Serial.println(cardID);

      cardReadSuccess = true;
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
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
  
  // ===========================================
  // KONIEC KODU TESTOWEGO
  // ===========================================
  
  /*
  // ORYGINALNY KOD - ZAKOMENTOWANY NA CZAS TESTÓW
  bool cardReadSuccess = false;

  // --- FAZA 1: Odczyt RFID ---
  enableRFIDSPI();
  
  if (waitingForCard && !keypadActive) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      cardID = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        cardID += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        cardID += String(rfid.uid.uidByte[i], HEX);
      }
      cardID.toUpperCase();
      cardID.trim();
      
      Serial.print("Wykryto karte: ");
      Serial.println(cardID);

      cardReadSuccess = true;
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  // --- FAZA 2: Obsługa ekranu i dotyku ---
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
    // Logika przycisku "Zatwierdź" została przeniesiona do handleAuthorization
  } else {
    if(keypadActive) {
      handleKeypad(is_pressed, 0, 0);
    }
  }
  */
  
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
