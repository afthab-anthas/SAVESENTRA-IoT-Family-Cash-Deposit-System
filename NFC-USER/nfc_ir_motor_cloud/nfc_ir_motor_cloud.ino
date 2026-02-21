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

// --- Stepper Motor Pins ---
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
#include <time.h> // For NTP Clound Logging

char ssid[] = "Anthas Home";
char pass[] = "althaf1109";
bool authenticated = false;
int authenticatedUserIndex = -1; // Keep track of WHO is logged in
unsigned long authStartTime = 0;

// --- NTP Time Setup ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 14400; // UAE is UTC+4 (4 * 3600 = 14400)
const int   daylightOffset_sec = 0; // No DST in UAE

#define SS_PIN 10
#define RST_PIN 9
#define MAX_USERS 20

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences preferences;

// --- Stepper Setup ---
const int stepsPerRevolution = 2048; 
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4); 

// --- Motor Logic Variables ---
unsigned long lastIrBlockTime = 0;
const unsigned long motorStopDelay = 2000; // 2 seconds hysteresis
bool isMotorRunning = false;
bool depositInProgress = false;

struct User {
  String name;
  String uid;
  float balance; // [NEW] Track money
};

User users[MAX_USERS];
int userCount = 0;

bool registrationMode = false;
String pendingName = "";

// --- Memory Functions ---

void loadUsers() {
  preferences.begin("users", true); 
  userCount = preferences.getInt("count", 0);
  for (int i = 0; i < userCount; i++) {
    users[i].name = preferences.getString(("name" + String(i)).c_str(), "");
    users[i].uid = preferences.getString(("uid" + String(i)).c_str(), "");
    users[i].balance = preferences.getFloat(("bal" + String(i)).c_str(), 0.0);
  }
  preferences.end();
  Serial.print("Loaded ");
  Serial.print(userCount);
  Serial.println(" users.");
}

void saveUsers() {
  preferences.begin("users", false); 
  preferences.putInt("count", userCount);
  
  // Save only the latest user added properties
  int i = userCount - 1; 
  preferences.putString(("name" + String(i)).c_str(), users[i].name);
  preferences.putString(("uid" + String(i)).c_str(), users[i].uid);
  preferences.putFloat(("bal" + String(i)).c_str(), users[i].balance);
  preferences.end();
}

void saveUserBalance(int index) {
  preferences.begin("users", false);
  preferences.putFloat(("bal" + String(index)).c_str(), users[index].balance);
  preferences.end();
  Serial.println("Saved balance for " + users[index].name + ": " + String(users[index].balance));
}

// Function to wipe all data
void clearAllUsers() {
  Serial.println("Wiping Database...");
  preferences.begin("users", false);
  preferences.clear(); 
  preferences.end();
  
  userCount = 0;
  for(int i=0; i<MAX_USERS; i++) {
    users[i].name = "";
    users[i].uid = "";
    users[i].balance = 0.0;
  }
  
  Serial.println("System Reset: All users deleted.");
  Blynk.virtualWrite(V1, "System Reset: All Users Deleted");
}

String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "00:00:00";
  }
  char timeStringBuff[50]; //50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%b %d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
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

  Blynk.virtualWrite(V1, "Tap card to register " + pendingName + "...");
  Serial.println("Registration Mode: Waiting for card for " + pendingName);
}

// V3: Button to Reset All Users
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) clearAllUsers();
}

// --- Main Setup & Loop ---

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n\n--- Booting SaveSentra (Cloud Version) ---");

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  myStepper.setSpeed(10); 

  // Connect to Blynk
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk Connected!");
  
  // Init and Get Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("NTP Time Sync Requested.");

  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial(); 
  Serial.println("NFC Reader Initialized");

  loadUsers();

  Serial.println("--- System Ready ---");
  Blynk.virtualWrite(V1, "System Ready");
}

void loop() {
  Blynk.run();

  // 1. Check for RFID Card
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
        uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidString.toUpperCase();

    if (registrationMode) {
        for (int i = 0; i < userCount; i++) {
        if (users[i].uid == uidString) {
            String errorMsg = "Card already owned by " + users[i].name;
            Blynk.virtualWrite(V1, errorMsg);
            registrationMode = false;
            mfrc522.PICC_HaltA();
            goto skip_rfid;
        }
        }

        users[userCount].name = pendingName;
        users[userCount].uid = uidString;
        users[userCount].balance = 0.0; // Init balance
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
        // --- Auth Logic ---
        bool found = false;
        for (int i = 0; i < userCount; i++) {
        if (users[i].uid == uidString) {
            authenticatedUserIndex = i; // Save who logged in
            String welcomeMsg = "Welcome " + users[i].name;
            Serial.println(welcomeMsg);
            
            // Send welcome and current balance to App
            Blynk.virtualWrite(V1, welcomeMsg);
            Blynk.virtualWrite(V6, users[i].balance); 

            authenticated = true;
            depositInProgress = false; 
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

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  
  skip_rfid:; 

  // --- IR Monitoring & Motor Logic ---
  if (authenticated) {

    int currentIrState = digitalRead(IR_SENSOR_PIN);
    
    // --- Motor Control (Hysteresis) & Auto-Logout ---
    if (currentIrState == LOW) {
        lastIrBlockTime = millis();
        isMotorRunning = true;
        depositInProgress = true; 
        
        digitalWrite(LED_BUILTIN, HIGH); 
        Blynk.virtualWrite(V4, "Intake Active");
    } 
    else {
        digitalWrite(LED_BUILTIN, LOW);
        unsigned long timeSinceClear = millis() - lastIrBlockTime;

        // 1. Motor Hysteresis
        if (timeSinceClear < motorStopDelay) {
            isMotorRunning = true;
        } else {
            isMotorRunning = false;
            Blynk.virtualWrite(V4, "Waiting...");
        }

        // 2. Deposit Processing & Auto-Logout
        if (depositInProgress && timeSinceClear > (motorStopDelay + 3000)) {
             // DEPOSIT LOGIC
             float depositAmount = 10.0; // Hardcoded requirement
             users[authenticatedUserIndex].balance += depositAmount; // Add to struct
             
             // Save to memory
             saveUserBalance(authenticatedUserIndex);

             // Cloud Logging
             String timestamp = getTimestamp();
             String logMessage = timestamp + " | " + users[authenticatedUserIndex].name + " deposited " + String(depositAmount, 2) + " AED. Total: " + String(users[authenticatedUserIndex].balance, 2) + " AED";
             Serial.println(logMessage);
             
             Blynk.virtualWrite(V5, logMessage); // Terminal/Log string pin
             Blynk.virtualWrite(V6, users[authenticatedUserIndex].balance); // Update balance display

             // Logout Sequence
             Serial.println("Auto-Logout: Processing Complete");
             Blynk.virtualWrite(V1, "Session Ended");
             
             authenticated = false;
             depositInProgress = false;
             isMotorRunning = false; 
             authenticatedUserIndex = -1; // Clear tracking
             digitalWrite(LED_BUILTIN, LOW);
        }
    }

    // --- Execute Step (Non-blocking) ---
    if (isMotorRunning) {
        myStepper.step(10); 
    } 
  }
}
