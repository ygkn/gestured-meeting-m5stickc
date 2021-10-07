
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
  void onConnect(BLEServer* pServer) { deviceConnected = true; };

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

  M5.begin();
  M5.IMU.Init();
  M5.Lcd.setRotation(0);

  delay(1000);

  M5.update();
  M5.IMU.getAccelData(&previos_acceleration_x, &previos_acceleration_y,
                      &previos_acceleration_z);

  BLEDevice::init("Gestured Meeting");

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

  Serial.println("acc_x acc_y acc_z power connection risen waved");
}

void loop() {
  M5.update();

  // battery
  double vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
  M5.Lcd.setCursor(0, 5);
  M5.Lcd.printf("Volt: %.2fV", vbat);

  int8_t bat_charge_p = int8_t((vbat - 3.0) / 1.2 * 100);
  if (bat_charge_p > 100) {
    bat_charge_p = 100;
  } else if (bat_charge_p < 0) {
    bat_charge_p = 0;
  }

  // get sensor values
  M5.IMU.getAccelData(&current_acceleration_x, &current_acceleration_y,
                      &current_acceleration_z);

  bool current_is_hand_risen =
      -0.8 < current_acceleration_x && current_acceleration_x < 0.8 &&
      current_acceleration_y > 0.8 && -0.8 < current_acceleration_z &&
      current_acceleration_z < 0.8;

  float wave_power =
      sqrt(pow(current_acceleration_x - previos_acceleration_x, 2) +
           pow(current_acceleration_y - previos_acceleration_y, 2) +
           pow(current_acceleration_z - previos_acceleration_z, 2));

  bool current_is_hand_waved = wave_power > 0.8;

  // notify
  if (deviceConnected) {
    if (current_is_hand_risen != has_been_hand_risen) {
      pCharacteristic->setValue("toggle_hand");
      pCharacteristic->notify();
    }

    if (current_is_hand_waved) {
      if (!has_been_hand_waved) {
        wave_start_time = millis();

        pCharacteristic->setValue("leave_start");
        pCharacteristic->notify();
      } else if (millis() - wave_start_time > 3000) {
        pCharacteristic->setValue("leave_comfirm");
        pCharacteristic->notify();
      }
    }
  }

  // display to M5StickC
  if (current_is_hand_risen) {
    M5.Lcd.fillScreen(YELLOW);
  } else if (current_is_hand_waved) {
    M5.Lcd.fillScreen(RED);
  } else {
    M5.Lcd.fillScreen(BLACK);
  }

  // serial
  Serial.printf("%f %f %f %d %d %d %d\n", current_acceleration_x,
                current_acceleration_y, current_acceleration_z, bat_charge_p,
                deviceConnected ? 1 : 0, current_is_hand_risen ? 1 : 0,
                current_is_hand_waved ? 1 : 0);

  // process connection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  has_been_hand_risen = current_is_hand_risen;
  has_been_hand_waved = current_is_hand_waved;

  previos_acceleration_x = current_acceleration_x;
  previos_acceleration_y = current_acceleration_y;
  previos_acceleration_z = current_acceleration_z;

  delay(1000);
}
