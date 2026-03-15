from flask import Flask, request
from datetime import datetime
import csv
import os
import subprocess

app = Flask(__name__)


CSV_PATH = "/home/admin/Desktop/savings_dataset.csv"

@app.route('/deposit', methods=['POST'])
def deposit():
    uid = request.form.get('uid')
    name = request.form.get('name')
    amount = request.form.get('amount')
    balance = request.form.get('balance')
    goal = request.form.get('goal', '15000')


    print(f"--- GOT DEPOSIT ---")
    print(f"User: {name}, Amount: {amount}")

    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Append to CSV
    with open(CSV_PATH, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, uid, amount, balance])

    # Trigger ML 
    print("Starting the ML script now...")
    try:
        ml_script = "/home/admin/Desktop/ml/savesentra_ml.py"
        
        # Running ML and capturing output
        result = subprocess.run(["python3", ml_script, str(goal)], capture_output=True, text=True, cwd="/home/admin/Desktop")
        
        print("ML Done. Output:")
        print(result.stdout) 
    except:
        print("ML failed but deposit was saved.")
        
    return "OK"

@app.route('/predict', methods=['POST'])
def predict():
    # Get goal from ESP32
    the_goal = request.form.get('goal')
    print(f"Updating prediction for goal: {the_goal}")
    
    ml_script = "/home/admin/Desktop/ml/savesentra_ml.py"
    # run ml again
    subprocess.run(["python3", ml_script, str(the_goal)], cwd="/home/admin/Desktop")
    
    return "OK"


@app.route('/')
def index():
    return "Server is up!"

if __name__ == '__main__':
    print("Server starting on port 5000...")
    app.run(host='0.0.0.0', port=5000)