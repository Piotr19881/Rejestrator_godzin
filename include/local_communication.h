#ifndef LOCAL_COMMUNICATION_H
#define LOCAL_COMMUNICATION_H

#include <TFT_eSPI.h>

// Funkcje autoryzacji
void handleAuthorization(String id, TFT_eSPI &tft);
void testPingESPCAM(TFT_eSPI &tft);

// Funkcje pomocnicze
String parseESPCamResponse(String response);

// Funkcje ekranowe  
void displayAuthorizationScreen(TFT_eSPI &tft);
void displayResultScreen(TFT_eSPI &tft, const String& message, uint16_t color);

// Nowe funkcje obsługi gotowości ESP32-CAM
bool waitForESPCamReady(unsigned long timeoutMs = 90000); // 90 sekund timeout
bool isESPCamReady();
void setESPCamReady(bool ready);
String sendCommandToESPCam(const String& command, unsigned long timeoutMs = 5000);

#endif // LOCAL_COMMUNICATION_H
