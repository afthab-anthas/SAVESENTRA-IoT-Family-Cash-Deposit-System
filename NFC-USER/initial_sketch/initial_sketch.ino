/* 
RC522	    Nano ESP32
SDA(SS)	  D10
SCK	      D13
MOSI	    D11
MISO	    D12
RST	      D9
GND	      GND
3.3V	    3.3V
*/



#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
#define MAX_USERS 20

MFRC522 mfrc522(SS_PIN, RST_PIN);

struct User {
  String name;
  String uid;
};

User users[MAX_USERS];
int userCount = 0;

bool registrationMode = false;
String pendingName = "";

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("System Ready.");
  Serial.println("Type: register NAME");
}

void loop() {

  // Handle Serial Commands
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("register ")) {
      pendingName = input.substring(9);
      pendingName.trim();

      if (userCount >= MAX_USERS) {
        Serial.println("User limit reached.");
        return;
      }

      registrationMode = true;
      Serial.println("Waiting for card...");
    }
  }

  // NFC Logic
  if (registrationMode) {

    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;

    String uidString = "";

    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) {
        uidString += "0";
      }
      uidString += String(mfrc522.uid.uidByte[i], HEX);
    }

    uidString.toUpperCase();

    // Check duplicate
    for (int i = 0; i < userCount; i++) {
      if (users[i].uid == uidString) {
        Serial.println("Card already registered.");
        registrationMode = false;
        mfrc522.PICC_HaltA();
        return;
      }
    }

    // Save user
    users[userCount].name = pendingName;
    users[userCount].uid = uidString;
    userCount++;

    Serial.print("User ");
    Serial.print(pendingName);
    Serial.print(" registered with UID ");
    Serial.println(uidString);

    registrationMode = false;
    mfrc522.PICC_HaltA();
  }
}
