import pandas as pd

# load the raw dataset
df = pd.read_csv('savings_dataset.csv')

print("--- Starting Data Cleaning ---")

# checking how big the dataset is
print("Dataset size (rows, cols):", df.shape)
print("\nFirst few rows to see if it loaded right:")
print(df.head())

# check for empty rows
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
print("\nFixed the timestamp column to actual datetime format.")

# data cleaning for my particular data
# checking if the motor logged any impossible note amounts (only UAE dirhams allowed)
allowed_notes = [5, 10, 20, 50, 100, 200]
weird_notes = df[~df['Deposit_Amount'].isin(allowed_notes)]

print(f"\nInvalid notes detected: {len(weird_notes)}")
if len(weird_notes) > 0:
    print("Here are the invalid ones:")
    print(weird_notes)

# checking any negative values
neg_bals = df[df['Total_Balance'] < 0]
print(f"Negative balance errors: {len(neg_bals)}")


print(f"\nUsers registered in the system: {df['NFC_UID'].unique()}")
print(f"Total valid transactions left: {len(df)}")

# export to a new csv 
df.to_csv('savings_dataset_cleaned.csv', index=False)
print("\nAll clean! Saved as savings_dataset_cleaned.csv")