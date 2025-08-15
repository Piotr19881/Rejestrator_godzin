# 🚨 NAPRAWA GOOGLE APPS SCRIPT - BRAKUJĄCA AUTORYZACJA

## ❌ **Problem:**
Google Apps Script **NIE MIAŁ** funkcji autoryzacji!

ESP32-CAM wysyła:
```json
{"authorization":"88080703299"}
```

Apps Script odpowiada:
```
ERROR 404 - Brak funkcji obsługi
```

## ✅ **Rozwiązanie:**

### **1. Dodana funkcja `authorizeByPESEL()`**
```javascript
function authorizeByPESEL(pesel) {
  // Przeszukuje arkusz pracowników po PESEL
  // Zwraca: {confirm;Imie;Nazwisko} lub {denide}
}
```

### **2. Obsługa POST JSON**
```javascript
function doPost(e) {
  if (e.postData && e.postData.contents) {
    var jsonData = JSON.parse(e.postData.contents);
    
    // ESP32-CAM: {"authorization":"88080703299"}
    if (jsonData.authorization) {
      return authorizeByPESEL(jsonData.authorization);
    }
  }
}
```

### **3. Obsługa GET parametrów**
```javascript
function doGet(e) {
  if (action === 'authorize') {
    return authorizeUser(params);
  }
}
```

## 🔧 **Jak zastosować poprawkę:**

### **Krok 1: Otwórz Google Apps Script**
1. Idź na: https://script.google.com
2. Otwórz swój projekt Apps Script
3. Wybierz plik `Code.gs`

### **Krok 2: Zamień cały kod**
1. **Zaznacz całą zawartość** pliku `Code.gs`
2. **Usuń wszystko** (Ctrl+A, Delete)
3. **Wklej poprawiony kod** z pliku `GOOGLE_APPS_SCRIPT_FIX.js`
4. **Zapisz** (Ctrl+S)

### **Krok 3: Wdróż**
1. Kliknij **"Deploy"** → **"New deployment"**
2. Typ: **"Web app"**
3. Execute as: **"Me"**
4. Access: **"Anyone"**
5. **Deploy** i skopiuj URL

### **Krok 4: Testuj**
W Apps Script uruchom funkcję `testAuthorization()`:
```javascript
function testAuthorization() {
  var result = authorizeByPESEL('88080703299');
  console.log('Result: ' + result.getContent());
  // Powinno zwrócić: {confirm;Piotr;Prokop}
}
```

## 📊 **Oczekiwane rezultaty:**

### **Zapytanie ESP32-CAM:**
```
POST https://script.google.com/macros/s/YOUR_ID/exec
Content-Type: application/json

{"authorization":"88080703299"}
```

### **Odpowiedź Apps Script:**
```
{confirm;Piotr;Prokop}
```

### **PESEL nieznany:**
```
{denide}
```

## 🎯 **Co się zmieni:**

- ✅ ESP32-CAM będzie dostawać odpowiedzi autoryzacyjne
- ✅ WROOM będzie otrzymywać `{confirm;Piotr;Prokop}` 
- ✅ Parser w WROOM pokaże "AUTORYZOWANY: Piotr Prokop"
- ✅ Ekran powitalny "Witamy Piotr Prokop"

**Problem weryfikacji zostanie rozwiązany!** 🚀

## 📝 **Debugowanie:**

W Google Apps Script Console zobaczysz:
```
Autoryzacja dla PESEL: 88080703299
Sprawdzam PESEL: 88080703299 vs 88080703299
Znaleziono: Piotr Prokop
```

Lub:
```
Nie znaleziono pracownika dla PESEL: 12345678901
```

---

**⚠️ WAŻNE:** Bez tej poprawki autoryzacja **NIGDY** nie będzie działać!
