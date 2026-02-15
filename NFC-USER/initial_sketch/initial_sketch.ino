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
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

char ssid[] = "Anthas Home";
char pass[] = "althaf1109";

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

// --- Memory Functions ---

void loadUsers() {
  preferences.begin("users", true); // Open in read-only mode initially
  userCount = preferences.getInt("count", 0);
  for (int i = 0; i < userCount; i++) {
    users[i].name = preferences.getString(("name" + String(i)).c_str(), "");
    users[i].uid = preferences.getString(("uid" + String(i)).c_str(), "");
  }
  preferences.end();
  Serial.print("Loaded ");
  Serial.print(userCount);
  Serial.println(" users.");
}

void saveUsers() {
  preferences.begin("users", false); // Open in read/write mode
  preferences.putInt("count", userCount);
  // Save only the latest user added to be efficient
  int i = userCount - 1; 
  preferences.putString(("name" + String(i)).c_str(), users[i].name);
  preferences.putString(("uid" + String(i)).c_str(), users[i].uid);
  preferences.end();
}

// Function to wipe all data
void clearAllUsers() {
  Serial.println("Wiping Database...");
  preferences.begin("users", false);
  preferences.clear(); // Nuke all data in this namespace
  preferences.end();
  
  // Clear RAM data immediately
  userCount = 0;
  for(int i=0; i<MAX_USERS; i++) {
    users[i].name = "";
    users[i].uid = "";
  }
  
  Serial.println("System Reset: All users deleted.");
  Blynk.virtualWrite(V1, "System Reset: All Users Deleted");
}

// --- Blynk Handlers ---

// V0: Input for Registration Name
BLYNK_WRITE(V0) {
  String inputName = param.asStr();
  inputName.trim();

  if (inputName.length() == 0) return;

  if (userCount >= MAX_USERS) {
    Blynk.virtualWrite(V1, "Error: Memory Full");
    return;
  }

  pendingName = inputName;
  registrationMode = true;

  // Feedback to App
  Blynk.virtualWrite(V1, "Tap card to register " + pendingName + "...");
  Serial.println("Registration Mode: Waiting for card for " + pendingName);
}

// V3: Button to Reset All Users
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) { // If button is pressed
    clearAllUsers();
  }
}

// --- Main Setup & Loop ---

void setup() {
  Serial.begin(115200);
  
  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  SPI.begin();
  mfrc522.PCD_Init();

  loadUsers();

  Serial.println("--- System Ready ---");
  Blynk.virtualWrite(V1, "System Ready");
}

void loop() {
  Blynk.run();

  // 1. Check for RFID Card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Convert UID to String
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  // 2. Logic
  if (registrationMode) {
    // --- REGISTRATION LOGIC ---
    
    // Check if card is already taken
    for (int i = 0; i < userCount; i++) {
      if (users[i].uid == uidString) {
        String errorMsg = "Card already owned by " + users[i].name;
        Blynk.virtualWrite(V1, errorMsg);
        registrationMode = false;
        mfrc522.PICC_HaltA();
        return;
      }
    }

    // Save New User
    users[userCount].name = pendingName;
    users[userCount].uid = uidString;
    userCount++;
    saveUsers();

    // App Feedback: "User 'Name' Registered"
    String successMsg = "User '" + pendingName + "' Registered";
    Serial.println(successMsg);
    Blynk.virtualWrite(V1, successMsg);

    // *** FIX: Clear the Input Box on App ***
    Blynk.virtualWrite(V0, ""); 
    
    // Reset mode
    registrationMode = false;
    pendingName = ""; // Clear the name buffer
  } 
  else {
    // --- IDENTIFICATION LOGIC ---
    
    bool found = false;
    for (int i = 0; i < userCount; i++) {
      if (users[i].uid == uidString) {
        // App Feedback: "Welcome Name"
        String welcomeMsg = "Welcome " + users[i].name;
        Serial.println(welcomeMsg);
        Blynk.virtualWrite(V1, welcomeMsg);
        found = true;
        break;
      }
    }

    if (!found) {
      // App Feedback: "User Not Recognized"
      Serial.println("Unknown Card: " + uidString);
      Blynk.virtualWrite(V1, "User Not Recognized");
    }
  }

  // Halt PICC
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}