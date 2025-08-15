#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <time.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

#include "actualization_logic.h"
#include "communication_logic.h"
#include "registration_logic.h"  // DODANE: dla funkcji kamery
#include "sd_logger.h"          // DODANE: dla logToSD

// Deklaracje funkcji
void checkAutoLogout();
void performAutoLogout();
void updateWiFiLED();
void initWiFiLED();

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

// Konfiguracja diody WiFi status
const int WIFI_LED_PIN = 12;
bool sync_in_progress = false;
unsigned long last_led_toggle = 0;
bool led_state = false;

// Deklaracje funkcji
void initSerial();
void initSDCard();
bool loadConfig();
void connectToWiFi();
void checkWiFiConnection();
void syncTime();

void setup() {
  // KROK 0: Wyłączenie brownout detector i ustawienia zasilania
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Wyłącz brownout detector
  setCpuFrequencyMhz(160); // Zmniejsz częstotliwość CPU dla stabilności zasilania
  
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("[MAIN] === ESP32-CAM STARTUJE ===");
  Serial.println("[MAIN] Brownout detector wyłączony, CPU na 160MHz");
  
  // KROK 1: Inicjalizacja komunikacji z ESP WROOM (pierwszeństwo!)
  Serial.println("[MAIN] Inicjalizacja komunikacji z ESP WROOM...");
  setupCommunicationLogic();
  delay(500);
  
  // KROK 1.5: Inicjalizacja diody WiFi status
  Serial.println("[MAIN] Inicjalizacja diody WiFi status...");
  initWiFiLED();
  delay(200);
  
  // KROK 2: Inicjalizacja karty SD
  Serial.println("=== KROK 1: INICJALIZACJA KARTY SD ===");
  Serial.println("Proba montowania karty SD...");
  Serial.print("Pamiec przed SD: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.println("Rozpoczynam SD_MMC.begin()...");
  Serial.flush(); // Wymuś wysłanie przed potencjalnym zawieszeniem
  
  initSDCard();
  sd_initialized = true;
  
  Serial.println("Karta SD - inicjalizacja zakonczona");
  Serial.print("Pamiec po SD: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  delay(500);

  // KROK 2.5: Inicjalizacja kamery - DODANE!
  Serial.println("[MAIN] Inicjalizacja kamery...");
  if (initCameraOnDemand()) {
    Serial.println("[MAIN] Kamera zainicjalizowana pomyślnie");
    createPhotosDirectory(); // Stwórz folder photos jeśli nie istnieje
  } else {
    Serial.println("[MAIN] BŁĄD inicjalizacji kamery");
  }
  delay(500);
  
  // KROK 3: Wczytanie konfiguracji
  Serial.println("[MAIN] Wczytywanie konfiguracji...");
  if (loadConfig()) {
    config_loaded = true;
    Serial.println("[MAIN] Konfiguracja załadowana pomyślnie");
  } else {
    config_loaded = false;
    Serial.println("[MAIN] BŁĄD wczytywania konfiguracji");
  }
  delay(500);

  // KROK 4: Połączenie z WiFi (z timeout)
  if (config_loaded) {
    Serial.println("[MAIN] Próba połączenia z WiFi...");
    delay(1000); // Dodatkowy delay przed WiFi
    connectToWiFi();
  } else {
    Serial.println("[MAIN] Pomijam WiFi - brak konfiguracji");
  }
  wifi_attempted = true;
  delay(1000); // Zwiększony delay
  
  // KROK 5: Synchronizacja czasu i danych (jeśli WiFi)
  if (wifi_connected) {
    Serial.println("[MAIN] WiFi połączony - rozpoczynam synchronizację...");
    delay(500); // Delay przed synchronizacją
    
    // Oznacz że synchronizacja jest w toku
    sync_in_progress = true;
    
    syncTime();
    delay(500);
    Serial.println("[MAIN] Synchronizacja pracowników...");
    syncPracownicyFromGoogle(google_script_url);
    delay(500);
    Serial.println("[MAIN] Synchronizacja alarmów...");
    syncAlarmsFromGoogle(google_script_url);
    lastSync = millis();
    sync_attempted = true;
    
    // Zakończ synchronizację
    sync_in_progress = false;
    
    Serial.println("[MAIN] Synchronizacja zakończona");
  } else {
    sync_attempted = false;
    Serial.println("[MAIN] Brak WiFi - pomijam synchronizację");
  }
  delay(1000); // Zwiększony delay

  // KROK 6: Finalizacja i sygnał gotowości
  system_ready = true;
  
  Serial.println("[MAIN] === SYSTEM GOTOWY ===");
  Serial.print("[MAIN] Status: SD=");
  Serial.print(sd_initialized ? "OK" : "BŁĄD");
  Serial.print(", Config=");
  Serial.print(config_loaded ? "OK" : "BŁĄD");
  Serial.print(", WiFi=");
  Serial.print(wifi_connected ? "OK" : "BŁĄD");
  Serial.print(", Sync=");
  Serial.println(sync_attempted ? "OK" : "BŁĄD");
  
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
      
      // Oznacz że synchronizacja jest w toku
      sync_in_progress = true;
      
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
      
      // Zakończ synchronizację
      sync_in_progress = false;
    }
  }

  // Sprawdź automatyczne wylogowanie
  checkAutoLogout();

  // Aktualizuj diodę WiFi status
  updateWiFiLED();

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
  
  // Przed inicjalizacją - krótkie opóźnienie na stabilizację
  delay(500);
  
  // Użyj trybu 1-bitowego, aby uniknąć konfliktu pinów z kamerą
  // Zwiększamy timeout i dodajemy retry logic
  for (int retry = 0; retry < 3; retry++) {
    Serial.print("Proba ");
    Serial.print(retry + 1);
    Serial.println("/3...");
    Serial.flush();
    
    if (SD_MMC.begin("/sdcard", true)) {
      Serial.println("SD_MMC.begin() - SUKCES");
      Serial.flush();
      
      // Sprawdź typ karty
      uint8_t cardType = SD_MMC.cardType();
      Serial.print("Typ karty: ");
      Serial.println(cardType);
      Serial.flush();
      
      if (cardType != CARD_NONE) {
        // Karta SD zainicjalizowana pomyślnie
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Serial.print("Rozmiar karty: ");
        Serial.print(cardSize);
        Serial.println(" MB");
        Serial.flush();
        return; // Sukces
      }
    }
    
    Serial.println("SD_MMC.begin() - BLAD");
    Serial.flush();
    
    // Błąd - spróbuj ponownie po krótkim opóźnieniu
    SD_MMC.end();
    delay(1000);
  }
  
  Serial.println("Wszystkie proby nieudane - kontynuuj bez karty SD");
  Serial.flush();
  // Wszystkie próby nieudane - kontynuuj bez karty SD
  return;
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
  
  // Ustawienia energooszczędne przed połączeniem WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm); // Zmniejsz moc nadawania WiFi
  delay(500);
  
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  int attempts = 0;
  const int max_attempts = 20;
  
  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(1500); // Zwiększony delay dla stabilności zasilania
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

// Funkcja automatycznego wylogowywania pracowników i sprawdzania alarmów
void checkAutoLogout() {
  static unsigned long lastAutoLogoutCheck = 0;
  static unsigned long lastAlarmCheck = 0;
  unsigned long currentTime = millis();
  
  // Sprawdzaj alarmy audio/visual (typ 0) co minutę (60000 ms)
  if (currentTime - lastAlarmCheck >= 60000) {
    lastAlarmCheck = currentTime;
    
    // Sprawdź alarmy typu 0 (audio/visual)
    if (SD_MMC.exists("/czytnik_projekt/data/Alarms.csv")) {
      File alarmsFile = SD_MMC.open("/czytnik_projekt/data/Alarms.csv", FILE_READ);
      if (alarmsFile) {
        String currentTimeStr = getCurrentTimestamp().substring(11, 16); // HH:MM
        
        while (alarmsFile.available()) {
          String line = alarmsFile.readStringUntil('\n');
          line.trim();
          
          if (line.length() > 0) {
            // Format: ID,Type,Time,Description
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            
            if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
              String type = line.substring(comma1 + 1, comma2);
              String time = line.substring(comma2 + 1, comma3);
              type.trim();
              time.trim();
              
              // Jeśli to alarm typu 0 (audio/visual) i czas się zgadza
              if (type == "0" && time == currentTimeStr) {
                logToSD("[ALARM] Alarm audio/visual o " + currentTimeStr);
                
                // Wyślij sygnał dźwiękowy beep3
                sendBuzzerBeep3();
              }
            }
          }
        }
        alarmsFile.close();
      }
    }
  }
  
  // Sprawdzaj auto-logout (typ 2) co godzinę (3600000 ms)
  if (currentTime - lastAutoLogoutCheck >= 3600000) {
    lastAutoLogoutCheck = currentTime;
    
    // Czytaj plik alarmów i sprawdź czy jest alarm typu 2 (auto-logout)
    if (SD_MMC.exists("/czytnik_projekt/data/Alarms.csv")) {
      File alarmsFile = SD_MMC.open("/czytnik_projekt/data/Alarms.csv", FILE_READ);
      if (alarmsFile) {
        String currentTimeStr = getCurrentTimestamp().substring(11, 16); // HH:MM
        
        while (alarmsFile.available()) {
          String line = alarmsFile.readStringUntil('\n');
          line.trim();
          
          if (line.length() > 0) {
            // Format: ID,Type,Time,Description
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            
            if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
              String type = line.substring(comma1 + 1, comma2);
              String time = line.substring(comma2 + 1, comma3);
              type.trim();
              time.trim();
              
              // Jeśli to alarm typu 2 (auto-logout) i czas się zgadza
              if (type == "2" && time == currentTimeStr) {
                logToSD("[AUTO-LOGOUT] Wykonywanie automatycznego wylogowania o " + currentTimeStr);
                
                // Automatyczne wylogowanie wszystkich pracowników
                // Tworzymy wpisy "WYJŚCIE" dla każdego pracownika który ma dzisiaj tylko "WEJŚCIE"
                performAutoLogout();
              }
            }
          }
        }
        alarmsFile.close();
      }
    }
  }
}

// Funkcja wykonująca automatyczne wylogowanie
void performAutoLogout() {
  String currentDate = getCurrentTimestamp().substring(0, 10); // YYYY-MM-DD
  String currentTimestamp = getCurrentTimestamp();
  
  // Sprawdź kto jest zalogowany (ma WEJŚCIE ale nie ma WYJŚCIA)
  if (SD_MMC.exists("/czytnik_projekt/data/Pracownicy_logi.csv")) {
    File logFile = SD_MMC.open("/czytnik_projekt/data/Pracownicy_logi.csv", FILE_READ);
    if (logFile) {
      String workersToLogout = "";
      String line;
      
      // Przeczytaj wszystkie wpisy z dzisiejszego dnia
      while (logFile.available()) {
        line = logFile.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0 && line.startsWith(currentDate)) {
          // Format: timestamp,userId,name,surname,department,action,successful
          int comma1 = line.indexOf(',');
          int comma2 = line.indexOf(',', comma1 + 1);
          int comma3 = line.indexOf(',', comma2 + 1);
          int comma4 = line.indexOf(',', comma3 + 1);
          int comma5 = line.indexOf(',', comma4 + 1);
          int comma6 = line.indexOf(',', comma5 + 1);
          
          if (comma1 > 0 && comma2 > 0 && comma3 > 0 && comma4 > 0 && comma5 > 0 && comma6 > 0) {
            String userId = line.substring(comma1 + 1, comma2);
            String name = line.substring(comma2 + 1, comma3);
            String surname = line.substring(comma3 + 1, comma4);
            String department = line.substring(comma4 + 1, comma5);
            String action = line.substring(comma5 + 1, comma6);
            userId.trim();
            name.trim();
            surname.trim();
            department.trim();
            action.trim();
            
            // Sprawdź czy ten pracownik potrzebuje auto-wylogowania
            if (action == "WEJŚCIE") {
              // Dodaj do listy do wylogowania (jeśli nie ma już WYJŚCIA)
              if (workersToLogout.indexOf(userId + "|") == -1) {
                workersToLogout += userId + "|" + name + "|" + surname + "|" + department + "\n";
              }
            } else if (action == "WYJŚCIE") {
              // Usuń z listy do wylogowania
              int pos = workersToLogout.indexOf(userId + "|");
              if (pos >= 0) {
                int endPos = workersToLogout.indexOf('\n', pos);
                if (endPos > pos) {
                  workersToLogout = workersToLogout.substring(0, pos) + workersToLogout.substring(endPos + 1);
                }
              }
            }
          }
        }
      }
      logFile.close();
      
      // Wykonaj auto-wylogowanie dla każdego pracownika na liście
      if (workersToLogout.length() > 0) {
        logToSD("[AUTO-LOGOUT] Wylogowywanie pracowników: " + workersToLogout);
        
        // Parsuj listę i twórz wpisy WYJŚCIE
        int pos = 0;
        while (pos < workersToLogout.length()) {
          int endPos = workersToLogout.indexOf('\n', pos);
          if (endPos == -1) break;
          
          String workerData = workersToLogout.substring(pos, endPos);
          int pipe1 = workerData.indexOf('|');
          int pipe2 = workerData.indexOf('|', pipe1 + 1);
          int pipe3 = workerData.indexOf('|', pipe2 + 1);
          
          if (pipe1 > 0 && pipe2 > 0 && pipe3 > 0) {
            String userId = workerData.substring(0, pipe1);
            String name = workerData.substring(pipe1 + 1, pipe2);
            String surname = workerData.substring(pipe2 + 1, pipe3);
            String department = workerData.substring(pipe3 + 1);
            
            // Utwórz wpis WYJŚCIE
            String autoLogoutEntry = currentTimestamp + "," + userId + "," + name + "," + surname + "," + department + ",WYJŚCIE,1";
            
            // Zapisz do pliku
            File outFile = SD_MMC.open("/czytnik_projekt/data/Pracownicy_logi.csv", FILE_APPEND);
            if (outFile) {
              outFile.println(autoLogoutEntry);
              outFile.close();
              logToSD("[AUTO-LOGOUT] Wylogowano: " + name + " " + surname);
            }
          }
          
          pos = endPos + 1;
        }
      }
    }
  }
}

// === FUNKCJE OBSŁUGI DIODY WiFi STATUS ===

// Inicjalizacja diody WiFi status
void initWiFiLED() {
  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW); // Początkowy stan: wyłączona
  led_state = false;
  last_led_toggle = millis();
  logToSD("[WIFI_LED] Inicjalizacja diody WiFi status na GPIO " + String(WIFI_LED_PIN));
}

// Aktualizacja stanu diody WiFi
void updateWiFiLED() {
  unsigned long currentTime = millis();
  
  if (!wifi_connected) {
    // Brak połączenia WiFi - miganie powolne (0,3s świeci, 0,5s gaśnie)
    unsigned long interval = led_state ? 300 : 500; // 300ms ON, 500ms OFF
    
    if (currentTime - last_led_toggle >= interval) {
      led_state = !led_state;
      digitalWrite(WIFI_LED_PIN, led_state ? HIGH : LOW);
      last_led_toggle = currentTime;
    }
    
  } else if (sync_in_progress) {
    // Synchronizacja w toku - miganie szybkie (0,2s świeci, 0,2s gaśnie)
    if (currentTime - last_led_toggle >= 200) {
      led_state = !led_state;
      digitalWrite(WIFI_LED_PIN, led_state ? HIGH : LOW);
      last_led_toggle = currentTime;
    }
    
  } else {
    // WiFi połączony, brak synchronizacji - świeci stale
    if (!led_state) {
      led_state = true;
      digitalWrite(WIFI_LED_PIN, HIGH);
    }
  }
}