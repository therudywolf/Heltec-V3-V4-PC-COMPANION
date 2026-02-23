/*
 * NOCTURNE_OS — BLE key service (NimBLE peripheral).
 */
#include "BleKeyService.h"
#if __has_include("NimBLEDevice.h")
#include "NimBLEDevice.h"
#endif
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

static BleKeyServerCallbacks s_serverCb;
#endif

BleKeyService::BleKeyService() {}

void BleKeyService::onConnect() {
  connected_ = true;
  disconnectPending_ = false;
  disconnectReportedAt_ = 0;
  if (connectionCb_)
    connectionCb_(true);
}

void BleKeyService::onDisconnect() {
  connected_ = false;
  disconnectPending_ = true;
  disconnectReportedAt_ = millis();
}

void BleKeyService::begin() {
#if __has_include("NimBLEDevice.h")
  if (active_)
    return;
  s_keyService = this;
  if (!NimBLEDevice::getInitialized())
    NimBLEDevice::init("BMW E39 Key");
  NimBLEServer *pServer = NimBLEDevice::createServer();
  if (pServer)
    pServer->setCallbacks(&s_serverCb);
  NimBLEService *pService = pServer ? pServer->createService("1800") : nullptr;
  if (pService)
    pService->start();
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising) {
    pAdvertising->addServiceUUID("1800");
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
  }
  active_ = true;
  connected_ = false;
  disconnectPending_ = false;
#endif
}

void BleKeyService::end() {
#if __has_include("NimBLEDevice.h")
  if (!active_)
    return;
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising)
    pAdvertising->stop();
  if (NimBLEDevice::getInitialized())
    NimBLEDevice::deinit(true);
  s_keyService = nullptr;
  active_ = false;
  connected_ = false;
  disconnectPending_ = false;
#endif
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
