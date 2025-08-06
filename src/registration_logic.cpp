#include "registration_logic.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h"

// Zmienna do śledzenia stanu inicjalizacji kamery
bool cameraInitialized = false;

// Funkcja do inicjalizacji kamery na żądanie
bool initCameraOnDemand() {
    if (cameraInitialized) {
        return true; // Już zainicjalizowana
    }
    
    Serial.println("[CAMERA] Inicjalizacja kamery na żądanie...");
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;
    config.pin_xclk = 0;
    config.pin_pclk = 22;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_sscb_sda = 26;
    config.pin_sscb_scl = 27;
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    // Ustawienia jakości dla oszczędności pamięci
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] Błąd inicjalizacji kamery: 0x%x\n", err);
        return false;
    }
    
    cameraInitialized = true;
    Serial.println("[CAMERA] Kamera zainicjalizowana pomyślnie");
    return true;
}

// Funkcja do deicjalizacji kamery po użyciu
void deinitCamera() {
    if (cameraInitialized) {
        esp_camera_deinit();
        cameraInitialized = false;
        Serial.println("[CAMERA] Kamera dezaktywowana");
    }
}

// Funkcja do zapisu zdjęcia na karcie SD
void takePhoto(String userId) {
    // Inicjalizuj kamerę tylko gdy jest potrzebna
    if (!initCameraOnDemand()) {
        Serial.println("[CAMERA] Nie można zainicjalizować kamery do zdjęcia");
        return;
    }
    
    Serial.println("[CAMERA] Robienie zdjęcia dla użytkownika: " + userId);
    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) {
        Serial.println("[CAMERA] Błąd pobrania zdjęcia z kamery");
        return;
    }
    
    String path = "/czytnik_projekt/photos/" + userId + "_" + String(millis()) + ".jpg";
    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);
    if(!file) {
        Serial.println("[SD] Błąd zapisu pliku na SD: " + path);
    } else {
        file.write(fb->buf, fb->len);
        Serial.println("[SD] Zapisano zdjęcie: " + path);
    }
    file.close();
    esp_camera_fb_return(fb);
    
    // Dezaktywuj kamerę po użyciu, aby zwolnić zasoby
    deinitCamera();
}

// Funkcja do zapisu wpisu pracy w pliku Pracownicy_logi.csv
void logWorkEntry(LogEntry entry) {
    Serial.println("[LOG] Zapisuję wpis pracy: " + entry.name + " " + entry.surname);
    
    String logLine = formatLogLine(entry);
    
    File file = SD_MMC.open("/czytnik_projekt/data/Pracownicy_logi.csv", FILE_APPEND);
    if (!file) {
        Serial.println("[LOG] Błąd otwarcia pliku Pracownicy_logi.csv");
        return;
    }
    
    file.println(logLine);
    file.close();
    
    Serial.println("[LOG] Zapisano: " + logLine);
}

// Funkcja do zapisu wyjątku w pliku Exception_logs.csv
void logException(LogEntry entry, String errorMessage) {
    Serial.println("[EXCEPTION] Zapisuję wyjątek: " + errorMessage);
    
    String exceptionLine = formatExceptionLine(entry, errorMessage);
    
    File file = SD_MMC.open("/czytnik_projekt/data/Exception_logs.csv", FILE_APPEND);
    if (!file) {
        Serial.println("[EXCEPTION] Błąd otwarcia pliku Exception_logs.csv");
        return;
    }
    
    file.println(exceptionLine);
    file.close();
    
    Serial.println("[EXCEPTION] Zapisano: " + exceptionLine);
}

// Formatowanie linii dla arkusza pracownicy_logi
String formatLogLine(LogEntry entry) {
    // Format: Data/Godzina,ID,Imię,Nazwisko,Dział,Akcja
    return entry.timestamp + "," + 
           entry.userId + "," + 
           entry.name + "," + 
           entry.surname + "," + 
           entry.department + "," + 
           entry.action;
}

// Formatowanie linii dla arkusza exception_logs
String formatExceptionLine(LogEntry entry, String errorMessage) {
    // Format: Data/Godzina,ID,Imię,Nazwisko,Błąd,Szczegóły
    return entry.timestamp + "," + 
           entry.userId + "," + 
           entry.name + "," + 
           entry.surname + "," + 
           entry.action + "," + 
           errorMessage;
}
