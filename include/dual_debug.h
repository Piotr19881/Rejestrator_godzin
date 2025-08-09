#ifndef DUAL_DEBUG_H
#define DUAL_DEBUG_H

#include <Arduino.h>
#include "tft_debug.h"
#include <TFT_eSPI.h>

class DualDebug {
private:
    TFTDebug* tftDebug;
    bool serialEnabled;
    bool tftEnabled;
    
public:
    DualDebug(TFT_eSPI* tft = nullptr);
    ~DualDebug();
    
    void enableSerial(bool enable);
    void enableTFT(bool enable);
    void println(String message);
    void print(String message);
    void addTimestamp(String message);
    void clear();
    void refresh();
};

#endif
