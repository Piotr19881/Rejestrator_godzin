#ifndef COMMUNICATION_LOGIC_H
#define COMMUNICATION_LOGIC_H

#include <Arduino.h>

// Struktura do przechowywania danych weryfikacji
struct VerificationResult {
    bool isValid;
    bool isActive;
    String userId;
    String name;
    String surname;
    String department;
    String errorMessage;
};

// Struktura do przechowywania danych logu
struct LogEntry {
    String timestamp;
    String userId;
    String name;
    String surname;
    String department;
    String action; // "WEJŚCIE" lub "WYJŚCIE"
    bool isSuccessful;
};

// Inicjalizacja komunikacji UART
void setupCommunicationLogic();

// Główna funkcja obsługująca komunikację z ESP WROOM
void handleCommunication();

// Weryfikacja użytkownika w arkuszu Pracownicy_data.csv
VerificationResult verifyUserInSheet(String userId);

// NOWA FUNKCJA: Konwersja formatu RFID ze spacjami na format bez spacji
String convertRFIDFormat(String rfidWithSpaces);

// Pobranie aktualnego czasu w formacie do arkusza
String getCurrentTimestamp();

// Funkcje pomocnicze
void sendConfirmation(String name, String surname);
void sendDenial(String reason);
void sendProcessingComplete();

// Funkcje debugowe
void sendDebugResponse();
void testCommunication();
void simulateWROOMRequest();

// Funkcje ping-pong i logowania
void logCommunication(String type, String message);
void handlePingPong(String jsonData);
void sendStatusPing(String operation);

// Nowe funkcje kompatybilne z ESP WROOM
void handlePingPongTest();
void handleAuthorizationRequest(String userId);
void sendSystemStatus();

#endif // COMMUNICATION_LOGIC_H
