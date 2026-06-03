/*
  This takes the code for a Bluetooth Low Energy (BLE) server that takes
  Wi-Fi credentials and a POSIX time configuration string.

  For connection to the server, use a Chrome browser and visit:
  https://androidcrypto.github.io/WebBle1/wifi_posix_02.html

  The received data is stored in Preferences.
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>

// UUID's for the BLE service and characteristics
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_SSID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_PASS "ae418b1d-ef2a-4467-83b6-20d20d7da4e8"
#define CHARACTERISTIC_TZ "c9d31122-386d-495c-9c04-f584400c4366"

BLECharacteristic *pCharSSID;
BLECharacteristic *pCharPASS;
BLECharacteristic *pCharTZ;
Preferences preferences;

String ssid = "";
String password = "";
String tzString = "";
bool dataReceived = false;
bool bleActive = false;

void startBLE() {
  if (bleActive) return;

  Serial.println("Starting BLE Configurations-Mode...");
  bleActive = true;

  // flash LED as feedback
  for (int i = 0; i < 5; i++) {
    //digitalWrite(LED_PIN, HIGH);
    delay(100);
    //digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  //digitalWrite(LED_PIN, HIGH);  // LED stays on as long BLE is active
  
  BLEDevice::init("ESP32-Config-Portal");

  Serial.println("ESP32-Config-Portal done");
  BLEServer *pServer = BLEDevice::createServer();
  Serial.println("createServer done");
  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("createService done");
  pCharSSID = pService->createCharacteristic(CHARACTERISTIC_SSID, BLECharacteristic::PROPERTY_WRITE);
  pCharPASS = pService->createCharacteristic(CHARACTERISTIC_PASS, BLECharacteristic::PROPERTY_WRITE);
  pCharTZ = pService->createCharacteristic(CHARACTERISTIC_TZ, BLECharacteristic::PROPERTY_WRITE);

  Serial.println("before Callbacks");

  class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      preferences.begin("wifi-config", false);
      if (pCharacteristic == pCharSSID) {
        preferences.putString("ssid", String(pCharacteristic->getValue().c_str()));
      } else if (pCharacteristic == pCharPASS) {
        preferences.putString("password", String(pCharacteristic->getValue().c_str()));
      } else if (pCharacteristic == pCharTZ) {
        preferences.putString("tz", String(pCharacteristic->getValue().c_str()));
        dataReceived = true;
      }
      preferences.end();
    }
  };

  Serial.println("after callbacks");

  MyCallbacks *callbacks = new MyCallbacks();
  pCharSSID->setCallbacks(callbacks);
  pCharPASS->setCallbacks(callbacks);
  pCharTZ->setCallbacks(callbacks);

  Serial.println("before pService->start");
  pService->start();
  Serial.println("after pService->start");
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("start BLE done");
}

void stopBLE() {
  if (!bleActive) return;
  Serial.println("Stopping BLE...");
  BLEDevice::deinit(true);
  //digitalWrite(LED_PIN, LOW);  // LED aus
  bleActive = false;
}

/*
  // append these lines in setup():
  // configure GPIO pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // PULLUP for the Button
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // load stored data
  preferences.begin("wifi-config", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  tzString = preferences.getString("tz", "");
  preferences.end();

  // when data is available try to connect
  if (ssid.length() > 0) {
    connectToWifiAndSetTime(ssid, password, tzString);
  } else {
    Serial.println("No Wi-Fi credentials available, please press the BOOT-Button for starting BLE.");
  }
*/

/*
  // append these line in loop():
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Boot Button pressed, starting BLE...");
      startBLE();
    }
  }

  // after receiving all data by BLE
  if (dataReceived) {
    dataReceived = false;
    stopBLE();  // shut down BLE

    // get new data from Non Volatile Storage (NVS) and connect
    preferences.begin("wifi-config", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    tzString = preferences.getString("tz", "");
    preferences.end();

    connectToWifiAndSetTime(ssid, password, tzString);
  }
*/