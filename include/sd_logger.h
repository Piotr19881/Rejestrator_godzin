#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SD_MMC.h>
#include <time.h>

// Deklaracja funkcji do zapisu logu na karcie SD
void logToSD(String message);

#endif // SD_LOGGER_H
