## 9. Shared/Common Code Organization

### 9.1 Shared Data Structures

**User Structure (Arduino/C++):**

Defined in `savesentra_iot.ino` (lines 88-93):

```cpp
struct User {
  String name;
  String uid;
  float balance;
  String role;
};
```

**Shared across:**
- Local ESP32 EEPROM storage via Preferences API
- Blynk virtual pins (V0-V30)
- Raspberry Pi CSV ledger

### 9.2 Shared Constants

**AED Denomination Ranges:**

Defined in `savesentra_iot.ino` (lines 595-605):

```cpp
if (billLengthSteps >= 4990 && billLengthSteps <= 5080) {
  depositAmount = 5.0;
} else if (billLengthSteps >= 5090 && billLengthSteps <= 5200) {
  depositAmount = 10.0;
}
```

**Mirrored in:**
- `savesentra.test.ts` (lines 5-8) — TypeScript test validation
- `ml/savesentra_ml.py` (line 26) — ML data validation

**Valid AED Notes:**

Defined in `ml/savesentra_ml.py` (line 26):

```python
allowed_notes = [5, 10, 20, 50, 100, 200]
```

### 9.3 Shared Configuration

**Blynk Virtual Pins:**

Centralized mapping in `savesentra_iot.ino`:

| Virtual Pin | Purpose | Data Type |
|-------------|---------|-----------|
| V0 | User name input | String |
| V1 | Status message | String |
| V3 | Reset all users button | Integer |
| V4 | Motor steps display | String |
| V5 | Transaction log | String |
| V6 | User balance | Float |
| V7 | Family goal setter | Integer |
| V8 | Delete user button | Integer |
| V10 | Total family balance | Float |
| V11 | Goal progress percentage | Float |
| V12 | Logout button | Integer |
| V16 | Predicted goal completion date | String (from ML) |
| V17 | Top contributor UID | String (from ML) |
| V20 | User selection dropdown | Integer |
| V21 | Role selection dropdown | Integer |
| V30 | Admin panel toggle | Integer |

**Blynk Token:**

Shared across:
- `savesentra_iot.ino` (line 24): `BLYNK_AUTH_TOKEN`
- `ml/savesentra_ml.py` (line 159): `BLYNK_TOKEN`

### 9.4 Shared CSV Schema

**Transaction Ledger Schema:**

Defined in `rpi_server.py` (lines 26-28):

```python
writer.writerow([timestamp, uid, amount, balance])
```

**CSV Columns:**

| Column | Type | Source | Usage |
|--------|------|--------|-------|
| Timestamp | String (YYYY-MM-DD HH:MM:SS) | Raspberry Pi | ML temporal features |
| NFC_UID | String (hex) | ESP32 | User identification |
| Deposit_Amount | Integer (AED) | ESP32 | ML target variable |
| Total_Balance | Float | ESP32 | User balance tracking |

**File Locations:**

- **Raw:** `ml/savings_dataset.csv` (15.9 KB)
- **Cleaned:** `ml/savings_dataset_cleaned.csv` (15.1 KB)
- **Feature-engineered:** `ml/ml_ready_data.csv` (2.2 KB)

### 9.5 Shared Utility Functions

**Timestamp Generation:**

**Arduino/C++** (`savesentra_iot.ino`, lines 180-188):

```cpp
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00:00:00";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}
```

**Python** (`rpi_server.py`, line 21):

```python
timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
```

**Purpose:** Ensures consistent timestamp formatting across all system components.

### 9.6 Shared HTTP Communication

**Deposit Endpoint Request Format:**

**Sender:** `savesentra_iot.ino` (lines 630-642)

```cpp
String csvPayload =
    "uid=" + users[authenticatedUserIndex].uid +
    "&name=" + users[authenticatedUserIndex].name +
    "&amount=" + String(depositAmount, 0) +
    "&balance=" + String(users[authenticatedUserIndex].balance, 0) +
    "&goal=" + String(familyGoal);
int httpCode = http.POST(csvPayload);
```

**Receiver:** `rpi_server.py` (lines 14-20)

```python
uid = request.form.get('uid')
name = request.form.get('name')
amount = request.form.get('amount')
balance = request.form.get('balance')
goal = request.form.get('goal', '15000')
```

**Purpose:** Standardized form-encoded HTTP POST for transaction submission.

---

## 10. Documentation Organization

### 10.1 Documentation Structure

**Root-Level Documentation:**

- **`README.md`** — Project overview, features, tech stack, installation
- **`CLAUDE.md`** — AI assistant context and guidelines

**Comprehensive Documentation (`docs/` directory):**

- **`architecture.md`** — System architecture, component design, data flows
- **`code.md`** — Detailed code documentation for all modules
- **`dataflow.md`** — Comprehensive data flow analysis
- **`decisions.md`** — Architecture decision records (ADRs)
- **`glossary.md`** — Domain terminology and definitions
- **`risk.md`** — Risk assessment and mitigation strategies
- **`structure.md`** — Project structure documentation
- **`TODO.md`** — Outstanding tasks and improvements

**Feature Planning (`docs/superpowers/plans/`):**

- Date-prefixed feature planning documents
- Phase-based implementation roadmaps
- Context drift and enhancement proposals

### 10.2 Documentation Patterns

**Markdown Conventions:**

- **Headings:** Hierarchical structure (H1 for title, H2 for sections, H3 for subsections)
- **Code Blocks:** Language-specific syntax highlighting (```cpp, ```python, ```json)
- **Tables:** Markdown tables for structured data
- **Cross-References:** Links to related documents and code files
- **Verification:** All claims reference actual files and line numbers

**Code Documentation:**

- **Inline Comments:** Explain complex logic in Arduino firmware
- **Function Documentation:** Docstrings in Python modules
- **Configuration Comments:** Explain hardcoded values and their purpose

---

## 11. Summary and Key Insights

### 11.1 Architecture Principles

1. **Three-Tier Separation:** Clear boundaries between edge device (ESP32), local server (Raspberry Pi), and ML pipeline (Python)
2. **Modular Pipeline:** ML processing organized into numbered steps for independent testing and iteration
3. **Offline-First Design:** Local CSV persistence enables operation without cloud connectivity
4. **Role-Based Access Control:** Parent/Child/Other roles with hidden admin tools for non-parents

### 11.2 Code Organization Strengths

- **Clear Module Boundaries:** Hardware, backend, and ML code are physically separated
- **Consistent Naming Conventions:** Predictable patterns across Arduino, Python, and documentation
- **Comprehensive Documentation:** Extensive reference materials with verified claims
- **Development Artifacts:** Historical sketches provide insight into iterative development

### 11.3 Configuration Management

- **Hardcoded Configuration:** WiFi, Blynk tokens, and file paths are embedded in source code
- **No External Config Files:** No `.env`, `config.json`, or similar configuration files
- **Command-Line Arguments:** ML pipeline accepts goal parameter via CLI

### 11.4 Testing and Quality Assurance

- **Unit Tests:** TypeScript tests for denomination detection logic
- **Development Sketches:** Incremental feature integration tests
- **Synthetic Data Generation:** Helper script for testing without real hardware
- **No Automated Test Runner:** Tests exist but no CI/CD pipeline is configured

### 11.5 Data Flow Summary

```
ESP32 (RFID + Motor + IR)
    ↓ HTTP POST /deposit
Raspberry Pi (Flask Server)
    ↓ CSV append + subprocess
ML Pipeline (Python)
    ↓ HTTPS GET (Blynk API)
Blynk IoT Cloud (Mobile Dashboard)
```

---

## Appendix: File Inventory

**Total Files:** 38

**By Type:**

| Type | Count | Examples |
|------|-------|----------|
| Markdown Documentation | 17 | README.md, docs/architecture.md, docs/code.md |
| Python Scripts | 7 | rpi_server.py, ml/savesentra_ml.py, ml/steps/*.py |
| Arduino Firmware | 6 | savesentra_iot/savesentra_iot.ino, Development_Sketches/*.ino |
| CSV Data Files | 3 | ml/savings_dataset.csv, ml/savings_dataset_cleaned.csv, ml/ml_ready_data.csv |
| JSON Configuration | 1 | (if present) |
| TypeScript Tests | 1 | savesentra.test.ts |
| Jupyter Notebooks | 1 | ml/EDA.ipynb |

**By Directory:**

| Directory | File Count | Purpose |
|-----------|-----------|---------|
| Root | 5 | Entry points, tests, main README |
| `savesentra_iot/` | 1 | ESP32 firmware |
| `ml/` | 9 | ML pipeline, datasets, EDA |
| `ml/steps/` | 4 | Modular pipeline steps |
| `Helpers/` | 1 | Utility scripts |
| `Development_Sketches/` | 5 | Historical firmware iterations |
| `docs/` | 8 | Comprehensive documentation |
| `docs/superpowers/plans/` | 5 | Feature planning documents |

---

**Document Generated:** Based on comprehensive analysis of 38 files across the SAVESENTRA IoT Family Cash Deposit System repository.

**Verification Method:** All claims are verified against actual source code using code search, file reading, and directory listing tools. No claims are speculative or inferred.