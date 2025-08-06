#include "local_communication.h"
#include "tft_debug.h"
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// Używamy Serial2 do komunikacji z ESP-CAM
// ESP32 WROOM Domyślne piny dla Serial2 to RX: 16, TX: 17
#define CAM_SERIAL Serial2

// Globalny debugger TFT (będzie ustawiony w handleAuthorization)
TFTDebug* globalDebugger = nullptr;

void displayAuthorizationScreen(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Autoryzacja...", tft.width() / 2, tft.height() / 2, 4);
}

void displayResultScreen(TFT_eSPI &tft, const String& message, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(message, tft.width() / 2, tft.height() / 2, 4);
    delay(3000); // Wyświetlaj przez 3 sekundy
}

void handleAuthorization(String id, TFT_eSPI &tft) {
    // Utwórz debugger TFT dla tej sesji autoryzacji
    globalDebugger = new TFTDebug(&tft);
    globalDebugger->clear();
    globalDebugger->addTimestamp("AUTORYZACJA START");
    globalDebugger->println("ID: " + id);

    // Nie inicjalizuj Serial2 ponownie - już zainicjalizowany w setup()
    globalDebugger->println("UART: RX:16, TX:17");
    
    // Wyczyść bufor przed wysłaniem
    int cleared = 0;
    while(CAM_SERIAL.available()) {
        char c = CAM_SERIAL.read();
        cleared++;
    }
    if (cleared > 0) {
        globalDebugger->println("Bufor: " + String(cleared) + " znakow");
    }

    // Tworzenie JSON do wysłania
    StaticJsonDocument<200> doc;
    doc["authorization"] = id;
    String output;
    serializeJson(doc, output);

    globalDebugger->addTimestamp("TX: " + output.substring(0, 25));
    
    CAM_SERIAL.println(output);
    CAM_SERIAL.flush(); // Wymusz natychmiastowe wysłanie

    // Oczekiwanie na odpowiedź - zwiększone do 15 sekund
    String response = "";
    unsigned long startTime = millis();
    int receivedBytes = 0;
    int lineCount = 0;
    
    while (millis() - startTime < 15000) { // Zwiększone do 15 sekund
        if (CAM_SERIAL.available()) {
            char c = CAM_SERIAL.read();
            receivedBytes++;
            
            if (c == '\n' || c == '\r') {
                if (response.length() > 0) {
                    response.trim();
                    lineCount++;
                    
                    // Wyświetl na debug TFT
                    String shortResponse = response.substring(0, 30);
                    globalDebugger->addTimestamp("RX: " + shortResponse);
                    
                    // Sprawdź czy to odpowiedź autoryzacyjna
                    if (response.startsWith("{confirm;") || response.startsWith("{denide}")) {
                        globalDebugger->println("*** ODPOWIEDZ OK! ***");
                        break;
                    } else {
                        // Kategoryzuj otrzymane komunikaty
                        if (response.indexOf("WiFi") >= 0 || response.indexOf("connected") >= 0) {
                            globalDebugger->println("└─ (WiFi log)");
                        } else if (response.indexOf("Google") >= 0 || response.indexOf("Sheets") >= 0) {
                            globalDebugger->println("└─ (Google Sheets)");
                        } else if (response.startsWith("[")) {
                            globalDebugger->println("└─ (system log)");
                        } else if (response.length() > 100) {
                            globalDebugger->println("└─ (inicjalizacja)");
                        } else {
                            globalDebugger->println("└─ (nieznany)");
                        }
                        response = "";
                    }
                }
            } else {
                response += c;
                // Zabezpieczenie przed przepełnieniem
                if (response.length() > 1000) {
                    globalDebugger->println("LINIA ZBYT DLUGA!");
                    response = "";
                }
            }
        }
        
        // Status co 2 sekundy
        if ((millis() - startTime) / 2000 > ((millis() - startTime - 2000) / 2000)) {
            int seconds = (millis() - startTime) / 1000;
            globalDebugger->println("Czekam... " + String(seconds) + "s");
            globalDebugger->println("Odebrano: " + String(receivedBytes) + " B");
        }
        delay(50);
    }
    
    globalDebugger->println("=== PODSUMOWANIE ===");
    globalDebugger->println("Czas: " + String((millis() - startTime) / 1000) + "s");
    globalDebugger->println("Bajty: " + String(receivedBytes));

    if (response == "" || response.length() == 0) {
        globalDebugger->println("BRAK ODPOWIEDZI!");
        globalDebugger->println("Sprawdz polaczenia:");
        globalDebugger->println("ESP-CAM TX->WROOM RX16");
        globalDebugger->println("ESP-CAM RX->WROOM TX17");
        
        // Czekaj 5 sekund na przegląd logów
        delay(5000);
        
        displayResultScreen(tft, "Brak odpowiedzi", TFT_RED);
        
        delete globalDebugger;
        globalDebugger = nullptr;
        return;
    }

    Serial.print("Analizuję odpowiedź: ");
    Serial.println(response);

    if (response.startsWith("{denide}")) {
        Serial.println("Autoryzacja ODRZUCONA przez ESP-CAM");
        displayResultScreen(tft, "Odrzucono", TFT_RED);
    } else if (response.startsWith("{confirm;")) {
        Serial.println("Autoryzacja ZAAKCEPTOWANA przez ESP-CAM");
        
        // Parsowanie odpowiedzi
        String tempResponse = response;
        tempResponse.replace("{confirm;", "");
        tempResponse.replace("}", "");
        int firstSemicolon = tempResponse.indexOf(';');
        
        if (firstSemicolon == -1) {
            Serial.println("BŁĄD: Nieprawidłowy format odpowiedzi confirm");
            displayResultScreen(tft, "Blad formatu", TFT_RED);
            return;
        }
        
        String imie = tempResponse.substring(0, firstSemicolon);
        String nazwisko = tempResponse.substring(firstSemicolon + 1);
        String fullName = imie + " " + nazwisko;
        
        Serial.print("Parsowane dane - Imię: '");
        Serial.print(imie);
        Serial.print("', Nazwisko: '");
        Serial.print(nazwisko);
        Serial.println("'");

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(fullName, tft.width() / 2, tft.height() / 2 - 40, 4);

        // Rysowanie przycisków
        tft.drawRoundRect(30, tft.height() / 2 + 20, 180, 50, 5, TFT_GREEN);
        tft.setTextColor(TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Zatwierdz", 120, tft.height() / 2 + 45, 4);

        tft.drawRoundRect(30, tft.height() / 2 + 90, 180, 50, 5, TFT_RED);
        tft.setTextColor(TFT_RED);
        tft.drawString("Odrzuc", 120, tft.height() / 2 + 115, 4);

        Serial.println("Wyświetlam przyciski potwierdzenia, czekam na decyzję użytkownika...");

        // Oczekiwanie na dotyk
        uint16_t touchX, touchY;
        startTime = millis();
        while (millis() - startTime < 10000) { // Czekaj 10 sekund na decyzję
            if (tft.getTouch(&touchX, &touchY)) {
                Serial.print("Dotyk wykryty na pozycji: X=");
                Serial.print(touchX);
                Serial.print(", Y=");
                Serial.println(touchY);
                
                // Sprawdzenie przycisku "Zatwierdź"
                if (touchX > 30 && touchX < 210 && touchY > tft.height() / 2 + 20 && touchY < tft.height() / 2 + 70) {
                    Serial.println("Użytkownik wybrał ZATWIERDŹ");
                    Serial.println("Wysyłam potwierdzenie do ESP-CAM: {confirmed}");
                    CAM_SERIAL.println("{confirmed}");
                    displayResultScreen(tft, "Zatwierdzono", TFT_GREEN);
                    return;
                }
                // Sprawdzenie przycisku "Odrzuć"
                if (touchX > 30 && touchX < 210 && touchY > tft.height() / 2 + 90 && touchY < tft.height() / 2 + 140) {
                    Serial.println("Użytkownik wybrał ODRZUĆ");
                    Serial.println("Nie wysyłam niczego do ESP-CAM (anulowanie)");
                    displayResultScreen(tft, "Anulowano", TFT_YELLOW);
                    return;
                }
            }
        }
        // Timeout
        Serial.println("TIMEOUT: Użytkownik nie podjął decyzji w ciągu 10 sekund");
        displayResultScreen(tft, "Timeout", TFT_YELLOW);

    } else {
        Serial.print("BŁĄD: Nieznany format odpowiedzi: '");
        Serial.print(response);
        Serial.println("'");
        displayResultScreen(tft, "Blad odpowiedzi", TFT_RED);
    }
    
    // Zwolnij pamięć debuggera
    delete globalDebugger;
    globalDebugger = nullptr;
    
    Serial.println("=== KONIEC AUTORYZACJI ===");
}
