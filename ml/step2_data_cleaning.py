import pandas as pd

# Step 1: Load the dataset
df = pd.read_csv('savings_dataset.csv')

print("=== STEP 2: DATA CLEANING ===\n")

# Show basic info
print("Shape:", df.shape)
print("\nFirst 5 rows:")
print(df.head())

# Check for missing values
print("\n--- Missing Values ---")
print(df.isnull().sum())

# Check for duplicates
duplicates = df.duplicated().sum()
print(f"\nDuplicate rows: {duplicates}")
if duplicates > 0:
    df = df.drop_duplicates()
    print(f"Removed {duplicates} duplicates.")

# Check data types
print("\n--- Data Types ---")
print(df.dtypes)

# Convert Timestamp to datetime
df['Timestamp'] = pd.to_datetime(df['Timestamp'])
print("\nTimestamp converted to datetime.")

# Check for invalid deposit amounts
valid_notes = [5, 10, 20, 50, 100, 200]
invalid = df[~df['Deposit_Amount'].isin(valid_notes)]
print(f"\nInvalid deposit amounts: {len(invalid)}")
if len(invalid) > 0:
    print(invalid)

# Check for negative balances
negative = df[df['Total_Balance'] < 0]
print(f"Negative balances: {len(negative)}")

# Show unique users
print(f"\nUnique users: {df['NFC_UID'].unique()}")
print(f"Total transactions: {len(df)}")

# Save cleaned data
df.to_csv('savings_dataset_cleaned.csv', index=False)
print("\nCleaned data saved to savings_dataset_cleaned.csv")
print("\n=== CLEANING COMPLETE ===")
