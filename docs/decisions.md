# SAVESENTRA IoT Family Cash Deposit System - Architecture Decision Records

## Executive Summary

This document captures the architectural decisions made in the SAVESENTRA IoT Family Cash Deposit System. The system is a three-tier IoT application comprising an ESP32 edge device for physical cash intake, a Raspberry Pi local server for transaction persistence, and a Python ML pipeline for predictive analytics. This ADR document provides evidence-based rationale for technology choices, design patterns, trade-offs, and deployment strategies.

---

## ADR-001: Three-Tier Architecture with Edge-Local-Cloud Integration

**Status:** Accepted

### Context

The SAVESENTRA system must handle physical cash deposits in real-time, persist transaction data locally, and provide predictive analytics to users via a mobile dashboard. The system operates in a family environment where multiple users may deposit cash simultaneously, and network connectivity may be intermittent.

### Decision

Adopt a three-tier architecture:
1. **Edge Tier (ESP32)**: Real-time hardware control and local authentication
2. **Local Tier (Raspberry Pi)**: Transaction persistence and ML pipeline orchestration
3. **Cloud Tier (Blynk IoT)**: User-facing dashboard and remote monitoring

### Consequences

**Positive:**
- Decouples hardware control from cloud dependencies, enabling offline operation
- Reduces latency for hardware interactions (RFID, stepper motor, IR sensor)
- Enables local data persistence independent of cloud availability
- Allows ML training to occur locally without exposing raw transaction data to cloud

**Negative:**
- Increases operational complexity with three distinct deployment targets
- Requires synchronization logic between local and cloud tiers
- Introduces potential data consistency issues if Raspberry Pi and Blynk diverge

**Evidence:**
- `savesentra_iot.ino` (lines 1-667): ESP32 firmware with local RFID authentication and Blynk integration
- `rpi_server.py` (lines 1-65): Flask server receiving deposits from ESP32
- `ml/savesentra_ml.py` (lines 1-172): Local ML pipeline triggered by server

---

## ADR-002: Arduino/C++ for ESP32 Edge Device Firmware

**Status:** Accepted

### Context

The edge device must interact with multiple hardware peripherals (RFID reader, stepper motor, IR sensor) with precise timing and low latency. The device has limited computational resources (240 MHz dual-core processor, 520 KB SRAM).

### Decision

Use Arduino IDE with C++ to develop ESP32 firmware, leveraging existing Arduino ecosystem libraries for hardware abstraction.

### Consequences

**Positive:**
- Mature ecosystem with well-tested libraries (MFRC522, Blynk, Stepper)
- Direct hardware register access for timing-critical operations
- Minimal runtime overhead suitable for resource-constrained device
- Extensive community support and documentation

**Negative:**
- Limited built-in memory for complex logic
- No native support for advanced data structures or OOP patterns
- Difficult to implement sophisticated error handling
- Testing requires physical hardware or emulation

**Evidence:**
- File: `savesentra_iot/savesentra_iot.ino` - 667 lines of Arduino C++ code
- Uses libraries: `MFRC522.h` (RFID), `Blynk.h` (IoT), `Stepper.h` (motor control)
- Hardware definitions at lines 1-50 (pins, constants)

---

## ADR-003: Python for Raspberry Pi Server and ML Pipeline

**Status:** Accepted

### Context

The local server must receive HTTP requests, persist data to CSV, and trigger a machine learning pipeline. The ML pipeline requires numerical computing, statistical analysis, and model training capabilities.

### Decision

Use Python 3 with Flask for the HTTP server and scikit-learn for the ML pipeline.

### Consequences

**Positive:**
- Flask provides lightweight HTTP server suitable for local deployment
- Python ecosystem (pandas, scikit-learn, numpy) is industry-standard for ML
- Rapid development and iteration for data processing workflows
- Easy integration with system utilities via subprocess

**Negative:**
- Python runtime adds startup latency compared to compiled languages
- GIL (Global Interpreter Lock) limits true parallelism
- Requires Python runtime installation on Raspberry Pi
- Slower execution than compiled alternatives for compute-intensive operations

**Evidence:**
- File: `rpi_server.py` - Flask application with `/deposit` endpoint
- File: `ml/savesentra_ml.py` - ML pipeline using pandas, scikit-learn
- Dependencies visible in code imports (lines 1-20 of both files)

---

## ADR-004: CSV-Based Local Ledger for Transaction Persistence

**Status:** Accepted

### Context

The system must persist transaction data locally on the Raspberry Pi. The data volume is expected to be small (family-scale, not enterprise), and the system operates in a resource-constrained environment.

### Decision

Use CSV files as the primary transaction ledger, appended to by the Flask server and read by the ML pipeline.

### Consequences

**Positive:**
- No database server required; minimal resource overhead
- Human-readable format for debugging and auditing
- Simple append-only semantics prevent data loss
- Easy to backup and transfer data
- Compatible with standard data analysis tools

**Negative:**
- No ACID guarantees; concurrent writes may corrupt data
- No indexing; full-file scans required for queries
- No built-in schema validation
- Difficult to implement complex queries or transactions
- Scaling beyond family-scale usage would require database migration

**Evidence:**
- File: `rpi_server.py` (lines 40-50): CSV append operation
  ```python
  with open('transactions.csv', 'a') as f:
      writer = csv.writer(f)
      writer.writerow([timestamp, user_id, amount, denomination])
  ```
- File: `ml/savesentra_ml.py` (lines 30-40): CSV read operation with pandas

---

## ADR-005: Blynk IoT Platform for Mobile Dashboard and Real-Time Updates

**Status:** Accepted

### Context

The system must provide real-time visibility to family members about savings progress, transaction history, and ML-predicted goal completion date. A custom mobile app would require significant development effort.

### Decision

Integrate with Blynk IoT platform, using virtual pins to push balance updates, transaction logs, and predictions from ESP32 and Raspberry Pi to a pre-configured mobile dashboard.

### Consequences

**Positive:**
- Eliminates need to develop custom mobile app
- Provides real-time push notifications and UI updates
- Blynk handles user authentication and multi-device management
- Reduces development time to market
- Supports RBAC (role-based access control) for family members

**Negative:**
- Introduces cloud dependency for user-facing features
- Blynk service availability affects system usability
- Vendor lock-in; switching platforms requires significant refactoring
- Data flows through third-party cloud (privacy consideration)
- Blynk API rate limits may constrain update frequency

**Evidence:**
- File: `savesentra_iot.ino` (lines 100-150): Blynk initialization and virtual pin handlers
  ```cpp
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Blynk.virtualWrite(V0, balance); // Push balance to virtual pin 0
  ```
- Multiple virtual pins used for different data types (balance, transactions, predictions)

---

## ADR-006: Support Vector Regression (SVR) as Primary ML Model

**Status:** Accepted

### Context

The system must predict when the family will reach their savings goal based on historical deposit patterns. The prediction must be interpretable and computationally efficient on a Raspberry Pi.

### Decision

Train three regression models (Random Forest, Gradient Boosting, SVR) and select SVR as the primary model for goal completion date prediction.

### Consequences

**Positive:**
- SVR handles non-linear relationships in deposit patterns
- Computationally efficient; suitable for Raspberry Pi execution
- Robust to outliers in historical data
- Produces single-point predictions suitable for mobile display
- Ensemble approach (three models) provides robustness

**Negative:**
- SVR hyperparameters require tuning; no automatic optimization implemented
- Limited interpretability compared to linear models
- Requires feature scaling (implemented via StandardScaler)
- Prediction confidence intervals not provided to users
- Training data requirements may be high for accurate predictions

**Evidence:**
- File: `ml/savesentra_ml.py` (lines 80-120): Model training code
  ```python
  from sklearn.svm import SVR
  svr_model = SVR(kernel='rbf', C=100, gamma='scale')
  svr_model.fit(X_train, y_train)
  ```
- Three models trained and compared (lines 100-130)

---

## ADR-007: RFID-Based User Authentication on Edge Device

**Status:** Accepted

### Context

Multiple family members may use the cash deposit system. The system must identify which family member is making a deposit to attribute transactions correctly.

### Decision

Use RFID card authentication via MFRC522 reader. Each family member has a unique RFID card. Authentication occurs locally on ESP32 before accepting deposits.

### Consequences

**Positive:**
- Fast, contactless authentication (< 100ms)
- No network dependency for authentication
- Low cost (RFID cards ~$0.50 each)
- Familiar UX (similar to transit cards)
- Offline operation capability

**Negative:**
- RFID cards can be lost or shared between family members
- No cryptographic security; cards can be cloned
- Limited to pre-registered card IDs
- No audit trail of who physically inserted the card
- Requires manual card registration process

**Evidence:**
- File: `savesentra_iot.ino` (lines 150-200): RFID reader initialization and card detection
  ```cpp
  MFRC522 rfid(SS_PIN, RST_PIN);
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      // Authenticate user based on card UID
  }
  ```
- Card UID stored in ESP32 Preferences (lines 250-280)

---

## ADR-008: Stepper Motor with IR Sensor for Denomination Detection

**Status:** Accepted

### Context

The system must accept UAE Dirham banknotes of different denominations (5, 10, 20, 50, 100, 200 AED) and correctly identify each denomination to calculate deposit amounts.

### Decision

Use a 28BYJ-48 stepper motor to intake notes and an IR sensor (TCRT5000) to measure note length. Denomination is determined by note length via a calibration lookup table.

### Consequences

**Positive:**
- Mechanical approach avoids image processing complexity
- IR sensor is low-cost and reliable
- Stepper motor provides precise control for note intake
- Calibration table is simple and maintainable
- Works with physical currency without modification

**Negative:**
- Requires precise mechanical calibration for each note denomination
- Worn or damaged notes may have incorrect length measurements
- No validation that inserted item is actually currency
- Mechanical wear requires periodic maintenance
- Cannot detect counterfeit notes
- Single-note intake; cannot handle multiple simultaneous deposits

**Evidence:**
- File: `savesentra_iot.ino` (lines 300-350): Stepper motor control
  ```cpp
  Stepper stepper(STEPS_PER_REVOLUTION, IN1, IN2, IN3, IN4);
  stepper.step(STEPS_TO_INTAKE);
  ```
- File: `savesentra_iot.ino` (lines 350-400): IR sensor reading and denomination lookup
  ```cpp
  int noteLength = readIRSensor();
  int denomination = lookupDenomination(noteLength);
  ```

---

## ADR-009: REST API for ESP32-to-Raspberry Pi Communication

**Status:** Accepted

### Context

The ESP32 edge device must transmit deposit transactions to the Raspberry Pi local server for persistence and ML pipeline triggering.

### Decision

Use HTTP POST requests with JSON payload to communicate deposit data from ESP32 to Raspberry Pi. Flask server exposes `/deposit` endpoint.

### Consequences

**Positive:**
- Simple, stateless protocol suitable for IoT devices
- JSON provides human-readable data format
- HTTP is widely supported and debuggable
- Decouples ESP32 from server implementation details
- Easy to add new endpoints without firmware changes

**Negative:**
- HTTP overhead compared to binary protocols (CoAP, MQTT)
- No built-in reliability; requires application-level retry logic
- No authentication between ESP32 and Raspberry Pi (both on local network)
- Synchronous request-response model may block ESP32 during network latency
- No message queuing; lost requests are not retried

**Evidence:**
- File: `savesentra_iot.ino` (lines 450-500): HTTP POST to Raspberry Pi
  ```cpp
  HTTPClient http;
  http.begin("http://raspberrypi.local:5000/deposit");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(jsonPayload);
  ```
- File: `rpi_server.py` (lines 20-50): Flask endpoint definition
  ```python
  @app.route('/deposit', methods=['POST'])
  def receive_deposit():
      data = request.json
      # Persist to CSV
  ```

---

## ADR-010: Local Network Communication (No VPN/Encryption for LAN)

**Status:** Accepted

### Context

ESP32 and Raspberry Pi communicate over a local home network. The network is assumed to be trusted (family home WiFi).

### Decision

Use unencrypted HTTP for local communication between ESP32 and Raspberry Pi. HTTPS is used only for Blynk cloud communication.

### Consequences

**Positive:**
- Reduces computational overhead on ESP32 (no TLS handshake)
- Simplifies certificate management
- Faster communication latency
- Suitable for trusted local network environment

**Negative:**
- Vulnerable to network sniffing on local WiFi
- No authentication between ESP32 and Raspberry Pi
- Deposit amounts and user IDs transmitted in plaintext
- Does not meet security standards for production financial systems
- Vulnerable to MITM attacks if WiFi is compromised

**Evidence:**
- File: `savesentra_iot.ino` (lines 450-460): HTTP (not HTTPS) used for local communication
- File: `rpi_server.py`: No TLS/SSL configuration for Flask server

---

## ADR-011: Temporal Feature Engineering for ML Prediction

**Status:** Accepted

### Context

The ML pipeline must predict when the family will reach their savings goal. Historical deposit patterns likely exhibit temporal patterns (e.g., weekly deposits, seasonal variations).

### Decision

Engineer temporal features from transaction timestamps: day of week, day of month, month, and time of day. These features are used as inputs to regression models.

### Consequences

**Positive:**
- Captures recurring deposit patterns (e.g., "deposits happen on Fridays")
- Improves prediction accuracy by modeling temporal seasonality
- Interpretable features that domain experts can understand
- Standard approach in time-series forecasting

**Negative:**
- Requires sufficient historical data to learn patterns
- Assumes future patterns match historical patterns
- Does not capture external events (holidays, financial changes)
- Feature engineering is manual; not data-driven

**Evidence:**
- File: `ml/savesentra_ml.py` (lines 50-80): Feature engineering code
  ```python
  df['day_of_week'] = df['timestamp'].dt.dayofweek
  df['day_of_month'] = df['timestamp'].dt.day
  df['month'] = df['timestamp'].dt.month
  df['hour'] = df['timestamp'].dt.hour
  ```

---

## ADR-012: Day-by-Day Simulation for Goal Completion Prediction

**Status:** Accepted

### Context

The system must predict a specific date when the family will reach their savings goal. The prediction must be understandable to non-technical users.

### Decision

Use the trained ML model to simulate future deposits day-by-day, accumulating the predicted daily deposit amount until the goal is reached. Return the predicted completion date.

### Consequences

**Positive:**
- Produces interpretable output (specific date) suitable for mobile display
- Accounts for varying deposit amounts across different days
- Flexible; can incorporate external constraints (e.g., "no deposits on weekends")
- Provides single point estimate suitable for goal-setting

**Negative:**
- Computationally expensive for long prediction horizons (years)
- Assumes model predictions are accurate; errors compound over time
- Does not provide confidence intervals or uncertainty bounds
- Sensitive to model hyperparameters and training data quality
- No validation that predicted date is realistic

**Evidence:**
- File: `ml/savesentra_ml.py` (lines 130-160): Simulation loop
  ```python
  current_balance = initial_balance
  current_date = today
  while current_balance < goal:
      predicted_deposit = model.predict(features_for_date)
      current_balance += predicted_deposit
      current_date += timedelta(days=1)
  return current_date
  ```

---

## ADR-013: CSV-Based Configuration for Denomination Calibration

**Status:** Accepted

### Context

The system must support UAE Dirham banknotes of different denominations. Each denomination has a different physical length, which the IR sensor measures.

### Decision

Store denomination calibration data (note length ranges) in a CSV file. The ESP32 reads this file at startup to populate the denomination lookup table.

### Consequences

**Positive:**
- Calibration data is human-readable and editable
- Easy to adjust calibration without recompiling firmware
- Supports multiple currency types by swapping CSV file
- Decouples hardware calibration from firmware logic

**Negative:**
- Requires manual calibration process for each note denomination
- CSV parsing on ESP32 adds startup time
- No validation that calibration data is correct
- Calibration errors directly impact deposit accuracy
- No versioning or audit trail for calibration changes

**Evidence:**
- File: `savesentra_iot/denomination_calibration.csv` - Calibration lookup table
- File: `savesentra_iot.ino` (lines 200-250): CSV parsing and denomination lookup initialization

---

## ADR-014: Subprocess-Based ML Pipeline Triggering

**Status:** Accepted

### Context

The Flask server must trigger the ML pipeline after receiving a deposit. The pipeline is a separate Python script that performs data processing and model training.

### Decision

Use Python's `subprocess` module to spawn the ML pipeline as a separate process when a deposit is received.

### Consequences

**Positive:**
- Decouples server from ML pipeline; either can be updated independently
- ML pipeline runs asynchronously; does not block HTTP response
- Pipeline can be reused by other components (cron jobs, manual triggers)
- Isolates ML pipeline failures from server stability

**Negative:**
- No inter-process communication; pipeline must read from shared CSV
- Potential race conditions if multiple deposits trigger simultaneous pipeline runs
- No error handling or retry logic if pipeline fails
- Pipeline output not returned to client; user unaware of prediction updates
- Resource contention if pipeline runs during high-load periods

**Evidence:**
- File: `rpi_server.py` (lines 55-65): Subprocess invocation
  ```python
  import subprocess
  subprocess.Popen(['python3', 'ml/savesentra_ml.py'])
  ```

---

## ADR-015: Blynk Virtual Pins for Data Synchronization

**Status:** Accepted

### Context

The system must synchronize data between ESP32, Raspberry Pi, and Blynk cloud. Multiple components need to read and write shared state (balance, transactions, predictions).

### Decision

Use Blynk virtual pins as the primary synchronization mechanism. Each data type (balance, transactions, predictions) is assigned a virtual pin. Components write updates to their assigned pins; Blynk propagates updates to all connected clients.

### Consequences

**Positive:**
- Centralized state management via Blynk cloud
- Automatic synchronization across all clients
- Blynk handles conflict resolution and consistency
- Simple API for reading/writing virtual pins

**Negative:**
- Introduces cloud dependency for state consistency
- Blynk API rate limits may prevent frequent updates
- Virtual pin values are limited in size (string length)
- No transactional semantics; partial updates possible
- Difficult to implement complex state machines

**Evidence:**
- File: `savesentra_iot.ino` (lines 100-150): Virtual pin write operations
  ```cpp
  Blynk.virtualWrite(V0, currentBalance);
  Blynk.virtualWrite(V1, lastTransaction);
  Blynk.virtualWrite(V2, predictedCompletionDate);
  ```

---

## ADR-016: No Explicit Error Handling or Retry Logic

**Status:** Accepted (with Caveats)

### Context

The system operates in a home environment with potentially unreliable WiFi connectivity. Network requests may fail due to temporary outages.

### Decision

Implement minimal error handling. Failed HTTP requests are logged but not retried. Failed Blynk updates are silently ignored.

### Consequences

**Positive:**
- Simpler code; easier to understand and maintain
- Reduced complexity on resource-constrained ESP32
- Faster response times (no retry delays)

**Negative:**
- Deposits may be lost if Raspberry Pi is unreachable
- Blynk updates may fail silently; users unaware of stale data
- No visibility into system failures
- Difficult to debug intermittent network issues
- Does not meet reliability standards for financial systems

**Evidence:**
- File: `savesentra_iot.ino` (lines 450-500): HTTP POST without retry logic
  ```cpp
  int httpCode = http.POST(jsonPayload);
  if (httpCode != 200) {
      Serial.println("Deposit failed"); // Logged but not retried
  }
  ```
- File: `rpi_server.py`: No error handling for CSV write operations

---

## ADR-017: Markdown Documentation for System Design

**Status:** Accepted

### Context

The system is complex with multiple components and integration points. Developers and users need to understand the architecture, configuration, and operation.

### Decision

Use Markdown files for all documentation: architecture overview, code documentation, setup guides, and operational procedures.

### Consequences

**Positive:**
- Markdown is human-readable and version-controllable
- Easy to maintain alongside code in Git repository
- Supports embedding diagrams and code snippets
- Widely supported by documentation platforms (GitHub, GitLab)
- Low barrier to contribution from team members

**Negative:**
- No automatic validation of documentation accuracy
- Difficult to keep documentation synchronized with code changes
- No built-in search or cross-referencing
- Requires manual updates when code changes

**Evidence:**
- File: `README.md` - System overview and setup guide
- File: `architecture.md` - Detailed architecture documentation
- File: `code.md` - Code-level documentation
- File: `SETUP.md` - Configuration and deployment guide

---

## ADR-018: ESP32 Preferences API for Local Persistent Storage

**Status:** Accepted

### Context

The ESP32 must store configuration data (RFID card IDs, user names, calibration data) persistently across power cycles.

### Decision

Use the ESP32 Preferences API (NVS - Non-Volatile Storage) to store key-value pairs in flash memory.

### Consequences

**Positive:**
- Built-in to ESP32 SDK; no external storage required
- Atomic writes prevent data corruption during power loss
- Simple key-value API suitable for configuration data
- Sufficient capacity for family-scale configuration (4 MB NVS partition)

**Negative:**
- Limited to key-value pairs; no complex data structures
- NVS has limited write cycles (~100,000 per key)
- No encryption; stored in plaintext
- Difficult to backup or transfer configuration
- No versioning or schema management

**Evidence:**
- File: `savesentra_iot.ino` (lines 250-280): Preferences API usage
  ```cpp
  Preferences preferences;
  preferences.begin("savesentra", false);
  preferences.putString("card_uid_1", "A1B2C3D4");
  String cardUID = preferences.getString("card_uid_1");
  ```

---

## ADR-019: No Database Server; Single-Machine Deployment

**Status:** Accepted

### Context

The system is designed for a single family household. Data volume is small (hundreds of transactions per year). Operational complexity should be minimized.

### Decision

Deploy all components (Flask server, ML pipeline, CSV ledger) on a single Raspberry Pi. No separate database server.

### Consequences

**Positive:**
- Minimal operational overhead; single machine to manage
- No network latency between server and storage
- Low cost; no additional hardware required
- Simple backup procedure (copy Raspberry Pi SD card)
- Suitable for family-scale usage

**Negative:**
- Single point of failure; Raspberry Pi outage affects entire system
- No redundancy or failover capability
- Difficult to scale beyond single household
- Resource contention between Flask server and ML pipeline
- No separation of concerns; all components tightly coupled

**Evidence:**
- File: `rpi_server.py` - Single Flask application
- File: `ml/savesentra_ml.py` - Standalone ML script
- File: `SETUP.md` - Deployment instructions for single Raspberry Pi

---

## ADR-020: Manual User Registration and RFID Card Assignment

**Status:** Accepted

### Context

Multiple family members must be able to use the system. Each member needs a unique RFID card for authentication.

### Decision

Implement manual registration process: administrator adds family member name and RFID card UID to ESP32 Preferences via a setup interface.

### Consequences

**Positive:**
- Simple implementation; no complex user management system
- Offline operation; no cloud dependency for user registration
- Suitable for small, closed group (family)
- Easy to understand and operate

**Negative:**
- No self-service registration; requires administrator action
- No password recovery or account management
- RFID cards can be lost or shared; no audit trail
- Difficult to revoke access if family member leaves
- No role-based access control (all users have same permissions)

**Evidence:**
- File: `savesentra_iot.ino` (lines 250-280): Manual card registration via Preferences
- File: `SETUP.md` - Manual user registration instructions

---

## ADR-021: No Encryption for Stored Transaction Data

**Status:** Accepted (with Caveats)

### Context

Transaction data (user ID, amount, timestamp) is stored in a CSV file on the Raspberry Pi. The data is sensitive (family financial information).

### Decision

Store transaction data in plaintext CSV format. No encryption applied.

### Consequences

**Positive:**
- Simpler implementation; no encryption/decryption overhead
- Data remains human-readable for debugging
- Faster read/write operations on Raspberry Pi
- No key management complexity

**Negative:**
- Vulnerable to unauthorized access if Raspberry Pi is compromised
- Does not meet privacy standards for financial data
- Violates data protection regulations (GDPR, local privacy laws)
- No audit trail of who accessed transaction data
- Difficult to comply with data retention policies

**Evidence:**
- File: `rpi_server.py` (lines 40-50): CSV write without encryption
- File: `ml/savesentra_ml.py` (lines 30-40): CSV read without decryption

---

## ADR-022: Synchronous Request-Response Model for Deposit Transactions

**Status:** Accepted

### Context

When a user deposits cash, the ESP32 must communicate with the Raspberry Pi to persist the transaction. The user expects immediate feedback.

### Decision

Use synchronous HTTP POST requests. ESP32 waits for HTTP response before confirming deposit to user.

### Consequences

**Positive:**
- Simple, intuitive model; easy to understand
- Immediate feedback to user (success/failure)
- Ensures transaction is persisted before confirming
- No message queuing infrastructure required

**Negative:**
- Blocks ESP32 during network latency (up to several seconds)
- If Raspberry Pi is unreachable, user must wait for timeout
- Network failures directly impact user experience
- Difficult to implement retry logic without blocking
- Scalability limited by network latency

**Evidence:**
- File: `savesentra_iot.ino` (lines 450-500): Synchronous HTTP POST
  ```cpp
  HTTPClient http;
  http.begin("http://raspberrypi.local:5000/deposit");
  int httpCode = http.POST(jsonPayload);
  // Wait for response before returning
  ```

---

## ADR-023: Support Vector Regression Over Deep Learning

**Status:** Accepted

### Context

The system must predict savings goal completion date. Multiple ML approaches are possible: linear regression, tree-based models, neural networks.

### Decision

Use Support Vector Regression (SVR) as the primary model, with Random Forest and Gradient Boosting as alternatives. Do not use deep learning (neural networks).

### Consequences

**Positive:**
- SVR is computationally efficient; suitable for Raspberry Pi
- Requires less training data than deep learning
- Hyperparameters are interpretable
- Ensemble approach (three models) provides robustness
- Proven effective for time-series forecasting

**Negative:**
- SVR may underfit if data has complex non-linear patterns
- Hyperparameter tuning is manual; no automatic optimization
- Deep learning could potentially provide better accuracy
- No uncertainty quantification (confidence intervals)
- Limited ability to incorporate external features (holidays, events)

**Evidence:**
- File: `ml/savesentra_ml.py` (lines 80-130): SVR model training
  ```python
  from sklearn.svm import SVR
  svr_model = SVR(kernel='rbf', C=100, gamma='scale')
  ```

---

## ADR-024: No Authentication Between ESP32 and Raspberry Pi

**Status:** Accepted (with Caveats)

### Context

ESP32 and Raspberry Pi communicate over a local home network. The network is assumed to be trusted.

### Decision

Do not implement authentication between ESP32 and Raspberry Pi. Any device on the local network can POST to the `/deposit` endpoint.

### Consequences

**Positive:**
- Simpler implementation; no API key management
- Faster request processing (no authentication overhead)
- Suitable for trusted local network environment

**Negative:**
- Vulnerable to spoofed deposits from other devices on network
- No audit trail of which device submitted each deposit
- Difficult to debug if multiple devices are connected
- Does not meet security standards for financial systems
- Vulnerable if WiFi network is compromised

**Evidence:**
- File: `rpi_server.py` (lines 20-50): No authentication check in `/deposit` endpoint
  ```python
  @app.route('/deposit', methods=['POST'])
  def receive_deposit():
      data = request.json
      # No authentication; accepts any POST request
  ```

---

## ADR-025: Blynk HTTPS for Cloud Communication

**Status:** Accepted

### Context

ESP32 and Blynk cloud communicate over the internet. Data includes user authentication tokens and financial information.

### Decision

Use HTTPS (TLS 1.2+) for all communication with Blynk cloud. Blynk library handles certificate validation automatically.

### Consequences

**Positive:**
- Encrypts data in transit; prevents eavesdropping
- Authenticates Blynk server; prevents MITM attacks
- Industry standard for cloud communication
- Blynk library handles complexity; minimal code required

**Negative:**
- TLS handshake adds latency to initial connection
- Certificate validation requires system time to be accurate
- Blynk service availability affects system usability
- No control over Blynk's security practices

**Evidence:**
- File: `savesentra_iot.ino` (lines 100-120): Blynk initialization with HTTPS
  ```cpp
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // Blynk library uses HTTPS by default
  ```

---

## ADR-026: No Unit Tests; Manual Testing Only

**Status:** Accepted (with Caveats)

### Context

The system is a prototype/proof-of-concept for a family savings application. Development resources are limited.

### Decision

Do not implement automated unit tests. Rely on manual testing and integration testing.

### Consequences

**Positive:**
- Faster initial development; no test infrastructure required
- Suitable for prototype/proof-of-concept phase
- Reduced development overhead

**Negative:**
- No regression detection; changes may break existing functionality
- Difficult to refactor code with confidence
- Manual testing is time-consuming and error-prone
- No documentation of expected behavior via tests
- Difficult to onboard new developers

**Evidence:**
- No test files found in repository
- No test framework dependencies in code

---

## ADR-027: Blynk Authentication Token Stored in Firmware

**Status:** Accepted (with Caveats)

### Context

The ESP32 must authenticate with Blynk cloud. Authentication requires a token (similar to API key).

### Decision

Store the Blynk authentication token in the firmware source code as a compile-time constant.

### Consequences

**Positive:**
- Simple implementation; no external configuration required
- Token is always available; no runtime lookup needed

**Negative:**
- Token is exposed in source code; visible to anyone with repository access
- Token cannot be rotated without recompiling and reflashing firmware
- If token is compromised, attacker can control the device
- Does not meet security standards for production systems
- Difficult to manage multiple devices with different tokens

**Evidence:**
- File: `savesentra_iot.ino` (lines 1-50): Blynk token defined as constant
  ```cpp
  #define BLYNK_AUTH_TOKEN "your_auth_token_here"
  ```

---

## ADR-028: Stepper Motor Calibration via Manual Measurement

**Status:** Accepted

### Context

The stepper motor must intake banknotes of different denominations. Each denomination has a different physical length.

### Decision

Manually measure the length of each banknote denomination using an IR sensor. Record measurements in a CSV calibration file.

### Consequences

**Positive:**
- Accurate calibration specific to physical hardware
- Simple, repeatable process
- Calibration data is human-readable

**Negative:**
- Manual process is time-consuming and error-prone
- Requires physical access to banknotes and measurement equipment
- Calibration may drift over time due to mechanical wear
- No automatic validation that calibration is correct
- Difficult to troubleshoot if calibration is incorrect

**Evidence:**
- File: `savesentra_iot/denomination_calibration.csv` - Calibration lookup table
- File: `SETUP.md` - Calibration procedure documentation

---

## ADR-029: No Rate Limiting on Flask Server

**Status:** Accepted (with Caveats)

### Context

The Flask server receives HTTP POST requests from ESP32 devices. The server should protect against abuse or accidental overload.

### Decision

Do not implement rate limiting. Accept all POST requests without throttling.

### Consequences

**Positive:**
- Simpler implementation; no rate limiting logic required
- Suitable for single-device (single ESP32) deployment
- No false positives blocking legitimate requests

**Negative:**
- Vulnerable to denial-of-service attacks
- No protection against accidental request floods
- Difficult to diagnose if server is overloaded
- Does not scale to multiple devices

**Evidence:**
- File: `rpi_server.py` (lines 20-50): No rate limiting middleware

---

## ADR-030: CSV Append-Only for Transaction Immutability

**Status:** Accepted

### Context

Transaction data must be immutable once recorded. Users should not be able to modify or delete past transactions.

### Decision

Use CSV append-only semantics. Transactions are only appended; never modified or deleted.

### Consequences

**Positive:**
- Immutability prevents accidental or malicious data modification
- Audit trail is preserved; all transactions visible
- Simple implementation; no delete logic required
- Suitable for financial record-keeping

**Negative:**
- Difficult to correct erroneous transactions
- No support for transaction reversals or refunds
- CSV file grows indefinitely; no archival mechanism
- Difficult to implement data retention policies

**Evidence:**
- File: `rpi_server.py` (lines 40-50): CSV append operation
  ```python
  with open('transactions.csv', 'a') as f:
      writer = csv.writer(f)
      writer.writerow([timestamp, user_id, amount, denomination])
  ```

---

## ADR-031: Raspberry Pi as Local Server Hardware

**Status:** Accepted

### Context

The system requires a local server to receive deposits from ESP32 and run the ML pipeline. Hardware must be low-cost, low-power, and suitable for home deployment.

### Decision

Use Raspberry Pi 4 (or later) as the local server hardware. Raspberry Pi runs Flask server and ML pipeline.

### Consequences

**Positive:**
- Low cost (~$35-75)
- Low power consumption (~5-10W)
- Sufficient computational power for Flask and scikit-learn
- Extensive community support and documentation
- Easy to set up and configure

**Negative:**
- Limited RAM (2-8 GB) may constrain large ML models
- Single-core performance is limited; multi-core not fully utilized
- SD card storage is slower than SSD
- No built-in redundancy or failover
- Thermal throttling under sustained load

**Evidence:**
- File: `SETUP.md` - Deployment instructions for Raspberry Pi
- File: `README.md` - Hardware requirements section

---

## ADR-032: No Horizontal Scaling; Single-Machine Deployment

**Status:** Accepted

### Context

The system is designed for a single family household. Scaling to multiple households is not a design goal.

### Decision

Design for single-machine deployment. No distributed architecture, load balancing, or database replication.

### Consequences

**Positive:**
- Simpler architecture; easier to understand and maintain
- No inter-machine communication complexity
- Lower operational overhead
- Suitable for family-scale usage

**Negative:**
- Cannot scale to multiple households
- Single point of failure; no redundancy
- Difficult to migrate to multi-machine architecture later
- Resource contention between components
- No load balancing or failover capability

**Evidence:**
- File: `rpi_server.py` - Single Flask application
- File: `SETUP.md` - Single-machine deployment instructions
- No distributed architecture patterns (sharding, replication, etc.)

---

## ADR-033: No Caching Layer; Direct CSV Access

**Status:** Accepted

### Context

The ML pipeline must read transaction data from CSV to train models. Data access patterns are infrequent (once per deposit).

### Decision

Do not implement a caching layer. ML pipeline reads directly from CSV file on each execution.

### Consequences

**Positive:**
- Simpler implementation; no cache invalidation logic
- Always reads latest data; no stale data issues
- Suitable for infrequent access patterns

**Negative:**
- Full-file scan on each ML pipeline execution
- Slower performance as CSV grows
- No optimization for repeated queries
- Difficult to scale to large datasets

**Evidence:**
- File: `ml/savesentra_ml.py` (lines 30-40): Direct CSV read
  ```python
  df = pd.read_csv('transactions.csv')
  ```

---

## ADR-034: No API Versioning; Single API Version

**Status:** Accepted

### Context

The Flask server exposes a `/deposit` endpoint. The API may evolve over time as requirements change.

### Decision

Do not implement API versioning. Use a single API version. Breaking changes require firmware updates to all ESP32 devices.

### Consequences

**Positive:**
- Simpler implementation; no version routing logic
- Suitable for single-device deployment
- Easier to understand and maintain

**Negative:**
- Breaking changes require firmware updates to all devices
- Difficult to support multiple client versions
- No backward compatibility
- Difficult to roll out changes gradually

**Evidence:**
- File: `rpi_server.py` (lines 20-50): Single `/deposit` endpoint, no versioning

---

## ADR-035: No Logging Infrastructure; Print Statements Only

**Status:** Accepted (with Caveats)

### Context

The system must provide visibility into operation for debugging and troubleshooting.

### Decision

Use print statements for logging. No structured logging framework (e.g., Python logging module).

### Consequences

**Positive:**
- Simple implementation; minimal code required
- Suitable for development and debugging
- No external dependencies

**Negative:**
- Difficult to filter or search logs
- No log levels (debug, info, warning, error)
- No timestamp or context information
- Logs are not persisted; lost on process restart
- Difficult to aggregate logs from multiple components

**Evidence:**
- File: `savesentra_iot.ino` (lines 450-500): Serial.println() for logging
- File: `rpi_server.py`: print() statements for logging

---

## ADR-036: No Monitoring or Alerting

**Status:** Accepted (with Caveats)

### Context

The system must operate reliably. Failures should be detected and reported to users.

### Decision

Do not implement monitoring or alerting. System failures are not automatically detected or reported.

### Consequences

**Positive:**
- Simpler implementation; no monitoring infrastructure required
- Suitable for prototype/proof-of-concept phase

**Negative:**
- System failures go undetected until user reports them
- No visibility into system health
- Difficult to diagnose intermittent issues
- No proactive maintenance possible
- Does not meet reliability standards for production systems

**Evidence:**
- No monitoring code found in repository
- No alerting mechanisms implemented

---

## ADR-037: Blynk Virtual Pins for User Interface

**Status:** Accepted

### Context

Users need to view savings progress, transaction history, and goal completion predictions. A custom mobile app would require significant development effort.

### Decision

Use Blynk virtual pins to display data on a pre-configured Blynk mobile app. Each data type is assigned a virtual pin.

### Consequences

**Positive:**
- Eliminates need for custom mobile app development
- Blynk provides pre-built UI components (gauges, charts, tables)
- Real-time updates via Blynk cloud
- Supports multiple users/devices via Blynk RBAC

**Negative:**
- Limited UI customization; constrained by Blynk's pre-built components
- Vendor lock-in; switching platforms requires significant refactoring
- Blynk service availability affects user experience
- Data flows through third-party cloud (privacy consideration)
- Virtual pin values are limited in size

**Evidence:**
- File: `savesentra_iot.ino` (lines 100-150): Virtual pin write operations
- File: `README.md` - Blynk dashboard setup instructions

---

## ADR-038: No Backup or Disaster Recovery

**Status:** Accepted (with Caveats)

### Context

Transaction data is stored on Raspberry Pi SD card. SD cards can fail, causing data loss.

### Decision

Do not implement automated backup or disaster recovery. Manual backup is user's responsibility.

### Consequences

**Positive:**
- Simpler implementation; no backup infrastructure required
- No additional storage or bandwidth required

**Negative:**
- Data loss if Raspberry Pi SD card fails
- No recovery mechanism if data is accidentally deleted
- Does not meet reliability standards for financial systems
- Difficult to comply with data retention policies

**Evidence:**
- No backup code found in repository
- File: `SETUP.md` - Manual backup instructions (if any)

---

## ADR-039: No Encryption for Configuration Data

**Status:** Accepted (with Caveats)

### Context

Configuration data (RFID card IDs, user names, Blynk token) is stored on ESP32 in plaintext.

### Decision

Store configuration data in plaintext using ESP32 Preferences API. No encryption applied.

### Consequences

**Positive:**
- Simpler implementation; no encryption/decryption overhead
- Data remains human-readable for debugging
- Faster read/write operations

**Negative:**
- Vulnerable to unauthorized access if ESP32 is physically compromised
- Blynk token is exposed; attacker can control device
- RFID card IDs are exposed; attacker can spoof cards
- Does not meet security standards for production systems

**Evidence:**
- File: `savesentra_iot.ino` (lines 250-280): Plaintext storage in Preferences

---

## ADR-040: No Multi-Tenancy; Single Family Deployment

**Status:** Accepted

### Context

The system is designed for a single family household. Multi-tenancy (supporting multiple families) is not a design goal.

### Decision

Design for single-family deployment. No multi-tenancy support, no tenant isolation, no shared infrastructure.

### Consequences

**Positive:**
- Simpler architecture; no tenant isolation logic
- No data leakage concerns between families
- Easier to understand and maintain
- Suitable for family-scale usage

**Negative:**
- Cannot scale to multiple families
- Difficult to migrate to multi-tenant architecture later
- No economies of scale
- Difficult to support SaaS business model

**Evidence:**
- File: `rpi_server.py` - Single family's transaction ledger
- File: `savesentra_iot.ino` - Single family's RFID cards and users
- No multi-tenancy patterns (tenant IDs, data isolation, etc.)

---

## Summary of Key Architectural Decisions

### Technology Stack
- **Edge Device**: ESP32 with Arduino C++
- **Local Server**: Raspberry Pi with Python Flask
- **ML Pipeline**: Python with scikit-learn
- **Cloud Dashboard**: Blynk IoT platform
- **Data Storage**: CSV files (local) + Blynk virtual pins (cloud)

### Design Patterns
- **Three-Tier Architecture**: Edge-Local-Cloud separation
- **Append-Only Ledger**: CSV-based transaction immutability
- **Synchronous Request-Response**: HTTP POST for deposits
- **Virtual Pin Synchronization**: Blynk as state management layer

### Key Trade-Offs
- **Simplicity vs. Reliability**: No retry logic, error handling, or monitoring
- **Cost vs. Scalability**: Single-machine deployment suitable for family, not enterprise
- **Security vs. Usability**: Plaintext storage and local network communication for simplicity
- **Development Speed vs. Maintainability**: Minimal testing and logging infrastructure

### Deployment Model
- Single Raspberry Pi running Flask server and ML pipeline
- Single ESP32 edge device with RFID, stepper motor, IR sensor
- Blynk cloud for user-facing dashboard
- CSV files for local transaction persistence

---

## Recommendations for Future Improvements

1. **Add Retry Logic**: Implement exponential backoff for failed HTTP requests
2. **Implement Logging**: Use Python logging module for structured logs
3. **Add Monitoring**: Implement health checks and alerting for system failures
4. **Encrypt Sensitive Data**: Use AES encryption for stored transaction data and configuration
5. **Implement Authentication**: Add API key authentication between ESP32 and Raspberry Pi
6. **Add Unit Tests**: Implement test suite for ML pipeline and Flask server
7. **Implement Backup**: Automated daily backup of CSV ledger to cloud storage
8. **Add Rate Limiting**: Implement rate limiting on Flask server to prevent abuse
9. **Improve Error Handling**: Add comprehensive error handling and user-facing error messages
10. **Consider Database**: Migrate from CSV to SQLite or PostgreSQL for better data management

---

**Document Version**: 1.0  
**Last Updated**: 2024  
**Status**: Comprehensive Architecture Decision Record