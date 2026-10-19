
#define BLYNK_TEMPLATE_ID "template_id_here"
#define BLYNK_TEMPLATE_NAME "template_name_here"
#define BLYNK_AUTH_TOKEN "auth_token_here"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <math.h>
#include <string.h>

// Wi-Fi credentials
char ssid[] = "wifi_name";
char pass[] = "wifi_password";

// Hardware pins
#define SDA_PIN 8
#define SCL_PIN 9
#define BUTTON_PIN 4

// BLE UUIDs
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define EVENT_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

Adafruit_MPU6050 mpu;
BLECharacteristic *eventChar = nullptr;
BlynkTimer timer;

// System states
bool connected = false;
bool fallPending = false;
bool manualSOSPending = false;
bool sosSent = false;
bool lowGSeen = false;

bool buttonWaiting = false;
bool lastReading = HIGH;
bool stableReading = HIGH;
bool pressHandled = false;

// Timing
unsigned long lowGTime = 0;
unsigned long fallTime = 0;
unsigned long firstPressTime = 0;
unsigned long lastDebounce = 0;
unsigned long lastSample = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastBlynkAttempt = 0;

const unsigned long SAMPLE_MS = 20;
const unsigned long LOWG_WINDOW_MS = 1200;
const unsigned long DOUBLE_WINDOW_MS = 650;
const unsigned long SOS_TIMEOUT_MS = 12000;
const unsigned long DEBOUNCE_MS = 35;

const float LOW_G = 0.55;
const float IMPACT_G = 2.5;
const float G_TO_MS2 = 9.80665;

float latestAccelG = 1.0;
float latestGyroDps = 0.0;

// --------------------------------------------------
// SEND EVENTS TO BLE AND BLYNK
// --------------------------------------------------

void sendEvent(const char *message) {
  Serial.print("Event: ");
  Serial.println(message);

  // Bluetooth notification to Android
  if (connected && eventChar != nullptr) {
    eventChar->setValue(message);
    eventChar->notify();
  }

  // Upload event to Blynk
  if (!Blynk.connected()) {
    Serial.println("Blynk offline; event not uploaded.");
    return;
  }

  if (strcmp(message, "FALL_DETECTED") == 0) {
    Blynk.virtualWrite(V2, "FALL DETECTED");
    Blynk.virtualWrite(V3, "NONE");

    Blynk.logEvent(
      "fall_detected",
      "SmartFall detected a possible fall"
    );
  }

  else if (strcmp(message, "MANUAL_SOS") == 0) {
    // Manual SOS may mean an undetected fall.
    manualSOSPending = true;

    Blynk.virtualWrite(
      V2, "POSSIBLE FALL - MANUAL SOS"
    );
    Blynk.virtualWrite(V3, "MANUAL SOS");

    Blynk.logEvent(
      "sos_emergency",
      "Manual SOS: possible fall or other emergency. Check user."
    );
  }

  else if (strcmp(message, "SOS_EMERGENCY") == 0) {
    if (fallPending) {
      Blynk.virtualWrite(V2, "FALL DETECTED");
    }

    Blynk.virtualWrite(V3, "SOS EMERGENCY");

    Blynk.logEvent(
      "sos_emergency",
      "Emergency SOS after possible fall"
    );
  }

  else if (strcmp(message, "USER_SAFE") == 0) {
    Blynk.virtualWrite(V2, "SAFE");
    Blynk.virtualWrite(V3, "NONE");
  }

  else if (strcmp(message, "SMARTFALL_READY") == 0) {
    Blynk.virtualWrite(V2, "MONITORING");
    Blynk.virtualWrite(V3, "NONE");
  }
}

// --------------------------------------------------
// SENSOR DATA UPLOAD
// --------------------------------------------------

void uploadSensorData() {
  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, latestAccelG);
    Blynk.virtualWrite(V1, latestGyroDps);

    Blynk.virtualWrite(
      V4,
      WiFi.status() == WL_CONNECTED ? 1 : 0
    );
  }
}

// --------------------------------------------------
// FALL DETECTION
// --------------------------------------------------

void startFallAlert() {
  if (fallPending) return;

  fallPending = true;
  manualSOSPending = false;
  sosSent = false;
  fallTime = millis();

  buttonWaiting = false;
  firstPressTime = 0;

  sendEvent("FALL_DETECTED");

  Serial.println("Possible fall detected!");
  Serial.println("Double press = SAFE");
  Serial.println("Single press = SOS");
}

void sendSOS() {
  if (sosSent) return;

  sosSent = true;
  sendEvent("SOS_EMERGENCY");
}

void confirmSafe() {
  sendEvent("USER_SAFE");

  fallPending = false;
  manualSOSPending = false;
  sosSent = false;
  buttonWaiting = false;
  firstPressTime = 0;
  lowGSeen = false;

  Serial.println("User confirmed safe.");
}

// --------------------------------------------------
// BLE CALLBACKS
// --------------------------------------------------

class ServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer *server) override {
    connected = true;
    Serial.println("Android phone connected.");
  }

  void onDisconnect(BLEServer *server) override {
    connected = false;
    Serial.println("Android phone disconnected.");

    BLEDevice::startAdvertising();
  }
};

// --------------------------------------------------
// BLE INITIALIZATION
// --------------------------------------------------

void setupBLE() {
  BLEDevice::init("SmartFall");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service =
    server->createService(SERVICE_UUID);

  eventChar = service->createCharacteristic(
    EVENT_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  eventChar->addDescriptor(new BLE2902());
  eventChar->setValue("SMARTFALL_READY");

  service->start();

  BLEAdvertising *advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();

  Serial.println("BLE started: SmartFall");
}

// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!mpu.begin()) {
    Serial.println("MPU6050 not detected.");
    Serial.println("Check VCC, GND, SDA and SCL.");

    while (true) {
      delay(1000);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Start Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  lastWiFiAttempt = millis();

  // Configure Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Start BLE independently
  setupBLE();

  // Upload sensor readings every second
  timer.setInterval(1000L, uploadSensorData);

  Serial.println("SmartFall initialized.");
  Serial.println("Connecting to Wi-Fi and Blynk...");
}

// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------

void loop() {
  unsigned long now = millis();

  Blynk.run();
  timer.run();

  // Reconnect Wi-Fi when needed
  if (WiFi.status() != WL_CONNECTED &&
      now - lastWiFiAttempt >= 10000) {

    lastWiFiAttempt = now;
    Serial.println("Retrying Wi-Fi...");
    WiFi.begin(ssid, pass);
  }

  // Reconnect Blynk when needed
  if (WiFi.status() == WL_CONNECTED &&
      !Blynk.connected() &&
      now - lastBlynkAttempt >= 3000) {

    lastBlynkAttempt = now;
    Blynk.connect(100);
  }

  // Read MPU6050 every 20 ms
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;

    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    float ax = accel.acceleration.x;
    float ay = accel.acceleration.y;
    float az = accel.acceleration.z;

    float gx = gyro.gyro.x * 57.2958;
    float gy = gyro.gyro.y * 57.2958;
    float gz = gyro.gyro.z * 57.2958;

    latestAccelG =
      sqrt(ax * ax + ay * ay + az * az)
      / G_TO_MS2;

    latestGyroDps =
      sqrt(gx * gx + gy * gy + gz * gz);

    static unsigned long lastPrint = 0;

    if (now - lastPrint >= 500) {
      lastPrint = now;

      Serial.printf(
        "Accel: %.2f g | Gyro: %.1f deg/s | WiFi: %s | Blynk: %s\n",
        latestAccelG,
        latestGyroDps,
        WiFi.status() == WL_CONNECTED
          ? "Connected" : "Disconnected",
        Blynk.connected()
          ? "Connected" : "Disconnected"
      );
    }

    // Preliminary low-G followed by impact detection
    if (!fallPending) {

      if (latestAccelG < LOW_G) {
        lowGSeen = true;
        lowGTime = now;
      }

      if (lowGSeen &&
          now - lowGTime > LOWG_WINDOW_MS) {
        lowGSeen = false;
      }

      if (lowGSeen &&
          latestAccelG >= IMPACT_G &&
          now - lowGTime <= LOWG_WINDOW_MS) {

        lowGSeen = false;
        startFallAlert();
      }
    }
  }

  // Debounced push-button handling
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounce = now;
    lastReading = reading;
  }

  if (now - lastDebounce >= DEBOUNCE_MS &&
      reading != stableReading) {

    stableReading = reading;

    if (stableReading == LOW && !pressHandled) {
      pressHandled = true;

      if (!buttonWaiting) {
        buttonWaiting = true;
        firstPressTime = now;
      }

      else if (now - firstPressTime <= DOUBLE_WINDOW_MS) {

        // Double press confirms safety after either
        // automatic fall detection or manual SOS.
        if (fallPending || manualSOSPending) {
          confirmSafe();
        }
        else {
          sendEvent("BUTTON_DOUBLE_PRESS");
        }

        buttonWaiting = false;
        firstPressTime = 0;
      }
    }

    if (stableReading == HIGH) {
      pressHandled = false;
    }
  }

  // Single press = SOS after double-press window
  if (buttonWaiting &&
      now - firstPressTime > DOUBLE_WINDOW_MS) {

    buttonWaiting = false;
    firstPressTime = 0;

    if (fallPending) {
      sendSOS();
    }
    else {
      sendEvent("MANUAL_SOS");
    }
  }

  // Automatic SOS if there is no response after a fall
  if (fallPending &&
      !sosSent &&
      now - fallTime >= SOS_TIMEOUT_MS) {

    sendSOS();
  }

  delay(1);
}
