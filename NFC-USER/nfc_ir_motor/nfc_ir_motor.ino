/* RC522      Nano ESP32
   SDA(SS)    D10
   SCK        D13
   MOSI       D11
   MISO       D12
   RST        D9
   GND        GND
   3.3V       3.3V

   TCRT5000    Nano ESP32
   DO          D4
   VCC         3.3V
   GND         GND

   Stepper     Nano ESP32
   IN1         D5
   IN2         D6
   IN3         D7
   IN4         D8
*/

#define BLYNK_TEMPLATE_ID "TMPL2BRlMcCny"
#define BLYNK_TEMPLATE_NAME "RFID Access System"
#define BLYNK_AUTH_TOKEN "z1_KMsJq3kzSu1xmg3dWCBQS33S5dscX"
#define BLYNK_PRINT Serial
#define IR_SENSOR_PIN 4


#define IN1 5
#define IN2 6
#define IN3 7
#define IN4 8

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <Stepper.h>

char ssid[] = "Anthas Home";
char pass[] = "althaf1109";
bool authenticated = false;
unsigned long authStartTime = 0;


#define SS_PIN 10
#define RST_PIN 9
#define MAX_USERS 20

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences preferences;

// --- Stepper Setup ---
const int stepsPerRevolution = 2048; 
// Note: Stepper library expects 1-3-2-4 sequence for 28BYJ-48
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4); 

// --- Motor Logic Variables ---
unsigned long lastIrBlockTime = 0;
const unsigned long motorStopDelay = 2000; // 2 seconds hysteresis
bool isMotorRunning = false;
bool depositInProgress = false;

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
  pinMode(IR_SENSOR_PIN, INPUT);
  
  // LED Feedback setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Motor Speed (RPM)
  myStepper.setSpeed(10); // 10-15 RPM is safe for 5V

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
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Convert UID to String
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
        uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidString.toUpperCase();

    // 2. Logic (Rest of the RFID handling)
    if (registrationMode) {
        // ... Registration Logic ...
        for (int i = 0; i < userCount; i++) {
        if (users[i].uid == uidString) {
            String errorMsg = "Card already owned by " + users[i].name;
            Blynk.virtualWrite(V1, errorMsg);
            registrationMode = false;
            mfrc522.PICC_HaltA();
            goto skip_rfid; // Exit to end of RFID block
        }
        }

        users[userCount].name = pendingName;
        users[userCount].uid = uidString;
        userCount++;
        saveUsers();

        String successMsg = "User '" + pendingName + "' Registered";
        Serial.println(successMsg);
        Blynk.virtualWrite(V1, successMsg);
        Blynk.virtualWrite(V0, ""); 
        registrationMode = false;
        pendingName = "";
    } 
    else {
        // ... Auth Logic ...
        bool found = false;
        for (int i = 0; i < userCount; i++) {
        if (users[i].uid == uidString) {
            String welcomeMsg = "Welcome " + users[i].name;
            Serial.println(welcomeMsg);
            Blynk.virtualWrite(V1, welcomeMsg);
            authenticated = true;
            depositInProgress = false; // Reset for new session
            authStartTime = millis();
            found = true;
            break;
        }
        }

        if (!found) {
        Serial.println("Unknown Card: " + uidString);
        Blynk.virtualWrite(V1, "User Not Recognized");
        }
    }

    // Halt PICC
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  
  skip_rfid:; // Label to jump to if needed

  // --- IR Monitoring & Motor Logic ---
  if (authenticated) {
    // Note: Auth timeout checking removed as requested

    int currentIrState = digitalRead(IR_SENSOR_PIN);
    
    // --- Motor Control (Hysteresis) & Auto-Logout ---
    if (currentIrState == LOW) {
        // Object Detected -> Reset timer and ensure running
        lastIrBlockTime = millis();
        isMotorRunning = true;
        depositInProgress = true; // Mark that a deposit has started
        
        digitalWrite(LED_BUILTIN, HIGH); 
        Blynk.virtualWrite(V4, "Intake Active");
    } 
    else {
        digitalWrite(LED_BUILTIN, LOW);
        unsigned long timeSinceClear = millis() - lastIrBlockTime;

        // 1. Motor Hysteresis (0s - 2s)
        if (timeSinceClear < motorStopDelay) {
            // Keep running for a bit longer
            isMotorRunning = true;
        } else {
            // Time up, stop motor
            isMotorRunning = false;
            Blynk.virtualWrite(V4, "Waiting...");
        }

        // 2. Auto-Logout (After 2s Motor + 3s Wait = 5s)
        if (depositInProgress && timeSinceClear > (motorStopDelay + 3000)) {
             Serial.println("Auto-Logout: Processing Complete");
             Blynk.virtualWrite(V1, "Session Ended");
             authenticated = false;
             depositInProgress = false;
             isMotorRunning = false; // Ensure motor is off
             digitalWrite(LED_BUILTIN, LOW);
        }
    }


    // --- Execute Step (Non-blocking) ---
    if (isMotorRunning) {
        // Move small amount so we don't block the loop
        myStepper.step(10); 
    } else {
        // Optional: Cut power to coils to save energy? 
        // For now, keep energized to hold position or just stop stepping.
        // To turn off coils: digitalWrite(IN1, LOW); ... etc
    }
  }
}
