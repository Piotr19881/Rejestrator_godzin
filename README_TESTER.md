# ESP32 WROOM - ESP-CAM Communication Tester

## Przegląd

Nowy główny plik `main.cpp` zawiera interaktywny tester komunikacji z ESP-CAM wyświetlający debug na ekranie TFT. Oryginalny kod aplikacji został przeniesiony do `test/main_original.cpp`.

## Funkcje testera

### 1. **Monitor** (tryb nasłuchu)
- Wyświetla wszystkie komunikaty z ESP-CAM na ekranie TFT
- Kategoryzuje komunikaty: [AUTH], [TEST], [WIFI], [BOOT], [LONG], [INFO]
- Pokazuje timestamp dla każdej wiadomości
- Idealny do obserwacji inicjalizacji ESP-CAM

### 2. **Test Ping**
- **Wyślij Ping**: `{"test":"ping"}`
- **Heartbeat**: `{"heartbeat":"alive"}`  
- **Status Check**: `{"status":"check"}`
- Pozwala sprawdzić podstawową komunikację

### 3. **Test Autoryzacji**
- **ID: 12345**: `{"authorization":"12345"}`
- **PESEL: 90010112345**: `{"authorization":"90010112345"}`
- **RFID: A1B2C3D4**: `{"authorization":"A1B2C3D4"}`
- Testuje pełen protokół autoryzacji

### 4. **Custom Messages**
- Predefiniowane wiadomości JSON
- Możliwość wysłania dowolnych komend
- Przydatne do testowania nowych funkcji ESP-CAM

## Struktura plików

```
src/
├── main.cpp                 # Nowy tester TFT (aktywny)
├── tft_debug.h             # Klasa debug console na TFT
├── tft_debug.cpp           # Implementacja debug console
├── local_communication.cpp # Zmodyfikowana obsługa autoryzacji
└── [inne pliki...]

test/
└── main_original.cpp       # Oryginalny kod aplikacji RFID
```

## Użycie

1. **Start**: Po włączeniu wyświetla się menu główne
2. **Monitor**: Wybierz "1. Monitor" do nasłuchu komunikacji ESP-CAM
3. **Testy**: Wybierz odpowiednią opcję (2-4) i dotknij przycisk do wysłania
4. **Powrót**: Przycisk "MENU" w prawym dolnym rogu wraca do głównego menu

## Debugowanie autoryzacji

Gdy wywołasz `handleAuthorization()`, system automatycznie przełączy się w tryb debug TFT:
- Pokazuje wysłane JSON
- Śledzi odebrane bajty w czasie rzeczywistym  
- Kategoryzuje komunikaty od ESP-CAM
- Wyświetla podsumowanie komunikacji
- Pokazuje błędy połączenia

## Połączenia UART

```
ESP-CAM TX  →  ESP32 WROOM RX (GPIO 16)
ESP-CAM RX  →  ESP32 WROOM TX (GPIO 17)
ESP-CAM GND →  ESP32 WROOM GND
```

## Powrót do oryginalnej aplikacji

Aby wrócić do oryginalnej aplikacji RFID:
1. Skopiuj `test/main_original.cpp` → `src/main.cpp`
2. Przywróć oryginalne funkcje w `local_communication.cpp` (jeśli potrzeba)

## Klasa TFTDebug

```cpp
TFTDebug debugConsole(&tft);
debugConsole.clear();                    // Wyczyść ekran
debugConsole.println("Tekst");          // Dodaj linię
debugConsole.addTimestamp("Wiadomość"); // Dodaj z timestampem
```

Ten system pozwala na pełną diagnostykę komunikacji bez konieczności podłączania monitora portu szeregowego.
