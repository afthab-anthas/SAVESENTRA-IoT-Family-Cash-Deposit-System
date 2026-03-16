# SaveSentra 🏦🤖

**An IoT-based Family Cash Deposit System with Machine Learning Predictive Analytics.**

SaveSentra modernizes the traditional piggy bank by digitizing physical cash deposits. It uses an ESP32 edge device to securely accept UAE Dirham (AED) banknotes, instantly sync balances to a mobile dashboard (Blynk), and forward transaction data to a local Raspberry Pi. The Raspberry Pi then runs a Machine Learning pipeline to predict exactly when the family will reach their collective savings goal.

---

## ✨ Features

* **RFID Authentication:** Secure, individual user logins via MFRC522 tags.
* **Smart Intake Mechanism:** Physically measures banknote denomination by counting Stepper Motor steps while a TCRT5000 IR sensor is blocked.
* **Real-time Mobile Dashboard:** Hosted on Blynk IoT, featuring individual/family balances, transaction logs, and live ML predictions.
* **Role-Based Access Control (RBAC):** Parents have hidden administrative tools to register children, delete users, and adjust family goals directly from the app.
* **Local Data Sovereignty:** Transactions are securely logged to a CSV dataset acting as a local ledger on a Raspberry Pi.
* **Machine Learning Forecasting:** An automated 8-step ML pipeline (utilizing Support Vector Regression) trains on chronological deposit behaviors to forecast the goal completion date and the top contributor.

---

## 🛠️ Tech Stack

**Hardware:**
* Arduino Nano ESP32
* MFRC522 RFID Reader
* 28BYJ-48 Stepper Motor + ULN2003 Driver
* TCRT5000 Infrared Sensor

**Software / Backend:**
* **C++** (Arduino IDE) for embedded hardware logic
* **Blynk IoT** for the mobile user interface
* **Python (Flask)** for the local Raspberry Pi server
* **Pandas & Scikit-learn (SVR)** for Data Science and Machine Learning
* **Jupyter Notebook** for Exploratory Data Analysis (EDA)

---

## 🏗️ System Architecture

1. **Edge Device (ESP32):** Reads RFID, controls intake motor, calculates denomination (5/10 AED), and updates Blynk.
2. **Local Server (Raspberry Pi):** Receives HTTP POST from ESP32, writes to `savings_dataset.csv`, and triggers ML script via `subprocess`.
3. **ML Pipeline (`savesentra_ml.py`):** Cleans dataset, engineers temporal features (`Is_Weekend`, `Month`), trains an SVR model, and pushes predicted target dates directly to the Blynk cloud via HTTPS GET.

---

## 🚀 Installation & Setup

### 1. Hardware Firmware (ESP32)
1. Open the `savesentra_iot` folder in Arduino IDE.
2. Install necessary libraries: `MFRC522`, `BlynkSimpleEsp32`.
3. Update your `ssid`, `pass`, and `BLYNK_AUTH_TOKEN` in the `.ino` sketch.
4. Upload to your Arduino Nano ESP32.

### 2. Backend Server (Raspberry Pi)
1. Clone this repository onto your home Raspberry Pi.
2. Install the required Python packages:
   ```bash
   pip3 install flask pandas scikit-learn requests
   ```
3. Run the Flask listener:
   ```bash
   python3 rpi_server.py
   ```
   *(Ensure the IP address in your ESP32 sketch matches your Pi's static local IP).*

### 3. Blynk Mobile App
1. Create a new template on Blynk Cloud.
2. Set up Datastreams (e.g., `V6` for Balance, `V10` for Total, `V16` for Predicted Date).
3. Connect the app, and you are ready to log in with an RFID card!

---

## 📈 Machine Learning Highlights

The predictive capability is driven by an SVR (Support Vector Regression) model, which outperformed Random Forest and Gradient Boosting during historical dataset validation. It natively accounts for non-linear, cyclical human behavior (e.g., higher deposits on weekends or early in the month).

> **Note:** Currently calibrated exclusively for UAE Dirhams (AED), using strict mathematical thresholds protecting against anomalous or illegitimate banknote insertions. 

---
*Created by Afthab.*
