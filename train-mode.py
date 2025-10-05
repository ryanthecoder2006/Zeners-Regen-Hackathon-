# train_model.py

import numpy as np
import pandas as pd
import joblib
from sklearn.ensemble import IsolationForest

# ----------------------------
# 1️⃣ Generate fake sensor data
# ----------------------------
def generate_series(length=1440, base=50, noise=1.5, drop_start=None, drop_mag=20, drop_len=60):
    """
    Generates a time series of flow data.
    drop_start, drop_mag, drop_len -> simulate anomalies
    """
    series = base + np.random.randn(length) * noise
    if drop_start is not None:
        series[drop_start:drop_start + drop_len] -= drop_mag
    return pd.DataFrame({
        'flow': series,
        'ts': pd.date_range('2025-10-05', periods=length, freq='T')
    })

# Normal and anomalous data
df_normal = generate_series()
df_anomaly = generate_series(drop_start=400, drop_mag=25, drop_len=120)

# Combine and reset index
df_train = pd.concat([df_normal, df_anomaly]).reset_index(drop=True)

# ----------------------------
# 2️⃣ Feature engineering
# ----------------------------
df_train['flow_delta'] = df_train['flow'].diff().fillna(0)
df_train['flow_roll_mean_3'] = df_train['flow'].rolling(3).mean().fillna(df_train['flow'])
df_train['hour_of_day'] = df_train['ts'].dt.hour

features = df_train[['flow', 'flow_delta', 'flow_roll_mean_3', 'hour_of_day']].values

# ----------------------------
# 3️⃣ Train Isolation Forest
# ----------------------------
model = IsolationForest(n_estimators=100, contamination=0.1, random_state=42)
model.fit(features)

# ----------------------------
# 4️⃣ Save trained model
# ----------------------------
joblib.dump(model, "iso_model.joblib")
print("✅ Model trained and saved as iso_model.joblib")
