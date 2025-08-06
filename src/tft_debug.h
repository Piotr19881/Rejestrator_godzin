#ifndef TFT_DEBUG_H
#define TFT_DEBUG_H

#include <TFT_eSPI.h>

class TFTDebug {
private:
    TFT_eSPI* tft;
    String lines[12]; // 12 linii na ekranie
    int currentLine;
    int lineHeight;
    uint16_t textColor;
    uint16_t backgroundColor;
    
public:
    TFTDebug(TFT_eSPI* display);
    void println(String text);
    void print(String text);
    void clear();
    void refresh();
    void setColors(uint16_t text, uint16_t bg);
    void addTimestamp(String text);
};

#endif
