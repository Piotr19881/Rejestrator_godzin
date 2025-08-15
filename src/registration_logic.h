#ifndef REGISTRATION_LOGIC_H
#define REGISTRATION_LOGIC_H

#include <Arduino.h>
#include "communication_logic.h" // Dla struktury LogEntry

// Funkcje kamery
bool initCameraOnDemand();
void deinitCamera();
void takePhoto(String userId);
void createPhotosDirectory();
String capturePhotoToFile(String userId);

// Funkcje zapisu logów
void logWorkEntry(LogEntry entry);
void logException(LogEntry entry, String errorMessage);

// Funkcje pomocnicze
String formatLogLine(LogEntry entry);
String formatExceptionLine(LogEntry entry, String errorMessage);

#endif // REGISTRATION_LOGIC_H
