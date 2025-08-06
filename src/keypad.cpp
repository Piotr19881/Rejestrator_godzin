/*
 * keypad.cpp
 *
 * Przeznaczenie:
 * - Obsługa klawiatury numerycznej do wprowadzania PESEL
 * - Odczyt wprowadzanych kodów PIN
 * - Przekazywanie wprowadzonych danych do głównej logiki
 * 
 * Bazuje na przykładzie TFT_eSPI keypad
 */

#include "keypad.h"
#include "screens.h"
#include <TFT_eSPI.h>

// Keypad start position, key sizes and spacing
#define KEY_X 40 // Centre of key
#define KEY_Y 96
#define KEY_W 62 // Width and height
#define KEY_H 30
#define KEY_SPACING_X 18 // X and Y gap
#define KEY_SPACING_Y 20
#define KEY_TEXTSIZE 1   // Font size multiplier

// Numeric display box size and location
#define DISP_X 1
#define DISP_Y 10
#define DISP_W 238
#define DISP_H 50
#define DISP_TSIZE 3
#define DISP_TCOLOR TFT_CYAN

// PESEL length
#define PESEL_LEN 11

// We have a status line for messages
#define STATUS_X 120 // Centred on this
#define STATUS_Y 65

extern TFT_eSPI tft;
extern String enteredPESEL;

// Deklaracja funkcji z main.cpp
void handleKeypadInput(String input);

// Etykiety klawiszy dla PESEL (3x4 layout + 3 górne przyciski)
char keyLabel[15][5] = {"Clr", "Del", "OK", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "#" };

// Kolory klawiszy
uint16_t keyColor[15] = {
  TFT_RED, TFT_DARKGREY, TFT_DARKGREEN,  // Clr, Del, OK
  TFT_BLUE, TFT_BLUE, TFT_BLUE,           // 1, 2, 3
  TFT_BLUE, TFT_BLUE, TFT_BLUE,           // 4, 5, 6
  TFT_BLUE, TFT_BLUE, TFT_BLUE,           // 7, 8, 9
  TFT_BLUE, TFT_BLUE, TFT_BLUE            // ., 0, #
};

// Obiekty przycisków (15 przycisków jak w przykładzie)
TFT_eSPI_Button key[15];

// Zmienne do debounce
static unsigned long lastTouchTime = 0;
static bool lastTouchState = false;
static int lastPressedButton = -1;

// Inicjalizacja klawiatury
void initKeypad() {
  // Klawiatura zostanie narysowana przez showKeypadScreen()
}

// Rysowanie klawiatury
void drawKeypad() {
  // Wyczyść tło
  tft.fillScreen(TFT_DARKGREY);
  
  // Pole wyświetlania PESEL
  tft.fillRect(DISP_X, DISP_Y, DISP_W, DISP_H, TFT_BLACK);
  tft.drawRect(DISP_X, DISP_Y, DISP_W, DISP_H, TFT_WHITE);
  
  // Rysowanie klawiszy w układzie 5x3 (jak w przykładzie)
  for (uint8_t row = 0; row < 5; row++) {
    for (uint8_t col = 0; col < 3; col++) {
      uint8_t b = col + row * 3;
      
      // Inicjalizacja przycisku
      key[b].initButton(&tft, 
                        KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), // x, y
                        KEY_W, KEY_H,         // szerokość, wysokość
                        TFT_WHITE,            // kolor obramowania
                        keyColor[b],          // kolor tła
                        TFT_WHITE,            // kolor tekstu
                        keyLabel[b],          // etykieta
                        KEY_TEXTSIZE);        // rozmiar tekstu
      
      // Narysowanie przycisku
      key[b].drawButton();
    }
  }
  
  // Instrukcja
  showKeypadStatus("Wprowadź 11-cyfrowy PESEL");
  
  // Aktualizuj wyświetlanie
  updatePESELDisplay();
}

// Nowa funkcja obsługi klawiatury zgodna z main.cpp
void handleKeypad(bool pressed, uint16_t x, uint16_t y) {
  unsigned long currentTime = millis();
  
  // Debounce - ignoruj zmiany przez 150ms po ostatnim dotyku
  if (currentTime - lastTouchTime < 150) {
    return;
  }
  
  // Jeśli zmienił się stan dotyku (z dotykany na nie dotykany lub odwrotnie)
  if (pressed != lastTouchState) {
    lastTouchTime = currentTime;
    lastTouchState = pressed;
    
    if (pressed) {
      // Ekran został dotknięty - znajdź przycisk
      lastPressedButton = -1;
      for (uint8_t b = 0; b < 15; b++) {
        if (key[b].contains(x, y)) {
          key[b].press(true);
          key[b].drawButton(true); // Narysuj jako naciśnięty
          lastPressedButton = b;
        } else {
          key[b].press(false);
        }
      }
    } else {
      // Ekran został zwolniony
      if (lastPressedButton >= 0) {
        // Przywróć wygląd przycisku i wykonaj akcję
        key[lastPressedButton].press(false);
        key[lastPressedButton].drawButton(false);
        
        // Wykonaj akcję dla zwolnionego przycisku
        executeKeyAction(lastPressedButton);
        
        lastPressedButton = -1;
      }
      
      // Ustaw wszystkie przyciski jako nie naciśnięte
      for (uint8_t b = 0; b < 15; b++) {
        key[b].press(false);
      }
    }
  }
}

// Funkcja wykonująca akcję dla danego przycisku
void executeKeyAction(int buttonIndex) {
  String keyPressed = String(keyLabel[buttonIndex]);
  
  if (keyPressed == "OK") {
    // Klawisz OK - sprawdź długość i wyślij
    if (enteredPESEL.length() == PESEL_LEN) {
      showKeypadStatus("Wysyłam PESEL...");
      delay(500);
      handleKeypadInput("OK");
    } else {
      showKeypadStatus("PESEL musi mieć 11 cyfr!");
      delay(1000);
      showKeypadStatus("Wprowadz 11-cyfrowy PESEL");
    }
    
  } else if (keyPressed == "Clr") {
    // Klawisz Clear - wyczyść wszystko
    enteredPESEL = "";
    updatePESELDisplay();
    showKeypadStatus("PESEL wyczyszczony");
    
  } else if (keyPressed == "Del") {
    // Klawisz Delete - usuń ostatnią cyfrę
    if (enteredPESEL.length() > 0) {
      enteredPESEL = enteredPESEL.substring(0, enteredPESEL.length() - 1);
      updatePESELDisplay();
      String statusMsg = "Cyfr: " + String(enteredPESEL.length()) + "/" + String(PESEL_LEN);
      showKeypadStatus(statusMsg.c_str());
    } else {
      showKeypadStatus("PESEL jest już pusty");
    }
    
  } else if (keyPressed == "." || keyPressed == "#") {
    // Te klawisze nie są używane dla PESEL
    showKeypadStatus("Tylko cyfry 0-9 dla PESEL");
    
  } else {
    // Klawisz numeryczny (0-9)
    if (enteredPESEL.length() < PESEL_LEN) {
      enteredPESEL += keyPressed;
      updatePESELDisplay();
      
      if (enteredPESEL.length() == PESEL_LEN) {
        showKeypadStatus("Naciśnij OK aby wysłać");
      } else {
        String statusMsg = "Cyfr: " + String(enteredPESEL.length()) + "/" + String(PESEL_LEN);
        showKeypadStatus(statusMsg.c_str());
      }
    } else {
      showKeypadStatus("PESEL ma już 11 cyfr");
    }
  }
}

// Obsługa dotyku klawiatury - stara funkcja, zachowana dla kompatybilności
void handleKeypadTouch(int touchX, int touchY) {
  // Ta funkcja nie jest już używana - zastąpiona przez handleKeypad
  // Pozostawiona tylko dla kompatybilności
}

// Pokaż status na klawiaturze (wzorowane na funkcji status z przykładu)
void showKeypadStatus(const char* msg) {
  tft.setTextPadding(240);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
}

// Aktualizacja wyświetlacza PESEL - nowa implementacja
void updatePESELDisplay() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(DISP_TSIZE);
  tft.setTextColor(DISP_TCOLOR, TFT_BLACK);
  
  // Wyczyść pole wyświetlacza
  tft.fillRect(DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2, TFT_BLACK);
  
  // Wyświetl PESEL
  tft.drawString(enteredPESEL, DISP_X + 10, DISP_Y + 15);
  
  // Pokaż licznik w prawym dolnym rogu
  String counter = String(enteredPESEL.length()) + "/" + String(PESEL_LEN);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(counter, DISP_X + DISP_W - 5, DISP_Y + DISP_H - 5);
}
