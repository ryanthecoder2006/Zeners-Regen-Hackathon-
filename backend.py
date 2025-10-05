import joblib
import numpy as np
import firebase_admin
from firebase_admin import credentials, db
from datetime import datetime
import time

# ----------------------------
# 1️⃣ Firebase setup
# ----------------------------
cred = credentials.Certificate("serviceAccountKey.json")  # download from Firebase Console
firebase_admin.initialize_app(cred, {
    'databaseURL': "https://smart-water-demo-default-rtdb.firebaseio.com/"
})

ref = db.reference("sensors_data")

# ----------------------------
# 2️⃣ Load Isolation Forest model
# ----------------------------
MODEL_PATH = "iso_model.joblib"
try:
    model = joblib.load(MODEL_PATH)
    print(f"✅ Model loaded from {MODEL_PATH}")
except Exception as e:
    print(f"❌ ERROR: Could not load model.\n{e}")
    exit()

# ----------------------------
# 3️⃣ Feature extraction
# ----------------------------
def compute_features(entry):
    flow = entry.get('flowRate', 50)
    prev_flow = entry.get('prev_flow', flow)
    flow_delta = flow - prev_flow
    roll_mean = (flow + prev_flow) / 2
    hour = datetime.now().hour
    return [flow, flow_delta, roll_mean, hour]

def estimate_loss(entry, expected_flow=50):
    flow = entry.get('flowRate', 50)
    loss = max(0, expected_flow - flow) / 50 * 100
    return round(loss, 2)

# ----------------------------
# 4️⃣ Main processing loop
# ----------------------------
def process_data():
    while True:
        items = ref.get()
        if not items:
            print("⚠️ No sensor data found.")
            time.sleep(3)
            continue

        print(f"📡 Checking {len(items)} entries")

        for key, entry in items.items():
            # Skip if already processed
            if 'status' in entry and 'confidence' in entry:
                continue

            feat = compute_features(entry)

            try:
                score = model.decision_function([feat])[0]
                is_anom = score < -0.1
                status = "Leak" if is_anom else "Normal"

                confidence = (1 - abs(score)) * 100
                confidence = max(0, min(100, confidence))

                est_loss = estimate_loss(entry)

                payload = {
                    "status": status,
                    "confidence": round(confidence, 2),
                    "estimated_loss": est_loss,
                    "timestamp": entry.get("timestamp", datetime.now().isoformat())
                }

                # ✅ Update same node in Firebase
                ref.child(key).update(payload)

                print(f"✅ Updated {key}: {status}, {confidence:.2f}%, loss={est_loss}%")

            except Exception as e:
                print(f"❌ Prediction error for {key}: {e}")

        time.sleep(3)

# ----------------------------
# 5️⃣ Run
# ----------------------------
if __name__ == "__main__":
    process_data()
