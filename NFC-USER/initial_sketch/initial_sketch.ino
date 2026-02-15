#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin();  // Uses hardware SPI pins (D11, D12, D13)
  mfrc522.PCD_Init();

  Serial.println("Nano ESP32 NFC Ready. Tap card...");
}

void loop() {

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("UID: ");

  String uidString = "";

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uidString += "0";
    }
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }

  uidString.toUpperCase();

  Serial.println(uidString);

  mfrc522.PICC_HaltA();
}
