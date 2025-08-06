# ESP32 RFID Time Logger with ESP32-CAM Authorization

🕐 **System rejestracji czasu pracy z autoryzacją ESP32-CAM**

## 📋 Opis Projektu

System składa się z dwóch głównych komponentów:
- **ESP32 WROOM32** - główny kontroler z czytnikiem RFID RC522, wyświetlaczem TFT ILI9341 i interfejsem dotykowym
- **ESP32-CAM** - backend autoryzacyjny z bazą danych w Google Sheets

## 🔧 Komponenty Sprzętowe

### ESP32 WROOM32 (Główny kontroler)
- **RFID Reader**: RC522 (SPI)
- **Display**: ILI9341 TFT 320x240 z kontrolerem dotykowym XPT2046
- **Komunikacja**: UART Serial2 (RX:16, TX:17) do ESP32-CAM

### ESP32-CAM (Backend autoryzacyjny)
- Obsługa bazy danych Google Sheets
- Autoryzacja użytkowników na podstawie ID karty RFID
- Komunikacja przez UART z głównym kontrolerem

## 🚀 Główne Funkcje

### ✨ Obecna wersja (TFT Tester)
- **Interaktywny system testowania** komunikacji z ESP32-CAM
- **TFT Debug Console** - 12-liniowy system debugowania na wyświetlaczu
- **Menu testowe**:
  1. 🔍 Monitor - podgląd komunikatów w czasie rzeczywistym
  2. 🏓 Test Ping - sprawdzenie połączenia
  3. 🔐 Test Auth - test autoryzacji z przykładowym ID
  4. ✉️ Custom Message - wysyłanie własnych wiadomości

### 📁 Oryginalna aplikacja RFID (test/main_original.cpp)
- Odczyt kart RFID RC522
- Wyświetlanie ID karty na TFT i Serial
- Ekrany bezczynności z animacjami
- Klawiatura numeryczna do wprowadzania PESEL
- Kalibracja dotyku TFT
- Pełna integracja z ESP32-CAM

## 🔌 Schemat Połączeń

### RC522 RFID Reader
```
RC522     ESP32 WROOM32
------------------------
SDA       GPIO 5
SCK       GPIO 18
MOSI      GPIO 23
MISO      GPIO 19
IRQ       -
GND       GND
RST       GPIO 22
3.3V      3V3
```

### ILI9341 TFT + XPT2046 Touch
```
TFT/Touch    ESP32 WROOM32
---------------------------
TFT_MISO     GPIO 19
TFT_MOSI     GPIO 23  
TFT_SCLK     GPIO 18
TFT_CS       GPIO 15
TFT_DC       GPIO 2
TFT_RST      GPIO 4
TOUCH_CS     GPIO 21
LED          3V3
VCC          3V3
GND          GND
```

### ESP32-CAM Communication
```
ESP32-CAM    ESP32 WROOM32
--------------------------
TX           RX (GPIO 16)
RX           TX (GPIO 17)
GND          GND
```

## 📡 Protokół Komunikacji

### JSON Messages ESP32 → ESP32-CAM
```json
// Autoryzacja
{"authorization": "1234567890"}

// Ping test  
{"ping": "test"}
```

### Responses ESP32-CAM → ESP32
```json
// Autoryzacja zaakceptowana
{confirm;Jan;Kowalski}

// Autoryzacja odrzucona
{denide}

// Ping response
{pong}
```

## 🛠️ Instalacja i Kompilacja

### Wymagania
- PlatformIO IDE
- ESP32 Arduino Core
- Biblioteki:
  - `MFRC522` (RFID)
  - `TFT_eSPI` (Display + Touch)
  - `ArduinoJson` (Komunikacja)

### Konfiguracja TFT_eSPI
Sprawdź plik `include/User_Setup.h` dla konfiguracji pinów TFT.

### Kompilacja
```bash
# Sklonuj repozytorium
git clone https://github.com/[username]/esp32-rfid-time-logger.git
cd esp32-rfid-time-logger

# Kompiluj projekt
pio run

# Upload do ESP32
pio run --target upload

# Monitor Serial (opcjonalnie)
pio device monitor
```

## 📱 Interfejs Użytkownika

### TFT Debug Console
- **12 linii** tekstu z przewijaniem
- **Timestampy** dla każdej wiadomości
- **Kategoryzacja** wiadomości:
  - `[AUTH]` - autoryzacja
  - `[TEST]` - testy komunikacji
  - `[WIFI]` - logi WiFi ESP32-CAM
  - `[BOOT]` - inicjalizacja systemu

### Menu Główne (Current Main)
1. **Monitor Mode** - podgląd wszystkich komunikatów
2. **Test Ping** - weryfikacja połączenia z ESP32-CAM  
3. **Test Authorization** - test procesu autoryzacji
4. **Custom Message** - wysyłanie dowolnych wiadomości

### Oryginalna Aplikacja RFID
- **Ekran główny** z instrukcjami
- **Animowane ekrany bezczynności**
- **Klawiatura PESEL** z walidacją
- **Kalibracja dotyku** przy pierwszym uruchomieniu
- **Potwierdzenie autoryzacji** z przyciskami Zatwierdź/Odrzuć

## 🐛 System Debugowania

### Problem z Serial Monitor
Kiedy ESP32-CAM jest podłączony do pinów UART (RX:16, TX:17), nie można używać Serial Monitor do debugowania.

### Rozwiązanie: TFT Debug Console
- **Klasa TFTDebug** - wyświetla logi bezpośrednio na TFT
- **Real-time monitoring** komunikacji UART
- **Filtrowanie wiadomości** - ukrywa logi startowe ESP32-CAM
- **Kategoryzacja** - ułatwia identyfikację źródła wiadomości

## 📚 Struktura Projektu

```
src/
├── main.cpp              # Aktualny TFT Tester (interaktywne menu)
├── local_communication.* # Protokół komunikacji z ESP32-CAM
├── tft_debug.*           # System debugowania na TFT
├── keypad.cpp            # Klawiatura numeryczna
└── screens.cpp           # Ekrany bezczynności

test/
└── main_original.cpp     # Oryginalna aplikacja RFID

include/
├── *.h                   # Pliki nagłówkowe
└── User_Setup.h          # Konfiguracja TFT_eSPI
```

## 🔄 Workflow Rozwoju

### Testowanie Komunikacji
1. Upload kodu do ESP32 WROOM32
2. Podłącz ESP32-CAM do UART (RX:16, TX:17)  
3. Uruchom "1. Monitor" aby obserwować komunikaty
4. Użyj "2. Test Ping" do weryfikacji połączenia
5. Testuj autoryzację przez "3. Test Auth"

### Powrót do Pełnej Aplikacji RFID
```bash
# Skopiuj oryginalną aplikację z powrotem
cp test/main_original.cpp src/main.cpp
pio run --target upload
```

## 🤝 Wkład w Projekt

1. Fork repozytorium
2. Utwórz branch dla nowej funkcji (`git checkout -b feature/amazing-feature`)
3. Commit zmian (`git commit -m 'Add amazing feature'`)
4. Push do branch (`git push origin feature/amazing-feature`)
5. Otwórz Pull Request

## 📄 Licencja

Ten projekt jest udostępniony na licencji MIT - zobacz plik [LICENSE](LICENSE) dla szczegółów.

## 📞 Kontakt

Projekt: [https://github.com/[username]/esp32-rfid-time-logger](https://github.com/[username]/esp32-rfid-time-logger)

---

**⚡ ESP32 + RFID + TFT + ESP32-CAM = Complete Time Logging System!**
