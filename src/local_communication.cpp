#include "local_communication.h"
#include "dual_debug.h"
#include "cards_mapping.h"
#include "led.h"
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// Używamy Serial2 do komunikacji z ESP-CAM
// ESP32 WROOM Domyślne piny dla Serial2 to RX: 16, TX: 17
#define CAM_SERIAL Serial2

// Globalny dual debugger (TFT + Serial jednocześnie)
DualDebug* globalDebugger = nullptr;

// Funkcja parsowania odpowiedzi ESP32-CAM (ignoruje prefiksy logów)
String parseESPCamResponse(String response) {
    response.trim();
    
    // Usuń prefiksy logów ESP32-CAM
    if (response.startsWith("[TX TO WROOM]: ")) {
        response = response.substring(15); // Usuń "[TX TO WROOM]: "
    }
    if (response.startsWith("[RX FROM WROOM]: ")) {
        response = response.substring(17); // Usuń "[RX FROM WROOM]: "
    }
    if (response.startsWith("[AUTH")) {
        return ""; // Ignoruj logi autoryzacyjne
    }
    if (response.startsWith("[VERIFY")) {
        return ""; // Ignoruj logi weryfikacyjne
    }
    if (response.startsWith("[EXCEPTION")) {
        return ""; // Ignoruj logi błędów
    }
    
    // Sprawdź format odpowiedzi autoryzacyjnej
    if (response.startsWith("{confirm;") || response == "{denide}") {
        return response;
    }
    
    return ""; // Nieznany/ignorowany format
}

// Funkcja pomocnicza sprawdzania czy string to liczba
bool isNumeric(String str) {
  for (int i = 0; i < str.length(); i++) {
    if (!isDigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

// Funkcja mapowania różnych formatów ID
String mapCardIDFormat(String cardID) {
  // Sprawdź czy to już jest PESEL (11 cyfr)
  if (cardID.length() == 11 && isNumeric(cardID)) {
    return cardID; // To już jest PESEL
  }
  
  // Użyj funkcji mapowania z cards_mapping.h
  String mappedPESEL = mapRFIDtoPESEL(cardID);
  
  if (mappedPESEL != cardID) {
    Serial.print("[ID-MAPPING] RFID ");
    Serial.print(cardID);
    Serial.print(" -> PESEL ");
    Serial.println(mappedPESEL);
    return mappedPESEL;
  }
  
  Serial.print("[ID-MAPPING] Brak mapowania dla ");
  Serial.print(cardID);
  Serial.println(" - używam oryginalnego ID");
  
  // Jeśli nie ma mapowania, zwróć oryginalny ID
  return cardID;
}

void displayAuthorizationScreen(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Autoryzacja...", tft.width() / 2, tft.height() / 2, 4);
    
    // Włącz LED podczas weryfikacji
    ledOn();
}

void displayResultScreen(TFT_eSPI &tft, const String& message, uint16_t color) {
    // Wyłącz LED po zakończeniu weryfikacji
    ledOff();
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(message, tft.width() / 2, tft.height() / 2, 4);
    delay(3000); // Wyświetlaj przez 3 sekundy
}

void handleAuthorization(String id, TFT_eSPI &tft) {
    // Sprawdź gotowość ESP32-CAM
    if (!isESPCamReady()) {
        // Utwórz dual debugger dla komunikatu błędu
        globalDebugger = new DualDebug(&tft);
        globalDebugger->clear();
        globalDebugger->addTimestamp("=== BŁĄD AUTORYZACJI ===");
        globalDebugger->println("ESP32-CAM nie gotowy!");
        globalDebugger->println("Nie można autoryzować ID: " + id);
        globalDebugger->println("");
        globalDebugger->println("Możliwe przyczyny:");
        globalDebugger->println("- ESP32-CAM się restartuje");
        globalDebugger->println("- Brak połączenia WiFi");
        globalDebugger->println("- Błąd Google Sheets");
        globalDebugger->println("- Uszkodzone połączenie UART");
        globalDebugger->println("");
        globalDebugger->println("Spróbuj ponownie za moment");
        delay(5000);
        delete globalDebugger;
        return;
    }
    
    // Mapuj ID na właściwy format (RFID -> PESEL)
    String mappedID = mapCardIDFormat(id);
    
    // Utwórz dual debugger (TFT + Serial jednocześnie)
    globalDebugger = new DualDebug(&tft);
    globalDebugger->clear();
    globalDebugger->addTimestamp("=== AUTORYZACJA START ===");
    globalDebugger->println("ID oryginalny: " + id);
    globalDebugger->println("ID zmapowany: " + mappedID);
    globalDebugger->println("UART: RX:16, TX:17 @ 115200");
    globalDebugger->println("ESP32-CAM: GOTOWY ✓");
    
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
    doc["authorization"] = mappedID; // Używaj zmapowanego ID
    String output;
    serializeJson(doc, output);

    globalDebugger->addTimestamp("TX: " + output.substring(0, 25));
    
    CAM_SERIAL.println(output);
    CAM_SERIAL.flush(); // Wymusz natychmiastowe wysłanie

    // Oczekiwanie na odpowiedź - zwiększone do 45 sekund (ESP32-CAM potrzebuje ~30s)
    String response = "";
    unsigned long startTime = millis();
    int receivedBytes = 0;
    int lineCount = 0;
    
    while (millis() - startTime < 45000) { // Zwiększone do 45 sekund dla ESP32-CAM
        if (CAM_SERIAL.available()) {
            char c = CAM_SERIAL.read();
            receivedBytes++;
            
            if (c == '\n' || c == '\r') {
                if (response.length() > 0) {
                    response.trim();
                    lineCount++;
                    
                    // Parsuj odpowiedź (ignoruj prefiksy ESP32-CAM)
                    String parsedResponse = parseESPCamResponse(response);
                    
                    // Wyświetl na debug TFT
                    String shortResponse = response.substring(0, 30);
                    globalDebugger->addTimestamp("RX: " + shortResponse);
                    
                    // Sprawdź czy to odpowiedź autoryzacyjna
                    if (!parsedResponse.isEmpty()) {
                        globalDebugger->println("*** ODPOWIEDZ OK! ***");
                        response = parsedResponse; // Użyj oczyszczonej odpowiedzi
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

// Nowa funkcja do testowania komunikacji
void testPingESPCAM(TFT_eSPI &tft) {
    DualDebug* pingDebugger = new DualDebug(&tft);
    pingDebugger->clear();
    pingDebugger->addTimestamp("=== TEST PING ESP-CAM ===");
    
    // Wyczyść bufor
    int cleared = 0;
    while(CAM_SERIAL.available()) {
        char c = CAM_SERIAL.read();
        cleared++;
    }
    if (cleared > 0) {
        pingDebugger->println("Bufor: " + String(cleared) + " znakow");
    }
    
    // Wyślij ping
    StaticJsonDocument<100> doc;
    doc["test"] = "ping";
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    
    pingDebugger->addTimestamp("TX: " + jsonOutput);
    CAM_SERIAL.println(jsonOutput);
    CAM_SERIAL.flush();
    
    // Czekaj na odpowiedź
    String response = "";
    unsigned long startTime = millis();
    bool gotPong = false;
    
    while (millis() - startTime < 10000) { // 10 sekund timeout
        if (CAM_SERIAL.available()) {
            char c = CAM_SERIAL.read();
            
            if (c == '\n' || c == '\r') {
                if (response.length() > 0) {
                    response.trim();
                    pingDebugger->addTimestamp("RX: " + response.substring(0, 25));
                    
                    if (response.indexOf("pong") >= 0 || response.indexOf("alive") >= 0) {
                        pingDebugger->println("*** PING OK! ***");
                        gotPong = true;
                        break;
                    } else {
                        pingDebugger->println("└─ (inne)");
                    }
                    response = "";
                }
            } else {
                response += c;
                if (response.length() > 200) {
                    pingDebugger->println("LINIA ZBYT DLUGA!");
                    response = "";
                }
            }
        }
        
        // Status co 2 sekundy
        if ((millis() - startTime) % 2000 == 0) {
            int seconds = (millis() - startTime) / 1000;
            pingDebugger->println("Czekam... " + String(seconds) + "s");
        }
        
        delay(50);
    }
    
    pingDebugger->println("=== WYNIK PING ===");
    if (gotPong) {
        pingDebugger->println("ESP-CAM: ONLINE");
        displayResultScreen(tft, "ESP-CAM Online", TFT_GREEN);
    } else {
        pingDebugger->println("ESP-CAM: OFFLINE");
        pingDebugger->println("Sprawdz polaczenia!");
        displayResultScreen(tft, "ESP-CAM Offline", TFT_RED);
    }
    
    delay(3000);
    delete pingDebugger;
}

// ===========================================
// NOWE FUNKCJE OBSŁUGI GOTOWOŚCI ESP32-CAM
// ===========================================

// Zmienna globalna przechowująca stan gotowości ESP32-CAM
static bool espCamReadyState = false;

bool isESPCamReady() {
    return espCamReadyState;
}

void setESPCamReady(bool ready) {
    espCamReadyState = ready;
    if (ready) {
        Serial.println("*** ESP32-CAM GOTOWY DO KOMUNIKACJI! ***");
    } else {
        Serial.println("*** ESP32-CAM NIE GOTOWY - CZEKAM NA SYGNAŁ ***");
    }
}

bool waitForESPCamReady(unsigned long timeoutMs) {
    Serial.println("=== OCZEKIWANIE NA GOTOWOŚĆ ESP32-CAM ===");
    Serial.println("Szukam sygnału {\"ready\"} od ESP32-CAM...");
    
    unsigned long startTime = millis();
    String receivedData = "";
    int attemptCount = 0;
    
    // Wyczyść bufor
    while(Serial2.available()) {
        Serial2.read();
    }
    
    while (millis() - startTime < timeoutMs) {
        // Co 10 sekund wyślij ping żeby ESP32-CAM wiedział że czekamy
        if ((millis() - startTime) % 10000 == 0) {
            attemptCount++;
            Serial.print("Ping ");
            Serial.print(attemptCount);
            Serial.print("/");
            Serial.print(timeoutMs/10000);
            Serial.println(": {\"test\":\"ping\"}");
            Serial2.println("{\"test\":\"ping\"}");
            Serial2.flush();
        }
        
        // Sprawdź odpowiedzi
        if (Serial2.available()) {
            char c = Serial2.read();
            
            if (c == '\n' || c == '\r') {
                if (receivedData.length() > 0) {
                    receivedData.trim();
                    Serial.print("RX: '");
                    Serial.print(receivedData);
                    Serial.println("'");
                    
                    // Sprawdź czy to sygnał gotowości (ignoruj prefiksy ESP32-CAM)
                    String cleanMsg = receivedData;
                    if (cleanMsg.startsWith("[TX TO WROOM]: ")) {
                        cleanMsg = cleanMsg.substring(15);
                    }
                    
                    if (cleanMsg.indexOf("\"ready\"") >= 0 || 
                        cleanMsg.indexOf("ready") >= 0) {
                        Serial.println("*** OTRZYMANO SYGNAŁ GOTOWOŚCI! ***");
                        setESPCamReady(true);
                        return true;
                    }
                    
                    // Sprawdź inne sygnały które oznaczają gotowość
                    if (cleanMsg.indexOf("pong") >= 0 || 
                        cleanMsg.indexOf("alive") >= 0 ||
                        cleanMsg.indexOf("esp32cam_alive") >= 0 ||
                        cleanMsg.startsWith("{confirm") ||
                        cleanMsg.startsWith("{denide")) {
                        Serial.println("*** ESP32-CAM ODPOWIADA - GOTOWY! ***");
                        setESPCamReady(true);
                        return true;
                    }
                    
                    // Loguj inne komunikaty
                    if (receivedData.indexOf("WiFi") >= 0) {
                        Serial.println("  └─ (ESP32-CAM łączy się z WiFi...)");
                    } else if (receivedData.indexOf("Google") >= 0) {
                        Serial.println("  └─ (ESP32-CAM łączy się z Google Sheets...)");
                    } else if (receivedData.length() > 50) {
                        Serial.println("  └─ (Log inicjalizacji ESP32-CAM)");
                    }
                    
                    receivedData = "";
                }
            } else {
                receivedData += c;
                if (receivedData.length() > 500) {
                    Serial.println("[UWAGA] Linia zbyt długa, resetuję bufor");
                    receivedData = "";
                }
            }
        }
        
        // Pokaż progress co 5 sekund
        if ((millis() - startTime) % 5000 == 0 && (millis() - startTime) > 0) {
            int elapsed = (millis() - startTime) / 1000;
            int remaining = (timeoutMs - (millis() - startTime)) / 1000;
            Serial.print("Czekam ");
            Serial.print(elapsed);
            Serial.print("s, pozostało ");
            Serial.print(remaining);
            Serial.println("s...");
        }
        
        delay(100);
    }
    
    Serial.println("*** TIMEOUT! ESP32-CAM NIE GOTOWY ***");
    setESPCamReady(false);
    return false;
}

String sendCommandToESPCam(const String& command, unsigned long timeoutMs) {
    if (!isESPCamReady()) {
        Serial.println("⚠️  ESP32-CAM nie gotowy! Nie wysyłam komendy: " + command);
        return "ERROR: ESP32-CAM not ready";
    }
    
    Serial.println("📤 Wysyłam do ESP32-CAM: " + command);
    Serial2.println(command);
    Serial2.flush();
    
    // Czekaj na odpowiedź
    unsigned long startTime = millis();
    String response = "";
    String receivedData = "";
    
    while (millis() - startTime < timeoutMs) {
        if (Serial2.available()) {
            char c = Serial2.read();
            
            if (c == '\n' || c == '\r') {
                if (receivedData.length() > 0) {
                    receivedData.trim();
                    
                    // Parsuj odpowiedź (ignoruj prefiksy ESP32-CAM)
                    String parsedResponse = parseESPCamResponse(receivedData);
                    
                    // Sprawdź czy to odpowiedź na nasze polecenie
                    if (!parsedResponse.isEmpty() && 
                        (parsedResponse.startsWith("{confirm") || 
                         parsedResponse.startsWith("{denide") ||
                         parsedResponse.indexOf("pong") >= 0)) {
                        Serial.println("📥 Odpowiedź ESP32-CAM: " + parsedResponse);
                        return parsedResponse;
                    }
                    
                    // Ignoruj logi systemowe
                    if (!receivedData.startsWith("[") && 
                        receivedData.indexOf("WiFi") < 0 &&
                        receivedData.indexOf("Google") < 0) {
                        response = receivedData; // Zachowaj jako potencjalną odpowiedź
                    }
                    
                    receivedData = "";
                }
            } else {
                receivedData += c;
                if (receivedData.length() > 500) {
                    receivedData = "";
                }
            }
        }
        delay(50);
    }
    
    if (response.length() > 0) {
        Serial.println("📥 Odpowiedź ESP32-CAM (timeout): " + response);
        return response;
    }
    
    Serial.println("⏰ Timeout! Brak odpowiedzi od ESP32-CAM");
    return "TIMEOUT";
}
