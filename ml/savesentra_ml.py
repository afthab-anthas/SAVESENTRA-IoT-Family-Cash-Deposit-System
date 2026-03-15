import pandas as pd
import datetime
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
from sklearn.svm import SVR
from sklearn.metrics import mean_absolute_error


print("   SAVESENTRA: FULL ML PIPELINE SCRIPT    ")

# CLEAN DATA
# load the raw dataset
df = pd.read_csv('savings_dataset.csv')

print("Dataset size (rows, cols):", df.shape)
print("\nChecking for missing null values...")
print(df.isnull().sum())

# removing duplicates
dupes = df.duplicated().sum()
print(f"\nFound {dupes} duplicate rows.")
if dupes > 0:
    df = df.drop_duplicates()
    print("Dropped the duplicates!")

# fixing the timestamp because pandas loads it as a string by default
df['Timestamp'] = pd.to_datetime(df['Timestamp'])

# data cleaning for my particular data
# checking if the motor logged any impossible note amounts (only UAE dirhams allowed)
allowed_notes = [5, 10, 20, 50, 100, 200]
weird_notes = df[~df['Deposit_Amount'].isin(allowed_notes)]

print(f"\nInvalid notes detected: {len(weird_notes)}")
if len(weird_notes) > 0:
    print("Here are the invalid ones:")
    print(weird_notes)

print(f"Users registered in the system: {df['NFC_UID'].unique()}")
print(f"Total valid transactions left: {len(df)}")

# export to a new csv just to have a backup of the clean data
df.to_csv('savings_dataset_cleaned.csv', index=False)



# FEATURE ENGINEERING

# Group by Day
# We need to know how much was saved per day.
df['Date'] = df['Timestamp'].dt.date
daily_df = df.groupby('Date')['Deposit_Amount'].sum().reset_index()
daily_df['Date'] = pd.to_datetime(daily_df['Date'])

# Changing the date into numbers 
daily_df['Day_Of_Week'] = daily_df['Date'].dt.dayofweek  # 0=Monday, 6=Sunday
daily_df['Is_Weekend'] = daily_df['Day_Of_Week'].apply(lambda x: 1 if x >= 5 else 0)
daily_df['Month'] = daily_df['Date'].dt.month
daily_df['Day_Of_Month'] = daily_df['Date'].dt.day

# FEATURE SELECTION
# We only keep our engineered numerical hints (X) and the total deposit amount (Y)
selected_columns = ['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month', 'Deposit_Amount']
ml_ready_data = daily_df[selected_columns]

# saving this too just to keep track
ml_ready_data.to_csv('ml_ready_data.csv', index=False)



# MODEL TRAINING AND SHOOTOUT


# Define Inputs (X) and Target (y) using the data we just engineered
X = ml_ready_data[['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month']]
y = ml_ready_data['Deposit_Amount']

# Split the data 80% for training, 20% for testing
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
print(f"Training on {len(X_train)} days, testing on {len(X_test)} days.")

# Use Sklearn to initialize models
models = {
    "Random Forest": RandomForestRegressor(n_estimators=100, random_state=42),
    "Gradient Boosting": GradientBoostingRegressor(random_state=42),
    "SVR": SVR(kernel='rbf')
}

# Variables to track the best model
lowest_error = float('inf')
winning_model_name = ""
winning_model = None

# Compare Results
print("\n--- Model Performance Shootout ---")
for name, model in models.items():
    model.fit(X_train, y_train)
    predictions = model.predict(X_test)
    error = mean_absolute_error(y_test, predictions)
    print(f"{name} Error: {error:.2f} AED")
    
    if error < lowest_error:
        lowest_error = error
        winning_model_name = name
        winning_model = model

# FINAL GOAL PREDICTION

print("\n--- Running Final Goal Prediction ---")

# We retrain the chosen model on all the data
print(f"Training final {winning_model_name} model on all data...")
final_model = winning_model
final_model.fit(X, y)

# Setup the Simulation
FAMILY_GOAL = 15000  # Change this target if needed
current_balance = df['Deposit_Amount'].sum()
current_date = df['Timestamp'].max()

print(f"Current Balance: {current_balance} AED")
print(f"Goal: {FAMILY_GOAL} AED")

simulated_balance = current_balance
days_ahead = 0

# Predicting day by day into the future
while simulated_balance < FAMILY_GOAL:
    days_ahead += 1
    future_date = current_date + datetime.timedelta(days=days_ahead)
    
    # Make the features for tomorrow, next day, etc
    future_features = pd.DataFrame([{
        'Day_Of_Week': future_date.dayofweek,
        'Is_Weekend': 1 if future_date.dayofweek >= 5 else 0,
        'Month': future_date.month,
        'Day_Of_Month': future_date.day
    }])
    
    predicted_deposit = final_model.predict(future_features)[0]
    
    # We can't deposit negative money
    if predicted_deposit > 0:
        simulated_balance += predicted_deposit
        
    # Max Days to run for
    if days_ahead > 2000:
        print("Taking too long, simulation stopped.")
        break

predicted_goal_date = current_date + datetime.timedelta(days=days_ahead)

print("\n-------------------------------------------")
print(f"Goal will be reached on: {predicted_goal_date.strftime('%B %d, %Y')}")
print(f"Total days from today: {days_ahead}")
print("-------------------------------------------")