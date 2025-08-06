#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "tft_debug.h"

// TFT Display
TFT_eSPI tft = TFT_eSPI();
TFTDebug* debugConsole = nullptr;

// Stany aplikacji
enum AppState {
  STATE_MENU,
  STATE_MONITOR,
  STATE_SEND_TEST,
  STATE_SEND_AUTH,
  STATE_SEND_CUSTOM
};

AppState currentState = STATE_MENU;
String customMessage = "";
String lastResponse = "";
unsigned long lastActivityTime = 0;

// Funkcje menu
void drawMainMenu();
void drawMonitorScreen();
void drawSendTestScreen();
void drawSendAuthScreen();
void drawSendCustomScreen();
void handleMenuTouch(uint16_t x, uint16_t y);
void sendTestCommand();
void sendAuthCommand(String id);
void sendCustomCommand(String message);
void initUART();
void processIncomingData();

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32 WROOM - ESP-CAM Communication Tester ===");
  
  // Inicjalizacja TFT
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  // Kalibracja dotyku
  tft.setCursor(20, 100);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Kalibracja dotyku...");
  
  uint16_t calData[5];
  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
  tft.setTouch(calData);
  
  // Inicjalizacja UART dla ESP-CAM
  initUART();
  
  // Inicjalizacja debug console
  debugConsole = new TFTDebug(&tft);
  
  // Wyświetl menu główne
  drawMainMenu();
  
  Serial.println("System gotowy!");
}

void loop() {
  // Obsługa komunikacji z ESP-CAM
  processIncomingData();
  
  // Obsługa dotyku
  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y);
  
  if (touched) {
    handleMenuTouch(x, y);
    delay(200); // Debounce
  }
  
  // Auto-refresh monitora co 5 sekund
  if (currentState == STATE_MONITOR && millis() - lastActivityTime > 5000) {
    debugConsole->addTimestamp("Monitoring...");
    lastActivityTime = millis();
  }
  
  delay(50);
}

void initUART() {
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("Serial2 zainicjalizowany (RX:16, TX:17, 115200 baud)");
  
  // Wyczyść bufor
  int cleared = 0;
  while(Serial2.available()) {
    char c = Serial2.read();
    cleared++;
  }
  
  if (cleared > 0) {
    Serial.print("Wyczyszczono ");
    Serial.print(cleared);
    Serial.println(" znaków z bufora UART");
  }
}

void drawMainMenu() {
  currentState = STATE_MENU;
  tft.fillScreen(TFT_BLACK);
  
  // Nagłówek
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("ESP-CAM Tester", tft.width()/2, 20, 4);
  
  // Przyciski menu
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.fillRoundRect(20, 80, 200, 40, 5, TFT_BLUE);
  tft.drawString("1. Monitor", 120, 95, 2);
  
  tft.fillRoundRect(20, 130, 200, 40, 5, TFT_GREEN);
  tft.drawString("2. Test Ping", 120, 145, 2);
  
  tft.fillRoundRect(20, 180, 200, 40, 5, TFT_ORANGE);
  tft.drawString("3. Test Auth", 120, 195, 2);
  
  tft.fillRoundRect(20, 230, 200, 40, 5, TFT_PURPLE);
  tft.drawString("4. Custom Msg", 120, 245, 2);
  
  // Instrukcje
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Dotknij opcje:", 20, 290, 2);
}

void drawMonitorScreen() {
  currentState = STATE_MONITOR;
  debugConsole->clear();
  debugConsole->addTimestamp("Monitor aktywny");
  debugConsole->println("Nasluchuje na RX:16...");
  
  // Przycisk powrotu
  tft.fillRoundRect(180, 300, 60, 30, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("MENU", 210, 315, 2);
}

void drawSendTestScreen() {
  currentState = STATE_SEND_TEST;
  tft.fillScreen(TFT_BLACK);
  
  // Nagłówek
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Test Ping", tft.width()/2, 20, 4);
  
  // Przyciski testowe
  tft.fillRoundRect(20, 80, 200, 40, 5, TFT_GREEN);
  tft.setTextColor(TFT_WHITE, TFT_GREEN);
  tft.drawString("Wyslij Ping", 120, 95, 2);
  
  tft.fillRoundRect(20, 130, 200, 40, 5, TFT_CYAN);
  tft.drawString("Heartbeat", 120, 145, 2);
  
  tft.fillRoundRect(20, 180, 200, 40, 5, TFT_YELLOW);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  tft.drawString("Status Check", 120, 195, 2);
  
  // Status ostatniej odpowiedzi
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Ostatnia odpowiedz:", 20, 240, 2);
  tft.drawString(lastResponse.substring(0, 30), 20, 260, 1);
  
  // Przycisk powrotu
  tft.fillRoundRect(180, 300, 60, 30, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("MENU", 210, 315, 2);
}

void drawSendAuthScreen() {
  currentState = STATE_SEND_AUTH;
  tft.fillScreen(TFT_BLACK);
  
  // Nagłówek
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Test Autoryzacji", tft.width()/2, 20, 4);
  
  // Przyciski z przykładowymi ID
  tft.fillRoundRect(20, 80, 200, 40, 5, TFT_ORANGE);
  tft.setTextColor(TFT_WHITE, TFT_ORANGE);
  tft.drawString("ID: 12345", 120, 95, 2);
  
  tft.fillRoundRect(20, 130, 200, 40, 5, TFT_ORANGE);
  tft.drawString("PESEL: 90010112345", 120, 145, 2);
  
  tft.fillRoundRect(20, 180, 200, 40, 5, TFT_ORANGE);
  tft.drawString("RFID: A1B2C3D4", 120, 195, 2);
  
  // Status
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Ostatnia odpowiedz:", 20, 240, 2);
  tft.drawString(lastResponse.substring(0, 30), 20, 260, 1);
  
  // Przycisk powrotu
  tft.fillRoundRect(180, 300, 60, 30, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("MENU", 210, 315, 2);
}

void drawSendCustomScreen() {
  currentState = STATE_SEND_CUSTOM;
  tft.fillScreen(TFT_BLACK);
  
  // Nagłówek
  tft.setTextColor(TFT_PURPLE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Custom Message", tft.width()/2, 20, 4);
  
  // Predefiniowane wiadomości
  tft.fillRoundRect(20, 80, 200, 30, 5, TFT_PURPLE);
  tft.setTextColor(TFT_WHITE, TFT_PURPLE);
  tft.drawString("{\"test\":\"ping\"}", 120, 90, 1);
  
  tft.fillRoundRect(20, 120, 200, 30, 5, TFT_PURPLE);
  tft.drawString("{\"status\":\"check\"}", 120, 130, 1);
  
  tft.fillRoundRect(20, 160, 200, 30, 5, TFT_PURPLE);
  tft.drawString("{\"reset\":\"true\"}", 120, 170, 1);
  
  tft.fillRoundRect(20, 200, 200, 30, 5, TFT_PURPLE);
  tft.drawString("{\"config\":\"wifi\"}", 120, 210, 1);
  
  // Status
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Ostatnia odpowiedz:", 20, 240, 2);
  tft.drawString(lastResponse.substring(0, 30), 20, 260, 1);
  
  // Przycisk powrotu
  tft.fillRoundRect(180, 300, 60, 30, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("MENU", 210, 315, 2);
}

void handleMenuTouch(uint16_t x, uint16_t y) {
  // Sprawdź przycisk MENU (we wszystkich ekranach oprócz głównego)
  if (currentState != STATE_MENU && x > 180 && x < 240 && y > 300 && y < 330) {
    drawMainMenu();
    return;
  }
  
  switch (currentState) {
    case STATE_MENU:
      if (x > 20 && x < 220) {
        if (y > 80 && y < 120) {
          drawMonitorScreen();
        } else if (y > 130 && y < 170) {
          drawSendTestScreen();
        } else if (y > 180 && y < 220) {
          drawSendAuthScreen();
        } else if (y > 230 && y < 270) {
          drawSendCustomScreen();
        }
      }
      break;
      
    case STATE_SEND_TEST:
      if (x > 20 && x < 220) {
        if (y > 80 && y < 120) {
          sendTestCommand();
        } else if (y > 130 && y < 170) {
          sendCustomCommand("{\"heartbeat\":\"alive\"}");
        } else if (y > 180 && y < 220) {
          sendCustomCommand("{\"status\":\"check\"}");
        }
      }
      break;
      
    case STATE_SEND_AUTH:
      if (x > 20 && x < 220) {
        if (y > 80 && y < 120) {
          sendAuthCommand("12345");
        } else if (y > 130 && y < 170) {
          sendAuthCommand("90010112345");
        } else if (y > 180 && y < 220) {
          sendAuthCommand("A1B2C3D4");
        }
      }
      break;
      
    case STATE_SEND_CUSTOM:
      if (x > 20 && x < 220) {
        if (y > 80 && y < 110) {
          sendCustomCommand("{\"test\":\"ping\"}");
        } else if (y > 120 && y < 150) {
          sendCustomCommand("{\"status\":\"check\"}");
        } else if (y > 160 && y < 190) {
          sendCustomCommand("{\"reset\":\"true\"}");
        } else if (y > 200 && y < 230) {
          sendCustomCommand("{\"config\":\"wifi\"}");
        }
      }
      break;
  }
}

void sendTestCommand() {
  String cmd = "{\"test\":\"ping\"}";
  Serial2.println(cmd);
  Serial2.flush();
  
  Serial.println("TX: " + cmd);
  lastResponse = "Wysłano: " + cmd;
  
  if (currentState == STATE_MONITOR && debugConsole) {
    debugConsole->addTimestamp("TX: " + cmd);
  }
  
  // Odśwież ekran
  drawSendTestScreen();
}

void sendAuthCommand(String id) {
  String cmd = "{\"authorization\":\"" + id + "\"}";
  Serial2.println(cmd);
  Serial2.flush();
  
  Serial.println("TX: " + cmd);
  lastResponse = "Auth: " + id;
  
  if (currentState == STATE_MONITOR && debugConsole) {
    debugConsole->addTimestamp("TX: " + cmd);
  }
  
  // Odśwież ekran
  drawSendAuthScreen();
}

void sendCustomCommand(String message) {
  Serial2.println(message);
  Serial2.flush();
  
  Serial.println("TX: " + message);
  lastResponse = "Custom: " + message.substring(0, 15);
  
  if (currentState == STATE_MONITOR && debugConsole) {
    debugConsole->addTimestamp("TX: " + message);
  }
  
  // Odśwież odpowiedni ekran
  if (currentState == STATE_SEND_CUSTOM) {
    drawSendCustomScreen();
  } else if (currentState == STATE_SEND_TEST) {
    drawSendTestScreen();
  }
}

void processIncomingData() {
  if (Serial2.available()) {
    String receivedLine = Serial2.readStringUntil('\n');
    receivedLine.trim();
    
    if (receivedLine.length() > 0) {
      Serial.println("RX: " + receivedLine);
      lastResponse = receivedLine;
      
      // Wyświetl w konsoli debug jeśli w trybie monitora
      if (currentState == STATE_MONITOR && debugConsole) {
        // Skategoryzuj odpowiedź
        String category = "";
        if (receivedLine.startsWith("{confirm") || receivedLine.startsWith("{denide")) {
          category = "[AUTH] ";
        } else if (receivedLine.indexOf("pong") >= 0 || receivedLine.indexOf("test") >= 0) {
          category = "[TEST] ";
        } else if (receivedLine.indexOf("WiFi") >= 0 || receivedLine.indexOf("connected") >= 0) {
          category = "[WIFI] ";
        } else if (receivedLine.startsWith("ets Jul") || receivedLine.startsWith("rst:")) {
          category = "[BOOT] ";
        } else if (receivedLine.length() > 50) {
          category = "[LONG] ";
        } else {
          category = "[INFO] ";
        }
        
        debugConsole->addTimestamp(category + receivedLine.substring(0, 25));
      }
      
      lastActivityTime = millis();
    }
  }
}
