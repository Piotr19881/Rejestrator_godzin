#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <ArduinoJson.h>
#include "sd_logger.h"

// --- SYNC: Pracownicy_data.csv <- Google Sheets ---
void syncPracownicyFromGoogle(const String& google_script_url) {
    logToSD("[SYNC] Pobieram dane pracownikow z Google...");
    Serial.println("[SYNC] === POBIERANIE PRACOWNIKÓW Z GOOGLE SHEETS ===");
    
    if (WiFi.status() != WL_CONNECTED) {
        logToSD("[SYNC] BLAD: Brak polaczenia WiFi.");
        return;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000); // 20 sekund

    String url = google_script_url + "?action=get_workers";
    logToSD("[SYNC] URL: " + url);
    Serial.println("[SYNC] Wysyłam żądanie do: " + url);

    http.begin(url);
    int httpCode = http.GET();
    Serial.println("[SYNC] Otrzymany kod HTTP: " + String(httpCode));

    if (httpCode == 200) {
        String payload = http.getString();
        logToSD("[SYNC] Odpowiedz HTTP 200, rozmiar: " + String(payload.length()) + " bajtow.");

        JsonDocument doc; // Aktualizowane z DynamicJsonDocument
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            logToSD("[SYNC] BLAD parsowania JSON: " + String(error.c_str()));
            http.end();
            return;
        }

        if (doc["status"] != "success") {
            logToSD("[SYNC] BLAD: Skrypt zwrocil status bledu.");
            http.end();
            return;
        }

        JsonArray workers = doc["workers"];
        logToSD("[SYNC] Znaleziono " + String(workers.size()) + " pracownikow.");
        
        // NOWE: Wyświetl listę pobranych pracowników
        Serial.println("[SYNC] === LISTA POBRANYCH PRACOWNIKÓW ===");
        int count = 0;

        File file = SD_MMC.open("/czytnik_projekt/data/Pracownicy_data.csv", FILE_WRITE);
        if (!file) {
            logToSD("[SYNC] BLAD: Nie mozna otworzyc pliku Pracownicy_data.csv do zapisu.");
            http.end();
            return;
        }
        
        // Zapisz nagłówek dostosowany do arkusza Google
        file.println("id,firstName,lastName,pesel,rfid1,rfid2,rfid3,status");

        for (JsonObject worker : workers) {
            count++;
            
            // Poprawione nazwy kluczy zgodne z JSON z Google Apps Script
            String id = worker["id"];               // małe litery!
            String firstName = worker["firstName"]; // camelCase!
            String lastName = worker["lastName"];   // camelCase!
            String pesel = worker["pesel"];         // małe litery!
            String rfid1 = worker["rfid1"];         // małe litery!
            String rfid2 = worker["rfid2"];         // małe litery!
            String rfid3 = worker["rfid3"];         // małe litery!
            int status = worker["status"];          // małe litery!

            // NOWE: Wyświetl informacje o pracowniku z wszystkimi RFID-ami
            Serial.println("[SYNC] " + String(count) + ". " + firstName + " " + lastName + 
                          " (ID: " + id + ", PESEL: " + pesel + ", Status: " + String(status) + ")");
            Serial.println("    RFID1: " + rfid1 + ", RFID2: " + rfid2 + ", RFID3: " + rfid3);

            file.printf("%s,%s,%s,%s,%s,%s,%s,%d\n", id.c_str(), firstName.c_str(), lastName.c_str(), pesel.c_str(), rfid1.c_str(), rfid2.c_str(), rfid3.c_str(), status);
        }
        
        file.close();
        Serial.println("[SYNC] === KONIEC LISTY PRACOWNIKÓW ===");
        Serial.println("[SYNC] Zapisano " + String(count) + " pracowników do pliku CSV");
        logToSD("[SYNC] Zaktualizowano plik Pracownicy_data.csv.");

    } else {
        logToSD("[SYNC] BLAD HTTP: " + String(httpCode));
    }
    http.end();
}

// --- SYNC: Alarms.csv <- Google Sheets ---
void syncAlarmsFromGoogle(const String& google_script_url) {
    logToSD("[SYNC] Pobieram dane alarmow z Google...");
    Serial.println("[SYNC] === POBIERANIE ALARMÓW Z GOOGLE SHEETS ===");

    if (WiFi.status() != WL_CONNECTED) {
        logToSD("[SYNC] BLAD: Brak polaczenia WiFi.");
        return;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);

    String url = google_script_url + "?action=get_alarms";
    logToSD("[SYNC] URL: " + url);
    Serial.println("[SYNC] Wysyłam żądanie do: " + url);
    
    http.begin(url);
    int httpCode = http.GET();
    Serial.println("[SYNC] Otrzymany kod HTTP: " + String(httpCode));

    if (httpCode == 200) {
        String payload = http.getString();
        logToSD("[SYNC] Odpowiedz HTTP 200, rozmiar: " + String(payload.length()) + " bajtow.");
        
        // NOWE: Wyświetl zawartość pobranych alarmów
        Serial.println("[SYNC] === POBRANE ALARMY Z GOOGLE SHEETS ===");
        Serial.println("[SYNC] Surowa odpowiedź:");
        Serial.println(payload);
        Serial.println("[SYNC] === KONIEC ALARMÓW ===");
        
        // Zapisujemy bezpośrednio, bo format alarmów może być prostszy (np. CSV)
        // lub wymagać innej logiki parsowania, której na razie nie implementujemy.
        File file = SD_MMC.open("/czytnik_projekt/data/Alarms.csv", FILE_WRITE);
        if (file) {
            file.print(payload);
            file.close();
            Serial.println("[SYNC] Zapisano " + String(payload.length()) + " bajtów do Alarms.csv");
            logToSD("[SYNC] Zaktualizowano Alarms.csv z Google.");
        } else {
            logToSD("[SYNC] BLAD: Nie mozna otworzyc pliku Alarms.csv do zapisu.");
        }
    } else {
        logToSD("[SYNC] BLAD HTTP: " + String(httpCode));
    }
    http.end();
}

// --- SYNC: Wysyłanie logów i wyjątków ---
void syncLogToGoogle(const String& google_script_url, const String& sheet_id, const String& logData, const String& logType) {
    if (WiFi.status() != WL_CONNECTED) {
        logToSD("[SYNC] Brak WiFi, nie mozna wyslac loga: " + logType);
        return;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(google_script_url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "action=save_" + logType + "_record&sheet_id=" + sheet_id + "&data=" + logData;
    
    int httpCode = http.POST(postData);
    if (httpCode == 200) {
        logToSD("[SYNC] Wyslano log " + logType + " do Google.");
    } else {
        logToSD("[SYNC] BLAD wysylania loga " + logType + ", kod: " + String(httpCode));
    }
    http.end();
}

void syncPracownicyLogiToGoogle(const String& google_script_url, const String& sheet_id) {
    logToSD("[SYNC] Wysylanie Pracownicy_logi.csv do Google...");
    File logFile = SD_MMC.open("/czytnik_projekt/data/Pracownicy_logi.csv");
    if (!logFile) {
        logToSD("[SYNC] Brak pliku Pracownicy_logi.csv.");
        return;
    }

    logFile.readStringUntil('\n'); // Pomiń nagłówek
    while (logFile.available()) {
        String line = logFile.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            syncLogToGoogle(google_script_url, sheet_id, line, "hour");
            delay(500); // Mała pauza między wysyłkami
        }
    }
    logFile.close();
    // Opcjonalnie: wyczyść plik po wysłaniu
    // SD_MMC.remove("/czytnik_projekt/data/Pracownicy_logi.csv");
}

void syncExceptionsToGoogle(const String& google_script_url, const String& sheet_id) {
    logToSD("[SYNC] Wysylanie exceptions_logs.csv do Google...");
    File excFile = SD_MMC.open("/czytnik_projekt/data/exceptions_logs.csv");
    if (!excFile) {
        logToSD("[SYNC] Brak pliku exceptions_logs.csv.");
        return;
    }
    
    excFile.readStringUntil('\n'); // Pomiń nagłówek
    while (excFile.available()) {
        String line = excFile.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            syncLogToGoogle(google_script_url, sheet_id, line, "exception");
            delay(500);
        }
    }
    excFile.close();
    // Opcjonalnie: wyczyść plik po wysłaniu
    // SD_MMC.remove("/czytnik_projekt/data/exceptions_logs.csv");
}

// Zmienna buforująca interwał aktualizacji (aby nie czytać za każdym razem z SD)
unsigned long cachedUpdateInterval = 3600000; // Domyślnie 1 godzina
unsigned long lastIntervalCheck = 0;
const unsigned long INTERVAL_CHECK_PERIOD = 300000; // Sprawdzaj interwał co 5 minut

unsigned long getUpdateIntervalFromAlarms() {
    // Sprawdź interwał tylko co 5 minut, aby nie obciążać karty SD
    if (millis() - lastIntervalCheck < INTERVAL_CHECK_PERIOD) {
        return cachedUpdateInterval;
    }
    
    logToSD("[ALARMS] Sprawdzam interwal aktualizacji w Alarms.csv...");
    
    File file = SD_MMC.open("/czytnik_projekt/data/Alarms.csv");
    if (!file) {
        logToSD("[ALARMS] Nie mozna otworzyc Alarms.csv, uzywam domyslnego interwalu (1h)");
        cachedUpdateInterval = 3600000; // 1 godzina
        lastIntervalCheck = millis();
        return cachedUpdateInterval;
    }
    
    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.indexOf("*/") == 0) {
            int pos = line.indexOf(",");
            if (pos > 2) {
                String intervalStr = line.substring(2, pos);
                unsigned long interval = intervalStr.toInt() * 3600000; // Konwersja z godzin na milisekundy
                file.close();
                
                cachedUpdateInterval = interval > 0 ? interval : 3600000;
                lastIntervalCheck = millis();
                
                logToSD("[ALARMS] Ustawiono interwal aktualizacji: " + String(cachedUpdateInterval) + " ms");
                return cachedUpdateInterval;
            }
        }
    }
    file.close();
    
    // Jeśli nie znaleziono prawidłowego interwału, użyj domyślnego
    cachedUpdateInterval = 3600000; // 1 godzina
    lastIntervalCheck = millis();
    logToSD("[ALARMS] Nie znaleziono interwalu, uzywam domyslnego (1h)");
    
    return cachedUpdateInterval;
}

void actualizeSheets(const String& google_script_url, const String& workers_sheet_id, const String& alarms_sheet_id, const String& hour_log_sheet_id, const String& exception_log_sheet_id) {
    logToSD("[SYNC] --- Rozpoczynam pelna synchronizacje arkuszy ---");
    syncPracownicyLogiToGoogle(google_script_url, hour_log_sheet_id);
    syncExceptionsToGoogle(google_script_url, exception_log_sheet_id);
    syncPracownicyFromGoogle(google_script_url);
    syncAlarmsFromGoogle(google_script_url);
    logToSD("[SYNC] --- Synchronizacja arkuszy zakonczona ---");
}

// === FUNKCJE DO OBSŁUGI ZDJĘĆ GOOGLE DRIVE ===

// Funkcja uploadowania pojedynczego zdjęcia do Google Drive
String uploadSinglePhotoToGoogleDrive(const String& google_script_url, const String& folder_id, const String& photoPath) {
    logToSD("[DRIVE] Uploaduje zdjecie: " + photoPath);
    
    File photoFile = SD_MMC.open(photoPath);
    if (!photoFile) {
        logToSD("[DRIVE] Blad otwierania pliku zdjecia: " + photoPath);
        return "";
    }
    
    // Przygotuj dane POST do Google Apps Script
    String fileName = photoPath.substring(photoPath.lastIndexOf("/") + 1);
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(google_script_url);
    http.addHeader("Content-Type", "application/upload"); // Specjalny header dla skryptu
    http.addHeader("File-Name", fileName);
    http.addHeader("Folder-Id", folder_id);
    http.setTimeout(60000); // 60 sekund timeout dla uploadów
    
    int httpCode = http.sendRequest("POST", &photoFile, photoFile.size());
    
    String response = "";
    
    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
        logToSD("[DRIVE] Zdjecie wyslane pomyslnie. Odpowiedz: " + response);
    } else {
        logToSD("[DRIVE] BLAD HTTP: " + String(httpCode) + " | " + http.errorToString(httpCode));
    }
    
    http.end();
    photoFile.close();
    return response; // Zwraca link do pliku na Google Drive
}

// Funkcja uploadowania wszystkich zdjęć z folderu photos
void uploadPhotosToGoogleDrive(const String& google_script_url, const String& folder_id) {
    logToSD("[DRIVE] Rozpoczynam upload zdjec do Google Drive...");
    
    File photosDir = SD_MMC.open("/czytnik_projekt/photos");
    if (!photosDir || !photosDir.isDirectory()) {
        logToSD("[DRIVE] Folder /czytnik_projekt/photos nie istnieje lub nie jest folderem!");
        if(photosDir) photosDir.close();
        return;
    }
    
    File file = photosDir.openNextFile();
    int uploadedCount = 0;
    
    while (file) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            String fullPath = String(file.path());
            
            // Upload tylko plików .jpg
            if (fileName.endsWith(".jpg") || fileName.endsWith(".JPG")) {
                String driveLink = uploadSinglePhotoToGoogleDrive(google_script_url, folder_id, fullPath);
                
                if (driveLink != "") {
                    uploadedCount++;
                    logToSD("[DRIVE] Wyslano: " + fileName);
                    
                    // Usuń lokalny plik po pomyślnym uploadzie
                    if (!SD_MMC.remove(fullPath)) {
                        logToSD("[DRIVE] BLAD: Nie mozna usunac pliku " + fullPath);
                    } else {
                        logToSD("[DRIVE] Usunieto lokalny plik " + fullPath);
                    }
                } else {
                    logToSD("[DRIVE] Blad wysylania: " + fileName);
                }
                
                delay(1000); // Pauza między uploadami
            }
        }
        file = photosDir.openNextFile();
    }
    
    photosDir.close();
    logToSD("[DRIVE] Upload zakonczony. Wysłano " + String(uploadedCount) + " zdjec.");
}

// --- Przykład użycia w loop() ---
// unsigned long lastSync = 0;
// void loop() {
//   unsigned long interval = getUpdateIntervalFromAlarms();
//   if (millis() - lastSync > interval) {
//     actualizeSheets(google_script_url, workers_sheet_id, alarms_sheet_id, hour_log_sheet_id, exception_log_sheet_id);
//     lastSync = millis();
//   }
// }
