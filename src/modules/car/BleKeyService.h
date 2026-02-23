/*
 * NOCTURNE_OS — BLE key: peripheral for proximity unlock/lock.
 * Phone connects = unlock, disconnects (after debounce) = lock.
 */
#ifndef NOCTURNE_BLE_KEY_SERVICE_H
#define NOCTURNE_BLE_KEY_SERVICE_H

#include <Arduino.h>
#include <cstdint>

class BleKeyService {
 public:
  BleKeyService();
  void begin();
  void end();
  void tick();

  bool isConnected() const { return connected_; }
  bool isActive() const { return active_; }

  void setConnectionCallback(void (*cb)(bool connected)) { connectionCb_ = cb; }

  /** Called from NimBLE server callbacks (internal). */
  void onConnect();
  void onDisconnect();

 private:
  bool active_ = false;
  bool connected_ = false;
  void (*connectionCb_)(bool) = nullptr;

  /** Debounce: delay before reporting disconnect (avoid brief dropouts). */
  static const unsigned long kDisconnectDebounceMs = 2500;
  unsigned long disconnectReportedAt_ = 0;
  bool disconnectPending_ = false;
};

#endif
