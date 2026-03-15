import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
from sklearn.svm import SVR
from sklearn.metrics import mean_absolute_error

# Load the ML-ready data 
data = pd.read_csv('ml_ready_data.csv')

# Define Inputs (X) and Target (y)
X = data[['Day_Of_Week', 'Is_Weekend', 'Month', 'Day_Of_Month']]
y = data['Deposit_Amount']

# Split the data 80% for training, 20% for testing
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
print(f"Training on {len(X_train)} days, testing on {len(X_test)} days.")

#Use Sklearn to initialize models
models = {
    "Random Forest": RandomForestRegressor(n_estimators=100, random_state=42),
    "Gradient Boosting": GradientBoostingRegressor(random_state=42),
    "SVR": SVR(kernel='rbf')
}

# Compare Results
print("\n--- Model Performance Shootout ---")
for name, model in models.items():
    # Fit the model (Training)
    model.fit(X_train, y_train)
    
    # Make predictions on the test set
    predictions = model.predict(X_test)
    
    # Calculate Mean Absolute Error (MAE)
    error = mean_absolute_error(y_test, predictions)
    print(f"{name} Error: {error:.2f} AED")

