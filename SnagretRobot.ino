#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <math.h>

// ===== BLE UUIDs (must match controller) =====
static BLEUUID serviceUUID("12345678-1234-1234-1234-1234567890ab");
static BLEUUID charUUID   ("abcd1234-1234-1234-1234-abcdef123456");

// Shared joystick values (updated via BLE callback)
volatile uint16_t g_lx = 2048;
volatile uint16_t g_ly = 2048;
volatile uint16_t g_rx = 2048;
volatile uint16_t g_ry = 2048;

// ===== PCA9685 servo driver =====
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40); // default I2C address

// Servo channels (adjust these to match your wiring)
const int SERVO_LOWER_X = 0;  // lower segment left/right
const int SERVO_LOWER_Y = 1;  // lower segment forward/back
const int SERVO_UPPER_X = 2;  // upper segment left/right
const int SERVO_UPPER_Y = 3;  // upper segment forward/back

// Servo pulse range (tweak for your specific servos)
const int SERVO_MIN  = 100; // ~500us
const int SERVO_MAX  = 500; // ~2500us

// Motion limits
const float SERVO_CENTER = 90.0f; // degrees

// Per-servo ranges so we can calibrate later if needed
float RANGE_LOWER_X = 35.0f;  // ±20°
float RANGE_LOWER_Y = 35.0f;
float RANGE_UPPER_X = 35.0f;
float RANGE_UPPER_Y = 35.0f;

// Easing (per-servo)
float curLowerX = SERVO_CENTER;
float curLowerY = SERVO_CENTER;
float curUpperX = SERVO_CENTER;
float curUpperY = SERVO_CENTER;

const float EASE_ALPHA = 0.5f;  // 0–1, higher = snappier

// helper: unpack uint16 little-endian
uint16_t getU16(const uint8_t *buf, int idx) {
  return (uint16_t)buf[idx] | ((uint16_t)buf[idx + 1] << 8);
}

// ---- BLE characteristic callback: receive joystick packet ----
class ControlCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String value = pCharacteristic->getValue();
    if (value.length() != 8) {
      return;
    }

    const uint8_t *d = (const uint8_t *)value.c_str();

    uint16_t lx = getU16(d, 0);
    uint16_t ly = getU16(d, 2);
    uint16_t rx = getU16(d, 4);
    uint16_t ry = getU16(d, 6);

    g_lx = lx;
    g_ly = ly;
    g_rx = rx;
    g_ry = ry;
  }
};

// ---- BLE server connect/disconnect logs ----
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    Serial.println("Controller connected");
  }
  void onDisconnect(BLEServer *pServer) override {
    Serial.println("Controller disconnected, restarting advertising");
    BLEDevice::startAdvertising();
  }
};

void setupBLE() {
  BLEDevice::init("SNAGRET_ROBOT");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(serviceUUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    charUUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );

  pCharacteristic->setCallbacks(new ControlCharCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE advertising as SNAGRET_ROBOT...");
}

// ---- Servo helpers ----

void setServoAngle(uint8_t channel, float angleDeg) {
  if (angleDeg < 0.0f) angleDeg = 0.0f;
  if (angleDeg > 180.0f) angleDeg = 180.0f;

  int pulse = map((int)angleDeg, 0, 180, SERVO_MIN, SERVO_MAX);
  if (pulse < SERVO_MIN) pulse = SERVO_MIN;
  if (pulse > SERVO_MAX) pulse = SERVO_MAX;

  pwm.setPWM(channel, 0, pulse);
}

// convert joystick ADC (0–4095) to normalized -1.0 … +1.0 with deadzone
float adcToNormalized(uint16_t adc) {
  float x = (float)adc - 2048.0f;
  float norm = x / 2048.0f; // roughly -1 .. +1

  if (norm < -1.0f) norm = -1.0f;
  if (norm >  1.0f) norm =  1.0f;

  const float DEADZONE = 0.05f; // 5%
  if (fabs(norm) < DEADZONE) {
    norm = 0.0f;
  }

  return norm;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Snagret Robot starting...");

  // I2C + PCA9685
  Wire.begin(21, 22);   // SDA, SCL
  pwm.begin();
  pwm.setPWMFreq(50);   // standard servo frequency

  delay(10);

  // Center all 4 servos at startup
  setServoAngle(SERVO_LOWER_X, SERVO_CENTER);
  setServoAngle(SERVO_LOWER_Y, SERVO_CENTER);
  setServoAngle(SERVO_UPPER_X, SERVO_CENTER);
  setServoAngle(SERVO_UPPER_Y, SERVO_CENTER);

  // BLE
  setupBLE();
}

unsigned long lastPrint = 0;
unsigned long lastServoUpdate = 0;

void loop() {
  unsigned long now = millis();

  // Debug print every 200 ms
  if (now - lastPrint >= 200) {
    lastPrint = now;
    uint16_t lx = g_lx;
    uint16_t ly = g_ly;
    uint16_t rx = g_rx;
    uint16_t ry = g_ry;

    Serial.print("LX: "); Serial.print(lx);
    Serial.print("  LY: "); Serial.print(ly);
    Serial.print("  RX: "); Serial.print(rx);
    Serial.print("  RY: "); Serial.println(ry);
  }

  // Update servos every ~30 ms with easing
  if (now - lastServoUpdate >= 30) {
    lastServoUpdate = now;

    // Grab joystick values once per frame
    uint16_t lx = g_lx;
    uint16_t ly = g_ly;
    uint16_t rx = g_rx;
    uint16_t ry = g_ry;

    // Normalize each axis
    float nLX = adcToNormalized(lx);
    float nLY = adcToNormalized(ly);
    float nRX = adcToNormalized(rx);
    float nRY = adcToNormalized(ry);

    // Targets for each segment
    float targetLowerX = SERVO_CENTER + nLX * RANGE_LOWER_X;
    float targetLowerY = SERVO_CENTER + nLY * RANGE_LOWER_Y;

    float targetUpperX = SERVO_CENTER + nRX * RANGE_UPPER_X;
    float targetUpperY = SERVO_CENTER + nRY * RANGE_UPPER_Y;

    // Easing toward targets
    curLowerX += EASE_ALPHA * (targetLowerX - curLowerX);
    curLowerY += EASE_ALPHA * (targetLowerY - curLowerY);
    curUpperX += EASE_ALPHA * (targetUpperX - curUpperX);
    curUpperY += EASE_ALPHA * (targetUpperY - curUpperY);

    // Send to servos
    setServoAngle(SERVO_LOWER_X, curLowerX);
    setServoAngle(SERVO_LOWER_Y, curLowerY);
    setServoAngle(SERVO_UPPER_X, curUpperX);
    setServoAngle(SERVO_UPPER_Y, curUpperY);
  }

  delay(5);
}