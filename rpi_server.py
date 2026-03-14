from flask import Flask, request
from datetime import datetime
import csv
import os

app = Flask(__name__)

# Path to the CSV file on Raspberry Pi
CSV_PATH = os.path.expanduser("~/Desktop/savings_dataset.csv")

@app.route('/deposit', methods=['POST'])
def deposit():
    uid = request.form.get('uid', '')
    name = request.form.get('name', '')
    amount = request.form.get('amount', '')
    balance = request.form.get('balance', '')

    # Get current timestamp
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Append to CSV
    with open(CSV_PATH, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, uid, int(float(amount)), int(float(balance))])

    print(f"[+] {timestamp} | {name} ({uid}) deposited {amount} AED | Balance: {balance} AED")
    return "OK", 200

@app.route('/health', methods=['GET'])
def health():
    return "SaveSentra RPi Server Running", 200

if __name__ == '__main__':
    print(f"CSV path: {CSV_PATH}")
    print("Starting SaveSentra RPi Server on port 5000...")
    app.run(host='0.0.0.0', port=5000)
