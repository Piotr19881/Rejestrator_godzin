#include "screens.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// Tablica tekstów motywacyjnych
const char* motivationalTexts[] = {
  "Przyloz karte RFID",
  "Kazdy dzien to nowa szansa!",
  "Sukces zaczyna sie od pierwszego kroku",
  "Twoja praca ma znaczenie",
  "Dzisiaj bedzie dobry dzien!",
  "Osiagnij swoje cele",
  "Nie poddawaj sie, walcz dalej",
  "Wierz w siebie",
  "Zmiana zaczyna sie od Ciebie",
  "Bądz najlepsza wersja siebie",
  "Czas to najcenniejszy zasob",
  "Twoje marzenia sa w zasiegu",
  "Kazda chwila sie liczy",
  "Rób to co kochasz",
  "Jutro zaczyna sie dzisiaj"
};

const int NUM_TEXTS = sizeof(motivationalTexts) / sizeof(motivationalTexts[0]);

// Zmienne stanu ekranu oczekiwania
static int currentTextIndex = 0;
static unsigned long lastTextChange = 0;
static const unsigned long TEXT_DISPLAY_TIME = 3000; // 3 sekundy na tekst
static bool screenNeedsUpdate = true;

void initScreens() {
  currentTextIndex = 0;
  lastTextChange = millis();
  screenNeedsUpdate = true;
}

void updateWaitingScreen() {
  unsigned long currentTime = millis();
  
  // Sprawdź czy czas na zmianę tekstu
  if (currentTime - lastTextChange >= TEXT_DISPLAY_TIME) {
    currentTextIndex = (currentTextIndex + 1) % NUM_TEXTS;
    lastTextChange = currentTime;
    screenNeedsUpdate = true;
  }
  
  // Aktualizuj ekran jeśli potrzeba (ale nie częściej niż co 50ms)
  static unsigned long lastScreenUpdate = 0;
  if (screenNeedsUpdate && (currentTime - lastScreenUpdate > 50)) {
    displayMotivationalText();
    screenNeedsUpdate = false;
    lastScreenUpdate = currentTime;
  }
}

void displayMotivationalText() {
  tft.fillScreen(TFT_BLACK);
  
  // Wybierz kolor w zależności od typu tekstu
  uint16_t textColor = TFT_WHITE;
  if (currentTextIndex == 0) {
    textColor = TFT_CYAN; // Główna instrukcja
  } else if (currentTextIndex % 3 == 1) {
    textColor = TFT_YELLOW; // Co trzeci tekst żółty
  } else if (currentTextIndex % 3 == 2) {
    textColor = TFT_GREEN; // Co trzeci tekst zielony
  }
  
  tft.setTextColor(textColor, TFT_BLACK);
  
  // Wyśrodkuj tekst na ekranie
  String text = String(motivationalTexts[currentTextIndex]);
  
  // Podziel długie teksty na linie
  if (text.length() > 20) {
    int spacePos = text.indexOf(' ', text.length() / 2);
    if (spacePos > 0) {
      String line1 = text.substring(0, spacePos);
      String line2 = text.substring(spacePos + 1);
      
      tft.drawString(line1, 20, 90, 2);
      tft.drawString(line2, 20, 120, 2);
    } else {
      tft.drawString(text, 20, 100, 2);
    }
  } else {
    tft.drawString(text, 20, 100, 2);
  }
  
  // Dodaj wskaźnik postępu (kropki)
  drawProgressIndicator();
}

void drawProgressIndicator() {
  int dotY = 200;
  int startX = 120;
  int dotSpacing = 15;
  
  for (int i = 0; i < 5; i++) {
    uint16_t dotColor = (i == (currentTextIndex % 5)) ? TFT_WHITE : TFT_DARKGREY;
    tft.fillCircle(startX + (i * dotSpacing), dotY, 3, dotColor);
  }
}

void forceScreenUpdate() {
  screenNeedsUpdate = true;
}

void resetTextCycle() {
  currentTextIndex = 0;
  lastTextChange = millis();
  screenNeedsUpdate = true;
}

void stopScreenUpdates() {
  screenNeedsUpdate = false;
}

bool isScreenTouched(uint16_t x, uint16_t y) {
  // Sprawdź czy dotknięto ekran (dowolne miejsce poza obszarem przycisku)
  return (x > 0 && x < 240 && y > 0 && y < 320);
}

void showLoadingScreen(TFT_eSPI& display, String message) {
  display.fillScreen(TFT_BLACK);
  
  // Tytuł
  display.setTextColor(TFT_YELLOW);
  display.setTextSize(2);
  display.setTextDatum(TC_DATUM);
  display.drawString("PROSZĘ CZEKAĆ", display.width()/2, 50, 2);
  
  // Wiadomość
  display.setTextColor(TFT_WHITE);
  display.setTextSize(1);
  display.setTextDatum(TC_DATUM);
  display.drawString(message, display.width()/2, 100, 2);
  
  // Animowany wskaźnik ładowania
  static int loadingDots = 0;
  String dots = "";
  for (int i = 0; i <= loadingDots; i++) {
    dots += ".";
  }
  display.drawString(dots, display.width()/2, 120, 2);
  loadingDots = (loadingDots + 1) % 4;
}

void showWelcomeScreen(TFT_eSPI& display, String userName) {
  display.fillScreen(TFT_GREEN);
  
  // Nagłówek
  display.setTextColor(TFT_WHITE);
  display.setTextSize(2);
  display.setTextDatum(TC_DATUM);
  display.drawString("WITAMY!", display.width()/2, 80, 4);
  
  // Nazwa użytkownika
  display.setTextColor(TFT_BLACK);
  display.setTextSize(1);
  display.setTextDatum(TC_DATUM);
  display.drawString(userName, display.width()/2, 160, 2);
  
  // Czas
  display.setTextColor(TFT_WHITE);
  display.drawString("Czas: " + String(millis()/1000) + "s", display.width()/2, 200, 2);
}

void showErrorScreen(TFT_eSPI& display, String title, String message) {
  display.fillScreen(TFT_RED);
  
  // Tytuł błędu
  display.setTextColor(TFT_WHITE);
  display.setTextSize(2);
  display.setTextDatum(TC_DATUM);
  display.drawString(title, display.width()/2, 80, 2);
  
  // Wiadomość błędu
  display.setTextColor(TFT_YELLOW);
  display.setTextSize(1);
  display.setTextDatum(TC_DATUM);
  display.drawString(message, display.width()/2, 140, 2);
  
  // Instrukcja
  display.setTextColor(TFT_WHITE);
  display.drawString("Spróbuj ponownie", display.width()/2, 200, 2);
}

void showMainScreen(TFT_eSPI& display) {
  // Rozpocznij normalny cykl tekstów motywacyjnych
  initScreens();
  updateWaitingScreen();
}
