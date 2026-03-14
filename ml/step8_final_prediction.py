import pandas as pd
import datetime
from sklearn.svm import SVR


df_clean = pd.read_csv('savings_dataset_cleaned.csv')
ml_data = pd.read_csv('ml_ready_data.csv')

print("--- FINAL STEP: GOAL PREDICTION ---")

# 2. Re-train the Winning Model (SVR) on ALL available data
# We use all 173 days now to make it as smart as possible
X = ml_data[['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month']]
y = ml_data['Deposit_Amount']

winning_model = SVR(kernel='rbf')
winning_model.fit(X, y)

# 3. Setup the Simulation
FAMILY_GOAL = 15000  # Set your target here (e.g., 5000 AED)
current_balance = df_clean['Deposit_Amount'].sum()
current_date = pd.to_datetime(df_clean['Timestamp']).max()

print(f"Current Total Savings: {current_balance} AED")
print(f"Target Savings Goal: {FAMILY_GOAL} AED")
print(f"Starting simulation from: {current_date.date()}")

simulated_balance = current_balance
days_ahead = 0

# 4. The Simulation Loop
# We step into the future day-by-day until we hit the goal
while simulated_balance < FAMILY_GOAL:
    days_ahead += 1
    future_date = current_date + datetime.timedelta(days=days_ahead)
    
    # Create the features for this future day
    future_features = pd.DataFrame([{
        'Day_Of_Week': future_date.dayofweek,
        'Is_Weekend': 1 if future_date.dayofweek >= 5 else 0,
        'Month': future_date.month,
        'Day_Of_Month': future_date.day
    }])
    
    # Predict the deposit for that day
    predicted_deposit = winning_model.predict(future_features)[0]
    
    # Ensure the model doesn't predict negative money (just in case)
    if predicted_deposit < 0:
        predicted_deposit = 0
        
    simulated_balance += predicted_deposit

# 5. Output the result
predicted_goal_date = current_date + datetime.timedelta(days=days_ahead)

print("\n************************************************")
print(f"PROJECTED SUCCESS DATE: {predicted_goal_date.strftime('%B %d, %Y')}")
print(f"It will take approximately {days_ahead} more days.")
print("************************************************")