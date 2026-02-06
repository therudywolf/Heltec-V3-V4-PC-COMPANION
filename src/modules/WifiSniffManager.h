/*
 * NOCTURNE_OS — WifiSniffManager: promiscuous WiFi sniffer.
 * Collects APs and clients from beacons/probes; optional EAPOL handshake hint.
 * Enhanced: Packet Monitor, Station Scan, improved EAPOL capture.
 */
#pragma once
#include <Arduino.h>

#define WIFISNIFF_AP_MAX 24
#define WIFISNIFF_SSID_LEN 33
#define WIFISNIFF_MAC_LEN 18
#define WIFISNIFF_STATION_MAX 32

enum SniffMode {
  SNIFF_MODE_AP = 0,
  SNIFF_MODE_PACKET_MONITOR,
  SNIFF_MODE_EAPOL_CAPTURE,
  SNIFF_MODE_STATION_SCAN
};

struct WifiSniffAp {
  char ssid[WIFISNIFF_SSID_LEN];
  char bssidStr[WIFISNIFF_MAC_LEN];
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t channel;
  bool hasEapol;
};

struct WifiStation {
  uint8_t mac[6];
  char macStr[WIFISNIFF_MAC_LEN];
  uint8_t apBSSID[6];
  char apBSSIDStr[WIFISNIFF_MAC_LEN];
  int8_t rssi;
  uint32_t lastSeen;
};

struct PacketStats {
  uint32_t mgmtFrames;
  uint32_t dataFrames;
  uint32_t beaconFrames;
  uint32_t deauthFrames;
  uint32_t eapolFrames;
  int8_t minRssi;
  int8_t maxRssi;
};

class WifiSniffManager {
public:
  WifiSniffManager();
  void begin();
  void begin(SniffMode mode);
  void stop();
  void tick();

  bool isActive() const { return active_; }
  SniffMode getMode() const { return mode_; }
  void setMode(SniffMode mode);

  // AP scan
  int getApCount() const { return apCount_; }
  const WifiSniffAp *getAp(int index) const;

  // Packet Monitor
  void getPacketStats(PacketStats &stats) const;

  // EAPOL Capture
  int getEapolCount() const { return eapolCount_; }
  bool hasEapolHandshake(int apIndex) const;

  // Station Scan
  int getStationCount() const { return stationCount_; }
  const WifiStation *getStation(int index) const;

  int getPacketCount() const { return packetCount_; }

private:
  static void promiscuousCb(void *buf, wifi_promiscuous_pkt_type_t type);
  void processBeaconFrame(uint8_t *payload, size_t len, int8_t rssi);
  void processDataFrame(uint8_t *payload, size_t len, int8_t rssi);
  void processEapolFrame(uint8_t *payload, size_t len);
  int findApByBSSID(uint8_t *bssid);
  int findStationByMAC(uint8_t *mac);

  bool active_ = false;
  SniffMode mode_ = SNIFF_MODE_AP;
  WifiSniffAp apList_[WIFISNIFF_AP_MAX];
  int apCount_ = 0;
  WifiStation stations_[WIFISNIFF_STATION_MAX];
  int stationCount_ = 0;
  int packetCount_ = 0;
  int eapolCount_ = 0;
  PacketStats stats_;
  unsigned long lastSortMs_ = 0;
  unsigned long lastStatsReset_ = 0;
};
