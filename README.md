# SAVESENTRA: IoT Family Cash Deposit System

SaveSentra is a smart IoT cash deposit machine built using an ESP32 and Raspberry Pi. It features secure NFC authentication, precise banknote validation, real-time cloud syncing via Blynk, and an integrated Machine Learning engine that predicts when savings goals will be reached.

## 📂 Project Structure

- **`savesentra_iot.ino`**: The ESP32 firmware. Handles NFC login, motor control for note intake, and sending deposit data to the server.
- **`rpi_server.py`**: A Python Flask server running on the Raspberry Pi. Receives deposit data and triggers the ML engine.
- **`ml/savesentra_ml.py`**: The Machine Learning pipeline. Performs data cleaning, training, and predicts goal dates using Random Forest/SVR models.
- **`ml/EDA.ipynb`**: Data visualization and Exploratory Data Analysis of the savings trends.
- **`ml/savings_dataset.csv`**: The dataset containing all recorded transaction history.

## ⚙️ Setup & Hardware

### Hardware Requirements
- Arduino Nano ESP32
- MFRC522 NFC Reader
- TCRT5000 IR Sensor
- ULN2003 Stepper Motor & Driver
- Raspberry Pi (running Flask & ML)

### Software Setup (Raspberry Pi)
1. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```
2. Run the server:
   ```bash
   python3 rpi_server.py
   ```

### Software Setup (ESP32)
1. Install `Blynk`, `MFRC522`, and `Stepper` libraries in Arduino IDE.
2. Update the `BLYNK_AUTH_TOKEN` and WiFi credentials in `savesentra_iot.ino`.
3. Upload to the ESP32.

## 🤖 Machine Learning Integration
The system automatically re-trains its model every time a new note is deposited. It calculates the average daily savings rate and simulates future earnings to provide a "Goal Reached" date estimation on the Blynk mobile dashboard.
