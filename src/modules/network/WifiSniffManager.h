/*
 * NOCTURNE_OS тАФ WifiSniffManager: promiscuous WiFi sniffer.
 * Collects APs and clients from beacons/probes; optional EAPOL handshake hint.
 * Enhanced: Packet Monitor, Station Scan, improved EAPOL capture.
 */
#pragma once
#include <Arduino.h>

#define WIFISNIFF_AP_MAX 24
#define WIFISNIFF_SSID_LEN 33
#define WIFISNIFF_MAC_LEN 18
#define WIFISNIFF_STATION_MAX 32
#define WIFISNIFF_PROBE_MAX 32
#define WIFISNIFF_RAW_MAX 16        // Ring buffer of recent raw frame summaries
#define WIFISNIFF_RAW_HEX_LEN 16    // Bytes of header hex retained per raw frame
#define WIFISNIFF_KARMA_SSID_MAX 8  // Distinct probed SSIDs tracked per suspect AP

/* Channel hopping: passive listening only. Cycle 1..13 on a timer so the
 * single radio can survey the whole 2.4 GHz band instead of one channel. */
#define WIFISNIFF_CH_MIN 1
#define WIFISNIFF_CH_MAX 13
#define WIFISNIFF_CH_HOP_INTERVAL_MS 300

enum SniffMode {
  SNIFF_MODE_AP = 0,
  SNIFF_MODE_PACKET_MONITOR,
  SNIFF_MODE_EAPOL_CAPTURE,
  SNIFF_MODE_STATION_SCAN,
  SNIFF_MODE_PROBE_SCAN,       // Probe request scanning
  SNIFF_MODE_CHANNEL_ANALYZER, // Channel analyzer with graph
  SNIFF_MODE_CHANNEL_ACTIVITY, // Channel activity summary
  SNIFF_MODE_PACKET_RATE,      // Packet rate monitoring
  SNIFF_MODE_PINESCAN,         // Pineapple detection
  SNIFF_MODE_MULTISSID,        // MultiSSID detection
  SNIFF_MODE_SIGNAL_STRENGTH,  // Signal strength scan
  SNIFF_MODE_RAW_CAPTURE,      // Raw packet capture
  SNIFF_MODE_AP_STA            // AP + Station combined scan
};

struct WifiSniffAp {
  char ssid[WIFISNIFF_SSID_LEN];
  char bssidStr[WIFISNIFF_MAC_LEN];
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t channel;
  bool hasEapol;
  uint8_t staCount; // Associated stations seen (for AP+STA view)
};

struct WifiStation {
  uint8_t mac[6];
  char macStr[WIFISNIFF_MAC_LEN];
  uint8_t apBSSID[6];
  char apBSSIDStr[WIFISNIFF_MAC_LEN];
  int8_t rssi;
  uint32_t lastSeen;
};

struct ProbeRequest {
  uint8_t mac[6];
  char macStr[WIFISNIFF_MAC_LEN];
  char ssid[WIFISNIFF_SSID_LEN];
  int8_t rssi;
  uint8_t channel;
  uint32_t lastSeen;
  uint32_t count; // How many times this probe was seen
};

struct PacketStats {
  uint32_t mgmtFrames;
  uint32_t dataFrames;
  uint32_t beaconFrames;
  uint32_t deauthFrames;
  uint32_t disassocFrames;   // 802.11 disassoc (subtype 10) - attack indicator
  uint32_t eapolFrames;
  int8_t minRssi;
  int8_t maxRssi;
};

/* RAW_CAPTURE: compact summary of a recently seen frame. We keep a tiny
 * ring buffer of these instead of full packets (RAM-bounded). */
struct RawFrame {
  uint8_t type;    // 0=mgmt 1=ctrl 2=data 3=ext
  uint8_t subtype; // FC subtype nibble
  uint16_t len;    // sig_len of the captured frame
  int8_t rssi;
  uint8_t channel;
  uint8_t hdr[WIFISNIFF_RAW_HEX_LEN]; // first bytes for a hex preview
  uint8_t hdrLen;                     // valid bytes in hdr
};

/* PINESCAN: an AP (BSSID) that has answered probe requests for several
 * different SSIDs is behaving like a karma/"Pineapple" rogue AP. We track,
 * per BSSID, the distinct SSID names it has been observed advertising in
 * probe RESPONSES. A high distinct count is the heuristic flag. */
struct KarmaSuspect {
  uint8_t bssid[6];
  char bssidStr[WIFISNIFF_MAC_LEN];
  char ssids[WIFISNIFF_KARMA_SSID_MAX][WIFISNIFF_SSID_LEN];
  uint8_t ssidCount;
  int8_t rssi;
  uint8_t channel;
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

  // Probe Scan
  int getProbeCount() const { return probeCount_; }
  const ProbeRequest *getProbe(int index) const;

  // Channel Analyzer (for graph)
  void getChannelActivity(uint32_t *channels, int maxChannels) const;

  // Packet Rate
  uint32_t getPacketsPerSecond() const;

  int getPacketCount() const { return packetCount_; }
  uint32_t getDeauthCount() const { return stats_.deauthFrames; }
  uint32_t getDisassocCount() const { return stats_.disassocFrames; }

  // Raw Capture (recent frame ring buffer, newest-first ordering helper)
  int getRawCount() const { return rawCount_; }
  const RawFrame *getRawFrame(int index) const; // index 0 = most recent

  // Pinescan (karma / rogue-AP heuristic)
  int getKarmaCount() const { return karmaCount_; }
  const KarmaSuspect *getKarmaSuspect(int index) const;

  // --- Channel hopping (passive: only changes the listen channel) ---
  void setChannelHop(bool enabled);
  bool isChannelHopping() const { return channelHop_; }
  uint8_t getCurrentChannel() const { return currentHopChannel_; }

private:
  static void promiscuousCb(void *buf, wifi_promiscuous_pkt_type_t type);
  void processBeaconFrame(uint8_t *payload, size_t len, int8_t rssi,
                          uint8_t channel);
  void processProbeRequestFrame(uint8_t *payload, size_t len, int8_t rssi);
  void processProbeResponseFrame(uint8_t *payload, size_t len, int8_t rssi,
                                 uint8_t channel);
  void processDataFrame(uint8_t *payload, size_t len, int8_t rssi);
  void processEapolFrame(uint8_t *payload, size_t len);
  void recordRawFrame(uint8_t *payload, size_t len, int8_t rssi,
                      uint8_t type, uint8_t subtype, uint8_t channel);
  void recordKarma(uint8_t *bssid, const char *ssid, int8_t rssi,
                   uint8_t channel);
  int findApByBSSID(uint8_t *bssid);
  int findStationByMAC(uint8_t *mac);
  int findProbeByMAC(uint8_t *mac);
  int findKarmaByBSSID(uint8_t *bssid);

  bool active_ = false;
  SniffMode mode_ = SNIFF_MODE_AP;
  WifiSniffAp apList_[WIFISNIFF_AP_MAX];
  int apCount_ = 0;
  WifiStation stations_[WIFISNIFF_STATION_MAX];
  int stationCount_ = 0;
  ProbeRequest probes_[WIFISNIFF_PROBE_MAX];
  int probeCount_ = 0;
  int packetCount_ = 0;
  int eapolCount_ = 0;
  PacketStats stats_;
  uint32_t channelActivity_[14]; // Channels 1-14
  uint32_t packetsPerSecond_ = 0;
  uint32_t lastPacketRateCalc_ = 0;
  uint32_t packetCountAtLastCalc_ = 0;
  unsigned long lastSortMs_ = 0;
  unsigned long lastStatsReset_ = 0;

  // Raw capture ring buffer (rawHead_ points at the next slot to write)
  RawFrame rawFrames_[WIFISNIFF_RAW_MAX];
  int rawCount_ = 0;
  int rawHead_ = 0;

  // Pinescan / karma suspects
  KarmaSuspect karma_[WIFISNIFF_AP_MAX];
  int karmaCount_ = 0;

  // Channel hopping state
  bool channelHop_ = false;
  uint8_t currentHopChannel_ = WIFISNIFF_CH_MIN;
  unsigned long lastHopMs_ = 0;
};