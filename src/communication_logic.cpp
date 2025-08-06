#include "communication_logic.h"
#include "registration_logic.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>

// Pin wbudowanej diody LED na ESP32-CAM - MOŻE BYĆ BŁĘDNY!
// #define LED_BUILTIN 4  // WYŁĄCZONE - może powoduje konflikt
#define LED_BUILTIN 33  // PRÓBA INNEGO PINU (pin który na pewno nie jest używany)

// Zmienne globalne dla stanu komunikacji
bool awaitingConfirmation = false;
LogEntry pendingLogEntry;

// Zmienne do kontroli częstotliwości migania LED
unsigned long lastLEDSignal = 0;
const unsigned long LED_COOLDOWN = 1000; // 1 sekunda przerwy między miganiami

// Funkcje pomocnicze do migania diodą
void blinkLED(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, LOW);   // Zapal diodę (LOW = ON na ESP32-CAM)
        delay(duration);
        digitalWrite(LED_BUILTIN, HIGH);  // Zgaś diodę (HIGH = OFF na ESP32-CAM)
        if (i < times - 1) {
            delay(duration);
        }
    }
}

void signalDataReceived() {
    // TYMCZASOWO WYŁĄCZONE - DEBUG PROBLEMU LED
    Serial.println("[LED] Sygnał: odebrano dane (LED WYŁĄCZONY)");
    // blinkLED(1, 100);  // WYŁĄCZONE
}

void signalResponseSent() {
    // TYMCZASOWO WYŁĄCZONE - DEBUG PROBLEMU LED
    Serial.println("[LED] Sygnał: wysłano odpowiedź (LED WYŁĄCZONY)");
    // blinkLED(1, 50);  // WYŁĄCZONE
}

void setupCommunicationLogic() {
    // TYMCZASOWO CAŁKOWICIE WYŁĄCZONE - DEBUG PROBLEMU
    Serial.println("[COMM] LED CAŁKOWICIE WYŁĄCZONY - nie inicjalizuję pinu LED");
    
    // ESP32-CAM: używamy pinów 1 (TX) i 3 (RX) - domyślny UART0
    // ESP WROOM: używa pinów 16 (TX) i 17 (RX) - UART2
    Serial2.begin(115200, SERIAL_8N1, 3, 1);  // RX=3, TX=1 dla ESP32-CAM
    Serial.println("[COMM] Komunikacja z ESP WROOM zainicjalizowana (ESP32-CAM piny 1/3, ESP WROOM piny 16/17, 115200 baud)");
    Serial.println("[COMM] Oczekuję na dane z ESP WROOM...");
    
    // Wyślij komunikat gotowości do ESP-WROOM - BEZ MIGANIA LED
    delay(2000); // Poczekaj 2 sekundy na stabilizację
    Serial2.println("{ready}");
    Serial.println("[COMM] Wysłano sygnał gotowości: {ready}");
    
    // Wysyłaj sygnał gotowości co 5 sekund przez pierwszą minutę - BEZ MIGANIA LED
    for (int i = 0; i < 12; i++) {
        delay(5000);
        Serial2.println("{ready}");
        Serial.println("[COMM] Ping gotowości #" + String(i+1));
    }
    
    Serial.println("[COMM] System gotowy do komunikacji - LED WYŁĄCZONY!");
}

void handleCommunication() {
    static unsigned long lastDebugTime = 0;
    
    // Debug co 10 sekund
    if (millis() - lastDebugTime > 10000) {
        Serial.println("[COMM] Nasłuchuję na Serial2... (ESP32-CAM piny 1/3, ESP WROOM piny 16/17)");
        lastDebugTime = millis();
    }
    
    if (Serial2.available()) {
        String jsonData = Serial2.readStringUntil('\n');
        jsonData.trim();
        
        // Sygnał LED po odebraniu danych
        signalDataReceived();
        
        Serial.println("=== ODEBRANO DANE ===");
        Serial.println("[COMM] RAW DATA: '" + jsonData + "'");
        Serial.println("[COMM] Długość: " + String(jsonData.length()));
        Serial.println("[COMM] Pierwszy znak: '" + String(jsonData.charAt(0)) + "' ASCII:" + String((int)jsonData.charAt(0)));
        Serial.println("[COMM] Ostatni znak: '" + String(jsonData.charAt(jsonData.length()-1)) + "' ASCII:" + String((int)jsonData.charAt(jsonData.length()-1)));
        
        // Loguj wszystkie odebrane dane
        logCommunication("DATA_RECEIVED", "'" + jsonData + "' [" + String(jsonData.length()) + " bajtów]");
        
        // Wyświetl każdy znak w hex
        Serial.print("[COMM] HEX: ");
        for (int i = 0; i < jsonData.length(); i++) {
            Serial.print(String(jsonData.charAt(i), HEX) + " ");
        }
        Serial.println();

        // NAJPIERW SPRAWDŹ PING-PONG I TESTY (przed parsowaniem JSON)
        if (jsonData.indexOf("ping") != -1 || jsonData.indexOf("PING") != -1 || 
            jsonData.indexOf("test") != -1 || jsonData.indexOf("TEST") != -1) {
            handlePingPong(jsonData);
            return; // Zakończ przetwarzanie - to był test komunikacji
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonData);

        if (error) {
            Serial.print("[COMM] Błąd parsowania JSON: ");
            Serial.println(error.c_str());
            logCommunication("JSON_PARSE_ERROR", String(error.c_str()) + " - dane: '" + jsonData + "'");
            return;
        }

        // Obsługa żądania autoryzacji
        if (doc.containsKey("authorization")) {
            String userId = doc["authorization"];
            
            // Normalizacja ID - usuń spacje aby dopasować format w CSV
            userId.replace(" ", "");
            
            Serial.println("[COMM] Żądanie autoryzacji dla ID: " + userId);
            Serial.println("[COMM] ID po normalizacji (bez spacji): " + userId);
            logCommunication("AUTHORIZATION_REQUEST", "ID: " + userId);

            // Weryfikacja użytkownika w arkuszu
            VerificationResult result = verifyUserInSheet(userId);
            
            if (result.isValid && result.isActive) {
                Serial.println("[COMM] Użytkownik zweryfikowany: " + result.name + " " + result.surname);
                
                // Przygotuj dane do logu (zapisane do zmiennej globalnej)
                pendingLogEntry.timestamp = getCurrentTimestamp();
                pendingLogEntry.userId = result.userId;
                pendingLogEntry.name = result.name;
                pendingLogEntry.surname = result.surname;
                pendingLogEntry.department = result.department;
                pendingLogEntry.action = "WEJŚCIE"; // Domyślnie wejście
                pendingLogEntry.isSuccessful = true;
                
                // Zrób zdjęcie
                takePhoto(userId);
                
                // Wyślij potwierdzenie do ESP WROOM
                sendConfirmation(result.name, result.surname);
                sendStatusPing("USER_VERIFIED_SUCCESS");
                awaitingConfirmation = true;
                
            } else {
                Serial.println("[COMM] Autoryzacja odrzucona: " + result.errorMessage);
                
                // Zapisz w exception_logs
                LogEntry exceptionEntry;
                exceptionEntry.timestamp = getCurrentTimestamp();
                exceptionEntry.userId = userId;
                exceptionEntry.name = result.name;
                exceptionEntry.surname = result.surname;
                exceptionEntry.department = result.department;
                exceptionEntry.action = "BŁĄD_AUTORYZACJI";
                exceptionEntry.isSuccessful = false;
                
                logException(exceptionEntry, result.errorMessage);
                
                // Wyślij odmowę do ESP WROOM
                sendDenial(result.errorMessage);
                sendStatusPing("USER_VERIFICATION_FAILED");
            }
        }
        // Obsługa potwierdzenia od użytkownika
        else if (jsonData.indexOf("{confirmed}") != -1 && awaitingConfirmation) {
            Serial.println("[COMM] Otrzymano potwierdzenie użytkownika");
            logCommunication("USER_CONFIRMED", "Użytkownik potwierdził: " + pendingLogEntry.name + " " + pendingLogEntry.surname);
            
            // Zapisz log do arkusza pracownicy_logi
            logWorkEntry(pendingLogEntry);
            
            // Wyślij informację o zakończeniu procesu
            sendProcessingComplete();
            sendStatusPing("WORK_ENTRY_LOGGED");
            
            awaitingConfirmation = false;
            Serial.println("[COMM] Proces rejestracji zakończony pomyślnie");
        }
        // Obsługa anulowania
        else if (jsonData.indexOf("{cancelled}") != -1 && awaitingConfirmation) {
            Serial.println("[COMM] Otrzymano anulowanie od użytkownika");
            logCommunication("USER_CANCELLED", "Użytkownik anulował: " + pendingLogEntry.name + " " + pendingLogEntry.surname);
            
            // Zapisz anulowanie w exception_logs
            LogEntry cancelEntry = pendingLogEntry;
            cancelEntry.action = "ANULOWANIE";
            cancelEntry.isSuccessful = false;
            
            logException(cancelEntry, "Użytkownik anulował proces");
            sendStatusPing("WORK_ENTRY_CANCELLED");
            
            awaitingConfirmation = false;
            Serial.println("[COMM] Proces anulowany przez użytkownika");
        }
        else {
            // Nieznana komenda
            Serial.println("[COMM] UWAGA: Nierozpoznana komenda JSON");
            logCommunication("UNKNOWN_COMMAND", "Nierozpoznane dane: '" + jsonData + "'");
        }
    }
}

VerificationResult verifyUserInSheet(String userId) {
    VerificationResult result;
    result.isValid = false;
    result.isActive = false;
    result.userId = userId;
    result.errorMessage = "Nieznany błąd";
    
    Serial.println("[VERIFY] Weryfikacja użytkownika ID: " + userId);
    
    File file = SD_MMC.open("/czytnik_projekt/data/Pracownicy_data.csv");
    if (!file) {
        result.errorMessage = "Nie można otworzyć pliku Pracownicy_data.csv";
        Serial.println("[VERIFY] " + result.errorMessage);
        return result;
    }
    
    // Odczytaj nagłówek
    String header = file.readStringUntil('\n');
    header.trim();
    Serial.println("[VERIFY] Nagłówek CSV: " + header);
    
    // Szukaj użytkownika
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) continue;
        
        // Parsowanie CSV: ID,Imię,Nazwisko,Dział,Status
        int firstComma = line.indexOf(',');
        if (firstComma == -1) continue;
        
        String csvUserId = line.substring(0, firstComma);
        csvUserId.trim();
        
        if (csvUserId.equals(userId)) {
            Serial.println("[VERIFY] Znaleziono użytkownika: " + line);
            
            // Parsuj pozostałe pola
            int secondComma = line.indexOf(',', firstComma + 1);
            int thirdComma = line.indexOf(',', secondComma + 1);
            int fourthComma = line.indexOf(',', thirdComma + 1);
            
            if (secondComma != -1 && thirdComma != -1 && fourthComma != -1) {
                result.name = line.substring(firstComma + 1, secondComma);
                result.surname = line.substring(secondComma + 1, thirdComma);
                result.department = line.substring(thirdComma + 1, fourthComma);
                String status = line.substring(fourthComma + 1);
                
                result.name.trim();
                result.surname.trim();
                result.department.trim();
                status.trim();
                
                result.isValid = true;
                
                if (status.equalsIgnoreCase("AKTYWNY") || status.equalsIgnoreCase("aktywny")) {
                    result.isActive = true;
                    result.errorMessage = "";
                } else {
                    result.isActive = false;
                    result.errorMessage = "Użytkownik nieaktywny (status: " + status + ")";
                }
            } else {
                result.errorMessage = "Błędny format danych w CSV";
            }
            break;
        }
    }
    
    file.close();
    
    if (!result.isValid) {
        result.errorMessage = "Użytkownik o ID " + userId + " nie został znaleziony";
        Serial.println("[VERIFY] " + result.errorMessage);
    }
    
    return result;
}

String getCurrentTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("[TIME] Nie można pobrać czasu lokalnego");
        return "ERROR_TIME";
    }
    
    char timeString[64];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

void sendConfirmation(String name, String surname) {
    String response = "{confirm;" + name + ";" + surname + "}";
    Serial2.println(response);
    Serial.println("[COMM] Wysłano potwierdzenie: " + response);
    logCommunication("CONFIRMATION_SENT", response);
    signalResponseSent(); // Miganie po wysłaniu odpowiedzi
}

void sendDenial(String reason) {
    String response = "{denide}"; // Zachowaj literówkę dla zgodności z ESP-WROOM
    Serial2.println(response);
    Serial.println("[COMM] Wysłano odmowę: " + response + " (powód: " + reason + ")");
    logCommunication("DENIAL_SENT", response + " (powód: " + reason + ")");
    signalResponseSent(); // Miganie po wysłaniu odpowiedzi
}

void sendProcessingComplete() {
    Serial2.println("{\"complete\":\"OK\"}");
    Serial.println("[COMM] Wysłano potwierdzenie zakończenia procesu");
    logCommunication("PROCESSING_COMPLETE", "{\"complete\":\"OK\"}");
    signalResponseSent(); // Miganie po wysłaniu odpowiedzi
}

// === FUNKCJE DEBUGOWE ===

void sendDebugResponse() {
    Serial.println("[DEBUG] Wysyłam testową odpowiedź...");
    Serial2.println("{confirm;Jan;Kowalski}");
    Serial.println("[DEBUG] Wysłano: {confirm;Jan;Kowalski}");
    signalResponseSent(); // Miganie po wysłaniu odpowiedzi
}

void testCommunication() {
    Serial.println("=== TEST KOMUNIKACJI ESP32-CAM ===");
    Serial.println("[TEST] Oczekuję na dane przez 30 sekund...");
    
    unsigned long startTime = millis();
    while (millis() - startTime < 30000) { // 30 sekund
        if (Serial2.available()) {
            String receivedData = Serial2.readStringUntil('\n');
            receivedData.trim();
            
            Serial.println("[TEST] Odebrano: '" + receivedData + "'");
            Serial.println("[TEST] Długość: " + String(receivedData.length()));
            Serial.println("[TEST] Pierwszy znak ASCII: " + String((int)receivedData.charAt(0)));
            
            // Test parsowania JSON
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, receivedData);
            
            if (error) {
                Serial.println("[TEST] Błąd JSON: " + String(error.c_str()));
            } else {
                Serial.println("[TEST] JSON poprawny!");
                if (doc.containsKey("authorization")) {
                    String userId = doc["authorization"];
                    Serial.println("[TEST] Znaleziono ID: '" + userId + "'");
                    
                    // Wysyłanie testowej odpowiedzi
                    Serial.println("[TEST] Wysyłam testową odpowiedź...");
                    sendDebugResponse();
                    return;
                }
            }
        }
        delay(100);
    }
    Serial.println("[TEST] Timeout - brak danych w ciągu 30 sekund");
}

// Symulacja zapytania z ESP WROOM - dla testów komunikacji
void simulateWROOMRequest() {
    Serial.println("[SIM] === ROZPOCZYNAM SYMULACJĘ ZAPYTANIA Z ESP WROOM ===");
    Serial.println("[SIM] Symuluje wysłanie zapytania z kartą RFID...");
    
    // Przykładowe zapytanie JSON jakie wysyła ESP WROOM
    String simulatedRequest = "{\"user_id\":\"12345\",\"action\":\"WEJŚCIE\"}";
    
    Serial.println("[SIM] Wysyłam do Serial2 (tak jakby to przyszło z ESP WROOM):");
    Serial.println("[SIM] " + simulatedRequest);
    
    // Symuluj otrzymanie danych przez UART
    Serial.println("[SIM] Teraz przetwarzam te dane tak, jakby przyszły z ESP WROOM...");
    
    // Parsowanie JSON
    if (simulatedRequest.indexOf("\"user_id\"") != -1 && simulatedRequest.indexOf("\"action\"") != -1) {
        // Wyciągnij user_id
        int userIdStart = simulatedRequest.indexOf("\"user_id\":\"") + 11;
        int userIdEnd = simulatedRequest.indexOf("\"", userIdStart);
        String userId = simulatedRequest.substring(userIdStart, userIdEnd);
        
        // Wyciągnij action
        int actionStart = simulatedRequest.indexOf("\"action\":\"") + 10;
        int actionEnd = simulatedRequest.indexOf("\"", actionStart);
        String action = simulatedRequest.substring(actionStart, actionEnd);
        
        Serial.println("[SIM] Parsowane dane:");
        Serial.println("[SIM] User ID: " + userId);
        Serial.println("[SIM] Akcja: " + action);
        
        // Weryfikacja użytkownika
        Serial.println("[SIM] Sprawdzam użytkownika w bazie danych...");
        VerificationResult result = verifyUserInSheet(userId);
        
        if (result.isValid && result.isActive) {
            Serial.println("[SIM] ✓ Użytkownik zweryfikowany pozytywnie!");
            Serial.println("[SIM] Imię: " + result.name);
            Serial.println("[SIM] Nazwisko: " + result.surname);
            Serial.println("[SIM] Dział: " + result.department);
            
            // Wysyłam potwierdzenie
            Serial.println("[SIM] Wysyłam potwierdzenie do ESP WROOM...");
            sendConfirmation(result.name, result.surname);
            
            // Logowanie i zdjęcie (jeśli system jest gotowy)
            Serial.println("[SIM] Wykonuję logowanie i robię zdjęcie...");
            LogEntry entry;
            entry.timestamp = getCurrentTimestamp();
            entry.userId = userId;
            entry.name = result.name;
            entry.surname = result.surname;
            entry.department = result.department;
            entry.action = action;
            entry.isSuccessful = true;
            
            // Wywołaj funkcje z registration_logic
            logWorkEntry(entry);
            takePhoto(userId);
            
        } else {
            Serial.println("[SIM] ✗ Użytkownik nie zweryfikowany!");
            Serial.println("[SIM] Powód: " + result.errorMessage);
            
            // Wysyłam odmowę
            Serial.println("[SIM] Wysyłam odmowę do ESP WROOM...");
            sendDenial(result.errorMessage);
        }
        
        // Sygnalizuj zakończenie
        Serial.println("[SIM] Wysyłam sygnał zakończenia przetwarzania...");
        sendProcessingComplete();
        
    } else {
        Serial.println("[SIM] ✗ Błąd parsowania JSON!");
        sendDenial("Błąd parsowania danych");
    }
    
    Serial.println("[SIM] === KONIEC SYMULACJI ===");
}

// === FUNKCJE PING-PONG I LOGOWANIA KOMUNIKACJI ===

// Funkcja logowania komunikacji na kartę SD
void logCommunication(String type, String message) {
    String timestamp = getCurrentTimestamp();
    String logEntry = timestamp + ",[" + type + "]," + message;
    
    Serial.println("[LOG] " + logEntry);
    
    // Zapisz na kartę SD
    File logFile = SD_MMC.open("/czytnik_projekt/logs/communication.log", FILE_APPEND);
    if (logFile) {
        logFile.println(logEntry);
        logFile.close();
        Serial.println("[LOG] Zapisano do communication.log");
    } else {
        Serial.println("[LOG] BŁĄD: Nie można otworzyć communication.log");
        
        // Spróbuj utworzyć katalog logs jeśli nie istnieje
        if (!SD_MMC.exists("/czytnik_projekt/logs")) {
            if (SD_MMC.mkdir("/czytnik_projekt/logs")) {
                Serial.println("[LOG] Utworzono katalog /czytnik_projekt/logs");
                // Spróbuj ponownie zapisać
                File retryFile = SD_MMC.open("/czytnik_projekt/logs/communication.log", FILE_APPEND);
                if (retryFile) {
                    retryFile.println(logEntry);
                    retryFile.close();
                    Serial.println("[LOG] Zapisano do communication.log (po utworzeniu katalogu)");
                }
            } else {
                Serial.println("[LOG] BŁĄD: Nie można utworzyć katalogu logs");
            }
        }
    }
}

// Funkcja obsługująca ping-pong test
void handlePingPong(String jsonData) {
    Serial.println("[PING] Otrzymano: " + jsonData);
    logCommunication("PING_RECEIVED", jsonData);
    
    // Sprawdź czy to ping
    if (jsonData.indexOf("ping") != -1 || jsonData.indexOf("PING") != -1) {
        Serial.println("[PING] Wykryto sygnał PING - odpowiadam PONG");
        
        // Wyślij odpowiedź pong
        String pongResponse = "{\"response\":\"pong\",\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
        Serial2.println(pongResponse);
        
        Serial.println("[PING] Wysłano PONG: " + pongResponse);
        logCommunication("PONG_SENT", pongResponse);
        
        signalResponseSent(); // Sygnał LED (jeśli włączony)
    }
    // Sprawdź czy to test komunikacji
    else if (jsonData.indexOf("test") != -1 || jsonData.indexOf("TEST") != -1) {
        Serial.println("[TEST] Wykryto sygnał TEST - odpowiadam OK");
        
        String testResponse = "{\"response\":\"OK\",\"status\":\"ESP32-CAM_READY\",\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
        Serial2.println(testResponse);
        
        Serial.println("[TEST] Wysłano TEST OK: " + testResponse);
        logCommunication("TEST_RESPONSE_SENT", testResponse);
        
        signalResponseSent(); // Sygnał LED (jeśli włączony)
    }
}

// Funkcja wysyłająca ping status do ESP WROOM
void sendStatusPing(String operation) {
    String timestamp = getCurrentTimestamp();
    String pingMessage = "{\"ping\":\"status\",\"operation\":\"" + operation + "\",\"timestamp\":\"" + timestamp + "\"}";
    
    Serial2.println(pingMessage);
    Serial.println("[PING-STATUS] Wysłano ping po operacji '" + operation + "': " + pingMessage);
    logCommunication("STATUS_PING_SENT", operation + " - " + pingMessage);
}
