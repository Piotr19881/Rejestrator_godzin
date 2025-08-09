# Poprawki Zgodności Protokołu ESP32-CAM ↔ ESP32-WROOM

## 🔍 **Analiza Problemu**

Na podstawie logów komunikacji zauważono następujące problemy:

1. **Timeout za krótki** - ESP32-CAM odpowiada po ~27-30 sekundach, WROOM czeka tylko 15s
2. **Prefiksy logów** - ESP32-CAM wysyła `[TX TO WROOM]: {denide}`, WROOM oczekuje `{denide}`
3. **Opóźnione odpowiedzi** - właściwa odpowiedź przychodzi po timeout, jako osobna linia

## ✅ **Wprowadzone Poprawki**

### **1. Zwiększenie Timeout (15s → 45s)**
```cpp
// W handleAuthorization()
while (millis() - startTime < 45000) { // Zwiększone do 45 sekund dla ESP32-CAM
```

### **2. Funkcja Parsowania Prefiksów**
```cpp
String parseESPCamResponse(String response) {
    response.trim();
    
    // Usuń prefiksy logów ESP32-CAM
    if (response.startsWith("[TX TO WROOM]: ")) {
        response = response.substring(15);
    }
    if (response.startsWith("[RX FROM WROOM]: ")) {
        response = response.substring(17);
    }
    if (response.startsWith("[AUTH")) return "";     // Ignoruj logi autoryzacyjne
    if (response.startsWith("[VERIFY")) return "";   // Ignoruj logi weryfikacyjne
    if (response.startsWith("[EXCEPTION")) return ""; // Ignoruj logi błędów
    
    // Sprawdź format odpowiedzi autoryzacyjnej
    if (response.startsWith("{confirm;") || response == "{denide}") {
        return response;
    }
    
    return ""; // Nieznany/ignorowany format
}
```

### **3. Ulepszone Rozpoznawanie Gotowości**
```cpp
// W waitForESPCamReady()
String cleanMsg = receivedData;
if (cleanMsg.startsWith("[TX TO WROOM]: ")) {
    cleanMsg = cleanMsg.substring(15);
}

if (cleanMsg.indexOf("pong") >= 0 || 
    cleanMsg.indexOf("alive") >= 0 ||
    cleanMsg.indexOf("esp32cam_alive") >= 0) {
    // ESP32-CAM gotowy!
}
```

### **4. Integracja w Pętli Głównej**
- Używa `parseESPCamResponse()` w `handleAuthorization()`
- Używa `parseESPCamResponse()` w `sendCommandToESPCam()`
- Ignoruje prefiksy we wszystkich funkcjach komunikacyjnych

## 📋 **Oczekiwane Komunikaty ESP32-CAM**

### **Gotowość:**
- `{"pong":"esp32cam_alive","timestamp":"..."}`
- `{"heartbeat":"esp32cam_alive"}`
- Dowolna linia zawierająca `pong`, `alive`, `ready`

### **Autoryzacja:**
- **Pozytywna:** `{confirm;Jan;Kowalski}`
- **Negatywna:** `{denide}`
- **Z prefiksami:** `[TX TO WROOM]: {denide}` → automatycznie czyszczone

### **Heartbeat:**
- **ESP32-CAM:** `{"heartbeat":"esp32cam_alive"}`
- **WROOM:** `{"heartbeat":"alive"}`
- **Odpowiedź:** `{"heartbeat":"ok"}`

## 🚀 **Rezultat**

Po wprowadzeniu poprawek:
- ✅ WROOM czeka wystarczająco długo na odpowiedź ESP32-CAM
- ✅ Ignoruje prefiksy logów i poprawnie parsuje odpowiedzi
- ✅ Rozpoznaje różne sygnały gotowości ESP32-CAM
- ✅ Kompatybilność z formatem `{confirm;Imie;Nazwisko}` i `{denide}`

## 📂 **Zmienione Pliki**

1. **src/local_communication.cpp**
   - Dodano `parseESPCamResponse()`
   - Zwiększono timeout do 45s
   - Zaktualizowano logikę parsowania

2. **include/local_communication.h**
   - Dodano deklarację `parseESPCamResponse()`

Komunikacja teraz jest w pełni zgodna z implementacją ESP32-CAM! 🎉
