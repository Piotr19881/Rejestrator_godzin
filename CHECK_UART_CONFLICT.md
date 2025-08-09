# 🚨 KONFLIKT UART - DIAGNOZA

## Problem: Po podłączeniu ESP32-CAM znika komunikacja USB

### ✅ POPRAWNE POŁĄCZENIE (UART2):
```
ESP32 WROOM    <--->    ESP32-CAM
GPIO16 (RX)    <--->    GPIO1 (TX) lub GPIO15 (TX)  
GPIO17 (TX)    <--->    GPIO3 (RX) lub GPIO13 (RX)
GND            <--->    GND
3.3V           <--->    3.3V
```

### ❌ NIEPRAWIDŁOWE POŁĄCZENIE (powoduje konflikt):
```
ESP32 WROOM    <--->    ESP32-CAM
GPIO1 (TX)     <--->    ESP32-CAM RX (to blokuje USB!)
GPIO3 (RX)     <--->    ESP32-CAM TX (to blokuje USB!)
```
⚠️  **WŁAŚNIE TO ROBIECIE! GPIO1/3 = PORT USB = BRAK SERIAL MONITOR!**

## 🔍 CO SPRAWDZIĆ:

### 1. **Piny ESP32-CAM:**
Sprawdź w kodzie ESP32-CAM jakie piny są używane do UART:
```cpp
// POPRAWNE (ESP32-CAM):
Serial.begin(115200);  // GPIO1/GPIO3 tylko dla USB/debugowania
Serial2.begin(115200, SERIAL_8N1, 13, 15);  // GPIO13=RX, GPIO15=TX dla komunikacji

// NIEPRAWIDŁOWE (ESP32-CAM):
Serial.begin(115200);  // używa GPIO1/3 do komunikacji - KONFLIKT!
```

### 2. **Sprawdź przewody:**
- ESP32-CAM TX (GPIO15 lub GPIO1) → ESP32 WROOM GPIO16 (RX)
- ESP32-CAM RX (GPIO13 lub GPIO3) → ESP32 WROOM GPIO17 (TX)

### 3. **Test bez ESP32-CAM:**
- Wgraj kod na ESP32 WROOM
- Sprawdź czy USB działa (powinien)
- Dopiero potem podłącz ESP32-CAM

## 🎯 ROZWIĄZANIE:

### A) **Jeśli ESP32-CAM używa Serial (GPIO1/3):**
**Zmień kod ESP32-CAM** żeby używać Serial2:
```cpp
// Zamiast:
Serial.begin(115200);
Serial.println("hello");

// Użyj:
Serial.begin(115200);  // tylko do debugowania lokalnego
Serial2.begin(115200, SERIAL_8N1, 13, 15);  // RX=GPIO13, TX=GPIO15
Serial2.println("hello");  // wysyłaj przez Serial2
```

### B) **Jeśli musisz użyć GPIO1/3 na ESP32-CAM:**
**Zmień piny na ESP32 WROOM** (ale to gorsze rozwiązanie):
```cpp
// W main.cpp WROOM:
Serial2.begin(115200, SERIAL_8N1, 4, 2);  // RX=GPIO4, TX=GPIO2
```

### C) **Użyj external UART converter:**
- FTDI lub CP2102 podłączony do GPIO16/17
- USB pozostaje wolny dla Serial Monitor

## 📊 DIAGNOSTYKA:

1. **Sprawdź kod ESP32-CAM** - czy używa Serial czy Serial2?
2. **Sprawdź physical connections** - jakie piny są podłączone?
3. **Test step-by-step** - najpierw USB, potem ESP32-CAM

## 🔧 NATYCHMIASTOWA AKCJA:

1. **Odłącz ESP32-CAM**
2. **Sprawdź czy USB Serial Monitor działa** 
3. **Sprawdź kod ESP32-CAM** - jakie piny UART używa
4. **Popraw połączenia lub kod ESP32-CAM**
5. **Test ponownie**

**Główna przyczyna: ESP32-CAM prawdopodobnie używa UART0 (GPIO1/3) zamiast dedykowanego portu do komunikacji, co powoduje konflikt z USB Serial Monitor.**
