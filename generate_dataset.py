import pandas as pd
import numpy as np
import datetime
import random
import os

# Set reproducible random seed for consistency
random.seed(42)
np.random.seed(42)

# Configuration
users = ['U001', 'U002', 'U003', 'U004']
start_date = datetime.datetime(2025, 9, 1)
end_date = datetime.datetime(2026, 2, 28, 23, 59, 59)
valid_notes = [5, 10, 20, 50, 100, 200]
# Make smaller notes much more frequent
note_weights = [0.40, 0.30, 0.15, 0.10, 0.04, 0.01]  

data = []

for uid in users:
    current_date = start_date
    balance = 0
    
    # Iterate week by week
    while current_date < end_date:
        # Determine number of deposits this week (3 to 5)
        num_deposits = random.randint(3, 5)
        
        # Pick random distinct days within the week
        days_offset = random.sample(range(7), num_deposits)
        
        for offset in sorted(days_offset):
            deposit_day = current_date + datetime.timedelta(days=offset)
            
            # Stop if we exceed the exact end date
            if deposit_day > end_date:
                continue
                
            # Random time between 8 AM and 10 PM
            hour = random.randint(8, 21)
            minute = random.randint(0, 59)
            second = random.randint(0, 59)
            
            timestamp = deposit_day.replace(hour=hour, minute=minute, second=second)
            
            # Select a random valid UAE note based on the custom weights
            deposit_amount = random.choices(valid_notes, weights=note_weights)[0]
            balance += deposit_amount
            
            data.append({
                'Timestamp': timestamp,
                'NFC_UID': uid,
                'Deposit_Amount': deposit_amount,
                'Total_Balance': balance
            })
            
        # Move forward by exactly 1 week
        current_date += datetime.timedelta(days=7)

# Create DataFrame
df = pd.DataFrame(data)

# Sort purely by Timestamp so the log looks like a live chronological sensor feed
df = df.sort_values(by='Timestamp').reset_index(drop=True)

# Format the timestamp as a clean string
df['Timestamp'] = df['Timestamp'].dt.strftime('%Y-%m-%d %H:%M:%S')

# Save to the project folder
output_path = os.path.join(os.path.dirname(__file__), 'savings_dataset.csv')
df.to_csv(output_path, index=False)

print(f"Dataset completely generated: {len(df)} rows.")
print(f"Saved directly to: {output_path}")
