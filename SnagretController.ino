#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>

// ===== BLE UUIDs (must match robot) =====
static BLEUUID serviceUUID("12345678-1234-1234-1234-1234567890ab");
static BLEUUID charUUID   ("abcd1234-1234-1234-1234-abcdef123456");

// ===== Pin definitions =====
const int JOY1_X_PIN = 34;  // ADC1
const int JOY1_Y_PIN = 35;  // ADC1
const int JOY2_X_PIN = 32;  // ADC1
const int JOY2_Y_PIN = 33;  // ADC1

// ===== BLE globals =====
BLEAddress      g_robotAddress((uint8_t*)"\0\0\0\0\0\0");
bool            g_hasRobotAddress = false;
BLEClient*      g_client          = nullptr;
BLERemoteCharacteristic* g_remoteChar = nullptr;
bool            g_connected       = false;

// helper to pack uint16 little-endian
void putU16(uint8_t *buf, int idx, uint16_t v) {
  buf[idx]     = v & 0xFF;
  buf[idx + 1] = (v >> 8) & 0xFF;
}

bool connectToRobot() {
  if (!g_hasRobotAddress) return false;

  if (g_client == nullptr) {
    g_client = BLEDevice::createClient();
  }

  Serial.print("Connecting to robot at ");
  Serial.println(g_robotAddress.toString().c_str());

  if (!g_client->connect(g_robotAddress)) {
    Serial.println("Failed to connect to robot");
    g_connected = false;
    return false;
  }

  Serial.println("Connected to robot!");

  BLERemoteService* remoteService = g_client->getService(serviceUUID);
  if (remoteService == nullptr) {
    Serial.println("Failed to find service on robot");
    g_client->disconnect();
    g_connected = false;
    return false;
  }

  g_remoteChar = remoteService->getCharacteristic(charUUID);
  if (g_remoteChar == nullptr || !g_remoteChar->canWrite()) {
    Serial.println("Failed to find writable characteristic");
    g_client->disconnect();
    g_connected = false;
    return false;
  }

  g_connected = true;
  return true;
}

void scanForRobot() {
  Serial.println("Scanning for robot...");

  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);

  // NOTE: on esp32 3.3.7 this returns BLEScanResults*
  BLEScanResults* results = scan->start(5, false); // scan for 5 seconds

  g_hasRobotAddress = false;

  if (results != nullptr) {
    int count = results->getCount();
    for (int i = 0; i < count; i++) {
      BLEAdvertisedDevice d = results->getDevice(i);
      if (d.haveServiceUUID() && d.isAdvertisingService(serviceUUID)) {
        Serial.print("Found robot: ");
        Serial.println(d.toString().c_str());

        g_robotAddress = d.getAddress();
        g_hasRobotAddress = true;
        break;
      }
    }
  }

  if (!g_hasRobotAddress) {
    Serial.println("Robot not found.");
  }
}

void ensureConnected() {
  if (g_connected && g_client && g_client->isConnected() && g_remoteChar) {
    return;
  }

  g_connected = false;
  g_remoteChar = nullptr;

  scanForRobot();
  if (g_hasRobotAddress) {
    connectToRobot();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Snagret Controller starting...");

  BLEDevice::init("SNAGRET_CONTROLLER");

  ensureConnected();
}

void sendControlPacket() {
  if (!g_connected || g_remoteChar == nullptr) return;

  uint16_t lx = analogRead(JOY1_X_PIN);
  uint16_t ly = analogRead(JOY1_Y_PIN);
  uint16_t rx = analogRead(JOY2_X_PIN);
  uint16_t ry = analogRead(JOY2_Y_PIN);

  uint8_t pkt[8];
  putU16(pkt, 0, lx);
  putU16(pkt, 2, ly);
  putU16(pkt, 4, rx);
  putU16(pkt, 6, ry);

  // Works whether writeValue() returns void or bool
  g_remoteChar->writeValue(pkt, sizeof(pkt), false);
}

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL_MS = 30;

void loop() {
  ensureConnected();

  if (g_connected) {
    unsigned long now = millis();
    if (now - lastSend >= SEND_INTERVAL_MS) {
      lastSend = now;
      sendControlPacket();
    }
  }

  delay(5);
}