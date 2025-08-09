#include "communication_logic.h"
#include "registration_logic.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>

// Zmienne globalne dla stanu komunikacji
bool awaitingConfirmation = false;
LogEntry pendingLogEntry;

// === FUNKCJE LED WYŁĄCZONE - POWODOWAŁY PROBLEMY ===
void signalDataReceived() {
    // LED wyłączony - debug tylko przez Serial
}

void signalResponseSent() {
    // LED wyłączony - debug tylko przez Serial  
}

void setupCommunicationLogic() {
    // ESP32-CAM używa domyślnego Serial (GPIO1/3) do komunikacji z ESP WROOM
    // ESP WROOM używa Serial2 (GPIO16/17) do komunikacji z ESP32-CAM
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("[COMM] === ESP32-CAM KOMUNIKACJA START ===");
    Serial.println("[COMM] UART na GPIO1(TX)/GPIO3(RX), 115200 baud");
    Serial.println("[COMM] Połączenie z ESP WROOM (GPIO16/17)");
    Serial.println("[COMM] Oczekuję na dane z ESP WROOM...");
    Serial.println("[COMM] === GOTOWY DO KOMUNIKACJI ===");
}

void handleCommunication() {
    static unsigned long lastHeartbeat = 0;
    
    // Wyślij heartbeat co 30 sekund - TYLKO JSON!
    if (millis() - lastHeartbeat > 30000) {
        Serial.println("{\"heartbeat\":\"esp32cam_alive\"}");
        Serial.flush();
        lastHeartbeat = millis();
        // LOG TYLKO DO SD, NIE DO UART!
        logCommunication("HEARTBEAT_SENT", "esp32cam_alive");
    }
    
    // Sprawdź dane przychodzące z ESP WROOM
    if (Serial.available()) {
        String jsonData = Serial.readStringUntil('\n');
        jsonData.trim();
        
        if (jsonData.length() == 0) return;
        
        // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
        logCommunication("DATA_RECEIVED", "'" + jsonData + "' [" + String(jsonData.length()) + " bajtów]");
        
        // Parsowanie JSON z ESP WROOM
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonData);
        
        if (!error) {
            // Obsługa różnych typów zapytań
            if (doc["test"].is<String>()) {
                // Test ping-pong - TYLKO JSON RESPONSE!
                handlePingPongTest();
                
            } else if (doc["status"].is<String>()) {
                // Żądanie statusu systemu - TYLKO JSON RESPONSE!
                sendSystemStatus();
                
            } else if (doc["authorization"].is<String>()) {
                // Żądanie autoryzacji - TYLKO JSON RESPONSE!
                String authId = doc["authorization"];
                handleAuthorizationRequest(authId);
                
            } else if (doc["user_action"].is<String>()) {
                // Obsługa akcji użytkownika (confirm/cancel)
                String action = doc["user_action"];
                if (action == "confirm") {
                    logCommunication("USER_CONFIRMED", "Potwierdzenie od użytkownika");
                    // Tutaj logika potwierdzenia
                } else if (action == "cancel") {
                    logCommunication("USER_CANCELLED", "Anulowanie od użytkownika");
                    // Tutaj logika anulowania
                }
                
            } else if (doc["heartbeat"].is<String>()) {
                // Heartbeat od WROOM - TYLKO JSON RESPONSE!
                Serial.println("{\"heartbeat\":\"ok\"}");
                Serial.flush();
                logCommunication("HEARTBEAT_RECEIVED", "od ESP WROOM");
                
            } else {
                logCommunication("UNKNOWN_COMMAND", "Nierozpoznane dane: '" + jsonData + "'");
            }
        } else {
            logCommunication("JSON_PARSE_ERROR", String(error.c_str()) + " - dane: '" + jsonData + "'");
        }
    }
}

// Nowa funkcja obsługi ping-pong dla ESP WROOM
void handlePingPongTest() {
    String response = "{\"pong\":\"esp32cam_alive\",\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
    // TYLKO JSON RESPONSE - BEZ LOGÓW DEBUGOWANIA!
    Serial.println(response);
    Serial.flush();
    // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
    logCommunication("PING_RESPONSE", response);
}

// Nowa funkcja obsługi autoryzacji
void handleAuthorizationRequest(String userId) {
    // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
    logCommunication("AUTH_REQUEST", "ID = " + userId);
    
    // Weryfikacja użytkownika w bazie danych CSV
    VerificationResult result = verifyUserInSheet(userId);
    
    if (result.isValid && result.isActive) {
        // Użytkownik zweryfikowany - TYLKO JSON RESPONSE!
        String confirmResponse = "{confirm;" + result.name + ";" + result.surname + "}";
        Serial.println(confirmResponse);
        Serial.flush();
        
        // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
        logCommunication("AUTH_SUCCESS", result.name + " " + result.surname);
        
        // Przygotuj wpis do logu
        pendingLogEntry.timestamp = getCurrentTimestamp();
        pendingLogEntry.userId = result.userId;
        pendingLogEntry.name = result.name;
        pendingLogEntry.surname = result.surname;
        pendingLogEntry.department = result.department;
        pendingLogEntry.action = "WEJŚCIE";
        pendingLogEntry.isSuccessful = true;
        
        // Zrób zdjęcie
        takePhoto(userId);
        
        // Zapisz log
        logWorkEntry(pendingLogEntry);
        
        awaitingConfirmation = false;
        
    } else {
        // Użytkownik nieaktywny lub nie znaleziony - TYLKO JSON RESPONSE!
        Serial.println("{denide}");
        Serial.flush();
        
        // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
        logCommunication("AUTH_DENIED", result.errorMessage);
        
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
    }
}

VerificationResult verifyUserInSheet(String userId) {
    VerificationResult result;
    result.isValid = false;
    result.isActive = false;
    result.userId = userId;
    result.errorMessage = "Nieznany błąd";
    
    // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
    logCommunication("VERIFY_REQUEST", "ID = " + userId);
    
    File file = SD_MMC.open("/czytnik_projekt/data/Pracownicy_data.csv");
    if (!file) {
        result.errorMessage = "Nie można otworzyć pliku Pracownicy_data.csv";
        // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
        logCommunication("FILE_ERROR", result.errorMessage);
        return result;
    }
    
    // Odczytaj nagłówek
    String header = file.readStringUntil('\n');
    header.trim();
    // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
    logCommunication("CSV_HEADER", header);
    
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
            // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
            logCommunication("USER_FOUND", line);
            
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
        // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
        logCommunication("USER_NOT_FOUND", result.errorMessage);
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

// === STARE FUNKCJE - ZACHOWANE DLA KOMPATYBILNOŚCI ===

void sendConfirmation(String name, String surname) {
    // Ta funkcja jest teraz obsługiwana w handleAuthorizationRequest()
    Serial.println("{confirm;" + name + ";" + surname + "}");
    Serial.flush();
}

void sendDenial(String reason) {
    // Ta funkcja jest teraz obsługiwana w handleAuthorizationRequest()
    Serial.println("{denide}");
    Serial.flush();
}

void sendProcessingComplete() {
    Serial.println("{\"complete\":\"OK\"}");
    Serial.flush();
}

// === FUNKCJE DEBUGOWE ===

void sendDebugResponse() {
    Serial.println("{confirm;Jan;Kowalski}");
    Serial.flush();
}

void testCommunication() {
    Serial.println("=== TEST KOMUNIKACJI ESP32-CAM ===");
    Serial.println("[TEST] Oczekuję na dane przez 30 sekund...");
    
    unsigned long startTime = millis();
    while (millis() - startTime < 30000) { // 30 sekund
        if (Serial.available()) {
            String receivedData = Serial.readStringUntil('\n');
            receivedData.trim();
            
            Serial.println("[TEST] Odebrano: '" + receivedData + "'");
            Serial.println("[TEST] Długość: " + String(receivedData.length()));
            Serial.println("[TEST] Pierwszy znak ASCII: " + String((int)receivedData.charAt(0)));
            
            // Test parsowania JSON
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, receivedData);
            
            if (error) {
                Serial.println("[TEST] Błąd JSON: " + String(error.c_str()));
            } else {
                Serial.println("[TEST] JSON poprawny!");
                if (doc["authorization"].is<String>()) {
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

// Funkcja logowania komunikacji na kartę SD - BEZ DEBUGOWANIA PRZEZ UART!
void logCommunication(String type, String message) {
    String timestamp = getCurrentTimestamp();
    String logEntry = timestamp + ",[" + type + "]," + message;
    
    // TYLKO ZAPIS NA SD - BEZ WYSYŁANIA PRZEZ UART!
    // Serial.println("[LOG] " + logEntry); // WYŁĄCZONE!
    
    // Zapisz na kartę SD
    File logFile = SD_MMC.open("/czytnik_projekt/logs/communication.log", FILE_APPEND);
    if (logFile) {
        logFile.println(logEntry);
        logFile.close();
        // Serial.println("[LOG] Zapisano do communication.log"); // WYŁĄCZONE!
    } else {
        // Serial.println("[LOG] BŁĄD: Nie można otworzyć communication.log"); // WYŁĄCZONE!
        
        // Spróbuj utworzyć katalog logs jeśli nie istnieje
        if (!SD_MMC.exists("/czytnik_projekt/logs")) {
            if (SD_MMC.mkdir("/czytnik_projekt/logs")) {
                // Serial.println("[LOG] Utworzono katalog /czytnik_projekt/logs"); // WYŁĄCZONE!
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
        Serial.println(pongResponse);
        Serial.flush();
        
        Serial.println("[PING] Wysłano PONG: " + pongResponse);
        logCommunication("PONG_SENT", pongResponse);
        
        signalResponseSent(); // Sygnał LED (jeśli włączony)
    }
    // Sprawdź czy to test komunikacji
    else if (jsonData.indexOf("test") != -1 || jsonData.indexOf("TEST") != -1) {
        Serial.println("[TEST] Wykryto sygnał TEST - odpowiadam OK");
        
        String testResponse = "{\"response\":\"OK\",\"status\":\"ESP32-CAM_READY\",\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
        Serial.println(testResponse);
        Serial.flush();
        
        Serial.println("[TEST] Wysłano TEST OK: " + testResponse);
        logCommunication("TEST_RESPONSE_SENT", testResponse);
        
        signalResponseSent(); // Sygnał LED (jeśli włączony)
    }
}

// Funkcja wysyłająca ping status do ESP WROOM
void sendStatusPing(String operation) {
    String timestamp = getCurrentTimestamp();
    String pingMessage = "{\"ping\":\"status\",\"operation\":\"" + operation + "\",\"timestamp\":\"" + timestamp + "\"}";
    
    Serial.println(pingMessage);
    Serial.flush();
    Serial.println("[PING-STATUS] Wysłano ping po operacji '" + operation + "': " + pingMessage);
    logCommunication("STATUS_PING_SENT", operation + " - " + pingMessage);
}

// Nowa funkcja: Status systemu dla ESP WROOM
void sendSystemStatus() {
    extern bool system_ready, wifi_connected, sd_initialized, config_loaded, sync_attempted;
    
    String status = "{";
    status += "\"system_status\":\"" + String(system_ready ? "ready" : "initializing") + "\",";
    status += "\"wifi\":\"" + String(wifi_connected ? "connected" : "disconnected") + "\",";
    status += "\"sd_card\":\"" + String(sd_initialized ? "ok" : "error") + "\",";
    status += "\"config\":\"" + String(config_loaded ? "loaded" : "default") + "\",";
    status += "\"sync\":\"" + String(sync_attempted ? "attempted" : "skipped") + "\",";
    status += "\"timestamp\":\"" + getCurrentTimestamp() + "\"";
    status += "}";
    
    // TYLKO JSON RESPONSE - BEZ LOGÓW DEBUGOWANIA!
    Serial.println(status);
    Serial.flush();
    // LOG TYLKO DO SD - BEZ WYSYŁANIA PRZEZ UART!
    logCommunication("SYSTEM_STATUS_SENT", status);
}
