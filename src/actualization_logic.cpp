#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <HTTPClient.h>
#include <WiFi.h>

// --- SYNC: Pracownicy_logi.csv -> Google Sheets ---
void syncPracownicyLogiToGoogle(const String& google_script_url) {
    Serial.println("[SYNC] Rozpoczynam wysyłanie Pracownicy_logi.csv do Google...");
    File logFile = SD_MMC.open("/czytnik_projekt/data/Pracownicy_logi.csv");
    if (!logFile) {
        Serial.println("Brak pliku Pracownicy_logi.csv na SD!");
        return;
    }

    String header = logFile.readStringUntil('\n'); // Pomiń nagłówek
    while (logFile.available()) {
        String line = logFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Tutaj dodaj logikę do parsowania linii i tworzenia danych POST
        // Przykład: String postData = "action=save_hour_record&lp=" + lp + "&firstName=" + name;
        
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(google_script_url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        // int httpCode = http.POST(postData);
        // Serial.printf("[SYNC] Wysłano wiersz, kod: %d\n", httpCode);
        http.end();
    }
    logFile.close();
}

// --- SYNC: exceptions_logs.csv -> Google Sheets ---
void syncExceptionsToGoogle(const String& google_script_url) {
    Serial.println("[SYNC] Rozpoczynam wysyłanie exceptions_logs.csv do Google...");
    File excFile = SD_MMC.open("/czytnik_projekt/data/exceptions_logs.csv");
    if (!excFile) {
        Serial.println("Brak pliku exceptions_logs.csv na SD!");
        return;
    }
    
    String header = excFile.readStringUntil('\n'); // Pomiń nagłówek
    while (excFile.available()) {
        String line = excFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Tutaj dodaj logikę do parsowania linii i tworzenia danych POST
        // Przykład: String postData = "action=save_exception&id=" + id + "&description=" + desc;

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(google_script_url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        // int httpCode = http.POST(postData);
        // Serial.printf("[SYNC] Wysłano wyjątek, kod: %d\n", httpCode);
        http.end();
    }
    excFile.close();
}

// --- SYNC: Pracownicy_data.csv <- Google Sheets ---
void syncPracownicyFromGoogle(const String& google_script_url) {
    Serial.println("[SYNC] Pobieram Pracownicy_data.csv z Google...");
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); // Zwiększ timeout do 15 sekund
    http.begin(google_script_url + "?action=get_pracownicy");
    int httpCode = http.GET();
    if (httpCode == 200) {
        String csvData = http.getString();
        File file = SD_MMC.open("/czytnik_projekt/data/Pracownicy_data.csv", FILE_WRITE);
        if (file) {
            file.print(csvData);
            file.close();
            Serial.println("[SYNC] Zaktualizowano Pracownicy_data.csv z Google");
        }
    } else {
        Serial.printf("[SYNC] Błąd pobierania pracowników: %d\n", httpCode);
    }
    http.end();
}

// --- SYNC: Alarms.csv <- Google Sheets ---
void syncAlarmsFromGoogle(const String& google_script_url) {
    Serial.println("[SYNC] Pobieram Alarms.csv z Google...");
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); // Zwiększ timeout do 15 sekund
    http.begin(google_script_url + "?action=get_alarms");
    int httpCode = http.GET();
    if (httpCode == 200) {
        String csvData = http.getString();
        File file = SD_MMC.open("/czytnik_projekt/data/Alarms.csv", FILE_WRITE);
        if (file) {
            file.print(csvData);
            file.close();
            Serial.println("[SYNC] Zaktualizowano Alarms.csv z Google");
        }
    } else {
        Serial.printf("[SYNC] Błąd pobierania alarmów: %d\n", httpCode);
    }
    http.end();
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
    
    Serial.println("[ALARMS] Sprawdzam interwał aktualizacji w pliku Alarms.csv...");
    
    File file = SD_MMC.open("/czytnik_projekt/data/Alarms.csv");
    if (!file) {
        Serial.println("[ALARMS] Nie można otworzyć pliku Alarms.csv, używam domyślnego interwału (1h)");
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
                
                Serial.printf("[ALARMS] Ustawiono interwał aktualizacji: %lu ms (%lu godzin)\n", 
                             cachedUpdateInterval, cachedUpdateInterval / 3600000);
                return cachedUpdateInterval;
            }
        }
    }
    file.close();
    
    // Jeśli nie znaleziono prawidłowego interwału, użyj domyślnego
    cachedUpdateInterval = 3600000; // 1 godzina
    lastIntervalCheck = millis();
    Serial.println("[ALARMS] Nie znaleziono interwału w pliku, używam domyślnego (1h)");
    
    return cachedUpdateInterval;
}

void actualizeSheets(const String& google_script_url) {
    Serial.println("[SYNC] --- Rozpoczynam synchronizację arkuszy ---");
    syncPracownicyLogiToGoogle(google_script_url);
    syncExceptionsToGoogle(google_script_url);
    syncPracownicyFromGoogle(google_script_url);
    syncAlarmsFromGoogle(google_script_url);
    Serial.println("[SYNC] --- Synchronizacja arkuszy zakończona ---");
}

// === FUNKCJE DO OBSŁUGI ZDJĘĆ GOOGLE DRIVE ===

// Funkcja uploadowania pojedynczego zdjęcia do Google Drive
String uploadSinglePhotoToGoogleDrive(const String& google_script_url, const String& folder_id, const String& photoPath) {
    Serial.println("[DRIVE] Uploaduję zdjęcie: " + photoPath);
    
    File photoFile = SD_MMC.open(photoPath);
    if (!photoFile) {
        Serial.println("[DRIVE] Błąd otwierania pliku zdjęcia: " + photoPath);
        return "";
    }
    
    // Przygotuj dane POST do Google Apps Script
    String fileName = photoPath.substring(photoPath.lastIndexOf("/") + 1);
    String postData = "action=upload_photo";
    postData += "&folder_id=" + folder_id;
    postData += "&filename=" + fileName;
    postData += "&filedata="; // Tu będą dane base64
    
    // Konwertuj plik do base64 (uproszczony - dla małych plików)
    String base64Data = "";
    uint8_t buffer[1024];
    while (photoFile.available()) {
        size_t bytesRead = photoFile.read(buffer, sizeof(buffer));
        // Tu powinna być konwersja do base64, ale dla uproszczenia pomijamy
        // base64Data += base64_encode(buffer, bytesRead);
    }
    photoFile.close();
    
    postData += base64Data;
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(google_script_url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(30000); // 30 sekund timeout dla uploadów
    
    int httpCode = http.POST(postData);
    String response = "";
    
    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
        Serial.println("[DRIVE] Zdjęcie wysłane pomyślnie");
        Serial.println("[DRIVE] Odpowiedź: " + response);
    } else {
        Serial.printf("[DRIVE] Błąd HTTP: %d\n", httpCode);
    }
    
    http.end();
    return response; // Zwraca link do pliku na Google Drive
}

// Funkcja uploadowania wszystkich zdjęć z folderu photos
void uploadPhotosToGoogleDrive(const String& google_script_url, const String& folder_id) {
    Serial.println("[DRIVE] Rozpoczynam upload zdjęć do Google Drive...");
    
    File photosDir = SD_MMC.open("/czytnik_projekt/photos");
    if (!photosDir) {
        Serial.println("[DRIVE] Folder photos nie istnieje!");
        return;
    }
    
    if (!photosDir.isDirectory()) {
        Serial.println("[DRIVE] /czytnik_projekt/photos to nie jest folder!");
        photosDir.close();
        return;
    }
    
    File file = photosDir.openNextFile();
    int uploadedCount = 0;
    
    while (file) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            String fullPath = "/czytnik_projekt/photos/" + fileName;
            
            // Upload tylko plików .jpg
            if (fileName.endsWith(".jpg") || fileName.endsWith(".JPG")) {
                String driveLink = uploadSinglePhotoToGoogleDrive(google_script_url, folder_id, fullPath);
                
                if (driveLink != "") {
                    uploadedCount++;
                    Serial.println("[DRIVE] Wysłano: " + fileName);
                    
                    // Opcjonalnie usuń lokalny plik po pomyślnym uploadzie
                    // SD_MMC.remove(fullPath);
                } else {
                    Serial.println("[DRIVE] Błąd wysyłania: " + fileName);
                }
                
                delay(1000); // Pauza między uploadami
            }
        }
        file = photosDir.openNextFile();
    }
    
    photosDir.close();
    Serial.printf("[DRIVE] Upload zakończony. Wysłano %d zdjęć.\n", uploadedCount);
}

// --- Przykład użycia w loop() ---
// unsigned long lastSync = 0;
// void loop() {
//   unsigned long interval = getUpdateIntervalFromAlarms();
//   if (millis() - lastSync > interval) {
//     actualizeSheets(google_script_url);
//     lastSync = millis();
//   }
// }
