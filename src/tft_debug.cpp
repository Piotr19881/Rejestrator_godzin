#include "tft_debug.h"

TFTDebug::TFTDebug(TFT_eSPI* display) {
    tft = display;
    currentLine = 0;
    lineHeight = 20; // Wysokość linii
    textColor = TFT_GREEN;
    backgroundColor = TFT_BLACK;
    clear();
}

void TFTDebug::println(String text) {
    // Skróć tekst jeśli za długi (max 35 znaków na linię)
    if (text.length() > 35) {
        text = text.substring(0, 32) + "...";
    }
    
    lines[currentLine] = text;
    currentLine = (currentLine + 1) % 12;
    refresh();
}

void TFTDebug::print(String text) {
    // Dodaj tekst do aktualnej linii bez przejścia do nowej
    if (currentLine == 0) {
        lines[11] += text;
    } else {
        lines[currentLine - 1] += text;
    }
    refresh();
}

void TFTDebug::addTimestamp(String text) {
    String timestampedText = "[" + String(millis()/1000) + "s] " + text;
    println(timestampedText);
}

void TFTDebug::clear() {
    for (int i = 0; i < 12; i++) {
        lines[i] = "";
    }
    currentLine = 0;
    tft->fillScreen(backgroundColor);
    
    // Dodaj nagłówek
    tft->setTextColor(TFT_YELLOW, backgroundColor);
    tft->setTextDatum(TL_DATUM);
    tft->drawString("ESP-CAM Debug Console", 5, 5, 2);
    tft->drawLine(0, 25, tft->width(), 25, TFT_YELLOW);
}

void TFTDebug::refresh() {
    // Wyczyść obszar tekstu (pozostaw nagłówek)
    tft->fillRect(0, 30, tft->width(), tft->height() - 30, backgroundColor);
    
    tft->setTextColor(textColor, backgroundColor);
    tft->setTextDatum(TL_DATUM);
    
    // Wyświetl linie w chronologicznej kolejności
    for (int i = 0; i < 12; i++) {
        int lineIndex = (currentLine + i) % 12;
        if (lines[lineIndex].length() > 0) {
            int yPos = 35 + i * lineHeight;
            if (yPos < tft->height() - lineHeight) {
                tft->drawString(lines[lineIndex], 5, yPos, 1);
            }
        }
    }
}

void TFTDebug::setColors(uint16_t text, uint16_t bg) {
    textColor = text;
    backgroundColor = bg;
}
