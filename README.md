# ESP32-CAM Rejestrator Godzin Pracy

System rejestracji godzin pracy oparty na ESP32-CAM z komunikacją UART do modułu ESP WROOM obsługującego czytnik RFID.

## Opis projektu

System składa się z dwóch modułów ESP32:
- **ESP32-CAM**: Główny moduł obsługujący kamerę, SD card, WiFi i weryfikację użytkowników
- **ESP WROOM**: Moduł z czytnikiem RFID komunikujący się przez UART

## Funkcje

### Główne funkcje
- 📷 **Automatyczne zdjęcia** - robienie zdjęć przy każdej rejestracji
- 👥 **Weryfikacja użytkowników** - sprawdzanie w bazie danych CSV na karcie SD
- 📊 **Logowanie aktywności** - zapisywanie wszystkich operacji do plików CSV
- 🌐 **Synchronizacja WiFi** - wysyłanie danych na zewnętrzny serwer
- 🚨 **System alarmów** - powiadomienia o ważnych wydarzeniach

### Komunikacja i debugging
- 🔄 **Ping-Pong testing** - test komunikacji między modułami
- 📝 **Szczegółowe logowanie** - zapisy komunikacji na kartę SD
- 📡 **Status monitoring** - automatyczne pingi po operacjach
- 🔍 **Hex data analysis** - analiza danych komunikacyjnych

## Struktura plików

```
src/
├── main.cpp                 # Główna pętla programu
├── communication_logic.cpp  # Obsługa komunikacji UART z ESP WROOM
├── communication_logic.h    # Nagłówki komunikacji
├── registration_logic.cpp   # Logika rejestracji i weryfikacji
├── registration_logic.h     # Nagłówki rejestracji
├── actualization_logic.cpp  # Synchronizacja danych
├── actualization_logic.h    # Nagłówki synchronizacji
└── alarms_logic.cpp         # System alarmów

data/
└── config.txt               # Konfiguracja systemu

platformio.ini               # Konfiguracja PlatformIO
```

## Konfiguracja sprzętowa

### ESP32-CAM
- **UART**: Piny 1 (TX) i 3 (RX) - komunikacja z ESP WROOM
- **Camera**: AI-Thinker ESP32-CAM
- **SD Card**: Przechowywanie danych i zdjęć
- **WiFi**: Synchronizacja z serwerem

### ESP WROOM  
- **UART**: Piny 16 (TX) i 17 (RX) - komunikacja z ESP32-CAM
- **RFID**: Czytnik kart pracowniczych

## Protokół komunikacji

System używa protokołu JSON przez UART (115200 baud):

### Żądania autoryzacji
```json
{"authorization": "12345"}
```

### Ping-Pong testing
```json
{"ping": "test"}
```
Odpowiedź:
```json
{"response": "pong", "timestamp": "2025-08-06 10:30:45"}
```

### Status ping
```json
{"ping": "status", "operation": "WIFI_CONNECTED", "timestamp": "2025-08-06 10:30:45"}
```

## Pliki danych na karcie SD

```
/czytnik_projekt/
├── data/
│   └── Pracownicy_data.csv     # Baza pracowników
├── logs/
│   ├── communication.log       # Logi komunikacji
│   ├── pracownicy_logi.csv     # Logi wejść/wyjść
│   └── exception_logs.csv      # Logi błędów
└── photos/
    └── [timestamp]_[userID].jpg # Zdjęcia pracowników
```

## Instalacja i użycie

### Wymagania
- PlatformIO
- ESP32-CAM (AI-Thinker)
- ESP32 WROOM
- Karta microSD (FAT32)
- Czytnik RFID (kompatybilny z ESP32)

### Kompilacja
```bash
pio run
```

### Upload do ESP32-CAM
```bash
pio run --target upload
```

### Monitoring
```bash
pio device monitor
```

## Debugging

System posiada rozbudowane funkcje debugowania:

1. **Test komunikacji**: Wysyłanie `{"ping":"test"}` zwróci status systemu
2. **Logi komunikacji**: Szczegółowe zapisy w `/czytnik_projekt/logs/communication.log`
3. **Status pings**: Automatyczne pingi po operacjach WiFi, synchronizacji itp.
4. **Hex analysis**: Analiza danych komunikacyjnych w formacie hex

## Funkcje bezpieczeństwa

- ✅ Weryfikacja statusu użytkowników (AKTYWNY/NIEAKTYWNY)
- ✅ Logowanie wyjątków i błędów autoryzacji
- ✅ Timeout dla operacji komunikacyjnych
- ✅ Walidacja danych JSON
- ✅ Backup danych na kartę SD

## Autor

Projekt stworzony dla systemu rejestracji czasu pracy z wykorzystaniem ESP32-CAM i technologii RFID.

## Licencja

Projekt open source - można używać i modyfikować zgodnie z potrzebami.
