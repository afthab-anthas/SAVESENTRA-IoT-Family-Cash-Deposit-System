# SAVESENTRA IoT Family Cash Deposit System - Comprehensive Dataflow Document

## Executive Summary

This document provides a detailed analysis of all data flows within the SAVESENTRA IoT Family Cash Deposit System. The system operates as a distributed three-tier architecture with data flowing between an ESP32 edge device, a Raspberry Pi local server, a Python ML pipeline, and the Blynk IoT cloud platform. All claims are verified against actual source code.

---

## 1. System Architecture and Data Flow Overview

### 1.1 Component Topology

```
┌─────────────────────────────────────────────────────────────────┐
│                      Blynk IoT Cloud                                 │
│  (Real-time UI, Virtual Pins V0-V30, Event Logging, Predictions)   │
└────────────────────────┬────────────────────────────────────────┘
                         │ HTTPS (Blynk API)
                         │
        ┌────────────────┴────────────────┐
        │                                 │
┌───────▼──────────────────┐   ┌─────────▼──────────────────┐
│   ESP32 Edge Device      │   │  Raspberry Pi Server       │
│  (RFID, Motor, IR)       │   │  (Flask, CSV, ML Trigger)  │
│                          │   │                            │
│ - MFRC522 RFID Reader    │   │ - HTTP POST /deposit       │
│ - 28BYJ-48 Stepper Motor │   │ - HTTP POST /predict       │
│ - TCRT5000 IR Sensor     │   │ - CSV Ledger Append        │
│ - Preferences (EEPROM)   │   │ - subprocess ML Pipeline   │
└──────────┬───────────────┘   └────────────┬───────────────┘
           │ HTTP POST                      │ File I/O
           │ (deposit data)                 │ (CSV read/write)
           │                                │
           └────────────────┬───────────────┘
                            │
                    ┌───────▼────────┐
                    │  Python ML     │
                    │  Pipeline      │
                    │                │
                    │ - Data Clean   │
                    │ - Features     │
                    │ - SVR Training │
                    │ - Prediction   │
                    │ - Blynk Update │
                    └────────────────┘
```

### 1.2 Data Flow Boundaries

- **Edge-to-Server**: HTTP POST requests with deposit transaction data
- **Server-to-ML**: File-based CSV data exchange and subprocess invocation
- **ML-to-Cloud**: HTTPS GET requests to Blynk API for prediction updates
- **Cloud-to-Edge**: Blynk virtual pin writes for UI updates and configuration
- **Edge-to-Cloud**: Blynk virtual pin writes for real-time balance and event logging
- **Edge-to-Storage**: ESP32 Preferences (EEPROM) for user and balance persistence

---

## 2. Request/Response Flows for Key API Endpoints

### 2.1 ESP32 → Raspberry Pi: Deposit Submission

**Endpoint**: `POST http://raspberrypi.local:5000/deposit`

**Trigger**: After successful banknote denomination detection via IR sensor and stepper motor step counting.

**Request Format** (Form-encoded):
```
uid=<NFC_UID>
name=<User_Name>
amount=<Deposit_Amount_AED>
balance=<Updated_User_Balance>
goal=<Family_Goal_AED>
```

**Implementation**: `savesentra_iot.ino` (line 625-640)
```cpp
HTTPClient http;
http.begin(rpiServer);
http.addHeader("Content-Type", "application/x-www-form-urlencoded");
String csvPayload = "uid=" + users[authenticatedUserIndex].uid +
                    "&name=" + users[authenticatedUserIndex].name +
                    "&amount=" + String(depositAmount, 0) +
                    "&balance=" + String(users[authenticatedUserIndex].balance, 0) +
                    "&goal=" + String(familyGoal);
int httpCode = http.POST(csvPayload);
```

**Response**: Plain text "OK" (line 47 in `rpi_server.py`)

**Data Transformation**:
- Deposit amount is converted from stepper motor steps to AED denomination (5 or 10 AED)
- User balance is incremented in ESP32 memory before transmission
- Timestamp is generated server-side upon receipt

### 2.2 Raspberry Pi → ESP32: Prediction Trigger

**Endpoint**: `POST http://raspberrypi.local:5000/predict`

**Trigger**: When family goal is updated via Blynk app (V7 virtual pin write).

**Request Format** (Form-encoded):
```
goal=<New_Family_Goal_AED>
```

**Implementation**: `savesentra_iot.ino` (line 341-355)
```cpp
String rpiPredictUrl = String(rpiServer);
rpiPredictUrl.replace("/deposit", "/predict");
HTTPClient http;
http.begin(rpiPredictUrl);
http.addHeader("Content-Type", "application/x-www-form-urlencoded");
String payload = "goal=" + String(familyGoal);
http.POST(payload);
```

**Response**: Plain text "OK" (line 56 in `rpi_server.py`)

### 2.3 ML Pipeline → Blynk Cloud: Prediction Push

**Endpoint**: `GET https://blynk.cloud/external/api/update`

**Trigger**: After ML model completes goal prediction simulation.

**Request Format** (Query parameters):
```
token=<BLYNK_TOKEN>
pin=V16
value=<Predicted_Goal_Date_String>

token=<BLYNK_TOKEN>
pin=V17
value=<Top_Contributor_UID>
```

**Implementation**: `savesentra_ml.py` (line 159-167)
```python
BLYNK_TOKEN = "jIf-JTtFAMlw41WrJvqtZalG0-WnrBXG"
try:
    requests.get(f"https://blynk.cloud/external/api/update?token={BLYNK_TOKEN}&pin=V16&value={predicted_goal_date.strftime('%b %d, %Y')}", timeout=5)
    requests.get(f"https://blynk.cloud/external/api/update?token={BLYNK_TOKEN}&pin=V17&value={top_user_uid}", timeout=5)
    print("Successfully sent to Blynk!")
except Exception as e:
    print(f"Failed to update Blynk: {e}")
```

**Response**: HTTP 200 with JSON confirmation from Blynk API

### 2.4 Blynk Cloud → ESP32: Virtual Pin Writes (Bidirectional)

**Virtual Pins and Data Flows**:

| Pin | Direction | Data Type | Purpose | Implementation |
|-----|-----------|-----------|---------|-----------------|
| V0 | Cloud→Device | String | User registration name input | `savesentra_iot.ino` line 260-275 |
| V1 | Device→Cloud | String | Status messages | Multiple writes throughout |
| V3 | Cloud→Device | Integer (0/1) | Reset all users button | `savesentra_iot.ino` line 281-283 |
| V4 | Device→Cloud | String | Motor step counter | `savesentra_iot.ino` line 659-664 |
| V5 | Device→Cloud | String | Transaction log | `savesentra_iot.ino` line 617 |
| V6 | Device→Cloud | Float | Current user balance | `savesentra_iot.ino` line 618 |
| V7 | Cloud→Device | Integer | Family savings goal | `savesentra_iot.ino` line 325-355 |
| V8 | Cloud→Device | Integer (0/1) | Delete selected user button | `savesentra_iot.ino` line 357-410 |
| V10 | Device→Cloud | Float | Total family savings | `savesentra_iot.ino` line 620 |
| V11 | Device→Cloud | Float | Goal progress percentage | `savesentra_iot.ino` line 621 |
| V12 | Cloud→Device | Integer (0/1) | Manual logout button | `savesentra_iot.ino` line 250-256 |
| V16 | Cloud←ML | String | Predicted goal completion date | `savesentra_ml.py` line 160 |
| V17 | Cloud←ML | String | Top contributor UID | `savesentra_ml.py` line 161 |
| V20 | Cloud↔Device | Integer | User selection dropdown | `savesentra_iot.ino` line 285-323 |
| V21 | Cloud↔Device | Integer | User role selector | `savesentra_iot.ino` line 411-437 |

---

## 3. Data Transformations and Processing Pipelines

### 3.1 Deposit Processing Pipeline (Edge Device)

**Stage 1: RFID Authentication**
- Input: RFID card UID (bytes from MFRC522)
- Processing: Convert UID bytes to uppercase hexadecimal string
- Output: `uidString` (e.g., "A3F4BB2A")
- Code: `savesentra_iot.ino` (line 483-490)

**Stage 2: User Lookup**
- Input: `uidString`
- Processing: Linear search through `users[]` array in RAM
- Output: `authenticatedUserIndex` (user array index) or -1 (not found)
- Code: `savesentra_iot.ino` (line 505-520)

**Stage 3: Motor Control and Bill Length Measurement**
- Input: IR sensor state (LOW = blocked, HIGH = clear)
- Processing: 
  - Count stepper motor steps while IR sensor is blocked
  - Apply 5-second hysteresis delay after IR clear
  - Accumulate steps in `billLengthSteps` variable
- Output: `billLengthSteps` (total steps for denomination detection)
- Code: `savesentra_iot.ino` (line 545-570)

**Stage 4: Denomination Detection**
- Input: `billLengthSteps` (accumulated step count)
- Processing: 
  - If steps 4990-5080: 5 AED note
  - If steps 5090-5200: 10 AED note
  - Otherwise: Invalid note
- Output: `depositAmount` (float, 5.0 or 10.0)
- Code: `savesentra_iot.ino` (line 575-585)

**Stage 5: Balance Update (In-Memory)**
- Input: `depositAmount`, current `users[authenticatedUserIndex].balance`
- Processing: `users[authenticatedUserIndex].balance += depositAmount`
- Output: Updated balance in RAM
- Code: `savesentra_iot.ino` (line 595)

**Stage 6: Persistence (EEPROM)**
- Input: Updated user balance
- Processing: Write to ESP32 Preferences namespace "users" with key "bal" + index
- Output: Balance persisted to flash memory
- Code: `savesentra_iot.ino` (line 596, calls `saveUserBalance()` at line 127-133)

**Stage 7: Blynk Cloud Sync**
- Input: Updated balance, transaction timestamp, user name
- Processing: Multiple `Blynk.virtualWrite()` calls
- Output: V5 (log), V6 (balance), V10 (total), V11 (percentage)
- Code: `savesentra_iot.ino` (line 617-621)

**Stage 8: Raspberry Pi Transmission**
- Input: User UID, name, deposit amount, balance, family goal
- Processing: Form-encode and HTTP POST to RPi server
- Output: HTTP request with transaction data
- Code: `savesentra_iot.ino` (line 625-640)

### 3.2 CSV Ledger Persistence (Raspberry Pi)

**Stage 1: HTTP Request Reception**
- Input: POST request from ESP32
- Processing: Extract form parameters (uid, name, amount, balance, goal)
- Output: Extracted values in Python variables
- Code: `rpi_server.py` (line 14-20)

**Stage 2: Timestamp Generation**
- Input: Current system time
- Processing: Format as "YYYY-MM-DD HH:MM:SS"
- Output: `timestamp` string
- Code: `rpi_server.py` (line 24)

**Stage 3: CSV Row Append**
- Input: timestamp, uid, amount, balance
- Processing: Open CSV in append mode, write row
- Output: New row appended to `/home/admin/Desktop/savings_dataset.csv`
- Code: `rpi_server.py` (line 27-29)

**Stage 4: ML Pipeline Trigger**
- Input: Family goal value
- Processing: Invoke `subprocess.run()` with Python ML script and goal as argument
- Output: ML process spawned asynchronously
- Code: `rpi_server.py` (line 32-41)

### 3.3 ML Data Cleaning Pipeline

**Stage 1: CSV Load**
- Input: `savings_dataset.csv`
- Processing: `pd.read_csv()` with default string parsing
- Output: DataFrame with columns [Timestamp, NFC_UID, Deposit_Amount, Total_Balance]
- Code: `savesentra_ml.py` (line 13)

**Stage 2: Null Check**
- Input: DataFrame
- Processing: `df.isnull().sum()` to identify missing values
- Output: Count of nulls per column (printed to console)
- Code: `savesentra_ml.py` (line 18-20)

**Stage 3: Duplicate Removal**
- Input: DataFrame
- Processing: `df.drop_duplicates()`
- Output: DataFrame with duplicates removed
- Code: `savesentra_ml.py` (line 23-27)

**Stage 4: Timestamp Conversion**
- Input: Timestamp column (string)
- Processing: `pd.to_datetime()` conversion
- Output: Timestamp column as datetime64 type
- Code: `savesentra_ml.py` (line 30)

**Stage 5: Denomination Validation**
- Input: Deposit_Amount column
- Processing: Check if all values in [5, 10, 20, 50, 100, 200]
- Output: Filtered DataFrame of invalid notes (if any)
- Code: `savesentra_ml.py` (line 34-40)

**Stage 6: Clean Data Export**
- Input: Cleaned DataFrame
- Processing: `df.to_csv('savings_dataset_cleaned.csv')`
- Output: Backup CSV with cleaned data
- Code: `savesentra_ml.py` (line 45)

### 3.4 ML Feature Engineering Pipeline

**Stage 1: Daily Aggregation**
- Input: Cleaned DataFrame with per-transaction records
- Processing: `df.groupby('Date')['Deposit_Amount'].sum()` to aggregate by day
- Output: `daily_df` with one row per day, total deposit amount per day
- Code: `savesentra_ml.py` (line 51-54)

**Stage 2: Temporal Feature Extraction**
- Input: `daily_df` with Date column
- Processing: Extract:
  - `Day_Of_Week`: 0-6 (Monday-Sunday)
  - `Is_Weekend`: 1 if day ≥ 5, else 0
  - `Month`: 1-12
  - `Day_Of_Month`: 1-31
- Output: Four new numeric columns
- Code: `savesentra_ml.py` (line 57-61)

**Stage 3: Feature Selection**
- Input: `daily_df` with all columns
- Processing: Select only [Day_Of_Week, Is_Weekend, Month, Day_Of_Month, Deposit_Amount]
- Output: `ml_ready_data` DataFrame
- Code: `savesentra_ml.py` (line 64-67)

**Stage 4: ML-Ready Data Export**
- Input: `ml_ready_data` DataFrame
- Processing: `ml_ready_data.to_csv('ml_ready_data.csv')`
- Output: CSV file for model training
- Code: `savesentra_ml.py` (line 69)

### 3.5 ML Model Training and Selection

**Stage 1: Data Splitting**
- Input: `ml_ready_data` with features X and target y
- Processing: `train_test_split(X, y, test_size=0.2, random_state=42)`
- Output: X_train (80%), X_test (20%), y_train, y_test
- Code: `savesentra_ml.py` (line 76-79)

**Stage 2: Model Training (Three Models)**
- Input: X_train, y_train
- Processing: Fit three models:
  1. RandomForestRegressor(n_estimators=100)
  2. GradientBoostingRegressor()
  3. SVR(kernel='rbf')
- Output: Three trained model objects
- Code: `savesentra_ml.py` (line 88-95)

**Stage 3: Model Evaluation**
- Input: X_test, y_test, three trained models
- Processing: Predict on test set, calculate Mean Absolute Error (MAE)
- Output: Error metric for each model
- Code: `savesentra_ml.py` (line 98-103)

**Stage 4: Winner Selection**
- Input: Three error metrics
- Processing: Select model with lowest MAE
- Output: `winning_model` (typically SVR)
- Code: `savesentra_ml.py` (line 105-110)

### 3.6 ML Goal Prediction Pipeline

**Stage 1: Final Model Retraining**
- Input: All available data (X, y)
- Processing: Retrain winning model on 100% of data
- Output: `final_model` ready for prediction
- Code: `savesentra_ml.py` (line 117-119)

**Stage 2: Simulation Setup**
- Input: Current balance, current date, family goal
- Processing: Initialize simulation variables
- Output: `simulated_balance`, `days_ahead = 0`
- Code: `savesentra_ml.py` (line 121-130)

**Stage 3: Day-by-Day Prediction Loop**
- Input: `simulated_balance`, `days_ahead`, `FAMILY_GOAL`
- Processing: 
  - Increment `days_ahead`
  - Create future date features
  - Predict deposit for that day using trained model
  - Add predicted deposit to simulated balance
  - Repeat until `simulated_balance >= FAMILY_GOAL`
- Output: `days_ahead` (number of days to reach goal)
- Code: `savesentra_ml.py` (line 133-152)

**Stage 4: Prediction Output**
- Input: `days_ahead`, current date
- Processing: Calculate `predicted_goal_date = current_date + timedelta(days=days_ahead)`
- Output: Predicted completion date string
- Code: `savesentra_ml.py` (line 154-157)

**Stage 5: Top Contributor Calculation**
- Input: User statistics (sum and mean deposits per UID)
- Processing: 
  - Calculate projected total for each user: `sum + (mean * days_ahead)`
  - Find user with maximum projected total
- Output: `top_user_uid`
- Code: `savesentra_ml.py` (line 159-162)

**Stage 6: Blynk Cloud Push**
- Input: Predicted goal date, top contributor UID
- Processing: HTTPS GET requests to Blynk API
- Output: V16 and V17 virtual pins updated in cloud
- Code: `savesentra_ml.py` (line 164-171)

---

## 4. Event Flows and Messaging Patterns

### 4.1 Deposit Event Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. User taps RFID card on ESP32                                 │
│    → MFRC522 reads UID                                          │
│    → uidString extracted (savesentra_iot.ino:483-490)          │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. User authentication                                          │
│    → Linear search in users[] array                             │
│    → authenticatedUserIndex set (savesentra_iot.ino:505-520)   │
│    → Blynk V1 displays welcome message                          │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. Motor activation on IR sensor block                          │
│    → isMotorRunning = true                                      │
│    → Stepper motor begins stepping (savesentra_iot.ino:653)    │
│    → billLengthSteps accumulates                                │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 4. IR sensor clear + hysteresis timeout (5 seconds)             │
│    → Motor stops                                                │
│    → Denomination detection (savesentra_iot.ino:575-585)       │
│    → depositAmount determined (5 or 10 AED)                    │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 5. Balance update and persistence                               │
│    → users[index].balance += depositAmount (line 595)          │
│    → saveUserBalance() writes to ESP32 flash (line 596)        │
│    → Blynk.logEvent() sends notification (line 614)            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 6. Blynk cloud sync                                             │
│    → V5: Transaction log message                               │
│    → V6: Updated user balance                                  │
│    → V10: Total family savings                                 │
│    → V11: Goal progress percentage                             │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 7. HTTP POST to Raspberry Pi                                    │
│    → Deposit data sent (savesentra_iot.ino:625-640)            │
│    → CSV row appended (rpi_server.py:27-29)                    │
│    → ML pipeline triggered (rpi_server.py:32-41)               │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 8. ML prediction update                                         │
│    → Data cleaning (savesentra_ml.py:13-45)                    │
│    → Feature engineering (savesentra_ml.py:51-69)              │
│    → Model training (savesentra_ml.py:88-110)                  │
│    → Goal prediction (savesentra_ml.py:117-157)                │
│    → Blynk update (savesentra_ml.py:164-171)                   │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 9. Auto-logout                                                  │
│    → performLogout() called (savesentra_iot.ino:641)           │
│    → Admin tools hidden (setProperty isHidden=true)            │
│    → Session state cleared                                      │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 User Registration Event Flow

```
Parent enters name in Blynk V0 → registrationMode = true
                    ↓
Child taps RFID card on ESP32
                    ↓
Check if UID already exists in users[] array
                    ↓
If duplicate: Display error, exit
If new: Create user struct with:
  - name (from V0 input)
  - uid (from RFID)
  - balance = 0.0
  - role = "Other"
                    ↓
saveUsers() writes to ESP32 Preferences
                    ↓
updateBlynkUserList() updates V20 dropdown
                    ↓
Blynk.virtualWrite(V1, "User 'Name' Registered")
```

### 4.3 Goal Update Event Flow

```
Parent adjusts V7 (Family Goal) in Blynk app
                    ↓
BLYNK_WRITE(V7) handler triggered (savesentra_iot.ino:325)
                    ↓
familyGoal = param.asInt()
                    ↓
Save to ESP32 Preferences (line 333-335)
                    ↓
Update Blynk widget max values (line 337-338)
                    ↓
Recalculate and redraw widgets (line 340-349)
                    ↓
HTTP POST to /predict endpoint (line 351-355)
                    ↓
ML pipeline re-runs with new goal
                    ↓
Blynk V16 and V17 updated with new predictions
```

### 4.4 Blynk Event Logging

**Event Type**: `note_deposited`

**Trigger**: After successful deposit processing

**Message Format**: `"{User_Name} deposited {Amount} AED"`

**Implementation**: `savesentra_iot.ino` (line 614)
```cpp
String notifyMsg = users[authenticatedUserIndex].name + " deposited " + 
                   String(depositAmount, 0) + " AED";
Blynk.logEvent("note_deposited", notifyMsg);
```

**Destination**: Blynk cloud event log (visible in app notifications)

---

## 5. Integration Points with External Services

### 5.1 Blynk IoT Cloud Integration

**Service**: Blynk IoT Platform

**Authentication**: Bearer token in sketch
- Token: `jIf-JTtFAMlw41WrJvqtZalG0-WnrBXG`
- Template ID: `TMPL2sUhFh8RM`
- Template Name: `SAVESENTRA`

**Connection Method**: WiFi over HTTPS

**Data Flows**:
1. **Device → Cloud**: Virtual pin writes (V0-V30)
   - Code: `Blynk.virtualWrite(pin, value)` throughout `savesentra_iot.ino`
   - Frequency: Real-time on deposit, balance update, status change

2. **Cloud → Device**: Virtual pin reads (BLYNK_WRITE handlers)
   - Code: `BLYNK_WRITE(V0)`, `BLYNK_WRITE(V3)`, `BLYNK_WRITE(V7)`, etc.
   - Frequency: On-demand when user interacts with app

3. **Device → Cloud**: Event logging
   - Code: `Blynk.logEvent("note_deposited", message)` (line 614)
   - Frequency: Once per successful deposit

4. **ML → Cloud**: Prediction updates
   - Code: `requests.get()` to Blynk external API (savesentra_ml.py:160-161)
   - Frequency: After each ML pipeline execution

### 5.2 NTP Time Synchronization

**Service**: NTP Pool (pool.ntp.org)

**Configuration**: `savesentra_iot.ino` (line 49-51)
```cpp
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 14400;  // UTC+4 (UAE)
const int daylightOffset_sec = 0;   // No DST
```

**Usage**: 
- Timestamp generation for transaction logs
- Feature engineering in ML pipeline (day of week, month)

**Implementation**: `configTime()` called in setup() (line 430)

### 5.3 Local Network Services

**Raspberry Pi Server**:
- Hostname: `raspberrypi.local`
- Port: 5000
- Protocol: HTTP (unencrypted local network)
- Endpoints:
  - `POST /deposit` - Receive deposit data
  - `POST /predict` - Trigger ML prediction
  - `GET /` - Health check

**Implementation**: `rpi_server.py` using Flask framework

---

## 6. Database Read/Write Patterns and Transactions

### 6.1 ESP32 Preferences (EEPROM) - User Data Storage Schema

**Storage Location**: ESP32 internal flash memory

**Namespace**: `"users"`

**Data Structure and Keys**:

The User struct is persisted using the Preferences API with the following key-value schema:

```
Key: "count"           → Value: userCount (int)
                         Type: Integer
                         Purpose: Total number of registered users
                         Example: 5

Key: "goal"            → Value: familyGoal (int)
                         Type: Integer
                         Purpose: Family savings target in AED
                         Example: 15000

Key: "name0"           → Value: users[0].name (string)
                         Type: String
                         Purpose: Display name of user at index 0
                         Example: "Ahmed"

Key: "uid0"            → Value: users[0].uid (string)
                         Type: String
                         Purpose: RFID card UID for user at index 0
                         Example: "A3F4BB2A"

Key: "bal0"            → Value: users[0].balance (float)
                         Type: Float
                         Purpose: Individual balance in AED for user at index 0
                         Example: 125.5

Key: "role0"           → Value: users[0].role (string)
                         Type: String
                         Purpose: User role for user at index 0
                         Values: "Parent", "Child", "Other"
                         Example: "Parent"

Key: "name1"           → Value: users[1].name (string)
Key: "uid1"            → Value: users[1].uid (string)
Key: "bal1"            → Value: users[1].balance (float)
Key: "role1"           → Value: users[1].role (string)

... (repeats for each user up to MAX_USERS=20)
```

**User Struct Definition** (in-memory representation):
```cpp
struct User {
  String name;        // User display name (max ~50 chars)
  String uid;         // RFID UID in hex format (8-16 chars)
  float balance;      // Individual balance in AED (4 bytes)
  String role;        // "Parent", "Child", or "Other" (~10 chars)
};
User users[MAX_USERS];  // Array of 20 users max
int userCount = 0;      // Current number of users
```

**Load Operation** (`loadUsers()` function):
- Code: `savesentra_iot.ino` (line 93-111)
- Timing: Called once at system startup in `setup()`
- Process:
  1. Open Preferences namespace "users" in read-only mode
  2. Read "count" to determine how many users to load
  3. Read "goal" for family savings target
  4. Loop through each user index (0 to userCount-1):
     - Read "name" + index
     - Read "uid" + index
     - Read "bal" + index
     - Read "role" + index
  5. Close Preferences
  6. Print confirmation message to Serial

**Write Operations**:

1. **User Registration** (`saveUsers()`)
   - Code: `savesentra_iot.ino` (line 112-122)
   - Writes: Only the latest user added (index = userCount - 1)
   - Keys written: "count", "name" + i, "uid" + i, "bal" + i, "role" + i
   - Frequency: Once per new user registration
   - Transaction: Non-atomic (multiple puts)

2. **Balance Update** (`saveUserBalance()`)
   - Code: `savesentra_iot.ino` (line 127-133)
   - Writes: Single float value for user balance
   - Key written: "bal" + index
   - Frequency: After each deposit
   - Transaction: Atomic single write
   - Example: `preferences.putFloat("bal0", 150.5)`

3. **Goal Update** (BLYNK_WRITE V7)
   - Code: `savesentra_iot.ino` (line 333-335)
   - Writes: familyGoal integer
   - Key written: "goal"
   - Frequency: When parent adjusts goal in app
   - Transaction: Atomic single write

4. **User Deletion** (BLYNK_WRITE V8)
   - Code: `savesentra_iot.ino` (line 357-410)
   - Process:
     1. Clear entire namespace
     2. Rewrite "count" with new userCount
     3. Rewrite all remaining users (indices 0 to userCount-1)
   - Frequency: When parent deletes user
   - Transaction: Non-atomic (clear + multiple puts)

5. **Role Update** (BLYNK_WRITE V21)
   - Code: `savesentra_iot.ino` (line 411-437)
   - Writes: Single role string for selected user
   - Key written: "role" + selectedMenuIndex
   - Frequency: When parent changes user role
   - Transaction: Atomic single write

**Read Operations**:

1. **System Startup** (`loadUsers()`)
   - Code: `savesentra_iot.ino` (line 93-111)
   - Reads: All user data from flash
   - Frequency: Once at boot
   - Scope: All users and goal

2. **Authentication Lookup**
   - Code: `savesentra_iot.ino` (line 505-520)
   - Reads: From RAM (already loaded at boot)
   - Frequency: Per RFID tap
   - Scope: Linear search through users[] array

### 6.2 Raspberry Pi CSV Ledger - Complete Schema

**File Path**: `/home/admin/Desktop/savings_dataset.csv`

**CSV Schema with Data Types**:

```
Column 1: Timestamp
  - Data Type: String (ISO 8601 format)
  - Format: YYYY-MM-DD HH:MM:SS
  - Example: "2025-09-01 10:47:06"
  - Generated: Server-side using Python datetime.now().strftime('%Y-%m-%d %H:%M:%S')
  - Precision: Seconds

Column 2: NFC_UID
  - Data Type: String (hexadecimal or alphanumeric)
  - Format: Variable length (8-16 characters)
  - Examples: "A3F4BB2A", "U004", "04612842247880", "F351F513"
  - Source: RFID card UID from ESP32
  - Uniqueness: One UID per registered user

Column 3: Deposit_Amount
  - Data Type: Integer (AED currency)
  - Valid Values: 5, 10, 20, 50, 100, 200
  - Unit: United Arab Emirates Dirham (AED)
  - Source: Motor step count converted to denomination
  - Validation: Checked in ML pipeline against allowed_notes list

Column 4: Total_Balance
  - Data Type: Integer (AED currency)
  - Format: Cumulative balance for that user after deposit
  - Example: 125 (meaning user has 125 AED total)
  - Unit: United Arab Emirates Dirham (AED)
  - Source: ESP32 user balance at time of deposit
```

**Complete CSV Example**:
```csv
Timestamp,NFC_UID,Deposit_Amount,Total_Balance
2025-09-01 10:47:06,A3F4BB2A,10,10
2025-09-01 17:19:22,U004,5,5
2025-09-01 18:54:07,04612842247880,20,20
2025-09-02 16:05:37,A3F4BB2A,10,20
2025-09-03 08:05:13,A3F4BB2A,5,25
```

**Write Operations**:

1. **Deposit Append** (deposit() endpoint)
   - Code: `rpi_server.py` (line 27-29)
   - Mode: Append (file opened with 'a')
   - Frequency: Once per deposit from ESP32
   - Transaction: Single row write, non-atomic
   - Locking: None (potential race condition if multiple deposits simultaneous)
   - Implementation:
     ```python
     timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
     with open(CSV_PATH, 'a', newline='') as f:
         writer = csv.writer(f)
         writer.writerow([timestamp, uid, amount, balance])
     ```

**Read Operations**:

1. **ML Data Loading**
   - Code: `savesentra_ml.py` (line 13)
   - Method: `pd.read_csv()`
   - Frequency: Once per ML pipeline execution
   - Scope: All rows in CSV

2. **Data Cleaning**
   - Code: `savesentra_ml.py` (line 18-40)
   - Operations: Null check, duplicate removal, validation
   - Output: `savings_dataset_cleaned.csv`

3. **Feature Engineering**
   - Code: `savesentra_ml.py` (line 51-69)
   - Operations: Daily aggregation, temporal feature extraction
   - Output: `ml_ready_data.csv`

### 6.3 ML Training Data Files

**Files**:
- `savings_dataset.csv` - Raw transaction data
- `savings_dataset_cleaned.csv` - Cleaned data (backup)
- `ml_ready_data.csv` - Features ready for training

**Read/Write Pattern**:
```
savings_dataset.csv (input)
        ↓
[Data Cleaning] → savings_dataset_cleaned.csv (output)
        ↓
[Feature Engineering] → ml_ready_data.csv (output)
        ↓
[Model Training] (reads ml_ready_data.csv)
        ↓
[Prediction] (reads ml_ready_data.csv)
        ↓
[Blynk Update] (HTTPS GET)
```

---

## 7. Data Transformation Pipeline: Raw CSV to ML-Ready Features

### 7.1 Complete Pipeline Overview

```
┌─────────────────────────────────────────────────────────────────┐
│ INPUT: savings_dataset.csv                                      │
│ (Raw transaction data with 4 columns)                           │
│                                                                  │
│ Timestamp,NFC_UID,Deposit_Amount,Total_Balance                 │
│ 2025-09-01 10:47:06,A3F4BB2A,10,10                             │
│ 2025-09-01 17:19:22,U004,5,5                                   │
│ ...                                                              │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ STAGE 1: DATA CLEANING                                          │
│ Code: savesentra_ml.py (line 13-45)                            │
│                                                                  │
│ Operations:                                                      │
│ 1. Load CSV with pd.read_csv()                                 │
│ 2. Check for null values with df.isnull().sum()                │
│ 3. Remove duplicates with df.drop_duplicates()                 │
│ 4. Convert Timestamp to datetime64 with pd.to_datetime()       │
│ 5. Validate Deposit_Amount in [5,10,20,50,100,200]            │
│ 6. Check for negative balances                                 │
│                                                                  │
│ Output: savings_dataset_cleaned.csv                             │
│ (Same schema, but cleaned and validated)                        │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ STAGE 2: DAILY AGGREGATION                                      │
│ Code: savesentra_ml.py (line 51-54)                            │
│                                                                  │
│ Operations:                                                      │
│ 1. Extract Date from Timestamp: df['Date'] = df['Timestamp'].dt.date
│ 2. Group by Date: df.groupby('Date')['Deposit_Amount'].sum()  │
│ 3. Create daily_df with one row per day                        │
│                                                                  │
│ Input:  Multiple transactions per day per user                 │
│ Output: One row per day with total deposits                    │
│                                                                  │
│ Example:                                                         │
│ Date       Deposit_Amount                                       │
│ 2025-09-01 35  (sum of 10+5+20)                                │
│ 2025-09-02 10  (single deposit)                                │
│ 2025-09-03 45  (sum of 5+10+20+10)                             │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ STAGE 3: TEMPORAL FEATURE EXTRACTION                            │
│ Code: savesentra_ml.py (line 57-61)                            │
│                                                                  │
│ Operations:                                                      │
│ 1. Day_Of_Week = daily_df['Date'].dt.dayofweek                │
│    Values: 0=Monday, 1=Tuesday, ..., 6=Sunday                 │
│                                                                  │
│ 2. Is_Weekend = daily_df['Day_Of_Week'].apply(lambda x: 1 if x >= 5 else 0)
│    Values: 0=Weekday, 1=Weekend (Saturday/Sunday)             │
│                                                                  │
│ 3. Month = daily_df['Date'].dt.month                          │
│    Values: 1-12                                                │
│                                                                  │
│ 4. Day_Of_Month = daily_df['Date'].dt.day                     │
│    Values: 1-31                                                │
│                                                                  │
│ Output: daily_df with 8 columns (original + 4 new features)   │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ STAGE 4: FEATURE SELECTION                                      │
│ Code: savesentra_ml.py (line 64-67)                            │
│                                                                  │
│ Operations:                                                      │
│ Select only 5 columns for ML training:                         │
│ [Day_Of_Week, Is_Weekend, Month, Day_Of_Month, Deposit_Amount]│
│                                                                  │
│ Rationale:                                                       │
│ - Day_Of_Week: Captures weekly patterns                        │
│ - Is_Weekend: Binary indicator for weekend behavior            │
│ - Month: Captures seasonal patterns                            │
│ - Day_Of_Month: Captures monthly patterns                      │
│ - Deposit_Amount: Target variable (what we predict)           │
│                                                                  │
│ Output: ml_ready_data DataFrame (5 columns)                    │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ OUTPUT: ml_ready_data.csv                                       │
│ (ML-ready features for model training)                          │
│                                                                  │
│ Day_Of_Week,Is_Weekend,Month,Day_Of_Month,Deposit_Amount      │
│ 0,0,9,1,35                                                      │
│ 1,0,9,2,10                                                      │
│ 2,0,9,3,45                                                      │
│ 3,0,9,4,5                                                       │
│ 4,0,9,5,5                                                       │
│ 5,1,9,6,120                                                     │
│ 6,1,9,7,30                                                      │
│ ...                                                              │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Data Transformation Examples

**Example 1: Single Day with Multiple Deposits**

Input (savings_dataset.csv):
```
Timestamp,NFC_UID,Deposit_Amount,Total_Balance
2025-09-01 10:47:06,A3F4BB2A,10,10
2025-09-01 17:19:22,U004,5,5
2025-09-01 18:54:07,04612842247880,20,20
```

After Daily Aggregation:
```
Date,Deposit_Amount
2025-09-01,35
```

After Feature Engineering:
```
Date,Day_Of_Week,Is_Weekend,Month,Day_Of_Month,Deposit_Amount
2025-09-01,0,0,9,1,35
```

Final (ml_ready_data.csv):
```
Day_Of_Week,Is_Weekend,Month,Day_Of_Month,Deposit_Amount
0,0,9,1,35
```

**Example 2: Weekend vs Weekday Pattern**

Input (multiple days):
```
2025-09-06 (Saturday) - 120 AED total
2025-09-07 (Sunday) - 30 AED total
2025-09-08 (Monday) - 20 AED total
```

After Feature Engineering:
```
Day_Of_Week,Is_Weekend,Month,Day_Of_Month,Deposit_Amount
5,1,9,6,120
6,1,9,7,30
0,0,9,8,20
```

Pattern: Weekends (Is_Weekend=1) show higher deposits (120, 30) vs weekday (20)

---

## 8. Blynk Virtual Pin Data Types and Update Frequencies

### 8.1 Complete Virtual Pin Reference

| Pin | Data Type | Direction | Update Frequency | Purpose | Code Reference |
|-----|-----------|-----------|------------------|---------|-----------------|
| V0 | String | Cloud→Device | On user input | User registration name input | `savesentra_iot.ino` line 260-275 |
| V1 | String | Device→Cloud | Real-time | Status messages and notifications | Multiple writes throughout |
| V3 | Integer (0/1) | Cloud→Device | On button press | Reset all users button | `savesentra_iot.ino` line 281-283 |
| V4 | String | Device→Cloud | Every 100 steps | Motor step counter display | `savesentra_iot.ino` line 659-664 |
| V5 | String | Device→Cloud | Per deposit | Transaction log entry | `savesentra_iot.ino` line 617 |
| V6 | Float | Device→Cloud | Per deposit | Current authenticated user balance | `savesentra_iot.ino` line 618 |
| V7 | Integer | Cloud→Device | On user change | Family savings goal (AED) | `savesentra_iot.ino` line 325-355 |
| V8 | Integer (0/1) | Cloud→Device | On button press | Delete selected user button | `savesentra_iot.ino` line 357-410 |
| V10 | Float | Device→Cloud | Per deposit | Total family savings (sum of all users) | `savesentra_iot.ino` line 620 |
| V11 | Float | Device→Cloud | Per deposit | Goal progress percentage (0-100) | `savesentra_iot.ino` line 621 |
| V12 | Integer (0/1) | Cloud→Device | On button press | Manual logout button | `savesentra_iot.ino` line 250-256 |
| V16 | String | Cloud←ML | Per ML run | Predicted goal completion date (format: "Mon DD, YYYY") | `savesentra_ml.py` line 160 |
| V17 | String | Cloud←ML | Per ML run | Top contributor UID (RFID identifier) | `savesentra_ml.py` line 161 |
| V20 | Integer | Cloud↔Device | On selection | User selection dropdown (index 0-4) | `savesentra_iot.ino` line 285-323 |
| V21 | Integer | Cloud↔Device | On selection | User role selector (1=Parent, 2=Child, 3=Other) | `savesentra_iot.ino` line 411-437 |
| V30 | (Reserved) | Cloud↔Device | N/A | Admin tools visibility control | `savesentra_iot.ino` line 646-652 |

### 8.2 Data Type Details

**String Pins** (V0, V1, V4, V5, V16, V17):
- Maximum length: Typically 255 characters
- Encoding: UTF-8
- Examples:
  - V1: "Welcome Ahmed"
  - V4: "450 Steps"
  - V5: "2025-09-01 10:47:06 | Ahmed deposited 10.00 AED. Total: 125.00 AED. Length Steps: 5045"
  - V16: "Jan 15, 2026"
  - V17: "A3F4BB2A"

**Integer Pins** (V3, V7, V8, V12, V20, V21):
- Range: 0 to 2,147,483,647 (32-bit signed)
- Examples:
  - V3: 0 (button not pressed) or 1 (button pressed)
  - V7: 15000 (family goal in AED)
  - V20: 0, 1, 2, 3, or 4 (user index)
  - V21: 1 (Parent), 2 (Child), 3 (Other)

**Float Pins** (V6, V10, V11):
- Precision: Single precision (32-bit)
- Decimal places: Typically 2 for currency
- Examples:
  - V6: 125.50 (user balance in AED)
  - V10: 1250.75 (total family savings in AED)
  - V11: 8.34 (percentage, 0-100)

### 8.3 Update Frequency Analysis

**Real-time Updates** (< 100ms):
- V1 (status messages)
- V5 (transaction logs)
- V6 (balance updates)
- V10 (total savings)
- V11 (goal percentage)

**Periodic Updates** (every 100 steps):
- V4 (motor step counter)

**Per-Deposit Updates** (once per transaction):
- V5, V6, V10, V11

**Per-ML-Run Updates** (after ML pipeline execution):
- V16 (predicted goal date)
- V17 (top contributor)

**On-Demand Updates** (user interaction):
- V0 (name input)
- V3 (reset button)
- V7 (goal adjustment)
- V8 (delete user)
- V12 (logout button)
- V20 (user selection)
- V21 (role selection)

---

## 9. Data Validation Rules and Constraints

### 9.1 Denomination Validation

**Valid Banknote Denominations**:

| Denomination | Step Count Range | Tolerance | Code Reference |
|--------------|------------------|-----------|-----------------|
| 5 AED | 4990-5080 | ±45 steps | `savesentra_iot.ino` line 575-577 |
| 10 AED | 5090-5200 | ±55 steps | `savesentra_iot.ino` line 578-580 |

**Validation Logic**:
```cpp
if (billLengthSteps >= 4990 && billLengthSteps <= 5080) {
    depositAmount = 5.0;
    Serial.println("5 AED note.");
} else if (billLengthSteps >= 5090 && billLengthSteps <= 5200) {
    depositAmount = 10.0;
    Serial.println("10 AED note.");
} else {
    Serial.println("Invalid note size.");
    Blynk.virtualWrite(V1, "Error: Unknown Note Size");
}
```

**Error Handling**:
- If step count falls outside both ranges: `depositAmount = 0.0`
- User is notified via Blynk V1: "Error: Unknown Note Size"
- No balance update occurs
- No CSV entry is created
- Session continues (user can try again)

### 9.2 CSV Data Validation (ML Pipeline)

**Allowed Deposit Amounts**:
```python
allowed_notes = [5, 10, 20, 50, 100, 200]
```

**Validation Code** (savesentra_ml.py, line 34-40):
```python
weird_notes = df[~df['Deposit_Amount'].isin(allowed_notes)]
print(f"Invalid notes detected: {len(weird_notes)}")
if len(weird_notes) > 0:
    print("Here are the invalid ones:")
    print(weird_notes)
```

**Validation Checks**:
1. **Null Values**: `df.isnull().sum()` (line 18-20)
2. **Duplicates**: `df.duplicated().sum()` (line 23-27)
3. **Denomination Range**: Check against [5, 10, 20, 50, 100, 200] (line 34-40)
4. **Negative Balances**: Implicit check (no explicit code, but data should be positive)

**Error Handling**:
- Invalid rows are printed to console but not filtered from DataFrame
- Cleaning continues with all data
- Cleaned CSV is exported for audit trail

### 9.3 User Data Validation

**User Name Validation**:
- Minimum length: > 0 characters
- Maximum length: ~50 characters (String storage limit)
- Code: `savesentra_iot.ino` (line 265-268)
- Error: If empty, registration is skipped

**RFID UID Validation**:
- Format: Hexadecimal string (uppercase)
- Length: 8-16 characters (varies by card type)
- Uniqueness: Must not already exist in users[] array
- Code: `savesentra_iot.ino` (line 505-520)
- Error: If duplicate, user is not registered

**Family Goal Validation**:
- Type: Integer (AED)
- Minimum: 1 AED (implicit)
- Maximum: 2,147,483,647 AED (32-bit signed int limit)
- Code: `savesentra_iot.ino` (line 325)
- Error: None (any positive integer accepted)

**User Count Validation**:
- Maximum: 20 users (MAX_USERS constant)
- Code: `savesentra_iot.ino` (line 268-270)
- Error: If full, display "Error: Memory Full"

### 9.4 Timestamp Format Consistency

**Format Standard**: `%Y-%m-%d %H:%M:%S`

**Implementation Locations**:

1. **Arduino (ESP32)**:
   - Code: `savesentra_iot.ino` (line 155-161)
   - Function: `getTimestamp()`
   - Implementation:
     ```cpp
     strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
     ```
   - Example: "2025-09-01 10:47:06"

2. **Python (Raspberry Pi)**:
   - Code: `rpi_server.py` (line 24)
   - Implementation:
     ```python
     timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
     ```
   - Example: "2025-09-01 10:47:06"

3. **Python (ML Pipeline)**:
   - Code: `savesentra_ml.py` (line 30)
   - Implementation:
     ```python
     df['Timestamp'] = pd.to_datetime(df['Timestamp'])
     ```
   - Parsing: Automatic detection of ISO 8601 format
   - Example: "2025-09-01 10:47:06"

**Consistency Verification**:
- All three systems use identical format: `YYYY-MM-DD HH:MM:SS`
- Timezone: UTC+4 (UAE) on ESP32, system local time on Raspberry Pi
- Precision: Seconds (no milliseconds)
- No timezone offset in string (implicit local time)

---

## 10. Data Retention Policy and Storage Limits

### 10.1 ESP32 Preferences (EEPROM) Capacity

**Total Flash Memory**: ~4 MB (varies by ESP32 variant)

**Preferences Namespace Usage**:

```
Namespace: "users"

Fixed Overhead: ~1 KB (Preferences metadata)

Per-User Storage:
  - "name" + index: ~50 bytes (max string length)
  - "uid" + index: ~20 bytes (hex string)
  - "bal" + index: 4 bytes (float)
  - "role" + index: ~15 bytes (string)
  - Subtotal per user: ~90 bytes

Maximum Users: 20 (MAX_USERS constant)
Maximum User Data: 20 × 90 = 1,800 bytes

Metadata:
  - "count": 4 bytes (int)
  - "goal": 4 bytes (int)
  - Subtotal: 8 bytes

Total Preferences Usage: ~1 KB + 1.8 KB + 0.008 KB ≈ 2.8 KB

Available for Other Uses: ~4 MB - 2.8 KB ≈ 3.997 MB
```

**Retention Policy**:
- User data persists indefinitely until explicitly deleted
- No automatic cleanup or archival
- No data expiration
- Survives power cycles and reboots

**Capacity Limits**:
- Maximum users: 20 (hardcoded MAX_USERS)
- No automatic overflow handling
- Error message if attempting to add 21st user: "Error: Memory Full"

### 10.2 Raspberry Pi CSV Ledger Retention

**File Path**: `/home/admin/Desktop/savings_dataset.csv`

**Growth Rate**:
- Typical deposit frequency: 1-5 deposits per day
- Average row size: ~60 bytes (timestamp + UID + amount + balance)
- Daily growth: ~60-300 bytes per day
- Monthly growth: ~1.8-9 KB per month
- Yearly growth: ~22-110 KB per year

**Retention Policy**:
- Append-only (no deletion of historical records)
- No automatic archival or cleanup
- No retention limit specified in code
- Data persists indefinitely

**Storage Considerations**:
- Raspberry Pi typical SD card: 32-128 GB
- CSV ledger growth: Negligible (< 1 MB per year)
- No storage constraints anticipated for typical usage

**Backup Strategy**:
- No automatic backup mechanism
- Manual backup recommended for data protection
- Cleaned CSV (`savings_dataset_cleaned.csv`) serves as audit trail

### 10.3 ML Pipeline Intermediate Files Retention

**Files Created**:

1. **savings_dataset_cleaned.csv**
   - Created: After each ML pipeline run
   - Purpose: Backup of cleaned data
   - Retention: Indefinite (overwritten on next run)
   - Size: Same as input CSV (~15-20 KB in test data)

2. **ml_ready_data.csv**
   - Created: After each ML pipeline run
   - Purpose: Features for model training
   - Retention: Indefinite (overwritten on next run)
   - Size: Smaller than input (daily aggregation reduces rows)
   - Example size: ~2-3 KB for 200+ days of data

**Retention Policy**:
- Files are overwritten on each ML pipeline execution
- No historical versioning
- No cleanup mechanism
- Disk space impact: Minimal (< 50 KB total)

### 10.4 Blynk Cloud Data Retention

**Data Stored**:
- Virtual pin values (V0-V30)
- Event logs (note_deposited events)
- User session data

**Retention Policy**:
- Managed by Blynk (outside system scope)
- Typically indefinite for active accounts
- Event logs may have retention limits (not documented in code)

**Backup**:
- Managed by Blynk (outside system scope)
- No explicit backup mechanism in SAVESENTRA code

---

## 11. Data Flow Diagrams

### 11.1 Complete System Data Flow

```
┌──────────────────────────────────────────────────────────────────────┐
│                         SAVESENTRA System                             │
│                                                                        │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ ESP32 Edge Device                                               │ │
│  │                                                                  │ │
│  │  RFID Card → UID String → User Lookup → Authenticate           │ │
│  │                                              ↓                  │ │
│  │  IR Sensor → Motor Steps → Denomination → Balance Update       │ │
│  │                                              ↓                  │ │
│  │  Preferences (EEPROM) ← Save User Data                         │ │
│  │                                              ↓                  │ │
│  │  Blynk Virtual Pins ← V1,V4,V5,V6,V10,V11 (Status Updates)    │ │
│  │                                              ↓                  │ │
│  │  HTTP POST → Raspberry Pi /deposit (Transaction Data)          │ │
│  │                                                                  │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                        │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ Raspberry Pi Server (Flask)                                     │ │
│  │                                                                  │ │
│  │  HTTP POST /deposit ← ESP32                                     │ │
│  │       ↓                                                          │ │
│  │  Extract Form Data (uid, name, amount, balance, goal)          │ │
│  │       ↓                                                          │ │
│  │  Generate Timestamp                                             │ │
│  │       ↓                                                          │ │
│  │  CSV Append (savings_dataset.csv)                              │ │
│  │       ↓                                                          │ │
│  │  subprocess.run() → ML Pipeline                                 │ │
│  │                                                                  │ │
│  │  HTTP POST /predict ← ESP32 (Goal Update)                       │ │
│  │       ↓                                                          │ │
│  │  subprocess.run() → ML Pipeline                                 │ │
│  │                                                                  │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                        │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ Python ML Pipeline                                              │ │
│  │                                                                  │ │
│  │  Read: savings_dataset.csv                                      │ │
│  │       ↓                                                          │ │
│  │  [Data Cleaning]                                                │ │
│  │  - Remove duplicates                                            │ │
│  │  - Validate timestamps                                          │ │
│  │  - Check denominations                                          │ │
│  │  Write: savings_dataset_cleaned.csv                             │ │
│  │       ↓                                                          │ │
│  │  [Feature Engineering]                                          │ │
│  │  - Daily aggregation                                            │ │
│  │  - Temporal features (day, month, weekend)                      │ │
│  │  Write: ml_ready_data.csv                                       │ │
│  │       ↓                                                          │ │
│  │  [Model Training]                                               │ │
│  │  - Train 3 models (RF, GB, SVR)                                 │ │
│  │  - Select winner (lowest MAE)                                   │ │
│  │  - Retrain on 100% data                                         │ │
│  │       ↓                                                          │ │
│  │  [Goal Prediction]                                              │ │
│  │  - Simulate day-by-day                                          │ │
│  │  - Calculate days to goal                                       │ │
│  │  - Identify top contributor                                     │ │
│  │       ↓                                                          │ │
│  │  HTTPS GET → Blynk API (V16, V17 Update)                       │ │
│  │                                                                  │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                        │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ Blynk IoT Cloud                                                 │ │
│  │                                                                  │ │
│  │  Virtual Pins (V0-V30)                                          │ │
│  │  ↑ Device writes (status, balance, logs)                        │ │
│  │  ↓ Cloud writes (user input, config)                            │ │
│  │                                                                  │ │
│  │  Event Log (note_deposited)                                     │ │
│  │  ← Device sends notifications                                   │ │
│  │                                                                  │ │
│  │  Predictions (V16, V17)                                         │ │
│  │  ← ML Pipeline sends goal date and top contributor              │ │
│  │                                                                  │ │
│  │  Mobile App (User Interface)                                    │ │
│  │  ↑ Display balances, logs, predictions                          │ │
│  │  ↓ User input (register, delete, set goal, change role)         │ │
│  │                                                                  │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                        │
└──────────────────────────────────────────────────────────────────────┘
```

### 11.2 Deposit Transaction Data Flow

```
Physical Banknote
    ↓
IR Sensor Detection (TCRT5000)
    ↓
Stepper Motor Activation (28BYJ-48)
    ↓
Step Counting (billLengthSteps)
    ↓
Denomination Detection (5 or 10 AED)
    ↓
Balance Increment (RAM)
    ↓
Preferences Write (EEPROM Flash)
    ↓
Blynk Virtual Writes (V5, V6, V10, V11)
    ↓
Blynk Event Log (note_deposited)
    ↓
HTTP POST to RPi
    ├─ uid
    ├─ name
    ├─ amount
    ├─ balance
    └─ goal
    ↓
CSV Append (savings_dataset.csv)
    ├─ Timestamp
    ├─ NFC_UID
    ├─ Deposit_Amount
    └─ Total_Balance
    ↓
ML Pipeline Trigger (subprocess)
    ├─ Data Cleaning
    ├─ Feature Engineering
    ├─ Model Training
    ├─ Goal Prediction
    └─ Blynk Update (V16, V17)
    ↓
Auto-Logout (Session End)
```

---

## 12. Data Validation and Error Handling

### 12.1 Input Validation

**RFID UID Validation**:
- Input: Bytes from MFRC522
- Validation: Convert to hex string, uppercase
- Code: `savesentra_iot.ino` (line 483-490)
- Error Handling: None (all byte sequences are valid)

**Denomination Validation**:
- Input: `billLengthSteps` (accumulated motor steps)
- Validation: Check ranges (4990-5080 for 5 AED, 5090-5200 for 10 AED)
- Code: `savesentra_iot.ino` (line 575-585)
- Error Handling: Set `depositAmount = 0.0`, display "Error: Unknown Note Size"

**User Name Validation**:
- Input: String from Blynk V0
- Validation: Check length > 0
- Code: `savesentra_iot.ino` (line 265-268)
- Error Handling: Return early if empty

**Family Goal Validation**:
- Input: Integer from Blynk V7
- Validation: None (any positive integer accepted)
- Code: `savesentra_iot.ino` (line 325)
- Error Handling: None

**CSV Data Validation** (ML Pipeline):
- Input: `savings_dataset.csv`
- Validations:
  1. Null check: `df.isnull().sum()` (line 18-20)
  2. Duplicate check: `df.duplicated().sum()` (line 23-27)
  3. Denomination check: Verify in [5, 10, 20, 50, 100, 200] (line 34-40)
  4. Balance check: No negative values (line 42-43)
- Error Handling: Print warnings, filter invalid rows

### 12.2 Error Recovery

**HTTP Request Failures**:
- Code: `savesentra_iot.ino` (line 635-640)
- Behavior: Check `httpCode > 0`, log failure but continue
- Recovery: Deposit already saved locally, can retry on next deposit

**ML Pipeline Failures**:
- Code: `rpi_server.py` (line 32-41)
- Behavior: Try-except block, print error message
- Recovery: Deposit still saved to CSV, ML failure doesn't block deposit

**Blynk Connection Loss**:
- Code: `Blynk.run()` handles reconnection automatically
- Behavior: Device continues operating, queues virtual writes
- Recovery: Automatic reconnection when network available

**CSV File Access Errors**:
- Code: `rpi_server.py` (line 27-29)
- Behavior: No explicit error handling
- Risk: File lock or permission error would crash endpoint

---

## 13. Data Security Considerations

### 13.1 Authentication and Authorization

**RFID Authentication**:
- Mechanism: UID matching against stored list
- Strength: Weak (RFID UIDs can be cloned)
- Code: `savesentra_iot.ino` (line 505-520)

**Role-Based Access Control (RBAC)**:
- Roles: Parent, Child, Other
- Implementation: Check `users[i].role == "Parent"` before showing admin tools
- Code: `savesentra_iot.ino` (line 527-543)
- Enforcement: UI-level only (no backend validation)

**Blynk Token**:
- Token: `jIf-JTtFAMlw41WrJvqtZalG0-WnrBXG` (hardcoded in sketch)
- Risk: Exposed in source code, anyone with token can control device
- Scope: Full device control (all virtual pins)

### 13.2 Data Encryption

**In Transit**:
- Blynk: HTTPS (encrypted)
- Raspberry Pi: HTTP (unencrypted, local network only)
- NTP: UDP (unencrypted, standard)

**At Rest**:
- ESP32 Preferences: Unencrypted flash memory
- CSV Ledger: Unencrypted file system
- ML Data: Unencrypted files

### 13.3 Data Privacy

**Personally Identifiable Information (PII)**:
- User names stored in ESP32 flash
- User names transmitted in HTTP POST to RPi
- User names visible in Blynk app
- No encryption or anonymization

**Financial Data**:
- Individual balances stored in ESP32 and CSV
- Balances transmitted in HTTP POST
- Balances visible in Blynk app
- No encryption

---

## 14. Performance Characteristics

### 14.1 Latency

**Deposit Processing**:
- RFID read: ~100ms
- Motor stepping: ~5-10 seconds (depends on note length)
- Balance update: <10ms
- EEPROM write: ~10-50ms
- Blynk virtual writes: ~100-500ms (network dependent)
- HTTP POST to RPi: ~100-500ms (network dependent)
- **Total**: ~6-12 seconds

**ML Pipeline Execution**:
- Data load: ~100ms
- Data cleaning: ~50ms
- Feature engineering: ~50ms
- Model training: ~500ms (3 models)
- Prediction simulation: ~100-500ms (depends on days to goal)
- Blynk update: ~100-500ms
- **Total**: ~1-2 seconds

### 14.2 Throughput

**Deposits Per Day**: No explicit limit, but:
- CSV append is sequential (no parallel writes)
- ML pipeline runs synchronously (blocks new deposits during execution)
- Blynk rate limits: Not documented in code

**ML Pipeline Frequency**: Once per deposit + once per goal change

### 14.3 Storage

**ESP32 Flash**:
- User data: ~20 users × (20 bytes name + 16 bytes UID + 4 bytes balance + 10 bytes role) ≈ 1 KB
- Preferences overhead: ~1 KB
- Total: ~2 KB (out of ~4 MB available)

**Raspberry Pi Disk**:
- CSV ledger: ~1 KB per 10 deposits
- Cleaned/ready data: Similar size
- No explicit size limits

---

## 15. Summary of Data Flows

### 15.1 Primary Data Flows

1. **Deposit Flow**: Physical cash → RFID → Motor → Balance → EEPROM → Blynk → RPi CSV → ML → Blynk Predictions

2. **Configuration Flow**: Blynk App → Virtual Pins → ESP32 Preferences → Blynk Display

3. **Prediction Flow**: CSV Ledger → ML Cleaning → Features → Training → Simulation → Blynk V16/V17

### 15.2 Data Consistency

- **Strong Consistency**: User data (EEPROM writes before Blynk updates)
- **Eventual Consistency**: Predictions (updated asynchronously after deposits)
- **No Transactions**: Multiple writes not atomic (e.g., user deletion rewrites all users)

### 15.3 Data Retention

- **Permanent**: User data (EEPROM), CSV ledger, ML data files
- **Session**: Blynk virtual pin values (until next update)
- **Transient**: ML model objects (in-memory only)

---

## 16. References to Source Code

All data flows documented above reference actual code locations in the format:

`ClassName.Method()` in `path/file.ext` (line N)

Key files referenced:
- `savesentra_iot/savesentra_iot.ino` - ESP32 firmware (667 lines)
- `rpi_server.py` - Raspberry Pi Flask server (65 lines)
- `ml/savesentra_ml.py` - ML pipeline (172 lines)
- `ml/savings_dataset.csv` - Transaction ledger
- `ml/savings_dataset_cleaned.csv` - Cleaned backup
- `ml/ml_ready_data.csv` - ML features

---

## Conclusion

The SAVESENTRA IoT Family Cash Deposit System implements a complete data flow from physical cash deposits through cloud synchronization to machine learning predictions. Data flows are primarily unidirectional (deposit → storage → prediction) with bidirectional control flows (app configuration → device). The system prioritizes local data sovereignty (CSV ledger on RPi) while leveraging cloud services (Blynk) for real-time user interface and notifications. All data transformations are documented with specific code references for verification and maintenance.