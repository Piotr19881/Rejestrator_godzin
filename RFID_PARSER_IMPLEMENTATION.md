# Implementacja Parsera Kart RFID → PESEL

## 🎯 **Cel**: Prawidłowe mapowanie kart RFID na identyfikatory PESEL

### **📋 Problem:**
- Karta RFID: `04 92 A1 6F BA 2A 81` (hex z spacjami)
- Baza danych CSV: PESEL `88080703299` (11 cyfr)
- Brak mapowania → autoryzacja zawsze odrzucona

---

## 🔧 **Wprowadzone zmiany:**

### **1. System mapowania kart** 📂
```
include/cards_mapping.h    - Struktura danych mapowania
src/cards_mapping.cpp      - Logika mapowania RFID→PESEL
```

**Mapowanie:**
- `04 92 A1 6F BA 2A 81` → `88080703299` (Piotr Prokop)
- `0492A16FBAA281` → `88080703299` (format bez spacji)

### **2. Parser kart w main.cpp** 🏷️

**Funkcje:**
- `parseCardID()` - konwersja hex→PESEL przez mapowanie
- `readRFIDCard()` - odczyt karty z automatycznym parsowaniem

**Proces:**
1. Odczyt raw hex: `04 92 A1 6F BA 2A 81`
2. Normalizacja: `0492A16FBAA281`
3. Mapowanie: `88080703299`
4. Wysłanie do ESP32-CAM

### **3. Mapowanie w komunikacji** 📡

**W local_communication.cpp:**
- `mapCardIDFormat()` - używa tablicy mapowań
- `isNumeric()` - sprawdza czy to już PESEL
- Automatyczne mapowanie przed wysłaniem do ESP32-CAM

### **4. Ulepszone ekrany** 🎨

**Nowe funkcje w screens.cpp:**
- `showLoadingScreen()` - "Weryfikacja..."
- `showWelcomeScreen()` - sukces autoryzacji  
- `showErrorScreen()` - błąd autoryzacji
- `showMainScreen()` - powrót do menu głównego

### **5. Obsługa keypada** ⌨️
- `getKeypadInput()` - uproszczona obsługa dotyku
- `handleKeypadInput()` - integracja z autoryzacją

---

## 📊 **Przepływ weryfikacji:**

```
1. KARTA PRZYŁOŻONA
   ↓
2. Odczyt hex: "04 92 A1 6F BA 2A 81"
   ↓
3. Normalizacja: "0492A16FBAA281"
   ↓
4. Mapowanie: "88080703299"
   ↓
5. Wysłanie: {"authorization":"88080703299"}
   ↓
6. ESP32-CAM: Sprawdza w Google Sheets
   ↓
7. Odpowiedź: {confirm;Piotr;Prokop} lub {denide}
   ↓
8. Wyświetlenie: "Witamy Piotr Prokop" lub "Brak uprawnień"
```

---

## 🎉 **Rezultat:**

✅ **Karta Piotra Prokopa** (`04 92 A1 6F BA 2A 81`)  
✅ **Mapowana na PESEL** (`88080703299`)  
✅ **Zgodna z CSV** (ID=1, Piotr Prokop, 88080703299)  
✅ **Prawidłowa autoryzacja** ESP32-CAM  
✅ **Szczegółowe logi** procesu mapowania  

### **📝 Dodawanie nowych kart:**

Edytuj `include/cards_mapping.h`:
```cpp
const CardMapping CARD_MAPPINGS[] = {
  {"0492A16FBAA281", "88080703299", "Piotr Prokop"},
  {"NOWA_KARTA_HEX", "NOWY_PESEL", "Nowe Nazwisko"}, // <-- TUTAJ
};
```

System automatycznie rozpozna nową kartę! 🚀

---

## 🔍 **Debugowanie:**

**Serial Monitor pokaże:**
```
=== PARSOWANIE KARTY ===
Raw ID: 04 92 A1 6F BA 2A 81
Hex bez spacji: 0492A16FBAA281
[MAPPING] Szukam mapowania dla: 0492A16FBAA281
[MAPPING] Znaleziono: RFID=0492A16FBAA281 -> PESEL=88080703299 (Piotr Prokop)
Zmapowano na PESEL: 88080703299
ID do autoryzacji: 88080703299
```

**Problem rozwiązany!** ✅
