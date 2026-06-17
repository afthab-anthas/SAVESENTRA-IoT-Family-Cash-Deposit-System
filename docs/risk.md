# SAVESENTRA IoT Family Cash Deposit System — Comprehensive Risk Assessment

## Executive Summary

This document provides a detailed risk assessment of the SAVESENTRA IoT Family Cash Deposit System, a three-tier architecture comprising an ESP32 edge device, Raspberry Pi local server, and Python ML pipeline. The assessment identifies **15 critical and high-severity risks** spanning security, data integrity, operational resilience, and technical debt. All findings are verified against actual source code.

**Risk Distribution:**
- **Critical (4)**: Hardcoded credentials, missing input validation, unencrypted local storage, no network authentication
- **High (6)**: Subprocess injection, missing error handling, race conditions, weak denomination detection, no rate limiting, missing health checks
- **Medium (4)**: Technical debt, incomplete ML pipeline, missing logging, dependency risks
- **Low (1)**: Documentation gaps

---

## CRITICAL RISKS

### CR-001: Hardcoded WiFi Credentials and Blynk Tokens in Source Code

**Severity:** CRITICAL  
**Impact:** Complete system compromise; credentials exposed in version control and compiled firmware  
**Affected Components:** ESP32 firmware (all variants)

**Evidence:**
- `savesentra_iot/savesentra_iot.ino` (lines 23-24): `char ssid[] = "Anthas Home"; char pass[] = "althaf1109";`
- `savesentra_iot/savesentra_iot.ino` (line 22): `#define BLYNK_AUTH_TOKEN "jIf-JTtFAMlw41WrJvqtZalG0-WnrBXG"`
- Development sketches contain identical hardcoded credentials: `Development_Sketches/nfc_and_ir/nfc_and_ir.ino` (lines 20-21)
- `Development_Sketches/nfc_ir_motor_cloud/nfc_ir_motor_cloud.ino` (lines 32-33)

**Risk Description:**
All WiFi SSID, password, and Blynk authentication tokens are hardcoded as C++ string literals in the source code. These credentials are:
1. Visible in the GitHub repository (if public)
2. Embedded in compiled firmware binaries
3. Extractable via reverse engineering of the ESP32 binary
4. Identical across all device instances

An attacker with access to the repository or firmware binary can:
- Connect to the home WiFi network
- Impersonate the ESP32 device to Blynk
- Inject malicious transactions
- Modify family savings data
- Trigger unauthorized motor operations

**Mitigation Strategy:**
1. **Immediate:** Remove all credentials from source code and `.gitignore` any config files containing secrets
2. **Short-term:** Implement credential provisioning via:
   - Blynk provisioning API (Blynk supports dynamic token provisioning)
   - WiFi provisioning via BLE or AP mode (ESP32 SmartConfig)
   - Secure EEPROM storage with per-device unique credentials
3. **Long-term:** Adopt a secrets management system (e.g., AWS Secrets Manager, HashiCorp Vault) for production deployments
4. **Verification:** Audit all `.ino` files and compiled binaries to ensure no plaintext credentials remain

---

### CR-002: No Input Validation on HTTP POST Deposit Data

**Severity:** CRITICAL  
**Impact:** Arbitrary transaction injection; financial data corruption; ML model poisoning  
**Affected Components:** `rpi_server.py` (lines 8-24)

**Evidence:**
```python
@app.route('/deposit', methods=['POST'])
def deposit():
    uid = request.form.get('uid')
    name = request.form.get('name')
    amount = request.form.get('amount')
    balance = request.form.get('balance')
    goal = request.form.get('goal', '15000')
    
    # NO VALIDATION - directly appended to CSV
    with open(CSV_PATH, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, uid, amount, balance])
```

**Risk Description:**
The `/deposit` endpoint accepts POST data with zero validation:
- No type checking (amount/balance could be strings like "abc")
- No range validation (amount could be negative or 999999)
- No UID format validation
- No authentication or HMAC signature verification
- No rate limiting
- Data is directly appended to CSV without sanitization

An attacker can:
1. Send arbitrary POST requests to `http://raspberrypi.local:5000/deposit` with fake data
2. Inject negative amounts to reverse transactions
3. Inject massive amounts to corrupt the savings goal
4. Poison the ML training dataset with invalid patterns
5. Spoof any user UID to attribute false transactions

**Mitigation Strategy:**
1. **Immediate:**
   - Add input validation: `float(amount)` with try/except, range checks (0-500 AED)
   - Validate UID format (hex string, length 8-16)
   - Validate balance as non-negative float
   - Reject requests with missing fields
2. **Short-term:**
   - Implement HMAC-SHA256 request signing (ESP32 signs with shared secret)
   - Add rate limiting (e.g., max 10 deposits/minute per UID)
   - Log all rejected requests for audit
3. **Long-term:**
   - Migrate to authenticated API (OAuth2 or JWT)
   - Use database instead of CSV for ACID guarantees
   - Implement transaction idempotency keys

---

### CR-003: User Data Stored Unencrypted in ESP32 EEPROM

**Severity:** CRITICAL  
**Impact:** User balances, names, and UIDs exposed if device is physically compromised  
**Affected Components:** `savesentra_iot/savesentra_iot.ino` (lines 65-80)

**Evidence:**
```cpp
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
}
```

**Risk Description:**
The ESP32 Preferences API stores all user data (names, RFID UIDs, balances, roles) in plaintext in the device's EEPROM. The Preferences namespace "users" is unencrypted. An attacker with physical access to the ESP32 can:
1. Extract the EEPROM contents via JTAG/SPI interface
2. Read all user balances and RFID UIDs
3. Modify balances directly in EEPROM
4. Clone RFID cards using extracted UIDs
5. Impersonate any user

**Mitigation Strategy:**
1. **Immediate:**
   - Enable ESP32 Preferences encryption: `preferences.begin("users", false, "NVRAM", 0, true)` (last parameter enables encryption)
   - Use a strong, unique encryption key derived from device MAC address or secure element
2. **Short-term:**
   - Implement EEPROM wear-leveling and backup
   - Add integrity checks (HMAC) to detect tampering
   - Log all EEPROM access attempts
3. **Long-term:**
   - Migrate to Secure Enclave (if available on ESP32-S3)
   - Store only RFID UID hashes, not plaintext UIDs
   - Implement secure key derivation (PBKDF2)

---

### CR-004: No Authentication Between ESP32 and Raspberry Pi

**Severity:** CRITICAL  
**Impact:** Man-in-the-middle attacks; unauthorized transaction injection; ML pipeline poisoning  
**Affected Components:** `savesentra_iot/savesentra_iot.ino` (lines 545-560), `rpi_server.py` (lines 1-24)

**Evidence:**
```cpp
// ESP32 sends unencrypted HTTP POST with no auth
HTTPClient http;
http.begin(rpiServer);  // "http://raspberrypi.local:5000/deposit"
http.addHeader("Content-Type", "application/x-www-form-urlencoded");
String csvPayload = "uid=" + users[authenticatedUserIndex].uid + 
                    "&name=" + users[authenticatedUserIndex].name + 
                    "&amount=" + String(depositAmount, 0) + 
                    "&balance=" + String(users[authenticatedUserIndex].balance, 0) + 
                    "&goal=" + String(familyGoal);
int httpCode = http.POST(csvPayload);
```

**Risk Description:**
The ESP32 sends deposit data to the Raspberry Pi over unencrypted HTTP with no authentication:
- No TLS/HTTPS encryption
- No API key or token in request headers
- No request signing (HMAC)
- No mutual authentication
- Network traffic is plaintext and interceptable

An attacker on the local WiFi network can:
1. Intercept HTTP POST requests and read all transaction data
2. Replay captured requests to inject duplicate transactions
3. Modify request body in-flight to change amounts or user IDs
4. Inject fake requests to the `/deposit` endpoint
5. Perform man-in-the-middle attacks

**Mitigation Strategy:**
1. **Immediate:**
   - Switch to HTTPS with self-signed certificate on Raspberry Pi
   - Add API key authentication: `Authorization: Bearer <api-key>` header
   - Implement HMAC-SHA256 request signing (body + timestamp)
2. **Short-term:**
   - Use mutual TLS (mTLS) with certificate pinning on ESP32
   - Implement request nonce to prevent replay attacks
   - Add timestamp validation (reject requests older than 5 minutes)
3. **Long-term:**
   - Migrate to OAuth2 or JWT-based authentication
   - Implement rate limiting per ESP32 device ID
   - Add audit logging of all requests

---

## HIGH-SEVERITY RISKS

### HR-001: Subprocess Command Injection in ML Pipeline

**Severity:** HIGH  
**Impact:** Remote code execution on Raspberry Pi; data exfiltration; system compromise  
**Affected Components:** `rpi_server.py` (lines 28-35)

**Evidence:**
```python
@app.route('/deposit', methods=['POST'])
def deposit():
    # ... no validation ...
    try:
        ml_script = "/home/admin/Desktop/ml/savesentra_ml.py"
        result = subprocess.run([
            "python3", ml_script, str(goal)
        ], capture_output=True, text=True, cwd="/home/admin/Desktop")
    except:
        print("ML failed but deposit was saved.")
```

**Risk Description:**
The `goal` parameter from the HTTP POST is passed directly to `subprocess.run()` as a command-line argument. While the current code uses a list (safer than shell=True), the `goal` parameter is not validated:
- Could be a non-numeric string
- Could contain shell metacharacters if passed differently
- No bounds checking on the integer value
- Exception handling silently swallows errors

If the ML script is modified to use `shell=True` or if the argument is used in an f-string, command injection becomes possible.

**Mitigation Strategy:**
1. **Immediate:**
   - Validate `goal` is a positive integer: `int(goal)` with try/except
   - Reject if goal < 0 or goal > 1000000
   - Keep `subprocess.run()` with list arguments (never use `shell=True`)
2. **Short-term:**
   - Implement proper exception handling with logging
   - Add timeout to subprocess call (e.g., `timeout=300`)
   - Log all subprocess invocations for audit
3. **Long-term:**
   - Migrate ML pipeline to async job queue (Celery + Redis)
   - Implement sandboxing for subprocess execution

---

### HR-002: Missing Error Handling and Silent Failures

**Severity:** HIGH  
**Impact:** Data loss; undetected failures; difficult debugging  
**Affected Components:** `rpi_server.py` (lines 28-35), `ml/savesentra_ml.py` (lines 1-172)

**Evidence:**
```python
# rpi_server.py - bare except clause
try:
    ml_script = "/home/admin/Desktop/ml/savesentra_ml.py"
    result = subprocess.run([...], capture_output=True, text=True, cwd="/home/admin/Desktop")
    print("ML Done. Output:")
    print(result.stdout) 
except:
    print("ML failed but deposit was saved.")  # Silent failure!
    
# ml/savesentra_ml.py - no error handling for file I/O
df = pd.read_csv('savings_dataset.csv')  # What if file doesn't exist?
df.to_csv('savings_dataset_cleaned.csv', index=False)  # What if write fails?
```

**Risk Description:**
1. Bare `except:` clause catches all exceptions including KeyboardInterrupt and SystemExit
2. ML pipeline failures are silently ignored; deposits are saved but predictions never update
3. No logging of errors; difficult to debug in production
4. File I/O operations have no error handling (missing files, permission errors, disk full)
5. HTTP requests have no timeout or retry logic

**Mitigation Strategy:**
1. **Immediate:**
   - Replace bare `except:` with specific exception types: `except (subprocess.CalledProcessError, FileNotFoundError, IOError) as e:`
   - Add proper logging: `logger.error(f"ML pipeline failed: {e}", exc_info=True)`
   - Return HTTP 500 if ML fails (don't silently ignore)
2. **Short-term:**
   - Add timeout to subprocess calls
   - Implement retry logic with exponential backoff
   - Add health check endpoint to verify ML pipeline status
3. **Long-term:**
   - Implement structured logging (JSON format)
   - Add distributed tracing for request flow
   - Implement alerting for pipeline failures

---

### HR-003: Race Condition in User Balance Updates

**Severity:** HIGH  
**Impact:** Lost updates; inconsistent balances; financial data corruption  
**Affected Components:** `savesentra_iot/savesentra_iot.ino` (lines 520-540)

**Evidence:**
```cpp
// No locking mechanism - concurrent access possible
void saveUserBalance(int index) {
  preferences.begin("users", false);
  preferences.putFloat(("bal" + String(index)).c_str(), users[index].balance);
  preferences.end();
}

// In loop() - multiple deposits could occur concurrently
if (depositInProgress && timeSinceClear > (motorStopDelay + 3000)) {
    users[authenticatedUserIndex].balance += depositAmount;  // Read-modify-write
    saveUserBalance(authenticatedUserIndex);  // Write to EEPROM
    // ... Blynk updates ...
}
```

**Risk Description:**
The ESP32 firmware has no synchronization primitives (mutexes, semaphores) protecting shared state:
1. `users[index].balance` is read, modified, and written without atomic operations
2. Multiple RFID cards could be detected simultaneously
3. Blynk virtual pin writes could trigger concurrent updates
4. EEPROM writes are not atomic; power loss during write corrupts data

If two deposits occur within the same loop iteration:
- Both read the same balance value
- Both increment independently
- One write overwrites the other (lost update)

**Mitigation Strategy:**
1. **Immediate:**
   - Implement a simple state machine to ensure only one deposit processes at a time
   - Add `depositInProgress` flag to prevent concurrent deposits
   - Use atomic operations for balance updates (if available)
2. **Short-term:**
   - Implement EEPROM transaction logging (write-ahead log)
   - Add CRC32 checksums to detect corruption
   - Implement recovery mechanism on startup
3. **Long-term:**
   - Migrate to database (SQLite on Raspberry Pi)
   - Implement proper transaction semantics
   - Add distributed locking for multi-device scenarios

---

### HR-004: Weak Denomination Detection Logic

**Severity:** HIGH  
**Impact:** Incorrect transaction amounts; financial data corruption; user distrust  
**Affected Components:** `savesentra_iot/savesentra_iot.ino` (lines 525-535)

**Evidence:**
```cpp
// Denomination detection based solely on stepper motor step count
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

**Risk Description:**
The system detects banknote denomination by counting stepper motor steps while an IR sensor is blocked. This approach has multiple weaknesses:
1. **No calibration:** Step counts are hardcoded; no per-device calibration
2. **No validation:** Only detects 5 and 10 AED notes; rejects all others (20, 50, 100, 200 AED)
3. **Mechanical drift:** Motor speed varies with voltage, temperature, wear
4. **Sensor noise:** IR sensor may have false triggers or missed detections
5. **No redundancy:** Single sensor failure causes complete system failure
6. **No error recovery:** Invalid notes are rejected without retry or manual override

Real-world scenarios:
- Worn banknotes may have different dimensions
- Folded notes trigger incorrect detection
- Motor speed variations cause step count variance
- User inserts note at wrong angle

**Mitigation Strategy:**
1. **Immediate:**
   - Add tolerance margins to step count ranges (±10%)
   - Implement multi-sample averaging (take 3 measurements, use median)
   - Add manual override button for invalid notes
2. **Short-term:**
   - Implement per-device calibration routine (user runs on startup)
   - Add weight sensor as secondary validation
   - Implement retry logic (up to 3 attempts)
3. **Long-term:**
   - Add computer vision (camera) for note recognition
   - Implement machine learning model for denomination detection
   - Add support for all AED denominations (5, 10, 20, 50, 100, 200)

---

### HR-005: No Rate Limiting on HTTP Endpoints

**Severity:** HIGH  
**Impact:** Denial of service; resource exhaustion; ML pipeline overload  
**Affected Components:** `rpi_server.py` (lines 8-24, 36-42)

**Evidence:**
```python
@app.route('/deposit', methods=['POST'])
def deposit():
    # NO RATE LIMITING - any client can send unlimited requests
    # NO AUTHENTICATION - any client can call this endpoint
    # ...
    
@app.route('/predict', methods=['POST'])
def predict():
    # NO RATE LIMITING - any client can trigger ML pipeline repeatedly
    # ...
```

**Risk Description:**
The Flask server has no rate limiting on any endpoints:
1. `/deposit` endpoint can be called unlimited times per second
2. `/predict` endpoint can trigger the ML pipeline repeatedly
3. No per-IP or per-user rate limiting
4. No request throttling or backpressure
5. ML pipeline subprocess can be spawned unlimited times

Attack scenarios:
- Attacker sends 1000 POST requests/second to `/deposit` endpoint
- Raspberry Pi disk fills with CSV entries
- ML pipeline spawns 100 concurrent Python processes
- System runs out of memory and crashes
- Legitimate users cannot access the system

**Mitigation Strategy:**
1. **Immediate:**
   - Add Flask-Limiter: `@limiter.limit("10/minute")` on `/deposit` endpoint
   - Add `@limiter.limit("1/minute")` on `/predict` endpoint
   - Implement per-IP rate limiting
2. **Short-term:**
   - Add request queue with max size
   - Implement backpressure (return 429 Too Many Requests)
   - Add monitoring for request rate and queue depth
3. **Long-term:**
   - Migrate to async framework (FastAPI)
   - Implement distributed rate limiting (Redis)
   - Add API key-based rate limiting tiers

---

### HR-006: No Health Checks or Monitoring

**Severity:** HIGH  
**Impact:** Silent failures; undetected system degradation; difficult troubleshooting  
**Affected Components:** `rpi_server.py`, `ml/savesentra_ml.py`, `savesentra_iot/savesentra_iot.ino`

**Evidence:**
No health check endpoints exist in the codebase. The system has:
- No `/health` endpoint
- No heartbeat mechanism
- No uptime monitoring
- No error alerting
- No performance metrics
- No disk space monitoring
- No database connection health checks

**Risk Description:**
Without health checks, failures go undetected:
1. Raspberry Pi server crashes silently; ESP32 continues sending requests to dead endpoint
2. ML pipeline fails; predictions never update but no alert is triggered
3. CSV file grows unbounded; disk fills up without warning
4. Blynk connection drops; users see stale data
5. RFID reader fails; system appears frozen

**Mitigation Strategy:**
1. **Immediate:**
   - Add `/health` endpoint to Flask server: returns `{"status": "ok", "timestamp": "..."}` with 200 status
   - Add basic health checks: file system writable, CSV file accessible
2. **Short-term:**
   - Add Prometheus metrics endpoint (`/metrics`)
   - Implement uptime monitoring (e.g., Uptime Robot)
   - Add email alerts for critical failures
3. **Long-term:**
   - Implement distributed tracing (Jaeger)
   - Add centralized logging (ELK stack)
   - Implement SLA monitoring and dashboards

---

## MEDIUM-SEVERITY RISKS

### MR-001: Technical Debt in ML Pipeline

**Severity:** MEDIUM  
**Impact:** Difficult maintenance; reduced model accuracy; scalability issues  
**Affected Components:** `ml/savesentra_ml.py` (lines 1-172)

**Evidence:**
```python
# Hardcoded file paths
df = pd.read_csv('savings_dataset.csv')
df.to_csv('savings_dataset_cleaned.csv', index=False)
ml_ready_data.to_csv('ml_ready_data.csv', index=False)

# Hardcoded model parameters
models = {
    "Random Forest": RandomForestRegressor(n_estimators=100, random_state=42),
    "Gradient Boosting": GradientBoostingRegressor(random_state=42),
    "SVR": SVR(kernel='rbf')
}

# Hardcoded goal value
FAMILY_GOAL = 15000  # Default value
if len(sys.argv) > 1:
    FAMILY_GOAL = int(sys.argv[1])

# Hardcoded Blynk token
BLYNK_TOKEN = "jIf-JTtFAMlw41WrJvqtZalG0-WnrBXG"
```

**Risk Description:**
The ML pipeline has significant technical debt:
1. Hardcoded file paths; no configuration management
2. Hardcoded model hyperparameters; no tuning or cross-validation
3. No model versioning or persistence
4. No feature scaling or normalization
5. No handling of missing data or outliers
6. No train/test split validation
7. Hardcoded Blynk token (security issue)
8. No logging or error handling
9. No unit tests
10. No documentation

**Mitigation Strategy:**
1. **Immediate:**
   - Extract hardcoded values to config file (YAML/JSON)
   - Add logging to ML pipeline
   - Add basic error handling
2. **Short-term:**
   - Implement model versioning (save trained models to disk)
   - Add hyperparameter tuning (GridSearchCV)
   - Implement cross-validation
3. **Long-term:**
   - Migrate to MLflow for model tracking
   - Implement automated retraining pipeline
   - Add unit tests for data processing steps

---

### MR-002: Incomplete ML Pipeline Implementation

**Severity:** MEDIUM  
**Impact:** Inaccurate predictions; limited model selection; poor generalization  
**Affected Components:** `ml/savesentra_ml.py` (lines 1-172)

**Evidence:**
```python
# Only 4 features used
selected_columns = ['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month', 'Deposit_Amount']
ml_ready_data = daily_df[selected_columns]

# No feature scaling
X = ml_ready_data[['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month']]
y = ml_ready_data['Deposit_Amount']

# No handling of imbalanced data
# No outlier detection
# No feature importance analysis
# No model interpretability

# Only 3 models tested
models = {
    "Random Forest": RandomForestRegressor(n_estimators=100, random_state=42),
    "Gradient Boosting": GradientBoostingRegressor(random_state=42),
    "SVR": SVR(kernel='rbf')
}
```

**Risk Description:**
The ML pipeline is incomplete:
1. Only 4 temporal features; missing user-specific features (user ID, deposit frequency)
2. No feature scaling; SVR may be biased toward larger values
3. No outlier detection; single anomalous deposit skews predictions
4. No class imbalance handling
5. No feature importance analysis
6. No model interpretability (SHAP values)
7. Limited model selection (only 3 models)
8. No ensemble methods

**Mitigation Strategy:**
1. **Immediate:**
   - Add feature scaling (StandardScaler)
   - Add outlier detection (IQR method)
   - Add more models (XGBoost, LightGBM)
2. **Short-term:**
   - Add user-specific features (user ID, historical deposit count)
   - Implement feature importance analysis
   - Add model interpretability (SHAP)
3. **Long-term:**
   - Implement automated feature engineering
   - Add ensemble methods (stacking, voting)
   - Implement online learning for model updates

---

### MR-003: Missing Logging and Audit Trail

**Severity:** MEDIUM  
**Impact:** Difficult debugging; no audit trail; compliance issues  
**Affected Components:** All components

**Evidence:**
- `savesentra_iot/savesentra_iot.ino`: Uses `Serial.println()` for logging; no persistent log
- `rpi_server.py`: Uses `print()` statements; no structured logging
- `ml/savesentra_ml.py`: Uses `print()` statements; no logging framework
- No transaction audit log
- No access control audit log
- No error log

**Risk Description:**
The system has no centralized logging:
1. Logs are printed to console; lost on restart
2. No structured logging (JSON format)
3. No log levels (DEBUG, INFO, WARNING, ERROR)
4. No log rotation or retention
5. No audit trail for financial transactions
6. No access control audit log
7. Difficult to debug issues in production

**Mitigation Strategy:**
1. **Immediate:**
   - Add Python logging module to Flask server and ML pipeline
   - Add structured logging (JSON format)
   - Log all HTTP requests and responses
2. **Short-term:**
   - Implement log rotation (e.g., RotatingFileHandler)
   - Add centralized logging (syslog or ELK stack)
   - Log all financial transactions to audit table
3. **Long-term:**
   - Implement distributed tracing
   - Add log aggregation and analysis
   - Implement compliance reporting

---

### MR-004: Dependency Risks and Outdated Packages

**Severity:** MEDIUM  
**Impact:** Security vulnerabilities; compatibility issues; maintenance burden  
**Affected Components:** `ml/savesentra_ml.py`, `rpi_server.py`

**Evidence:**
No `requirements.txt` or `setup.py` file exists. Dependencies are implicit:
- Flask (version unspecified)
- Pandas (version unspecified)
- Scikit-learn (version unspecified)
- Requests (version unspecified)

**Risk Description:**
1. No dependency pinning; different installations may use different versions
2. No security vulnerability scanning
3. No dependency update strategy
4. Potential compatibility issues between versions
5. No lock file (pip.lock or poetry.lock)
6. Difficult to reproduce environment

**Mitigation Strategy:**
1. **Immediate:**
   - Create `requirements.txt` with pinned versions: `pip freeze > requirements.txt`
   - Add `.python-version` file for Python version management
2. **Short-term:**
   - Implement dependency scanning (e.g., Safety, Snyk)
   - Set up automated dependency updates (Dependabot)
   - Add CI/CD pipeline to test dependency updates
3. **Long-term:**
   - Migrate to Poetry for dependency management
   - Implement security scanning in CI/CD
   - Add automated security patching

---

## LOW-SEVERITY RISKS

### LR-001: Documentation Gaps and Missing Comments

**Severity:** LOW  
**Impact:** Difficult onboarding; maintenance burden; knowledge loss  
**Affected Components:** All components

**Evidence:**
- No API documentation for HTTP endpoints
- No configuration guide
- No deployment instructions
- No troubleshooting guide
- Minimal inline code comments
- No architecture diagram in code

**Risk Description:**
The codebase lacks documentation:
1. No README for setup instructions
2. No API specification (OpenAPI/Swagger)
3. No deployment guide for Raspberry Pi
4. No troubleshooting guide
5. No architecture documentation in code

**Mitigation Strategy:**
1. **Immediate:**
   - Add README.md with setup instructions
   - Add inline comments for complex logic
2. **Short-term:**
   - Create API documentation (OpenAPI spec)
   - Add deployment guide
   - Add troubleshooting guide
3. **Long-term:**
   - Implement automated documentation generation
   - Add architecture diagrams
   - Create video tutorials

---

## SUMMARY TABLE

| Risk ID | Title | Severity | Impact | Mitigation Priority |
|---------|-------|----------|--------|-------------------|
| CR-001 | Hardcoded Credentials | CRITICAL | Complete compromise | P0 |
| CR-002 | No Input Validation | CRITICAL | Data corruption | P0 |
| CR-003 | Unencrypted Storage | CRITICAL | Physical compromise | P0 |
| CR-004 | No Network Auth | CRITICAL | MITM attacks | P0 |
| HR-001 | Command Injection | HIGH | RCE | P1 |
| HR-002 | Missing Error Handling | HIGH | Silent failures | P1 |
| HR-003 | Race Conditions | HIGH | Lost updates | P1 |
| HR-004 | Weak Detection | HIGH | Wrong amounts | P1 |
| HR-005 | No Rate Limiting | HIGH | DoS | P1 |
| HR-006 | No Health Checks | HIGH | Undetected failures | P1 |
| MR-001 | Technical Debt | MEDIUM | Maintenance burden | P2 |
| MR-002 | Incomplete ML | MEDIUM | Poor predictions | P2 |
| MR-003 | Missing Logging | MEDIUM | No audit trail | P2 |
| MR-004 | Dependency Risks | MEDIUM | Vulnerabilities | P2 |
| LR-001 | Documentation Gaps | LOW | Difficult onboarding | P3 |

---

## RECOMMENDED IMPLEMENTATION ROADMAP

### Phase 1: Critical Security (Weeks 1-2)
1. Remove hardcoded credentials; implement provisioning
2. Add input validation to `/deposit` endpoint
3. Enable EEPROM encryption on ESP32
4. Implement HTTPS + API key authentication

### Phase 2: Data Integrity (Weeks 3-4)
1. Fix race conditions with state machine
2. Add error handling and logging
3. Implement rate limiting
4. Add health check endpoints

### Phase 3: Operational Resilience (Weeks 5-6)
1. Improve denomination detection
2. Add comprehensive logging
3. Implement monitoring and alerting
4. Create deployment guide

### Phase 4: Technical Excellence (Weeks 7+)
1. Refactor ML pipeline
2. Add unit tests
3. Implement CI/CD pipeline
4. Add documentation

---

## CONCLUSION

The SAVESENTRA system has **4 critical security vulnerabilities** that require immediate remediation before production deployment. The most urgent issues are hardcoded credentials, missing input validation, and lack of network authentication. All identified risks have concrete mitigation strategies and can be addressed through systematic implementation of the recommended roadmap.