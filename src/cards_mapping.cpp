#include "cards_mapping.h"

String mapRFIDtoPESEL(String rfidHex) {
  // Normalizuj format RFID (usuń spacje, wielkie litery)
  rfidHex.replace(" ", "");
  rfidHex.toUpperCase();
  rfidHex.trim();
  
  Serial.print("[MAPPING] Szukam mapowania dla: ");
  Serial.println(rfidHex);
  
  // Przeszukaj tablicę mapowań
  for (int i = 0; i < MAPPINGS_COUNT; i++) {
    String mappedRFID = CARD_MAPPINGS[i].rfidHex;
    mappedRFID.replace(" ", "");
    mappedRFID.toUpperCase();
    mappedRFID.trim();
    
    if (mappedRFID == rfidHex) {
      Serial.print("[MAPPING] Znaleziono: RFID=");
      Serial.print(rfidHex);
      Serial.print(" -> PESEL=");
      Serial.print(CARD_MAPPINGS[i].pesel);
      Serial.print(" (");
      Serial.print(CARD_MAPPINGS[i].name);
      Serial.println(")");
      return CARD_MAPPINGS[i].pesel;
    }
  }
  
  Serial.println("[MAPPING] Brak mapowania - zwracam oryginalny ID");
  return rfidHex; // Brak mapowania, zwróć oryginalny
}

String getNameByRFID(String rfidHex) {
  rfidHex.replace(" ", "");
  rfidHex.toUpperCase();
  rfidHex.trim();
  
  for (int i = 0; i < MAPPINGS_COUNT; i++) {
    String mappedRFID = CARD_MAPPINGS[i].rfidHex;
    mappedRFID.replace(" ", "");
    mappedRFID.toUpperCase();
    mappedRFID.trim();
    
    if (mappedRFID == rfidHex) {
      return CARD_MAPPINGS[i].name;
    }
  }
  
  return "Nieznany użytkownik";
}
