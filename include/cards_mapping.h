#ifndef CARDS_MAPPING_H
#define CARDS_MAPPING_H

#include <Arduino.h>

// Mapowanie RFID hex -> PESEL
struct CardMapping {
  String rfidHex;
  String pesel;
  String name;
};

const CardMapping CARD_MAPPINGS[] = {
  {"0492A16FBAA281", "88080703299", "Piotr Prokop"},
  {"04 92 A1 6F BA 2A 81", "88080703299", "Piotr Prokop"}, // Format z spacjami
  // Dodaj więcej mapowań tutaj
  // {"INNE_RFID_HEX", "INNY_PESEL", "Inne Nazwisko"},
};

const int MAPPINGS_COUNT = sizeof(CARD_MAPPINGS) / sizeof(CardMapping);

// Funkcje pomocnicze
String mapRFIDtoPESEL(String rfidHex);
String getNameByRFID(String rfidHex);

#endif
