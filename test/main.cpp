#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "actualization_logic.h"
#include "communication_logic.h"
#include "registration_logic.h"

// Konfiguracja WiFi
String wifi_ssid = "";
String wifi_password = "";
String google_script_url = "";
String workers_sheet_id = "";
String alarms_sheet_id = "";

bool wifi_connected = false;

// Funkcje
bool loadConfig();
bool initializeWiFi();
void testGoogleSheets();

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("[TEST] === TEST POBIERANIA DANYCH Z GOOGLE SHEETS ===");
    
    // Inicjalizacja SD
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[ERROR] Nie można zainicjalizować karty SD");
        return;
    }
    Serial.println("[OK] Karta SD zainicjalizowana");
    
    // Wczytaj konfigurację
    if (loadConfig()) {
        Serial.println("[OK] Konfiguracja wczytana:");
        Serial.println("  WiFi SSID: " + wifi_ssid);
        Serial.println("  Script URL: " + google_script_url);
        Serial.println("  Workers Sheet ID: " + workers_sheet_id);
        Serial.println("  Alarms Sheet ID: " + alarms_sheet_id);
    } else {
        Serial.println("[ERROR] Nie można wczytać konfiguracji");
        return;
    }
    
    // Połącz z WiFi
    if (initializeWiFi()) {
        Serial.println("[OK] WiFi połączony: " + WiFi.localIP().toString());
        
        // Test Google Sheets
        delay(2000);
        testGoogleSheets();
        
    } else {
        Serial.println("[ERROR] Nie można połączyć z WiFi");
    }
    
    Serial.println("[TEST] === KONIEC TESTÓW ===");
}

void loop() {
    // Czekaj
    delay(10000);
}

bool loadConfig() {
    Serial.println("[CONFIG] Otwieranie /czytnik_projekt/config.txt...");
    
    File configFile = SD_MMC.open("/czytnik_projekt/config.txt", FILE_READ);
    if (!configFile) {
        Serial.println("[CONFIG] ERROR: Nie można otworzyć pliku config.txt");
        return false;
    }
    
    String line;
    int lineNum = 0;
    
    while (configFile.available()) {
        line = configFile.readStringUntil('\n');
        line.trim();
        lineNum++;
        
        Serial.println("[CONFIG] Linia " + String(lineNum) + ": " + line);
        
        if (line.startsWith("wifi_ssid=")) {
            wifi_ssid = line.substring(11);
            wifi_ssid.replace("{", "");
            wifi_ssid.replace("}", "");
            wifi_ssid.replace("\"", "");
            wifi_ssid.trim();
            Serial.println("[CONFIG] SSID: " + wifi_ssid);
        }
        else if (line.startsWith("wifi_password=")) {
            wifi_password = line.substring(15);
            wifi_password.replace("{", "");
            wifi_password.replace("}", "");
            wifi_password.replace("\"", "");
            wifi_password.trim();
            Serial.println("[CONFIG] Password: [UKRYTE]");
        }
        else if (line.startsWith("google_script_url=")) {
            google_script_url = line.substring(18);
            google_script_url.replace("{", "");
            google_script_url.replace("}", "");
            google_script_url.replace("\"", "");
            google_script_url.trim();
            Serial.println("[CONFIG] Script URL: " + google_script_url);
        }
        else if (line.startsWith("google_workers_sheet_id=")) {
            workers_sheet_id = line.substring(24);
            workers_sheet_id.replace("{", "");
            workers_sheet_id.replace("}", "");
            workers_sheet_id.replace("\"", "");
            workers_sheet_id.trim();
            Serial.println("[CONFIG] Workers Sheet: " + workers_sheet_id);
        }
        else if (line.startsWith("google_alarms_updatas_sheet_id=")) {
            alarms_sheet_id = line.substring(31);
            alarms_sheet_id.replace("{", "");
            alarms_sheet_id.replace("}", "");
            alarms_sheet_id.replace("\"", "");
            alarms_sheet_id.trim();
            Serial.println("[CONFIG] Alarms Sheet: " + alarms_sheet_id);
        }
    }
    
    configFile.close();
    Serial.println("[CONFIG] Przeczytano " + String(lineNum) + " linii");
    
    return (wifi_ssid.length() > 0 && wifi_password.length() > 0 && google_script_url.length() > 0);
}

bool initializeWiFi() {
    Serial.println("[WIFI] Łączenie z: " + wifi_ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        wifi_connected = true;
        return true;
    } else {
        Serial.println();
        Serial.println("[WIFI] BŁĄD: Status = " + String(WiFi.status()));
        return false;
    }
}

void testGoogleSheets() {
    Serial.println("\n[GOOGLE] === TEST POBIERANIA PRACOWNIKÓW ===");
    
    HTTPClient http;
    
    // KLUCZOWA ZMIANA: Automatyczna obsługa przekierowań HTTP (np. 302)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    // Test 1: Bez Sheet ID
    String url1 = google_script_url + "?action=get_workers";
    Serial.println("[GOOGLE] URL 1: " + url1);
    
    http.begin(url1);
    int httpCode1 = http.GET();
    
    if (httpCode1 == 200) {
        String response1 = http.getString();
        Serial.println("[GOOGLE] Odpowiedź 1 (HTTP " + String(httpCode1) + "):");
        Serial.println(response1);
        Serial.println("[GOOGLE] Długość: " + String(response1.length()) + " bajtów");
    } else {
        Serial.println("[GOOGLE] BŁĄD 1: HTTP " + String(httpCode1));
    }
    
    http.end();
    delay(2000);
    
    // Test 2: Z Sheet ID
    String url2 = google_script_url + "?action=get_workers&workers_sheet_id=" + workers_sheet_id;
    Serial.println("[GOOGLE] URL 2: " + url2);
    
    http.begin(url2);
    int httpCode2 = http.GET();
    
    if (httpCode2 == 200) {
        String response2 = http.getString();
        Serial.println("[GOOGLE] Odpowiedź 2 (HTTP " + String(httpCode2) + "):");
        Serial.println(response2);
        Serial.println("[GOOGLE] Długość: " + String(response2.length()) + " bajtów");
        
        // Parsuj JSON
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, response2);
        
        if (!error) {
            String status = doc["status"];
            Serial.println("[JSON] Status: " + status);
            
            if (doc.containsKey("workers")) {
                JsonArray workers = doc["workers"];
                int count = workers.size();
                Serial.println("[JSON] Liczba pracowników: " + String(count));
                
                for (int i = 0; i < count && i < 3; i++) {
                    JsonObject worker = workers[i];
                    String id = worker["id"];
                    String firstName = worker["firstName"];
                    String lastName = worker["lastName"];
                    String pesel = worker["pesel"];
                    
                    Serial.println("[JSON] Pracownik " + String(i+1) + ": " + id + " - " + firstName + " " + lastName + " (" + pesel + ")");
                }
                
                if (count > 3) {
                    Serial.println("[JSON] ... i " + String(count-3) + " więcej");
                }
            } else {
                Serial.println("[JSON] BŁĄD: Brak klucza 'workers'");
            }
            
            if (doc.containsKey("count")) {
                int reported_count = doc["count"];
                Serial.println("[JSON] Raportowana liczba: " + String(reported_count));
            }
            
        } else {
            Serial.println("[JSON] BŁĄD parsowania: " + String(error.c_str()));
        }
        
    } else {
        Serial.println("[GOOGLE] BŁĄD 2: HTTP " + String(httpCode2));
    }
    
    http.end();
    
    Serial.println("\n[GOOGLE] === TEST ZAKOŃCZONY ===\n");
}