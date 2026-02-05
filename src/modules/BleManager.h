/*
 * NOCTURNE_OS — BLE Phantom Spammer (SIGINT).
 * Broadcasts pairing-style advertisements (Apple, Google Fast Pair, Samsung).
 * WiFi MUST be off while active (2.4 GHz antenna conflict).
 */
#pragma once
#include <Arduino.h>

#define BLE_PHANTOM_ROTATE_MS 100
#define BLE_PHANTOM_PAYLOAD_COUNT 4

class BleManager {
public:
  BleManager();
  void
  begin(); // Init NimBLE and start advertising (call after WiFi.mode(WIFI_OFF))
  void stop(); // Stop advertising and deinit
  void tick(); // Rotate payload every 100ms, update packet count

  bool isActive() const { return active_; }
  int getPacketCount() const { return packetCount_; }
  /** Last payload index (0=Apple, 1=Google, 2=Samsung, 3=Windows) for UI glitch
   * trigger */
  int getCurrentPayloadIndex() const { return currentPayloadIndex_; }

private:
  void startSpam();
  void setPayload(int index);

  bool active_ = false;
  int packetCount_ = 0;
  int currentPayloadIndex_ = 0;
  unsigned long lastRotateMs_ = 0;
};
