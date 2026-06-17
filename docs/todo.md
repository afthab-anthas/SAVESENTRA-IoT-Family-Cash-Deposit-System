## Code Verification
- [x] 🔴 **TODO-CODE-081**: Document goto statement usage at [`savesentra_iot.ino:451`](savesentra_iot/savesentra_iot.ino:451) - consider refactoring to state machine
- [x] 🔴 **TODO-CODE-082**: Document static variable usage at [`savesentra_iot.ino:611`](savesentra_iot/savesentra_iot.ino:611) - explain persistence across loop iterations
- [x] ⚠️ **TODO-CODE-083**: Document string concatenation pattern at [`savesentra_iot.ino:422-425`](savesentra_iot/savesentra_iot.ino:422) - verify no buffer overflow with large UIDs
- [x] ⚠️ **TODO-CODE-084**: Document Preferences key naming convention - explain why "name0", "uid0" instead of "user0.name"
- [x] 📝 **TODO-CODE-085**: Document Python f-string usage in ML scripts - verify Python 3.6+ compatibility
- [x] 📝 **TODO-CODE-086**: Document pandas DataFrame operations - add performance notes for large datasets
- [x] 📝 **TODO-CODE-087**: Document scikit-learn model serialization - explain why models not saved to disk
- [x] 📝 **TODO-CODE-088**: Document CSV writer dialect - specify line terminator and quoting strategy

## Architecture Verification
- [x] ⚠️ **TODO-ARCH-094**: Document why SVR was chosen over other ML models (accuracy, training time, simplicity)
- [x] ⚠️ **TODO-ARCH-095**: Document why Blynk IoT was chosen for the mobile dashboard (ease of integration, real-time sync, free tier)
- [x] 📝 **TODO-ARCH-096**: Document why Raspberry Pi was chosen for the local server (cost, availability, Python support)
- [x] 📝 **TODO-ARCH-097**: Document why RFID was chosen for authentication (contactless, fast, low cost)
- [x] 📝 **TODO-ARCH-098**: Document why stepper motor + IR sensor was chosen for denomination detection (mechanical simplicity, no AI required)

## Dataflow Verification
- [x] 🔴 **TODO-STRUCT-032**: Document EEPROM storage schema — how User struct is persisted in ESP32 Preferences API with keys like `name0`, `uid0`, `bal0`, `role0`
- [x] 🔴 **TODO-STRUCT-033**: Document CSV schema in detail — verify all 4 columns (Timestamp, NFC_UID, Deposit_Amount, Total_Balance) with data types and examples
- [x] ⚠️ **TODO-STRUCT-034**: Add "Data Transformation Pipeline" section — show how raw CSV → cleaned CSV → feature-engineered CSV → ML-ready CSV
- [x] ⚠️ **TODO-STRUCT-035**: Document Blynk virtual pin data types — which pins are String, Integer, Float, and their update frequency
- [x] 📝 **TODO-STRUCT-036**: Create "Data Validation Rules" section — denomination ranges (5/10 AED), step count thresholds (4990-5080 for 5 AED, 5090-5200 for 10 AED)
- [x] 📝 **TODO-STRUCT-037**: Add "Data Retention Policy" section — how long CSV data is kept, EEPROM capacity limits (MAX_USERS = 20)
- [x] 📝 **TODO-STRUCT-038**: Document timestamp format consistency — `%Y-%m-%d %H:%M:%S` used across Arduino, Python, and CSV

