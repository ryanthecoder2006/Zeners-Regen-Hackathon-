#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ====== Wi-Fi & Firebase Details ======
#define WIFI_SSID "Alph"
#define WIFI_PASSWORD "alphin1233"

#define API_KEY "AIzaSyBmHSmj-retqc-34F08aQEsi7ufZpIu6sc"
#define DATABASE_URL "https://smart-water-demo-default-rtdb.firebaseio.com/"
#define USER_EMAIL "esp32leak@test.com"
#define USER_PASSWORD "zeners123"

// ====== Simulation Mode ======
#define SIMULATION_MODE true   // ✅ Set to true to simulate pulses (no real water)

// ====== Hardware Pins ======
#define FLOW_SENSOR_PIN 27   // YF-S201 Signal (Yellow wire)
#define ENA_PIN 5            // Motor driver enable pin (relay/valve)
#define LEAK_THRESHOLD_PULSES 10  // Adjust based on flow sensor sensitivity
#define LED  26

// ====== Global Variables ======
volatile int pulseCount = 0;
unsigned long oldTime = 0;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ====== SIMULATION VARIABLES ======
int fakePulse = 0;
int simMode = 0;  // 0 = normal, 1 = leak

// ISR for real flow sensor
void IRAM_ATTR flowPulse() {
  pulseCount++;
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  Serial.println("🚰 Smart Leak Detection System Booting...");

  // Wi-Fi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("✅ Wi-Fi Connected. IP: ");
  Serial.println(WiFi.localIP());

  // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("✅ Firebase Connected.");

  // Hardware setup
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(ENA_PIN, OUTPUT);
  digitalWrite(ENA_PIN, HIGH);  // valve open initially
  pinMode(LED, OUTPUT);

  if (!SIMULATION_MODE) {
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulse, RISING);
  }

  Serial.println("System Ready: Monitoring flow...");
  Serial.println(SIMULATION_MODE ? "💡 Running in SIMULATION MODE" : "💧 Running in SENSOR MODE");
}

// ====== MAIN LOOP ======
void loop() {
  if (millis() - oldTime >= 5000) { // every 5 seconds
    int currentPulses;

    if (SIMULATION_MODE) {
      // ----- Generate Fake Pulses -----
      // Randomly switch between normal and leak every 20 seconds
      if (millis() / 20000 % 2 == 0) {
        simMode = 0; // normal
        fakePulse = random(15, 30);  // normal water flow
      } else {
        simMode = 1; // leak
        fakePulse = random(0, 5);    // low flow (leak detected)
      }
      currentPulses = fakePulse;
    } else {
      // ----- Read from real sensor -----
      detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));
      currentPulses = pulseCount;
      pulseCount = 0;
      attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulse, RISING);
    }

    oldTime = millis();

    // Convert pulses to approximate flow rate (L/min)
    float flowRate = (currentPulses / 4.5); // calibration factor adjustment
    float flowLitersPerMin = (flowRate * 60) / 1000.0;
    float pressure = random(20, 36) / 10.0;  // 2.0 to 3.5 bar
    String zones[] = {"Zone A", "Zone B", "Zone C"};
    String zone = zones[random(0, 3)];

    String valveStatus;
    String leakStatus;

    // Leak Detection Logic
    if (currentPulses < LEAK_THRESHOLD_PULSES) {
      leakStatus = "LEAK DETECTED";
      valveStatus = "CLOSED";
      digitalWrite(ENA_PIN, LOW);  // turn motor OFF
      digitalWrite(LED, HIGH);
    } else {
      leakStatus = "NO LEAK";
      valveStatus = "OPEN";
      digitalWrite(ENA_PIN, HIGH); // motor ON
      digitalWrite(LED, LOW);
    }

    // Show in Serial Monitor
    Serial.println("----- STATUS UPDATE -----");
    Serial.println("Mode: " + String(simMode == 0 ? "NORMAL FLOW" : "LEAK"));
    //Serial.println("Pulses: " + String(currentPulses));
    //Serial.println("Flow (L/min): " + String(flowLitersPerMin, 2));
    Serial.println("Leak: " + leakStatus);
    Serial.println("Valve: " + valveStatus);

    // Send data to Firebase
    if (Firebase.ready()) {
      FirebaseJson json;
      json.set("flowRate", flowLitersPerMin);
      json.set("pressure", pressure);  
      json.set("pulses", currentPulses);
      json.set("leakStatus", leakStatus);
      json.set("valveStatus", valveStatus);
      json.set("mode", simMode == 0 ? "NORMAL" : "LEAK_SIM");
      json.set("timestamp", String(millis() / 1000));
      json.set("zone", zone);

      if (Firebase.RTDB.pushJSON(&fbdo, "sensors_data", &json)) {
        Serial.println("✅ Data sent to Firebase.");
      } else {
        Serial.println("❌ Firebase Error: " + fbdo.errorReason());
      }
    }

    Serial.println("---------------------------\n");
  }
}