/* RC522      Nano ESP32
SDA(SS)    D10
SCK        D13
MOSI       D11
MISO       D12
RST        D9
GND        GND
3.3V       3.3V
*/

#define BLYNK_TEMPLATE_ID "TMPL2BRlMcCny"
#define BLYNK_TEMPLATE_NAME "RFID Access System"
#define BLYNK_AUTH_TOKEN "z1_KMsJq3kzSu1xmg3dWCBQS33S5dscX"

char ssid[] = "Anthas Home";
char pass[] = "althaf1109";


#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>


#define SS_PIN 10
#define RST_PIN 9
#define MAX_USERS 20

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences preferences;

struct User {
  String name;
  String uid;
};

User users[MAX_USERS];
int userCount = 0;

bool registrationMode = false;
String pendingName = "";

void loadUsers() {
  preferences.begin("users", true);
  userCount = preferences.getInt("count", 0);
  for (int i = 0; i < userCount; i++) {
    users[i].name = preferences.getString(("name" + String(i)).c_str(), "");
    users[i].uid = preferences.getString(("uid" + String(i)).c_str(), "");
  }
  preferences.end();
  Serial.print("Loaded ");
  Serial.print(userCount);
  Serial.println(" users from flash.");
}

void saveUsers() {
  preferences.begin("users", false);
  preferences.putInt("count", userCount);
  // We save the last added user specifically to be efficient
  int i = userCount - 1; 
  preferences.putString(("name" + String(i)).c_str(), users[i].name);
  preferences.putString(("uid" + String(i)).c_str(), users[i].uid);
  preferences.end();
}

BLYNK_WRITE(V0) {
  String inputName = param.asString();
  inputName.trim();

  if (inputName.length() == 0) {
    Blynk.virtualWrite(V2, "Error: Name cannot be empty.");
    return;
  }

  if (userCount >= MAX_USERS) {
    Blynk.virtualWrite(V2, "Error: User limit reached.");
    return;
  }

  pendingName = inputName;
  registrationMode = true;

  Blynk.virtualWrite(V2, "Registration Mode Active. Scan card for: " + pendingName);
}

void setup() {
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  while (!Serial);

  SPI.begin();
  mfrc522.PCD_Init();

  loadUsers();

  Serial.println("--- RFID Access System ---");
  Serial.println("Scan a card to identify, or type 'register NAME' to add new.");
}

void loop() {
  Blynk.run();
  // 1. Handle Serial Commands
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("register ")) {
      pendingName = input.substring(9);
      pendingName.trim();

      if (userCount >= MAX_USERS) {
        Serial.println("Error: User limit reached.");
      } else {
        registrationMode = true;
        Serial.print("Registration Mode Active. Scan card for: ");
        Serial.println(pendingName);
      }
    }
  }

  // 2. Check for RFID Card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Convert UID to String
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  // 3. Logic: Register or Identify?
  if (registrationMode) {
    // Check if already exists
    for (int i = 0; i < userCount; i++) {
      if (users[i].uid == uidString) {
        Serial.println("Access Denied: Card already registered to " + users[i].name);
        registrationMode = false;
        mfrc522.PICC_HaltA();
        return;
      }
    }

    // Save new user
    users[userCount].name = pendingName;
    users[userCount].uid = uidString;
    userCount++;
    saveUsers();

    String successMsg = "SUCCESS: " + pendingName + " registered.";
    Serial.println(successMsg);
    Blynk.virtualWrite(V1, successMsg);
    Blynk.virtualWrite(V2, "Registration Complete");
    registrationMode = false;
  } 
  else {
    // IDENTIFICATION MODE
    bool found = false;
    for (int i = 0; i < userCount; i++) {
      if (users[i].uid == uidString) {
        Serial.println("--------------------------");
        String logMsg = "Access Granted: " + users[i].name;
        Serial.println(logMsg);
        Blynk.virtualWrite(V1, logMsg);
        Serial.println("UID: " + uidString);
        Serial.println("--------------------------");
        found = true;
        break;
      }
    }

    if (!found) {
      String unknownMsg = "Access Denied. UID: " + uidString;
      Serial.println(unknownMsg);
      Blynk.virtualWrite(V1, unknownMsg);
    }
  }

  // Halt PICC and stop encryption on PCD
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}