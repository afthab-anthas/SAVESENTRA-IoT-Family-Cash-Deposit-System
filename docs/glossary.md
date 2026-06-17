# SAVESENTRA IoT Family Cash Deposit System — Domain Glossary

> Every term is sourced from actual files read during this analysis. No definition is invented.

---

## Domain Concepts & Business Terms

**AED (Arab Emirates Dirham)** — The official currency of the United Arab Emirates. SAVESENTRA is calibrated exclusively for AED banknotes with denominations of 5, 10, 20, 50, 100, and 200 AED. Source: `savesentra_iot/savesentra_iot.ino` (line 597), `ml/savesentra_ml.py` (line 26), `README.md` (line 11).

**Banknote** — Physical currency note accepted by the system. The ESP32 device measures banknote length via stepper motor steps to determine denomination. Valid AED notes: 5 AED (4990–5080 steps), 10 AED (5090–5200 steps). Source: `savesentra_iot/savesentra_iot.ino` (lines 595–605), `savesentra.test.ts` (lines 5–30).

**Bill Length Steps** — The count of stepper motor steps recorded while the IR sensor is actively blocked by a banknote. Used to determine denomination. Stored in variable `billLengthSteps`. Source: `savesentra_iot/savesentra_iot.ino` (line 79), (line 650).

**Cash Deposit** — A single transaction where a user inserts a banknote into the system. Triggers RFID authentication, stepper motor intake, denomination detection, balance update, and ML pipeline execution. Source: `rpi_server.py` (line 10), `README.md` (line 8).

**Denomination** — The face value of a banknote (5 AED, 10 AED, etc.). Detected by comparing `billLengthSteps` against calibrated ranges. Invalid denominations trigger an error message. Source: `savesentra_iot/savesentra_iot.ino` (lines 595–605), `savesentra.test.ts` (line 5).

**Deposit Amount** — The monetary value of a single cash deposit in AED. Stored in CSV as `Deposit_Amount` column. Used as the target variable (y) in ML model training. Source: `ml/savesentra_ml.py` (line 26), `ml/savings_dataset.csv` (header).

**Family Goal** — The collective savings target in AED that the family is working toward. Configurable by parents via Blynk virtual pin V7. Default: 1000 AED. Used by ML pipeline to predict goal completion date. Source: `savesentra_iot/savesentra_iot.ino` (line 73), (line 330).

**Goal Completion Date** — The predicted date when the family will reach their `familyGoal` based on historical deposit patterns. Computed by the ML pipeline via day-by-day simulation. Pushed to Blynk virtual pin V16. Source: `ml/savesentra_ml.py` (line 147), `savesentra_iot/savesentra_iot.ino` (line 330).

**Intake Mechanism** — The stepper motor-based system that physically accepts and measures banknotes. Controlled by the 28BYJ-48 stepper motor via ULN2003 driver. Source: `README.md` (line 19), `savesentra_iot/savesentra_iot.ino` (lines 30–32).

**Motor Stop Delay** — Hysteresis constant controlling how long the stepper motor continues running after the IR sensor clears. Default: 5000 ms (5 seconds). Prevents motor from stopping prematurely during note insertion. Source: `savesentra_iot/savesentra_iot.ino` (line 82).

**NFC UID (Near Field Communication Unique Identifier)** — The unique identifier of an RFID card. Stored as a hexadecimal string (e.g., `F351F513`). Used to authenticate users and link deposits to individual accounts. Source: `savesentra_iot/savesentra_iot.ino` (line 445), `ml/savings_dataset.csv` (header: `NFC_UID`).

**Role-Based Access Control (RBAC)** — Permission system with three roles: Parent (admin), Child (standard user), Other (default). Parents can register users, delete users, set family goals, and view admin tools. Children and Others see only their own balance and transaction history. Source: `savesentra_iot/savesentra_iot.ino` (lines 254–269), (line 507).

**Session** — An authenticated user interaction from RFID card tap to auto-logout. During a session, the motor runs and deposits are accepted. Auto-logout occurs 8 seconds after the IR sensor clears. Source: `savesentra_iot/savesentra_iot.ino` (line 82), (line 580).

**Top Contributor** — The user predicted to contribute the most to the family goal by the completion date. Computed by the ML pipeline and pushed to Blynk virtual pin V17. Source: `ml/savesentra_ml.py` (line 162).

**Total Balance** — The cumulative sum of all deposits for a single user. Stored in ESP32 Preferences and updated after each successful deposit. Displayed on Blynk virtual pin V6 (individual) and V10 (family total). Source: `savesentra_iot/savesentra_iot.ino` (line 75), `ml/savings_dataset.csv` (header: `Total_Balance`).

**Transaction Ledger** — The CSV file (`savings_dataset.csv`) on the Raspberry Pi that persists all deposits. Columns: `Timestamp`, `NFC_UID`, `Deposit_Amount`, `Total_Balance`. Source: `rpi_server.py` (line 9), `README.md` (line 24).

**User Registration** — The process of associating an RFID card with a user name and role. Triggered by parent entering a name in Blynk V0 and user tapping their card. Source: `savesentra_iot/savesentra_iot.ino` (lines 193–211), (lines 450–475).

---

## Hardware & Sensor Terms

**28BYJ-48 Stepper Motor** — A 4-phase stepper motor with 2048 steps per revolution. Controlled via ULN2003 driver. Used for banknote intake. Pins: IN1 (D5), IN2 (D6), IN3 (D7), IN4 (D8). Source: `savesentra_iot/savesentra_iot.ino` (lines 30–32), `README.md` (line 19).

**Arduino Nano ESP32** — The main microcontroller running the SAVESENTRA firmware. Integrates WiFi, SPI, GPIO, and Preferences (flash storage). Source: `README.md` (line 18), `savesentra_iot/savesentra_ino` (line 1).

**IR Sensor (TCRT5000)** — An infrared reflective sensor that detects when a banknote is present in the intake slot. Output: digital (LOW when blocked, HIGH when clear). Pin: D4. Used to measure note length via step counting. Source: `savesentra_iot/savesentra_iot.ino` (lines 13–17), `README.md` (line 20).

**LED (Built-in)** — The ESP32's onboard LED. Lights up (HIGH) when the IR sensor is blocked (note present). Used for visual feedback. Source: `savesentra_iot/savesentra_iot.ino` (line 565).

**MFRC522 RFID Reader** — An SPI-based RFID/NFC reader module. Reads RFID card UIDs for user authentication. Pins: SDA/SS (D10), SCK (D13), MOSI (D11), MISO (D12), RST (D9). Source: `savesentra_iot/savesentra_iot.ino` (lines 7–11), `README.md` (line 19).

**ULN2003 Driver** — A Darlington transistor array that amplifies GPIO signals to drive the stepper motor coils. Pins: IN1–IN4 (D5–D8). Source: `README.md` (line 19).

---

## Software & System Architecture Terms

**Blynk IoT** — A cloud-based IoT platform providing a mobile dashboard and real-time data synchronization. SAVESENTRA uses Blynk for user authentication, balance display, transaction logs, and ML prediction updates. Template ID: `TMPL2sUhFh8RM`. Source: `savesentra_iot/savesentra_iot.ino` (lines 22–24), `README.md` (line 13).

**Blynk Virtual Pin** — A numbered data channel (V0–V30) linking the ESP32 to the Blynk mobile app. Used for UI widgets (buttons, sliders, labels, menus). Source: `savesentra_iot/savesentra_iot.ino` (lines 188–372).

**Edge Device** — The ESP32 microcontroller and attached sensors/motors. Responsible for real-time hardware control, RFID authentication, denomination detection, and local balance persistence. Source: `README.md` (line 24), `docs/architecture.md` (line 1).

**Edge Tier** — The first layer of the three-tier architecture. Handles physical cash intake and local authentication. Source: `docs/decisions.md` (line 1).

**ESP32 Preferences** — The ESP32's built-in non-volatile storage (flash memory). Stores user profiles (name, UID, balance, role) in a namespace called `"users"`. Source: `savesentra_iot/savesentra_iot.ino` (lines 88–135).

**Flask Server** — The Python HTTP server running on the Raspberry Pi. Listens on port 5000. Receives deposit POST requests, appends to CSV, and triggers the ML pipeline. Source: `rpi_server.py` (line 1), `README.md` (line 25).

**HTTP POST** — The request method used by the ESP32 to send deposit data to the Raspberry Pi server. Payload: form-encoded with fields `uid`, `name`, `amount`, `balance`, `goal`. Source: `savesentra_iot/savesentra_iot.ino` (lines 618–630).

**Local Server** — The Raspberry Pi running the Flask HTTP server and ML pipeline. Receives deposits, persists to CSV, and triggers predictions. Source: `README.md` (line 24), `docs/architecture.md` (line 1).

**Local Tier** — The second layer of the three-tier architecture. Persists transactions to CSV and orchestrates the ML pipeline. Source: `docs/decisions.md` (line 1).

**ML Pipeline** — An 8-step automated workflow that cleans transaction data, engineers temporal features, trains three regression models, and simulates future deposits to predict goal completion. Source: `ml/savesentra_ml.py` (line 1), `README.md` (line 9).

**NTP Server** — Network Time Protocol server used to synchronize the ESP32's clock. Address: `pool.ntp.org`. Timezone offset: UTC+4 (UAE). Source: `savesentra_iot/savesentra_iot.ino` (lines 52–54).

**Raspberry Pi** — A single-board computer running the Flask server and ML pipeline. Stores the transaction CSV ledger. Source: `README.md` (line 25), `rpi_server.py` (line 1).

**Three-Tier Architecture** — The system design comprising Edge (ESP32), Local (Raspberry Pi), and Cloud (Blynk) tiers. Enables offline operation, local data persistence, and cloud-based dashboards. Source: `docs/architecture.md` (line 1), `README.md` (line 23).

**WiFi Connection** — The ESP32 connects to a home WiFi network (SSID: `"Anthas Home"`) to reach the Blynk cloud and Raspberry Pi server. Source: `savesentra_iot/savesentra_iot.ino` (lines 48–50).

---

## Machine Learning Terms

**Allowed Notes** — The set of valid AED denominations: [5, 10, 20, 50, 100, 200]. Any deposit outside this range is flagged as invalid during data cleaning. Source: `ml/savesentra_ml.py` (line 25).

**Data Cleaning** — Step 1 of the ML pipeline. Removes duplicates, converts timestamps to datetime, and filters out invalid note amounts. Source: `ml/savesentra_ml.py` (lines 14–40).

**Day_Of_Month** — Engineered feature: the day of the month (1–31). Used to capture intra-month spending patterns. Source: `ml/savesentra_ml.py` (line 66).

**Day_Of_Week** — Engineered feature: the day of the week (0=Monday, 6=Sunday). Used to capture weekly spending patterns. Source: `ml/savesentra_ml.py` (line 64).

**Feature Engineering** — Step 2 of the ML pipeline. Aggregates daily deposits and creates temporal features (Day_Of_Week, Is_Weekend, Month, Day_Of_Month). Source: `ml/savesentra_ml.py` (lines 58–70).

**Gradient Boosting** — One of three regression models trained during the model shootout. Uses sequential tree building to minimize residuals. Source: `ml/savesentra_ml.py` (line 95).

**Is_Weekend** — Engineered feature: binary flag (1 if Saturday/Sunday, 0 otherwise). Captures weekend vs. weekday spending differences. Source: `ml/savesentra_ml.py` (line 65).

**Mean Absolute Error (MAE)** — The metric used to evaluate model performance. Computed as the average absolute difference between predicted and actual deposit amounts. Source: `ml/savesentra_ml.py` (line 101).

**Model Shootout** — Step 5 of the ML pipeline. Trains three models (Random Forest, Gradient Boosting, SVR) on the same training set and selects the one with the lowest MAE. Source: `ml/savesentra_ml.py` (lines 93–107).

**Month** — Engineered feature: the month (1–12). Used to capture seasonal spending patterns. Source: `ml/savesentra_ml.py` (line 66).

**Random Forest** — One of three regression models trained during the model shootout. Uses ensemble of decision trees. Source: `ml/savesentra_ml.py` (line 94).

**Simulation** — Step 8 of the ML pipeline. Iterates day-by-day into the future, predicting deposits using the trained model, until the simulated balance reaches the family goal. Source: `ml/savesentra_ml.py` (lines 130–145).

**Support Vector Regression (SVR)** — The winning regression model (outperforms Random Forest and Gradient Boosting). Uses RBF kernel to capture non-linear, cyclical human behavior. Source: `ml/savesentra_ml.py` (line 96), `README.md` (line 9).

**Temporal Features** — Time-based features (Day_Of_Week, Is_Weekend, Month, Day_Of_Month) engineered to capture cyclical deposit patterns. Source: `ml/savesentra_ml.py` (lines 64–66).

**Test Set** — 20% of the data used to evaluate model performance. Split via `train_test_split` with `test_size=0.2`. Source: `ml/savesentra_ml.py` (line 88).

**Training Set** — 80% of the data used to fit the regression models. Split via `train_test_split` with `test_size=0.2`. Source: `ml/savesentra_ml.py` (line 88).

---

## Data & File Terms

**CSV Path** — The file system location of the transaction ledger on the Raspberry Pi: `/home/admin/Desktop/savings_dataset.csv`. Source: `rpi_server.py` (line 9).

**ml_ready_data.csv** — Intermediate CSV file containing cleaned, aggregated daily deposits with engineered features. Used as input to model training. Source: `ml/savesentra_ml.py` (line 70).

**savings_dataset.csv** — The live transaction ledger. Columns: `Timestamp`, `NFC_UID`, `Deposit_Amount`, `Total_Balance`. Appended to by the Flask server on each deposit. Source: `rpi_server.py` (line 9), `ml/savesentra_ml.py` (line 15).

**savings_dataset_cleaned.csv** — Backup CSV created after data cleaning step. Contains the same rows as `savings_dataset.csv` but with duplicates removed and invalid notes filtered. Source: `ml/savesentra_ml.py` (line 41).

**Timestamp** — The date and time of a deposit in format `YYYY-MM-DD HH:MM:SS`. Stored in the first column of the CSV ledger. Source: `savesentra_iot/savesentra_iot.ino` (line 145), `ml/savings_dataset.csv` (header).

---

## API & Communication Terms

**Content-Type Header** — HTTP header set to `application/x-www-form-urlencoded` when POSTing deposit data to the Raspberry Pi. Source: `savesentra_iot/savesentra_iot.ino` (line 619).

**Deposit Endpoint** — The Flask route `/deposit` on the Raspberry Pi server. Accepts POST requests with deposit data and triggers the ML pipeline. Source: `rpi_server.py` (line 10).

**HTTP Response Code** — The status code returned by the Flask server. Checked by the ESP32 to confirm successful data transmission. Source: `savesentra_iot/savesentra_iot.ino` (line 627).

**Payload** — The form-encoded data sent in the HTTP POST request. Contains: `uid`, `name`, `amount`, `balance`, `goal`. Source: `savesentra_iot/savesentra_iot.ino` (lines 621–626).

**Predict Endpoint** — The Flask route `/predict` on the Raspberry Pi server. Accepts POST requests with an updated family goal and re-runs the ML pipeline. Source: `rpi_server.py` (line 43), `savesentra_iot/savesentra_iot.ino` (line 329).

**RPI Server URL** — The network address of the Raspberry Pi Flask server: `http://raspberrypi.local:5000/deposit`. Source: `savesentra_iot/savesentra_iot.ino` (line 45).

---

## Configuration & Constants

**BLYNK_AUTH_TOKEN** — The authentication token for the Blynk cloud connection. Stored in the ESP32 firmware. Source: `savesentra_iot/savesentra_iot.ino` (line 24).

**BLYNK_TEMPLATE_ID** — The Blynk template identifier: `TMPL2sUhFh8RM`. Source: `savesentra_iot/savesentra_iot.ino` (line 22).

**BLYNK_TEMPLATE_NAME** — The Blynk template name: `SAVESENTRA`. Source: `savesentra_iot/savesentra_iot.ino` (line 23).

**Default Family Goal** — The initial savings target: 1000 AED. Can be overridden by parents via Blynk. Source: `savesentra_iot/savesentra_iot.ino` (line 73).

**Default ML Goal** — The fallback family goal used by the ML pipeline if no argument is provided: 15000 AED. Source: `ml/savesentra_ml.py` (line 119).

**GMT Offset** — The timezone offset for the ESP32: 14400 seconds (UTC+4, UAE time). Source: `savesentra_iot/savesentra_iot.ino` (line 53).

**IR_SENSOR_PIN** — GPIO pin for the IR sensor: D4 (pin 4). Source: `savesentra_iot/savesentra_iot.ino` (line 28).

**MAX_USERS** — The maximum number of users that can be registered: 20. Limited by ESP32 memory. Source: `savesentra_iot/savesentra_iot.ino` (line 63).

**Motor Speed** — The stepper motor speed setting: 10 RPM. Source: `savesentra_iot/savesentra_iot.ino` (line 381).

**NTP Server Address** — The network time protocol server: `pool.ntp.org`. Source: `savesentra_iot/savesentra_iot.ino` (line 52).

**Stepper Motor Pins** — GPIO pins for the stepper motor: IN1 (D5), IN2 (D6), IN3 (D7), IN4 (D8). Source: `savesentra_iot/savesentra_iot.ino` (lines 30–32).

**Steps Per Revolution** — The number of steps in one full rotation of the stepper motor: 2048. Source: `savesentra_iot/savesentra_iot.ino` (line 68).

**WiFi SSID** — The home WiFi network name: `"Anthas Home"`. Source: `savesentra_iot/savesentra_iot.ino` (line 48).

---

## Blynk Virtual Pin Mapping

| Pin | Widget Type | Purpose | Source |
|-----|-------------|---------|--------|
| V0 | Text Input | User name registration input | `savesentra_iot/savesentra_iot.ino` (line 188) |
| V1 | Label | Status messages and feedback | `savesentra_iot/savesentra_iot.ino` (line 196) |
| V3 | Button | Reset all users (admin only) | `savesentra_iot/savesentra_iot.ino` (line 220) |
| V4 | Label | Step counter display | `savesentra_iot/savesentra_iot.ino` (line 654) |
| V5 | Terminal | Transaction log display | `savesentra_iot/savesentra_iot.ino` (line 614) |
| V6 | Gauge | Individual user balance | `savesentra_iot/savesentra_iot.ino` (line 485) |
| V7 | Slider | Family goal setting (admin only) | `savesentra_iot/savesentra_iot.ino` (line 330) |
| V8 | Button | Delete selected user (admin only) | `savesentra_iot/savesentra_iot.ino` (line 272) |
| V10 | Gauge | Total family savings | `savesentra_iot/savesentra_iot.ino` (line 610) |
| V11 | Progress Bar | Goal contribution percentage | `savesentra_iot/savesentra_iot.ino` (line 237) |
| V12 | Button | Manual logout | `savesentra_iot/savesentra_iot.ino` (line 177) |
| V16 | Label | Predicted goal completion date (from ML) | `ml/savesentra_ml.py` (line 159) |
| V17 | Label | Predicted top contributor UID (from ML) | `ml/savesentra_ml.py` (line 160) |
| V20 | Menu | User selection dropdown | `savesentra_iot/savesentra_iot.ino` (line 224) |
| V21 | Menu | Role selection (Parent/Child/Other) | `savesentra_iot/savesentra_iot.ino` (line 254) |
| V30 | Hidden Widget | Admin-only placeholder | `savesentra_iot/savesentra_iot.ino` (line 507) |

---

## User Roles & Permissions

| Role | Permissions | Source |
|------|-------------|--------|
| **Parent** | Register users, delete users, set family goal, view admin tools, change user roles | `savesentra_iot/savesentra_iot.ino` (lines 507–530) |
| **Child** | Deposit cash, view own balance, view family total | `savesentra_iot/savesentra_iot.ino` (lines 531–545) |
| **Other** | Deposit cash, view own balance, view family total | `savesentra_iot/savesentra_iot.ino` (lines 531–545) |

---

## Error Codes & Messages

| Condition | Message | Source |
|-----------|---------|--------|
| Memory full (MAX_USERS reached) | `"Error: Memory Full"` | `savesentra_iot/savesentra_iot.ino` (line 201) |
| Card already registered | `"Card already owned by [name]"` | `savesentra_iot/savesentra_iot.ino` (line 458) |
| Unknown card during auth | `"User Not Recognized"` | `savesentra_iot/savesentra_iot.ino` (line 542) |
| Invalid note size | `"Error: Unknown Note Size"` | `savesentra_iot/savesentra_iot.ino` (line 603) |
| No user selected for deletion | `"Error: No user selected"` | `savesentra_iot/savesentra_iot.ino` (line 316) |
| No user selected for role change | `"Select a user first!"` | `savesentra_iot/savesentra_iot.ino` (line 267) |

---

## Technical Abbreviations

| Abbreviation | Expansion | Context |
|---|---|---|
| AED | Arab Emirates Dirham | Currency unit |
| RFID | Radio-Frequency Identification | Card authentication technology |
| NFC | Near Field Communication | Alternative name for RFID in this context |
| UID | Unique Identifier | RFID card identifier |
| IR | Infrared | Sensor type for detecting banknotes |
| CSV | Comma-Separated Values | File format for transaction ledger |
| HTTP | HyperText Transfer Protocol | Communication protocol between ESP32 and Raspberry Pi |
| POST | HTTP POST method | Request type for sending deposit data |
| GPIO | General-Purpose Input/Output | Digital pins on the ESP32 |
| SPI | Serial Peripheral Interface | Communication protocol for RFID reader |
| RBF | Radial Basis Function | Kernel type used in SVR model |
| MAE | Mean Absolute Error | Model evaluation metric |
| ML | Machine Learning | Predictive analytics pipeline |
| SVR | Support Vector Regression | Winning ML model |
| NTP | Network Time Protocol | Time synchronization protocol |
| UTC | Coordinated Universal Time | Time standard |
| RBAC | Role-Based Access Control | Permission system |

---

## State & Status Values

**Authenticated** — Boolean flag indicating a user has successfully logged in via RFID card. Triggers motor startup and enables deposit acceptance. Source: `savesentra_iot/savesentra_iot.ino` (line 76).

**Deposit In Progress** — Boolean flag indicating a banknote is currently being processed. Set to true when IR sensor is blocked; triggers auto-logout logic after 8 seconds of clear. Source: `savesentra_iot/savesentra_iot.ino` (line 81).

**Is Motor Running** — Boolean flag controlling stepper motor operation. Set to true when IR sensor is blocked or within the motor stop delay window. Source: `savesentra_iot/savesentra_iot.ino` (line 80).

**Registration Mode** — Boolean flag indicating the system is waiting for an RFID card tap to register a new user. Activated when a parent enters a name in Blynk V0. Source: `savesentra_iot/savesentra_iot.ino` (line 85).

---

## Temporal Concepts

**Auto-Logout** — Automatic session termination 8 seconds after the IR sensor clears (5 seconds motor stop delay + 3 seconds processing delay). Clears authentication state and hides admin tools. Source: `savesentra_iot/savesentra_iot.ino` (line 580).

**Hysteresis** — Motor control strategy that keeps the motor running for 5 seconds after the IR sensor clears to ensure the banknote is fully ejected. Prevents premature motor shutdown. Source: `savesentra_iot/savesentra_iot.ino` (line 82).

**Session Timeout** — The duration a user remains authenticated after RFID card tap. Effectively 8 seconds (motor stop delay + processing delay). Source: `savesentra_iot/savesentra_iot.ino` (line 580).

---

## Measurement Units

**Steps** — The unit of stepper motor movement. One step = 1/2048 of a full rotation. Used to measure banknote length. Source: `savesentra_iot/savesentra_iot.ino` (line 68).

**Milliseconds (ms)** — Time unit used for delays and timeouts. Motor stop delay = 5000 ms. Source: `savesentra_iot/savesentra_iot.ino` (line 82).

**Seconds (s)** — Time unit used for NTP offset and processing delays. GMT offset = 14400 seconds. Source: `savesentra_iot/savesentra_iot.ino` (line 53).

---

## Related Documentation

- **Architecture Overview**: `docs/architecture.md`
- **Architecture Decision Records**: `docs/decisions.md`
- **Code Documentation**: `docs/code.md`
- **Data Flow Diagrams**: `docs/dataflow.md`
- **Risk Analysis**: `docs/risk.md`
- **Project Structure**: `docs/structure.md`

---

**Document Version**: 1.0  
**Last Updated**: 2025  
**Scope**: SAVESENTRA IoT Family Cash Deposit System v1.0