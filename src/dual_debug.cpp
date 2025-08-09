#include "dual_debug.h"

DualDebug::DualDebug(TFT_eSPI* tft) {
    if (tft != nullptr) {
        tftDebug = new TFTDebug(tft);
        tftEnabled = true;
    } else {
        tftDebug = nullptr;
        tftEnabled = false;
    }
    serialEnabled = true;
}

DualDebug::~DualDebug() {
    if (tftDebug != nullptr) {
        delete tftDebug;
        tftDebug = nullptr;
    }
}

void DualDebug::enableSerial(bool enable) {
    serialEnabled = enable;
}

void DualDebug::enableTFT(bool enable) {
    tftEnabled = enable && (tftDebug != nullptr);
}

void DualDebug::println(String message) {
    if (serialEnabled) {
        Serial.println(message);
    }
    if (tftEnabled && tftDebug != nullptr) {
        tftDebug->println(message);
    }
}

void DualDebug::print(String message) {
    if (serialEnabled) {
        Serial.print(message);
    }
    if (tftEnabled && tftDebug != nullptr) {
        tftDebug->print(message);
    }
}

void DualDebug::addTimestamp(String message) {
    String timestamped = "[" + String(millis()/1000) + "s] " + message;
    println(timestamped);
}

void DualDebug::clear() {
    if (tftEnabled && tftDebug != nullptr) {
        tftDebug->clear();
    }
}

void DualDebug::refresh() {
    if (tftEnabled && tftDebug != nullptr) {
        tftDebug->refresh();
    }
}
