/*
 * NOCTURNE_OS — WifiSniffManager: promiscuous callback and AP list.
 */
#include "WifiSniffManager.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

static WifiSniffManager *s_instance = nullptr;

void WifiSniffManager::promiscuousCb(void *buf,
                                     wifi_promiscuous_pkt_type_t type) {
  if (s_instance == nullptr || !s_instance->active_)
    return;
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  if (pkt == nullptr)
    return;
  s_instance->packetCount_++;
  uint8_t *payload = pkt->payload;
  size_t len = pkt->rx_ctrl.sig_len;
  int8_t rssi = pkt->rx_ctrl.rssi;

  if (len < 24)
    return;

  uint8_t fc0 = payload[0], fc1 = payload[1];
  uint8_t frameType = (fc0 & 0x0C) >> 2;
  uint8_t frameSubtype = (fc0 & 0xF0) >> 4;

  // Get current channel from packet
  uint8_t currentChannel = pkt->rx_ctrl.channel;
  if (currentChannel >= 1 && currentChannel <= 14) {
    s_instance->channelActivity_[currentChannel - 1]++;
  }

  // Packet Monitor: count all frame types
  if (s_instance->mode_ == SNIFF_MODE_PACKET_MONITOR ||
      s_instance->mode_ == SNIFF_MODE_CHANNEL_ANALYZER ||
      s_instance->mode_ == SNIFF_MODE_CHANNEL_ACTIVITY ||
      s_instance->mode_ == SNIFF_MODE_PACKET_RATE) {
    if (frameType == 0) {
      s_instance->stats_.mgmtFrames++;
      if (frameSubtype == 8) {
        s_instance->stats_.beaconFrames++;
      } else if (frameSubtype == 12) {
        s_instance->stats_.deauthFrames++;
      }
    } else if (frameType == 2) {
      s_instance->stats_.dataFrames++;
    }
    if (rssi < s_instance->stats_.minRssi)
      s_instance->stats_.minRssi = rssi;
    if (rssi > s_instance->stats_.maxRssi)
      s_instance->stats_.maxRssi = rssi;
  }

  // Calculate packet rate for Packet Rate mode
  if (s_instance->mode_ == SNIFF_MODE_PACKET_RATE) {
    unsigned long now = millis();
    if (now - s_instance->lastPacketRateCalc_ >= 1000) {
      uint32_t packetsDelta =
          s_instance->packetCount_ - s_instance->packetCountAtLastCalc_;
      s_instance->packetsPerSecond_ = packetsDelta;
      s_instance->packetCountAtLastCalc_ = s_instance->packetCount_;
      s_instance->lastPacketRateCalc_ = now;
    }
  }

  // Process management frames (beacons, probes, etc.)
  if (frameType == 0) {
    if (frameSubtype == 8) { // Beacon
      s_instance->processBeaconFrame(payload, len, rssi);
    } else if (frameSubtype == 4 &&
               s_instance->mode_ == SNIFF_MODE_PROBE_SCAN) { // Probe Request
      s_instance->processProbeRequestFrame(payload, len, rssi);
    }
  }

  // Process data frames for Station Scan
  if (frameType == 2 && s_instance->mode_ == SNIFF_MODE_STATION_SCAN) {
    s_instance->processDataFrame(payload, len, rssi);
  }

  // Process EAPOL frames
  if (len >= 32) {
    uint8_t *snap = payload + 30;
    if (snap[0] == 0x88 && snap[1] == 0x8e) {
      s_instance->processEapolFrame(payload, len);
    }
  }
}

void WifiSniffManager::processBeaconFrame(uint8_t *payload, size_t len,
                                          int8_t rssi) {
  if (len < 36)
    return;
  uint8_t *bssid = payload + 16;
  uint8_t *body = payload + 24;
  size_t pos = 12;
  char ssid[WIFISNIFF_SSID_LEN];
  ssid[0] = '\0';
  uint8_t channel = 0;
  while (pos + 2 <= len) {
    uint8_t tagId = body[pos];
    uint8_t tagLen = body[pos + 1];
    pos += 2;
    if (pos + tagLen > len)
      break;
    if (tagId == 0 && tagLen > 0 && tagLen < WIFISNIFF_SSID_LEN) {
      memcpy(ssid, body + pos, tagLen);
      ssid[tagLen] = '\0';
    } else if (tagId == 3 && tagLen >= 1) {
      channel = body[pos];
    }
    pos += tagLen;
  }
  int apIdx = findApByBSSID(bssid);
  if (apIdx >= 0) {
    // Update existing AP
    apList_[apIdx].rssi = rssi;
    apList_[apIdx].channel = channel ? channel : 1;
    if (ssid[0])
      strncpy(apList_[apIdx].ssid, ssid, WIFISNIFF_SSID_LEN - 1);
    apList_[apIdx].ssid[WIFISNIFF_SSID_LEN - 1] = '\0';
  } else if (apCount_ < WIFISNIFF_AP_MAX - 1) {
    // Add new AP
    memcpy(apList_[apCount_].bssid, bssid, 6);
    snprintf(apList_[apCount_].bssidStr, WIFISNIFF_MAC_LEN,
             "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5]);
    strncpy(apList_[apCount_].ssid, ssid[0] ? ssid : "(hidden)",
            WIFISNIFF_SSID_LEN - 1);
    apList_[apCount_].ssid[WIFISNIFF_SSID_LEN - 1] = '\0';
    apList_[apCount_].rssi = rssi;
    apList_[apCount_].channel = channel ? channel : 1;
    apList_[apCount_].hasEapol = false;
    apCount_++;
  }
}

void WifiSniffManager::processDataFrame(uint8_t *payload, size_t len,
                                        int8_t rssi) {
  if (len < 24)
    return;
  // Data frame: DA (offset 4), SA (offset 10), BSSID (offset 16)
  uint8_t *da = payload + 4;
  uint8_t *sa = payload + 10;
  uint8_t *bssid = payload + 16;

  // Check if BSSID matches any known AP
  int apIdx = findApByBSSID(bssid);
  if (apIdx < 0)
    return; // Not from a known AP

  // Determine if SA or DA is the station
  uint8_t *stationMAC = nullptr;
  bool apIsSrc = false;
  if (memcmp(sa, bssid, 6) == 0) {
    // AP is source, DA is station
    stationMAC = da;
    apIsSrc = true;
  } else if (memcmp(da, bssid, 6) == 0) {
    // AP is destination, SA is station
    stationMAC = sa;
    apIsSrc = false;
  } else {
    return; // Not a frame involving this AP
  }

  // Skip broadcast
  if (memcmp(stationMAC, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0)
    return;

  // Find or add station
  int staIdx = findStationByMAC(stationMAC);
  if (staIdx >= 0) {
    // Update existing station
    stations_[staIdx].rssi = rssi;
    stations_[staIdx].lastSeen = millis();
  } else if (stationCount_ < WIFISNIFF_STATION_MAX - 1) {
    // Add new station
    memcpy(stations_[stationCount_].mac, stationMAC, 6);
    snprintf(stations_[stationCount_].macStr, WIFISNIFF_MAC_LEN,
             "%02X:%02X:%02X:%02X:%02X:%02X", stationMAC[0], stationMAC[1],
             stationMAC[2], stationMAC[3], stationMAC[4], stationMAC[5]);
    memcpy(stations_[stationCount_].apBSSID, bssid, 6);
    snprintf(stations_[stationCount_].apBSSIDStr, WIFISNIFF_MAC_LEN,
             "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5]);
    stations_[stationCount_].rssi = rssi;
    stations_[stationCount_].lastSeen = millis();
    stationCount_++;
  }
}

void WifiSniffManager::processProbeRequestFrame(uint8_t *payload, size_t len,
                                                int8_t rssi) {
  if (len < 24)
    return;
  // Probe Request: SA (offset 10) is the client MAC
  uint8_t *clientMAC = payload + 10;
  uint8_t channel = 0;
  char ssid[WIFISNIFF_SSID_LEN];
  ssid[0] = '\0';

  // Extract SSID from tagged parameters (starts at offset 24)
  if (len > 24) {
    uint8_t *body = payload + 24;
    size_t pos = 0;
    while (pos + 2 <= len - 24) {
      uint8_t tagId = body[pos];
      uint8_t tagLen = body[pos + 1];
      pos += 2;
      if (pos + tagLen > len - 24)
        break;
      if (tagId == 0 && tagLen > 0 && tagLen < WIFISNIFF_SSID_LEN) {
        memcpy(ssid, body + pos, tagLen);
        ssid[tagLen] = '\0';
      }
      pos += tagLen;
    }
  }

  // Find or add probe request
  int probeIdx = findProbeByMAC(clientMAC);
  if (probeIdx >= 0) {
    // Update existing probe
    probes_[probeIdx].rssi = rssi;
    probes_[probeIdx].lastSeen = millis();
    probes_[probeIdx].count++;
    if (ssid[0] && strcmp(probes_[probeIdx].ssid, ssid) != 0) {
      // SSID changed, update it
      strncpy(probes_[probeIdx].ssid, ssid, WIFISNIFF_SSID_LEN - 1);
      probes_[probeIdx].ssid[WIFISNIFF_SSID_LEN - 1] = '\0';
    }
  } else if (probeCount_ < WIFISNIFF_PROBE_MAX - 1) {
    // Add new probe request
    memcpy(probes_[probeCount_].mac, clientMAC, 6);
    snprintf(probes_[probeCount_].macStr, WIFISNIFF_MAC_LEN,
             "%02X:%02X:%02X:%02X:%02X:%02X", clientMAC[0], clientMAC[1],
             clientMAC[2], clientMAC[3], clientMAC[4], clientMAC[5]);
    strncpy(probes_[probeCount_].ssid, ssid[0] ? ssid : "(broadcast)",
            WIFISNIFF_SSID_LEN - 1);
    probes_[probeCount_].ssid[WIFISNIFF_SSID_LEN - 1] = '\0';
    probes_[probeCount_].rssi = rssi;
    probes_[probeCount_].channel = channel;
    probes_[probeCount_].lastSeen = millis();
    probes_[probeCount_].count = 1;
    probeCount_++;
  }
}

void WifiSniffManager::processEapolFrame(uint8_t *payload, size_t len) {
  eapolCount_++;
  // Check if EAPOL is from/to a known AP
  if (len < 32)
    return;
  uint8_t *bssid1 = payload + 16;
  uint8_t *bssid2 = payload + 4;
  for (int j = 0; j < apCount_; j++) {
    if (memcmp(bssid1, apList_[j].bssid, 6) == 0 ||
        memcmp(bssid2, apList_[j].bssid, 6) == 0) {
      apList_[j].hasEapol = true;
      if (mode_ == SNIFF_MODE_EAPOL_CAPTURE) {
        Serial.printf("[SNIFF] EAPOL handshake captured for AP: %s\n",
                      apList_[j].ssid);
      }
    }
  }
}

WifiSniffManager::WifiSniffManager() {
  memset(apList_, 0, sizeof(apList_));
  memset(stations_, 0, sizeof(stations_));
  memset(probes_, 0, sizeof(probes_));
  memset(channelActivity_, 0, sizeof(channelActivity_));
  memset(&stats_, 0, sizeof(stats_));
  stats_.minRssi = 0;
  stats_.maxRssi = -128;
  mode_ = SNIFF_MODE_AP;
  probeCount_ = 0;
  packetsPerSecond_ = 0;
  lastPacketRateCalc_ = 0;
  packetCountAtLastCalc_ = 0;
}

void WifiSniffManager::begin() { begin(SNIFF_MODE_AP); }

void WifiSniffManager::begin(SniffMode mode) {
  if (active_)
    return;
  s_instance = this;
  mode_ = mode;
  apCount_ = 0;
  stationCount_ = 0;
  probeCount_ = 0;
  packetCount_ = 0;
  eapolCount_ = 0;
  memset(&stats_, 0, sizeof(stats_));
  memset(channelActivity_, 0, sizeof(channelActivity_));
  stats_.minRssi = 0;
  stats_.maxRssi = -128;
  lastStatsReset_ = millis();
  lastPacketRateCalc_ = millis();
  packetCountAtLastCalc_ = 0;
  packetsPerSecond_ = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(50);
  esp_wifi_set_promiscuous_rx_cb(&promiscuousCb);
  esp_wifi_set_promiscuous(true);
  active_ = true;
  const char *modeNames[] = {"AP", "Packet Monitor", "EAPOL Capture",
                             "Station Scan"};
  Serial.printf("[SNIFF] Promiscuous ACTIVE (mode: %s).\n", modeNames[mode]);
}

void WifiSniffManager::setMode(SniffMode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;
  // Reset counters when switching modes
  if (mode == SNIFF_MODE_PACKET_MONITOR) {
    memset(&stats_, 0, sizeof(stats_));
    stats_.minRssi = 0;
    stats_.maxRssi = -128;
    lastStatsReset_ = millis();
  }
}

void WifiSniffManager::stop() {
  if (!active_)
    return;
  esp_wifi_set_promiscuous(false);
  s_instance = nullptr;
  active_ = false;
  Serial.println("[SNIFF] Promiscuous STOP.");
}

void WifiSniffManager::tick() {
  (void)lastSortMs_;
  unsigned long now = millis();

  // Reset packet stats every 10 seconds for Packet Monitor mode
  if (mode_ == SNIFF_MODE_PACKET_MONITOR ||
      mode_ == SNIFF_MODE_CHANNEL_ANALYZER) {
    if (now - lastStatsReset_ > 10000) {
      // Keep max values, reset counters
      stats_.mgmtFrames = 0;
      stats_.dataFrames = 0;
      stats_.beaconFrames = 0;
      stats_.deauthFrames = 0;
      stats_.eapolFrames = 0;
      lastStatsReset_ = now;
    }
  }

  // Reset channel activity periodically for Channel Activity mode
  if (mode_ == SNIFF_MODE_CHANNEL_ACTIVITY) {
    if (now - lastStatsReset_ > 5000) {
      memset(channelActivity_, 0, sizeof(channelActivity_));
      lastStatsReset_ = now;
    }
  }
}

void WifiSniffManager::getPacketStats(PacketStats &stats) const {
  memcpy(&stats, &stats_, sizeof(PacketStats));
}

bool WifiSniffManager::hasEapolHandshake(int apIndex) const {
  if (apIndex < 0 || apIndex >= apCount_)
    return false;
  return apList_[apIndex].hasEapol;
}

const WifiStation *WifiSniffManager::getStation(int index) const {
  if (index < 0 || index >= stationCount_)
    return nullptr;
  return &stations_[index];
}

const ProbeRequest *WifiSniffManager::getProbe(int index) const {
  if (index < 0 || index >= probeCount_)
    return nullptr;
  return &probes_[index];
}

void WifiSniffManager::getChannelActivity(uint32_t *channels,
                                          int maxChannels) const {
  int count = maxChannels < 14 ? maxChannels : 14;
  for (int i = 0; i < count; i++) {
    channels[i] = channelActivity_[i];
  }
}

uint32_t WifiSniffManager::getPacketsPerSecond() const {
  return packetsPerSecond_;
}

int WifiSniffManager::findApByBSSID(uint8_t *bssid) {
  for (int i = 0; i < apCount_; i++) {
    if (memcmp(apList_[i].bssid, bssid, 6) == 0)
      return i;
  }
  return -1;
}

int WifiSniffManager::findStationByMAC(uint8_t *mac) {
  for (int i = 0; i < stationCount_; i++) {
    if (memcmp(stations_[i].mac, mac, 6) == 0)
      return i;
  }
  return -1;
}

int WifiSniffManager::findProbeByMAC(uint8_t *mac) {
  for (int i = 0; i < probeCount_; i++) {
    if (memcmp(probes_[i].mac, mac, 6) == 0)
      return i;
  }
  return -1;
}

const WifiSniffAp *WifiSniffManager::getAp(int index) const {
  if (index < 0 || index >= apCount_)
    return nullptr;
  return &apList_[index];
}
