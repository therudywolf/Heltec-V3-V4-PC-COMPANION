/*
 * NOCTURNE_OS — BeaconManager: WiFi beacon spam (fake AP list).
 * Sends 802.11 beacon frames with configurable SSIDs. No LoRa.
 */
#pragma once
#include <Arduino.h>

#define BEACON_SSID_MAX 32
#define BEACON_LIST_SIZE 8
#define BEACON_PAYLOAD_MAX 128

class BeaconManager {
public:
  BeaconManager();
  void begin();
  void stop();
  void tick();

  bool isActive() const { return active_; }
  int getBeaconCount() const { return beaconCount_; }
  int getCurrentIndex() const { return currentIndex_; }
  const char *getCurrentSSID() const { return ssidList_[currentIndex_]; }
  void nextSSID();
  void prevSSID();

private:
  void buildBeacon(const char *ssid, uint8_t channel);
  void sendBeacon();

  bool active_ = false;
  uint8_t beaconBuf_[BEACON_PAYLOAD_MAX];
  size_t beaconLen_ = 0;
  uint8_t bssid_[6];
  int beaconCount_ = 0;
  int currentIndex_ = 0;
  unsigned long lastSendMs_ = 0;
  const char *ssidList_[BEACON_LIST_SIZE];
  static const unsigned long BEACON_INTERVAL_MS;
};
