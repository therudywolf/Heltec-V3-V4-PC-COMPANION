/*
 * NOCTURNE_OS — BLE key service (NimBLE peripheral).
 */
#include "BleKeyService.h"
#include "nocturne/config.h"
#if __has_include("NimBLEDevice.h")
#include "NimBLEDevice.h"
#endif
#include <cstring>
#include <stdio.h>

#if __has_include("NimBLEDevice.h")
static BleKeyService *s_keyService = nullptr;

class BleKeyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) override {
    (void)pServer;
    if (s_keyService)
      s_keyService->onConnect();
  }
  void onDisconnect(NimBLEServer *pServer) override {
    (void)pServer;
    if (s_keyService)
      s_keyService->onDisconnect();
  }
};

class BmwControlCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    if (!s_keyService || !pCharacteristic)
      return;
    std::string value = pCharacteristic->getValue();
    if (value.length() >= 1)
      s_keyService->onLightCommandReceived((uint8_t)value[0]);
  }
};

class BmwNowPlayingCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    if (!s_keyService || !pCharacteristic)
      return;
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
      s_keyService->onNowPlayingReceived(
          reinterpret_cast<const uint8_t *>(value.data()), value.length());
  }
};

class BmwClusterTextCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    if (!s_keyService || !pCharacteristic)
      return;
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
      s_keyService->onClusterTextReceived(
          reinterpret_cast<const uint8_t *>(value.data()), value.length());
  }
};

static BleKeyServerCallbacks s_serverCb;
static BmwControlCharCallbacks s_controlCharCb;
static BmwNowPlayingCharCallbacks s_nowPlayingCharCb;
static BmwClusterTextCharCallbacks s_clusterTextCharCb;
static NimBLECharacteristic *s_pStatusChar = nullptr;
#endif

BleKeyService::BleKeyService() {}

void BleKeyService::onConnect() {
  connected_ = true;
  disconnectPending_ = false;
  disconnectReportedAt_ = 0;
  requestStatusNotifyOnNextUpdate();  /* Next updateStatus() will notify so phone gets data immediately. */
#if NOCT_BMW_DEBUG
  Serial.println("[BMW BLE] Phone connected");
#endif
  if (connectionCb_)
    connectionCb_(true);
}

void BleKeyService::onDisconnect() {
  connected_ = false;
  disconnectPending_ = true;
  disconnectReportedAt_ = millis();
#if NOCT_BMW_DEBUG
  Serial.println("[BMW BLE] Phone disconnected");
#endif
}

void BleKeyService::onLightCommandReceived(uint8_t cmd) {
#if NOCT_BMW_DEBUG
  Serial.printf("[BMW BLE] cmd from phone: 0x%02X\n", cmd);
#endif
  if (lightCommandCb_)
    lightCommandCb_(cmd);
}

void BleKeyService::begin() {
#if __has_include("NimBLEDevice.h")
  if (active_) {
#if NOCT_BMW_DEBUG
    Serial.println("[BMW BLE] begin skipped (already active)");
#endif
    return;
  }
  s_keyService = this;
#if NOCT_BMW_DEBUG
  Serial.printf("[BMW BLE] NimBLE initialized=%d, calling init...\n", NimBLEDevice::getInitialized() ? 1 : 0);
#endif
  if (!NimBLEDevice::getInitialized())
    NimBLEDevice::init("BMW E39 Key");
  NimBLEServer *pServer = NimBLEDevice::createServer();
#if NOCT_BMW_DEBUG
  if (!pServer) Serial.println("[BMW BLE] createServer failed");
#endif
  if (pServer)
    pServer->setCallbacks(&s_serverCb);
  NimBLEService *pService = pServer ? pServer->createService("1800") : nullptr;
  if (pService)
    pService->start();
  /* BMW control: one byte = action index 0..11 (Goodbye..DoorLock). */
  NimBLEService *pCtrl = pServer ? pServer->createService("1a2b0001-5e6f-4a5b-8c9d-0e1f2a3b4c5d") : nullptr;
#if NOCT_BMW_DEBUG
  if (!pCtrl) Serial.println("[BMW BLE] createService(1a2b0001..) failed");
#endif
  if (pCtrl) {
    NimBLECharacteristic *pChar = pCtrl->createCharacteristic(
        "1a2b0002-5e6f-4a5b-8c9d-0e1f2a3b4c5d",
        NIMBLE_PROPERTY::WRITE);
    if (pChar)
      pChar->setCallbacks(&s_controlCharCb);

    /* Status: READ + NOTIFY (flags, coolant, oil, rpm, PDC). */
    s_pStatusChar = pCtrl->createCharacteristic(
        "1a2b0003-5e6f-4a5b-8c9d-0e1f2a3b4c5d",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    /* Now Playing: WRITE (track\0artist UTF-8). */
    NimBLECharacteristic *pNp = pCtrl->createCharacteristic(
        "1a2b0004-5e6f-4a5b-8c9d-0e1f2a3b4c5d",
        NIMBLE_PROPERTY::WRITE);
    if (pNp)
      pNp->setCallbacks(&s_nowPlayingCharCb);

    /* Cluster text: WRITE (UTF-8 string up to 20 bytes -> sendClusterText). */
    NimBLECharacteristic *pCluster = pCtrl->createCharacteristic(
        "1a2b0005-5e6f-4a5b-8c9d-0e1f2a3b4c5d",
        NIMBLE_PROPERTY::WRITE);
    if (pCluster)
      pCluster->setCallbacks(&s_clusterTextCharCb);

    pCtrl->start();
  }
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising) {
    pAdvertising->addServiceUUID("1800");
    pAdvertising->addServiceUUID("1a2b0001-5e6f-4a5b-8c9d-0e1f2a3b4c5d");
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
  }
  active_ = true;
  connected_ = false;
  disconnectPending_ = false;
#if NOCT_BMW_DEBUG
  Serial.println("[BMW BLE] begin OK, advertising started (name: BMW E39 Key)");
#endif
#endif
}

void BleKeyService::end() {
#if __has_include("NimBLEDevice.h")
  if (!active_)
    return;
  s_pStatusChar = nullptr;
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising)
    pAdvertising->stop();
  /* deinit(false): shutdown stack without full clear; deinit(true) can trigger
   * heap_caps_free "pointer outside heap areas" on mode switch (MODE_7 -> MODE_0). */
  if (NimBLEDevice::getInitialized())
    NimBLEDevice::deinit(false);
  s_keyService = nullptr;
  active_ = false;
  connected_ = false;
  disconnectPending_ = false;
  lastStatusPacketValid_ = false;
#endif
}

void BleKeyService::updateStatus(bool ibusSynced, bool phoneConnected, bool pdcValid,
                                 bool obdConnected, int coolantC, int oilC, int rpm,
                                 const int *pdcDists, uint8_t lastMflAction,
                                 uint8_t doorByte1, uint8_t doorByte2, uint8_t lockState,
                                 int ignition, int odometerKm) {
#if __has_include("NimBLEDevice.h")
  if (!active_ || !s_pStatusChar)
    return;
  uint8_t flags = (ibusSynced ? 0x01u : 0u) | (phoneConnected ? 0x02u : 0u) |
                 (pdcValid ? 0x04u : 0u) | (obdConnected ? 0x08u : 0u);
  uint8_t buf[kStatusPacketLen];
  buf[0] = flags;
  buf[1] = (coolantC >= -40 && coolantC <= 127) ? (uint8_t)coolantC : 0xFF;
  buf[2] = (oilC >= -40 && oilC <= 127) ? (uint8_t)oilC : 0xFF;
  if (rpm >= 0 && rpm <= 0xFFFF) {
    buf[3] = (uint8_t)(rpm >> 8);
    buf[4] = (uint8_t)(rpm & 0xFF);
  } else
    buf[3] = 0xFF, buf[4] = 0xFF;
  for (int i = 0; i < 4 && pdcDists; i++)
    buf[5 + i] = (pdcDists[i] >= 0 && pdcDists[i] <= 254) ? (uint8_t)pdcDists[i] : 0xFF;
  if (!pdcDists)
    buf[5] = buf[6] = buf[7] = buf[8] = 0xFF;
  buf[9] = (lastMflAction <= 5) ? lastMflAction : 0;
  buf[10] = doorByte1;
  buf[11] = doorByte2;
  buf[12] = (lockState <= 2) ? lockState : 0xFF;
  buf[13] = (ignition >= 0 && ignition <= 3) ? (uint8_t)ignition : 0xFF;
  if (odometerKm >= 0 && odometerKm <= 65535) {
    buf[14] = (uint8_t)(odometerKm & 0xFF);
    buf[15] = (uint8_t)(odometerKm >> 8);
  } else
    buf[14] = 0xFF, buf[15] = 0xFF;

  bool changed = !lastStatusPacketValid_;
  if (!changed) {
    for (size_t i = 0; i < kStatusPacketLen; i++)
      if (lastStatusPacket_[i] != buf[i]) {
        changed = true;
        break;
      }
  }
  if (changed) {
    for (size_t i = 0; i < kStatusPacketLen; i++)
      lastStatusPacket_[i] = buf[i];
    lastStatusPacketValid_ = true;
    s_pStatusChar->setValue(buf, kStatusPacketLen);
    s_pStatusChar->notify();
  }
#endif
}

void BleKeyService::onClusterTextReceived(const uint8_t *data, size_t len) {
  if (!clusterTextCb_ || !data || len == 0)
    return;
  static char textBuf[21];
  size_t n = len < 20 ? len : 20;
  memcpy(textBuf, data, n);
  textBuf[n] = '\0';
#if NOCT_BMW_DEBUG
  Serial.printf("[BMW BLE] Cluster text from phone: \"%s\"\n", textBuf);
#endif
  clusterTextCb_(textBuf);
}

void BleKeyService::onNowPlayingReceived(const uint8_t *data, size_t len) {
  if (!nowPlayingCb_ || !data || len == 0)
    return;
  const char *p = reinterpret_cast<const char *>(data);
  size_t trackLen = 0;
  while (trackLen < len && p[trackLen] != '\0')
    trackLen++;
  const char *artist = (trackLen < len - 1) ? (p + trackLen + 1) : "";
  size_t artistLen = 0;
  while (artistLen < len - (trackLen + 1) && artist[artistLen] != '\0')
    artistLen++;
  static char trackBuf[96];
  static char artistBuf[96];
  size_t tMax = sizeof(trackBuf) - 1;
  size_t aMax = sizeof(artistBuf) - 1;
  if (trackLen > tMax)
    trackLen = tMax;
  if (artistLen > aMax)
    artistLen = aMax;
  memcpy(trackBuf, p, trackLen);
  trackBuf[trackLen] = '\0';
  memcpy(artistBuf, artist, artistLen);
  artistBuf[artistLen] = '\0';
#if NOCT_BMW_DEBUG
  Serial.printf("[BMW BLE] Now Playing from phone: \"%s\" - \"%s\"\n", trackBuf, artistBuf);
#endif
  nowPlayingCb_(trackBuf, artistBuf);
}

void BleKeyService::tick() {
#if __has_include("NimBLEDevice.h")
  if (!active_ || !disconnectPending_)
    return;
  unsigned long now = millis();
  if (now - disconnectReportedAt_ >= kDisconnectDebounceMs) {
    disconnectPending_ = false;
    if (connectionCb_)
      connectionCb_(false);
  }
#endif
}
