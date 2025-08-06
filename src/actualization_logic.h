#pragma once
#include <Arduino.h>

void syncPracownicyLogiToGoogle(const String& google_script_url);
void syncExceptionsToGoogle(const String& google_script_url);
void syncPracownicyFromGoogle(const String& google_script_url);
void syncAlarmsFromGoogle(const String& google_script_url);
unsigned long getUpdateIntervalFromAlarms();
void actualizeSheets(const String& google_script_url);
