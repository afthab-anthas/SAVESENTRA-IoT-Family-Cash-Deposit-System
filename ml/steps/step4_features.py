import pandas as pd

print("Feature Engineering")

# Load the clean data 
df = pd.read_csv('savings_dataset_cleaned.csv')
df['Timestamp'] = pd.to_datetime(df['Timestamp'])

# Group by Day
# We need to know how much was saved per day.
df['Date'] = df['Timestamp'].dt.date
daily_df = df.groupby('Date')['Deposit_Amount'].sum().reset_index()
daily_df['Date'] = pd.to_datetime(daily_df['Date'])

# FEATURE ENGINEERING 
# Changing the date into numbers 
daily_df['Day_Of_Week'] = daily_df['Date'].dt.dayofweek  # 0=Monday, 6=Sunday
daily_df['Is_Weekend'] = daily_df['Day_Of_Week'].apply(lambda x: 1 if x >= 5 else 0)
daily_df['Month'] = daily_df['Date'].dt.month
daily_df['Day_Of_Month'] = daily_df['Date'].dt.day

# FEATURE SELECTION
# We drop the raw 'Date' string
# We only keep our engineered numerical hints (X) and the total deposit amount (Y)
selected_columns = ['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month', 'Deposit_Amount']
ml_ready_data = daily_df[selected_columns]

ml_ready_data.to_csv('ml_ready_data.csv', index=False)
print("\nSaved as ml_ready_data.csv.")