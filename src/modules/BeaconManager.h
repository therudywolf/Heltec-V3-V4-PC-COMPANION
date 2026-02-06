/*
 * NOCTURNE_OS — BeaconManager: WiFi beacon spam (fake AP list).
 * Sends 802.11 beacon frames with configurable SSIDs. No LoRa.
 */
#pragma once
#include <Arduino.h>

#define BEACON_SSID_MAX 32
#define BEACON_LIST_SIZE 8
#define BEACON_RICKROLL_SIZE 8
#define BEACON_FUNNY_SIZE 12
#define BEACON_PAYLOAD_MAX 128

enum BeaconMode {
  BEACON_MODE_DEFAULT = 0,
  BEACON_MODE_RICK_ROLL,
  BEACON_MODE_FUNNY,
  BEACON_MODE_CUSTOM_LIST
};

class BeaconManager {
public:
  BeaconManager();
  void begin();
  void stop();
  void tick();

  bool isActive() const { return active_; }
  int getBeaconCount() const { return beaconCount_; }
  int getCurrentIndex() const { return currentIndex_; }
  const char *getCurrentSSID() const;
  BeaconMode getMode() const { return mode_; }
  void setMode(BeaconMode mode);
  void nextSSID();
  void prevSSID();
  void addCustomSSID(const char *ssid);
  int getCustomSSIDCount() const { return customSSIDCount_; }

private:
  void buildBeacon(const char *ssid, uint8_t channel);
  void sendBeacon();
  const char **getCurrentSSIDList() const;
  int getCurrentListSize() const;

  bool active_ = false;
  uint8_t beaconBuf_[BEACON_PAYLOAD_MAX];
  size_t beaconLen_ = 0;
  uint8_t bssid_[6];
  int beaconCount_ = 0;
  int currentIndex_ = 0;
  unsigned long lastSendMs_ = 0;
  BeaconMode mode_ = BEACON_MODE_DEFAULT;
  const char *ssidList_[BEACON_LIST_SIZE];
  const char *rickRollList_[BEACON_RICKROLL_SIZE];
  const char *funnyList_[BEACON_FUNNY_SIZE];
  char customSSIDs_[BEACON_LIST_SIZE][BEACON_SSID_MAX];
  int customSSIDCount_ = 0;
  static const unsigned long BEACON_INTERVAL_MS;
};
