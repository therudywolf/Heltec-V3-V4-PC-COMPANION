/*
 * NOCTURNE_OS — WifiSniffManager: promiscuous WiFi sniffer.
 * Collects APs and clients from beacons/probes; optional EAPOL handshake hint.
 */
#pragma once
#include <Arduino.h>

#define WIFISNIFF_AP_MAX 24
#define WIFISNIFF_SSID_LEN 33
#define WIFISNIFF_MAC_LEN 18

struct WifiSniffAp {
  char ssid[WIFISNIFF_SSID_LEN];
  char bssidStr[WIFISNIFF_MAC_LEN];
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t channel;
  bool hasEapol;
};

class WifiSniffManager {
public:
  WifiSniffManager();
  void begin();
  void stop();
  void tick();

  bool isActive() const { return active_; }
  int getApCount() const { return apCount_; }
  const WifiSniffAp *getAp(int index) const;
  int getPacketCount() const { return packetCount_; }
  int getEapolCount() const { return eapolCount_; }

private:
  static void promiscuousCb(void *buf, wifi_promiscuous_pkt_type_t type);

  bool active_ = false;
  WifiSniffAp apList_[WIFISNIFF_AP_MAX];
  int apCount_ = 0;
  int packetCount_ = 0;
  int eapolCount_ = 0;
  unsigned long lastSortMs_ = 0;
};
