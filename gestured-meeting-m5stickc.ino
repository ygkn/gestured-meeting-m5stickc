
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <M5StickC.h>

#include "./uuid.h"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("connected");
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

bool has_been_hand_risen = false;
bool has_been_hand_waved = false;

unsigned long wave_start_time;

float previos_acceleration_x;
float previos_acceleration_y;
float previos_acceleration_z;

float current_acceleration_x;
float current_acceleration_y;
float current_acceleration_z;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
  }

  Serial.println("hello");

  M5.begin();
  M5.IMU.Init();
  M5.Lcd.setRotation(0);

  delay(1000);

  M5.update();
  M5.IMU.getAccelData(&previos_acceleration_x, &previos_acceleration_y,
                      &previos_acceleration_z);

  BLEDevice::init("ESP32");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  M5.update();

  M5.IMU.getAccelData(&current_acceleration_x, &current_acceleration_y,
                      &current_acceleration_z);

  bool current_is_hand_risen = is_hand_risen();

  if (current_is_hand_risen != has_been_hand_risen && deviceConnected) {
    pCharacteristic->setValue("toggle_hand");
    pCharacteristic->notify();
  }

  has_been_hand_risen = current_is_hand_risen;

  bool current_is_hand_waved = is_hand_waved();

  if (current_is_hand_waved && deviceConnected) {
    if (!has_been_hand_waved) {
      wave_start_time = millis();

      pCharacteristic->setValue("leave_start");
      pCharacteristic->notify();
    } else if (millis() - wave_start_time > 3000) {
      pCharacteristic->setValue("leave_comfirm");
      pCharacteristic->notify();
    }
  }

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  has_been_hand_waved = current_is_hand_waved;

  Serial.printf("x: %.5f y: %.5f z: %.5f ", current_acceleration_x,
                current_acceleration_y, current_acceleration_z);

  Serial.printf("power: %.5f",
                sqrt(pow(current_acceleration_x - previos_acceleration_x, 2) +
                     pow(current_acceleration_y - previos_acceleration_y, 2) +
                     pow(current_acceleration_z - previos_acceleration_z, 2)));

  if (current_is_hand_risen) {
    M5.Lcd.fillScreen(YELLOW);
    Serial.print(" RISEN");
  } else if (current_is_hand_waved) {
    M5.Lcd.fillScreen(RED);
    Serial.print(" WAVED");
  } else {
    M5.Lcd.fillScreen(BLACK);
  }

  Serial.println();

  previos_acceleration_x = current_acceleration_x;
  previos_acceleration_y = current_acceleration_y;
  previos_acceleration_z = current_acceleration_z;

  delay(1000);
}

bool is_hand_risen() {
  return -0.8 < current_acceleration_x && current_acceleration_x < 0.8 &&
         current_acceleration_y > 0.8 && -0.8 < current_acceleration_z &&
         current_acceleration_z < 0.8;
}

bool is_hand_waved() {
  return sqrt(pow(current_acceleration_x - previos_acceleration_x, 2) +
              pow(current_acceleration_y - previos_acceleration_y, 2) +
              pow(current_acceleration_z - previos_acceleration_z, 2)) > 0.8;
}
