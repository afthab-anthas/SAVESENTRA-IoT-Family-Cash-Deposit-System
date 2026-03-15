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

#define BLYNK_TEMPLATE_ID "TMPL2sUhFh8RM"
#define BLYNK_TEMPLATE_NAME "SAVESENTRA"
#define BLYNK_AUTH_TOKEN "jIf-JTtFAMlw41WrJvqtZalG0-WnrBXG"
#define BLYNK_PRINT Serial
#define IR_SENSOR_PIN 4

// Stepper Motor Pins 
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
#include <HTTPClient.h>
#include <time.h> 

char ssid[] = "Anthas Home";
char pass[] = "althaf1109";

// Raspberry Pi ML Server 
const char* rpiServer = "http://raspberrypi.local:5000/deposit";

bool authenticated = false;
int authenticatedUserIndex = -1; // WHO is logged in
unsigned long authStartTime = 0;
int selectedMenuIndex = -1; // Tracks which user is clicked in the menu

// NTP Time Setup 
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 14400; 
const int   daylightOffset_sec = 0; 

#define SS_PIN 10
#define RST_PIN 9
#define MAX_USERS 20

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences preferences;

// Stepper Setup 
const int stepsPerRevolution = 2048; 
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4); 

// Motor Logic Variables 
unsigned long lastIrBlockTime = 0;
const unsigned long motorStopDelay = 5000; 
bool isMotorRunning = false;
bool depositInProgress = false;
long currentDepositSteps = 0; // Track Steps
long billLengthSteps = 0; // Track actual bill length for denomination check

int familyGoal = 1000; // Default goal

struct User {
  String name;
  String uid;
  float balance; 
  String role;   
};

User users[MAX_USERS];
int userCount = 0;

bool registrationMode = false;
String pendingName = "";

// Store the data inside ESP32 

void loadUsers() {
  preferences.begin("users", true); 
  userCount = preferences.getInt("count", 0);
  familyGoal = preferences.getInt("goal", 1000); 
  for (int i = 0; i < userCount; i++) {
    users[i].name = preferences.getString(("name" + String(i)).c_str(), "");
    users[i].uid = preferences.getString(("uid" + String(i)).c_str(), "");
    users[i].balance = preferences.getFloat(("bal" + String(i)).c_str(), 0.0);
    users[i].role = preferences.getString(("role" + String(i)).c_str(), "Other");
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
  preferences.putString(("role" + String(i)).c_str(), users[i].role);
  preferences.end();
}

void saveUserBalance(int index) {
  preferences.begin("users", false);
  preferences.putFloat(("bal" + String(index)).c_str(), users[index].balance);
  preferences.end();
  Serial.println("Saved balance for " + users[index].name + ": " + String(users[index].balance));
}

// Reset all the data
void clearAllUsers() {
  Serial.println("Wiping Database");
  preferences.begin("users", false);
  preferences.clear(); 
  preferences.end();
  
  userCount = 0;
  for(int i=0; i<MAX_USERS; i++) {
    users[i].name = "";
    users[i].uid = "";
    users[i].balance = 0.0;
    users[i].role = "";
  }
  
  Serial.println("All users deleted.");
  Blynk.virtualWrite(V1, "All users deleted.");
  updateBlynkUserList(); 
}

String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "00:00:00";
  }
  char timeStringBuff[50]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}


void updateBlynkUserList() {
    if (userCount == 0) {
        Blynk.setProperty(V20, "labels", "No Users Registered");
        return;
    }
    // To select users in app menu
    Blynk.setProperty(V20, "labels", 
        (userCount > 0 ? users[0].name : ""), 
        (userCount > 1 ? users[1].name : ""), 
        (userCount > 2 ? users[2].name : ""), 
        (userCount > 3 ? users[3].name : ""),
        (userCount > 4 ? users[4].name : "")
    );
    
    Serial.println("Blynk User List Refreshed.");
}

// logout button
BLYNK_WRITE(V12) {
  if (param.asInt() == 1 && authenticated) {
    Serial.println("Manual logout triggered via App.");
    performLogout();
  }
}

// name input
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

// reset button
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) clearAllUsers();
}

// user selection
BLYNK_WRITE(V20) {
  int selectedIndex = param.asInt();
  
  if (selectedIndex >= 0 && selectedIndex < userCount) {
    selectedMenuIndex = selectedIndex; // SAVE globally for V8 to delete
    Blynk.virtualWrite(V6, users[selectedIndex].balance);
    
    String profileStatus = "Profile: " + users[selectedIndex].name + " (" + users[selectedIndex].role + ")";
    Blynk.virtualWrite(V1, profileStatus);
    
    // Auto-update the V21 Role Dropdown
    int roleMenuIndex = 3; // Default to Other
    if (users[selectedIndex].role == "Parent") roleMenuIndex = 1;
    else if (users[selectedIndex].role == "Child") roleMenuIndex = 2;
    Blynk.virtualWrite(V21, roleMenuIndex);
    
    if (familyGoal > 0) {
      float goalPct = (users[selectedIndex].balance / (float)familyGoal) * 100.0;
      Blynk.virtualWrite(V11, goalPct);
    } else {
      Blynk.virtualWrite(V11, 0.0);
    }
    
    Serial.println("App selected: " + users[selectedIndex].name);
  }
}

// role selection
BLYNK_WRITE(V21) {
  if (selectedMenuIndex >= 0 && selectedMenuIndex < userCount) {
    int roleSelection = param.asInt(); 

    switch (roleSelection) {
      case 1: users[selectedMenuIndex].role = "Parent"; break;
      case 2: users[selectedMenuIndex].role = "Child"; break;
      case 3: users[selectedMenuIndex].role = "Other"; break;
      default: return; 
    }

    preferences.begin("users", false);
    preferences.putString(("role" + String(selectedMenuIndex)).c_str(), users[selectedMenuIndex].role);
    preferences.end();
    
    String profileStatus = "Profile: " + users[selectedMenuIndex].name + " (" + users[selectedMenuIndex].role + ")";
    Blynk.virtualWrite(V1, profileStatus);
    
    Serial.println("Role updated to: " + users[selectedMenuIndex].role);
  } else {
    Blynk.virtualWrite(V1, "Select a user first!");
  }
}

// delete particular user
BLYNK_WRITE(V8) {
  if (param.asInt() == 1) {
    int indexToDelete = selectedMenuIndex; 

    if (indexToDelete >= 0 && indexToDelete < userCount) {
      Serial.println("Deleting User: " + users[indexToDelete].name);

      for (int i = indexToDelete; i < userCount - 1; i++) {
        users[i] = users[i + 1];
      }
      userCount--;

      preferences.begin("users", false);
      preferences.clear(); 
      preferences.putInt("count", userCount);
      for (int i = 0; i < userCount; i++) {
        preferences.putString(("name" + String(i)).c_str(), users[i].name);
        preferences.putString(("uid" + String(i)).c_str(), users[i].uid);
        preferences.putFloat(("bal" + String(i)).c_str(), users[i].balance);
        preferences.putString(("role" + String(i)).c_str(), users[i].role);
      }
      preferences.end();

      updateBlynkUserList(); 
      Blynk.virtualWrite(V1, "User Deleted Successfully");
      Blynk.virtualWrite(V6, 0); 
      
      float total = 0;
      for (int j = 0; j < userCount; j++) total += users[j].balance;
      Blynk.virtualWrite(V10, total);
      
      if (authenticatedUserIndex == indexToDelete) authenticatedUserIndex = -1;
      else if (authenticatedUserIndex > indexToDelete) authenticatedUserIndex--; 
      
      selectedMenuIndex = -1; 
      Serial.println("User list updated.");
    } else {
      Blynk.virtualWrite(V1, "Error: No user selected");
    }
  }
}

// family goal setting
BLYNK_WRITE(V7) {
  familyGoal = param.asInt();
  Serial.print("New Family Savings Goal: ");
  Serial.println(familyGoal);

  preferences.begin("users", false);
  preferences.putInt("goal", familyGoal);
  preferences.end();
  
  Blynk.setProperty(V10, "max", familyGoal); 
  Blynk.setProperty(V6, "max", familyGoal);

  float totalSavings = 0;
  for (int j = 0; j < userCount; j++) totalSavings += users[j].balance;
  
  Blynk.virtualWrite(V10, totalSavings); 
  if (authenticatedUserIndex != -1) {
    Blynk.virtualWrite(V6, users[authenticatedUserIndex].balance); 
  }
  updateContributionStatus();

  // Send to Pi for ML prediction
  String rpiPredictUrl = String(rpiServer);
  rpiPredictUrl.replace("/deposit", "/predict");
  HTTPClient http;
  http.begin(rpiPredictUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String payload = "goal=" + String(familyGoal);
  http.POST(payload);
  http.end();
}

// contribution % calculation
void updateContributionStatus() {
  int indexToCalculate = -1;

  if (authenticatedUserIndex != -1) {
    indexToCalculate = authenticatedUserIndex;
  } 
  else if (selectedMenuIndex != -1) {
    indexToCalculate = selectedMenuIndex;
  }

  if (indexToCalculate != -1 && familyGoal > 0) {
    float goalPercentage = (users[indexToCalculate].balance / (float)familyGoal) * 100.0;
    Blynk.virtualWrite(V11, goalPercentage);
  } else {
    Blynk.virtualWrite(V11, 0.0);
  }
}

// logout
void performLogout() {
  Serial.println("Logging out...");
  Blynk.virtualWrite(V1, "Session Ended");
  
  // Hide Admin Tools
  Blynk.setProperty(V0, "isHidden", true);
  Blynk.setProperty(V3, "isHidden", true);
  Blynk.setProperty(V7, "isHidden", true);
  Blynk.setProperty(V8, "isHidden", true);
  Blynk.setProperty(V12, "isHidden", true);
  Blynk.setProperty(V21, "isHidden", true);
  Blynk.setProperty(V30, "isHidden", true);

  authenticated = false;
  depositInProgress = false;
  isMotorRunning = false;
  authenticatedUserIndex = -1;
  updateContributionStatus(); 
  digitalWrite(LED_BUILTIN, LOW);
}


void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n\nSaveSentra");

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  myStepper.setSpeed(10); 

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk Connected!");
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time Sync requested.");

  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial(); 
  Serial.println("NFC Ready");

  loadUsers();

  Blynk.virtualWrite(V1, "System Ready");
  updateBlynkUserList(); 

  Blynk.virtualWrite(V7, familyGoal);
  Blynk.setProperty(V10, "max", familyGoal);
  Blynk.setProperty(V6, "max", familyGoal);
  
  Blynk.setProperty(V0, "isHidden", true);
  Blynk.setProperty(V3, "isHidden", true);
  Blynk.setProperty(V7, "isHidden", true);
  Blynk.setProperty(V8, "isHidden", true);
  Blynk.setProperty(V12, "isHidden", true);
  Blynk.setProperty(V21, "isHidden", true);
  Blynk.setProperty(V30, "isHidden", true);
}

void loop() {
  Blynk.run();

  // Check for RFID Card
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
        users[userCount].balance = 0.0; 
        users[userCount].role = "Other"; 
        userCount++;
        saveUsers();

        String successMsg = "User '" + pendingName + "' Registered";
        Serial.println(successMsg);
        Blynk.virtualWrite(V1, successMsg);
        Blynk.virtualWrite(V0, ""); 
        registrationMode = false;
        pendingName = "";
        updateBlynkUserList(); 
    } 
    else {
        // Authentication
        bool found = false;
        for (int i = 0; i < userCount; i++) {
        if (users[i].uid == uidString) {
            authenticatedUserIndex = i; 
            String welcomeMsg = "Welcome " + users[i].name;
            Serial.println(welcomeMsg);
            
            Blynk.virtualWrite(V1, welcomeMsg);
            Blynk.virtualWrite(V6, users[i].balance); 

            float totalSavings = 0;
            for (int j = 0; j < userCount; j++) totalSavings += users[j].balance;
            Blynk.virtualWrite(V10, totalSavings);

            Blynk.virtualWrite(V20, i); 
            selectedMenuIndex = i; 
            
            int roleMenuIndex = 3; 
            if (users[i].role == "Parent") roleMenuIndex = 1;
            else if (users[i].role == "Child") roleMenuIndex = 2;
            Blynk.virtualWrite(V21, roleMenuIndex);

            // Show admin controls if it's a parent
            if (users[i].role == "Parent") {
                Serial.println("Admin logged in. Unlocking tools.");
                Blynk.setProperty(V0, "isHidden", false);
                Blynk.setProperty(V3, "isHidden", false);
                Blynk.setProperty(V7, "isHidden", false);
                Blynk.setProperty(V8, "isHidden", false);
                Blynk.setProperty(V12, "isHidden", false);
                Blynk.setProperty(V21, "isHidden", false);
                Blynk.setProperty(V30, "isHidden", false);
            } else {
                Serial.println("Normal user. Admin tools stay hidden.");
                Blynk.setProperty(V0, "isHidden", true);
                Blynk.setProperty(V3, "isHidden", true);
                Blynk.setProperty(V7, "isHidden", true);
                Blynk.setProperty(V8, "isHidden", true);
                Blynk.setProperty(V12, "isHidden", true);
                Blynk.setProperty(V21, "isHidden", true);
                Blynk.setProperty(V30, "isHidden", true);
            }

            authenticated = true;
            depositInProgress = false; 
            authStartTime = millis();
            
            // Start motor immediately on Auth
            isMotorRunning = true;
            lastIrBlockTime = millis(); 
            
            currentDepositSteps = 0;
            billLengthSteps = 0;
            Blynk.virtualWrite(V4, "0 Steps");
            updateContributionStatus(); 

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

  // IR Monitoring & Motor Logic
  if (authenticated) {

    int currentIrState = digitalRead(IR_SENSOR_PIN);
    
    if (currentIrState == LOW) {
        lastIrBlockTime = millis();
        isMotorRunning = true;
        depositInProgress = true;
        
        digitalWrite(LED_BUILTIN, HIGH); 
    } 
    else {
        digitalWrite(LED_BUILTIN, LOW);
        unsigned long timeSinceClear = millis() - lastIrBlockTime;

        if (timeSinceClear < motorStopDelay) {
            isMotorRunning = true;
        } else {
            isMotorRunning = false;
        }

        // Process the money 
        if (depositInProgress && timeSinceClear > (motorStopDelay + 3000)) {
             
             Serial.print("Checking bill length... Steps recorded: ");
             Serial.println(billLengthSteps);
             
             float depositAmount = 0.0;

             // Figure out note based on step count
             if (billLengthSteps >= 4990 && billLengthSteps <= 5080) {
                 depositAmount = 5.0;
                 Serial.println("5 AED note.");
             } 
             else if (billLengthSteps >= 5090 && billLengthSteps <= 5200) {
                 depositAmount = 10.0;
                 Serial.println("10 AED note.");
             } 
             else {
                 Serial.println("Invalid note size.");
                 Blynk.virtualWrite(V1, "Error: Unknown Note Size");
             }
             
             if (depositAmount > 0.0) {
                 String notifyMsg = users[authenticatedUserIndex].name + " deposited " + String(depositAmount, 0) + " AED";
                 Blynk.logEvent("note_deposited", notifyMsg);

                 users[authenticatedUserIndex].balance += depositAmount; 
                 saveUserBalance(authenticatedUserIndex);

                 String timestamp = getTimestamp();
                 String logMessage = timestamp + " | " + users[authenticatedUserIndex].name + " deposited " + String(depositAmount, 2) + " AED. Total: " + String(users[authenticatedUserIndex].balance, 2) + " AED. Length Steps: " + String(billLengthSteps);
                 Serial.println(logMessage);
                 
                 Blynk.virtualWrite(V5, logMessage); 
                 Blynk.virtualWrite(V6, users[authenticatedUserIndex].balance); 

                 float totalSavings = 0;
                 for (int j = 0; j < userCount; j++) totalSavings += users[j].balance;
                 Blynk.virtualWrite(V10, totalSavings);
                 updateContributionStatus(); 

                 // Send to Pi for ML
                 HTTPClient http;
                 http.begin(rpiServer);
                 http.addHeader("Content-Type", "application/x-www-form-urlencoded");
                 String csvPayload = "uid=" + users[authenticatedUserIndex].uid
                                   + "&name=" + users[authenticatedUserIndex].name
                                   + "&amount=" + String(depositAmount, 0)
                                   + "&balance=" + String(users[authenticatedUserIndex].balance, 0)
                                   + "&goal=" + String(familyGoal);
                 int httpCode = http.POST(csvPayload);
                 if (httpCode > 0) {
                     Serial.println("Data Sent!");
                 } else {
                     Serial.println("Data Not Sent!");
                 }
                 http.end();
             }

             // Auto-logout
             performLogout();
        }
    }

    // Execute Step
    if (isMotorRunning) {
        myStepper.step(10); 
        currentDepositSteps += 10;
        
        // Count steps toward the bill length if the IR sensor is actively blocked
        if (digitalRead(IR_SENSOR_PIN) == LOW) {
            billLengthSteps += 10; 
        }
        
        // Update Blynk V4 every 100 steps 
        static long lastReportedSteps = -1;
        if (currentDepositSteps - lastReportedSteps >= 100) {
            Blynk.virtualWrite(V4, String(currentDepositSteps) + " Steps");
            lastReportedSteps = currentDepositSteps;
        }
    } 
  }
}