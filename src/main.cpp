#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <time.h>

#include "actualization_logic.h"
#include "communication_logic.h"

// Konfiguracja WiFi
String wifi_ssid = "";
String wifi_password = "";
String google_script_url = "";

// Konfiguracja NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // Strefa czasowa dla Polski (GMT+1)
const int daylightOffset_sec = 3600; // Czas letni

// Zmienne globalne
bool wifi_connected = false;
unsigned long lastSync = 0;

// Deklaracje funkcji
void initSerial();
void initSDCard();
bool loadConfig();
void connectToWiFi();
void checkWiFiConnection();
void syncTime();

void setup() {
  // Inicjalizacja komunikacji szeregowej
  initSerial();

  // Inicjalizacja karty SD
  initSDCard();
  
  // Wczytanie konfiguracji
  if (!loadConfig()) {
    Serial.println("Błąd wczytywania konfiguracji - kontynuuję z domyślnymi ustawieniami");
  }

  // Połączenie z WiFi
  connectToWiFi();
  
  // Synchronizacja czasu
  if (wifi_connected) {
    syncTime();
    
    // Inicjalna synchronizacja danych
    Serial.println("Wykonywanie początkowej synchronizacji danych...");
    
    syncPracownicyFromGoogle(google_script_url);
    syncAlarmsFromGoogle(google_script_url);
    lastSync = millis();
  }

  // Inicjalizacja komunikacji z ESP WROOM
  setupCommunicationLogic();
  
  // Wyślij ping o gotowości systemu po pełnej inicjalizacji
  if (wifi_connected) {
    sendStatusPing("SYSTEM_READY_WITH_WIFI");
  } else {
    sendStatusPing("SYSTEM_READY_NO_WIFI");
  }
  
  Serial.println("=== SYSTEM GOTOWY ===");
  Serial.println("Naciśnij 't' aby przetestować nasłuchiwanie komunikacji");
  Serial.println("Naciśnij 's' aby zasymulować zapytanie z ESP WROOM");
  Serial.println("Naciśnij 'p' aby wysłać test PING do ESP WROOM");
}

void loop() {
  // Sprawdzenie połączenia WiFi
  checkWiFiConnection();
  
  // Wysyłaj ping po sprawdzeniu WiFi (jeśli połączenie się zmieniło)
  static bool prevWifiState = false;
  if (wifi_connected != prevWifiState) {
    if (wifi_connected) {
      sendStatusPing("WIFI_CONNECTED");
    } else {
      sendStatusPing("WIFI_DISCONNECTED");
    }
    prevWifiState = wifi_connected;
  }
  
  // Okresowa synchronizacja danych (co 5 minut)
  if (wifi_connected && millis() - lastSync > 300000) {
    Serial.println("Wykonywanie okresowej synchronizacji danych...");
    sendStatusPing("SYNC_START");
    
    syncPracownicyFromGoogle(google_script_url);
    sendStatusPing("SYNC_PRACOWNICY_COMPLETE");
    
    syncAlarmsFromGoogle(google_script_url);
    sendStatusPing("SYNC_ALARMS_COMPLETE");
    
    lastSync = millis();
    sendStatusPing("SYNC_ALL_COMPLETE");
  }

  // Obsługa komend testowych
  if (Serial.available()) {
    char command = Serial.read();
    if (command == 't' || command == 'T') {
      Serial.println("=== URUCHAMIANIE TESTU NASŁUCHIWANIA ===");
      testCommunication();
    }
    else if (command == 's' || command == 'S') {
      Serial.println("=== SYMULACJA ZAPYTANIA Z ESP WROOM ===");
      simulateWROOMRequest();
    }
    else if (command == 'p' || command == 'P') {
      Serial.println("=== WYSYŁANIE TESTU PING ===");
      Serial2.println("{\"ping\":\"test\"}");
      Serial.println("Wysłano ping do ESP WROOM: {\"ping\":\"test\"}");
    }
  }

  // Ciągłe nasłuchiwanie komunikacji z ESP WROOM
  handleCommunication();

  delay(100);
}

// Funkcja do synchronizacji czasu z serwerem NTP
void syncTime() {
  Serial.println("Synchronizowanie czasu z serwerem NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Nie udało się zsynchronizować czasu z serwerem NTP.");
    return;
  }
  
  Serial.print("Czas zsynchronizowany: ");
  Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
}

// Inicjalizacja komunikacji szeregowej
void initSerial() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32CAM Rejestrator Godzin - Start");
}

// Inicjalizacja karty SD
void initSDCard() {
  Serial.println("Inicjalizacja karty SD w trybie 1-bitowym (dla zgodności z kamerą)...");
  
  // Użyj trybu 1-bitowego, aby uniknąć konfliktu pinów z kamerą
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Błąd inicjalizacji karty SD!");
    return;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("Nie wykryto karty SD!");
    return;
  }
  
  Serial.println("Karta SD zainicjalizowana pomyślnie");
  
  // Wyświetlenie informacji o karcie SD
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("Rozmiar karty SD: %lluMB\n", cardSize);
}

// Wczytanie konfiguracji z pliku config.txt
bool loadConfig() {
  Serial.println("Wczytywanie konfiguracji z pliku config.txt...");
  
  // Próba otwarcia pliku config.txt w folderze czytnik_projekt
  File configFile = SD_MMC.open("/czytnik_projekt/config.txt", FILE_READ);
  if (!configFile) {
    Serial.println("Nie można otworzyć pliku /czytnik_projekt/config.txt");
    return false;
  } else {
    Serial.println("Znaleziono plik /czytnik_projekt/config.txt");
  }
  
  String line;
  while (configFile.available()) {
    line = configFile.readStringUntil('\n');
    line.trim();
    
    if (line.startsWith("wifi_ssid=")) {
      wifi_ssid = line.substring(11);
      wifi_ssid.replace("{", "");
      wifi_ssid.replace("}", "");
      wifi_ssid.replace("\"", "");
      wifi_ssid.trim();
      Serial.println("WiFi SSID: " + wifi_ssid);
    }
    else if (line.startsWith("wifi_password=")) {
      wifi_password = line.substring(15);
      wifi_password.replace("{", "");
      wifi_password.replace("}", "");
      wifi_password.replace("\"", "");
      wifi_password.trim();
      Serial.println("WiFi Password: [UKRYTE]");
    }
    else if (line.startsWith("google_script_url=")) {
      google_script_url = line.substring(18);
      google_script_url.replace("{", "");
      google_script_url.replace("}", "");
      google_script_url.replace("\"", "");
      google_script_url.trim();
      Serial.println("Google Script URL: " + google_script_url);
    }
  }
  
  configFile.close();
  
  if (wifi_ssid.length() > 0 && wifi_password.length() > 0) {
    return true;
  }
  
  return false;
}

// Połączenie z siecią WiFi
void connectToWiFi() {
  Serial.println("Łączenie z siecią WiFi...");
  Serial.println("SSID: " + wifi_ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  int attempts = 0;
  const int max_attempts = 20;
  
  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    Serial.println("");
    Serial.println("WiFi połączone!");
    Serial.print("Adres IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Siła sygnału (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    wifi_connected = false;
    Serial.println("");
    Serial.println("Nie udało się połączyć z WiFi!");
  }
}

// Sprawdzenie połączenia WiFi
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED && wifi_connected) {
    Serial.println("Utracono połączenie WiFi - próba ponownego połączenia...");
    wifi_connected = false;
    connectToWiFi();
  }
}