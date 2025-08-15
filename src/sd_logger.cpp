#include "sd_logger.h"
#include <Arduino.h>

// Funkcja do zapisu logu na karcie SD
void logToSD(String message) {
    // Otwórz plik logów w trybie dopisywania
    File logFile = SD_MMC.open("/czytnik_projekt/system_log.txt", FILE_APPEND);
    if (!logFile) {
        // Nie można otworzyć pliku, nic nie rób
        return;
    }

    // Przygotuj znacznik czasu
    char timeStr[20];
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) { // 5s timeout
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        strcpy(timeStr, "0000-00-00 00:00:00");
    }

    // Zapisz log z datą i godziną
    logFile.println(String(timeStr) + " - " + message);
    logFile.close();
}
