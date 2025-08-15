#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <time.h>

#include "actualization_logic.h"
#include "communication_logic.h"
#include "registration_logic.h"  // DODANE: dla funkcji kamery

// Konfiguracja WiFi
String wifi_ssid = "";
String wifi_password = "";
String google_script_url = "";
String google_drive_folder_id = ""; // DODANE: dla upload zdjęć na Google Drive

// Konfiguracja NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // Strefa czasowa dla Polski (GMT+1)
const int daylightOffset_sec = 3600; // Czas letni

// Zmienne globalne
bool wifi_connected = false;
unsigned long lastSync = 0;

// NOWE: Status inicjalizacji systemu
bool system_ready = false;
bool sd_initialized = false;
bool config_loaded = false;
bool wifi_attempted = false;
bool sync_attempted = false;

// Deklaracje funkcji
void initSerial();
void initSDCard();
bool loadConfig();
void connectToWiFi();
void checkWiFiConnection();
void syncTime();

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // UWAGA: BRAK LOGÓW DEBUGOWANIA PRZEZ UART!
  // Wszystkie logi inicjalizacji WYŁĄCZONE dla czystości protokołu z ESP WROOM
  
  // KROK 1: Inicjalizacja komunikacji z ESP WROOM (pierwszeństwo!)
  setupCommunicationLogic();
  delay(500);
  
  // KROK 2: Inicjalizacja karty SD
  initSDCard();
  sd_initialized = true;
  delay(500);

  // KROK 2.5: Inicjalizacja kamery - DODANE!
  if (initCameraOnDemand()) {
    // Kamera zainicjalizowana pomyślnie
    createPhotosDirectory(); // Stwórz folder photos jeśli nie istnieje
  }
  delay(500);
  
  // KROK 3: Wczytanie konfiguracji
  if (loadConfig()) {
    config_loaded = true;
  } else {
    config_loaded = false;
  }
  delay(500);

  // KROK 4: Połączenie z WiFi (z timeout)
  if (config_loaded) {
    connectToWiFi();
  }
  wifi_attempted = true;
  delay(500);
  
  // KROK 5: Synchronizacja czasu i danych (jeśli WiFi)
  if (wifi_connected) {
    syncTime();
    syncPracownicyFromGoogle(google_script_url);
    syncAlarmsFromGoogle(google_script_url);
    lastSync = millis();
    sync_attempted = true;
  } else {
    sync_attempted = false;
  }
  delay(500);

  // KROK 6: Finalizacja i sygnał gotowości
  system_ready = true;
  
  // KLUCZOWY MOMENT: Wysyłamy sygnał gotowości do ESP WROOM
  delay(1000); // Krótka pauza przed sygnałem
  
  // ESP WROOM oczekuje prostego sygnału {"ready"}
  Serial.println("{\"ready\"}");
  Serial.flush();
}

void loop() {
  // === PRIORYTET 1: Obsługa komunikacji z ESP WROOM ===
  handleCommunication();
  
  // === PRIORYTET 2: Monitorowanie stanu systemu ===
  if (system_ready) {
    // Sprawdzenie połączenia WiFi (tylko jeśli wcześniej działało)
    if (wifi_attempted) {
      checkWiFiConnection();
      
      // Informowanie o zmianie stanu WiFi - TYLKO JSON!
      static bool prevWifiState = wifi_connected;
      if (wifi_connected != prevWifiState) {
        if (wifi_connected) {
          // TYLKO JSON - BEZ LOGÓW DEBUGOWANIA!
          Serial.println("{\"wifi_status\":\"reconnected\"}");
          Serial.flush();
        } else {
          // TYLKO JSON - BEZ LOGÓW DEBUGOWANIA!
          Serial.println("{\"wifi_status\":\"disconnected\"}");
          Serial.flush();
        }
        prevWifiState = wifi_connected;
      }
    }
    
    // === PRIORYTET 3: Okresowa synchronizacja (mniej agresywnie) ===
    if (wifi_connected && sync_attempted && (millis() - lastSync > 1800000)) {  // 30 min
      // TYLKO JSON - BEZ LOGÓW DEBUGOWANIA!
      Serial.println("{\"sync_status\":\"starting\"}");
      Serial.flush();
      
      syncPracownicyFromGoogle(google_script_url);
      syncAlarmsFromGoogle(google_script_url);
      
      // Upload zdjęć do Google Drive
      if (google_drive_folder_id != "") {
        uploadPhotosToGoogleDrive(google_script_url, google_drive_folder_id);
      }
      
      lastSync = millis();
      // TYLKO JSON - BEZ LOGÓW DEBUGOWANIA!
      Serial.println("{\"sync_status\":\"completed\"}");
      Serial.flush();
    }
  }

  // Minimalne opóźnienie - UART ma najwyższy priorytet
  delay(50);
}

// Funkcja do synchronizacji czasu z serwerem NTP
void syncTime() {
  // BRAK LOGÓW DEBUGOWANIA - tylko wewnętrzne działanie
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // Błąd synchronizacji - loguj do SD, nie do UART
    return;
  }
  
  // Czas zsynchronizowany pomyślnie
}

// Inicjalizacja komunikacji szeregowej
void initSerial() {
  Serial.begin(115200);
  delay(1000);
  // BRAK LOGÓW DEBUGOWANIA
}

// Inicjalizacja karty SD
void initSDCard() {
  // BRAK LOGÓW DEBUGOWANIA PRZEZ UART!
  
  // Użyj trybu 1-bitowego, aby uniknąć konfliktu pinów z kamerą
  if (!SD_MMC.begin("/sdcard", true)) {
    return; // Błąd - tylko wewnętrzne działanie
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    return; // Brak karty - tylko wewnętrzne działanie
  }
  
  // Karta SD zainicjalizowana pomyślnie - tylko wewnętrzne działanie
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  // Rozmiar karty: cardSize MB - tylko wewnętrzne działanie
}

// Wczytanie konfiguracji z pliku config.txt
bool loadConfig() {
  // BRAK LOGÓW DEBUGOWANIA PRZEZ UART!
  
  // Próba otwarcia pliku config.txt w folderze czytnik_projekt
  File configFile = SD_MMC.open("/czytnik_projekt/config.txt", FILE_READ);
  if (!configFile) {
    return false; // Błąd pliku
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
    }
    else if (line.startsWith("wifi_password=")) {
      wifi_password = line.substring(15);
      wifi_password.replace("{", "");
      wifi_password.replace("}", "");
      wifi_password.replace("\"", "");
      wifi_password.trim();
    }
    else if (line.startsWith("google_script_url=")) {
      google_script_url = line.substring(18);
      google_script_url.replace("{", "");
      google_script_url.replace("}", "");
      google_script_url.replace("\"", "");
      google_script_url.trim();
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
  // BRAK LOGÓW DEBUGOWANIA PRZEZ UART!
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  int attempts = 0;
  const int max_attempts = 20;
  
  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(1000);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    // WiFi połączone pomyślnie - tylko wewnętrzne działanie
    // Adres IP i siła sygnału - tylko wewnętrzne działanie
  } else {
    wifi_connected = false;
    // Nie udało się połączyć - tylko wewnętrzne działanie
  }
}

// Sprawdzenie połączenia WiFi
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED && wifi_connected) {
    // Utracono połączenie WiFi - tylko wewnętrzne działanie
    wifi_connected = false;
    connectToWiFi(); // Próba ponownego połączenia
  }
}