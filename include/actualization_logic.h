#ifndef ACTUALIZATION_LOGIC_H
#define ACTUALIZATION_LOGIC_H

#include <Arduino.h>

// Funkcje synchronizacji z Google Sheets
void syncPracownicyFromGoogle(const String& google_script_url);
void syncAlarmsFromGoogle(const String& google_script_url);

// Inne funkcje aktualizacji
void syncLogToGoogle(const String& google_script_url, const String& alarms_sheet_id, const String& logType, const String& logData);
void syncPracownicyLogiToGoogle(const String& google_script_url, const String& alarms_sheet_id);
void syncExceptionsToGoogle(const String& google_script_url, const String& alarms_sheet_id);
unsigned long getUpdateIntervalFromAlarms();
void actualizeSheets(const String& google_script_url, const String& workers_sheet_id, const String& alarms_sheet_id, const String& log_sheet_id, const String& exception_sheet_id);

// Funkcje uploadowania zdjęć
String uploadSinglePhotoToGoogleDrive(const String& google_drive_folder_id, const String& photoPath, const String& fileName);
void uploadPhotosToGoogleDrive(const String& google_drive_folder_id, const String& google_drive_upload_url);

#endif // ACTUALIZATION_LOGIC_H
