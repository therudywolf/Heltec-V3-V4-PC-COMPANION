#include "nocturne/config.h"
#if NOCT_FEATURE_HACKER

/*
 * NOCTURNE_OS тАФ WifiSniffManager: promiscuous callback and AP list.
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

  // RAW_CAPTURE: keep a compact summary of every frame in a ring buffer.
  if (s_instance->mode_ == SNIFF_MODE_RAW_CAPTURE) {
    s_instance->recordRawFrame(payload, len, rssi, frameType, frameSubtype,
                               currentChannel);
  }

  // Process management frames (beacons, probes, etc.)
  if (frameType == 0) {
    if (frameSubtype == 8) { // Beacon -> AP list (used by AP/MULTISSID/SIG/etc.)
      s_instance->processBeaconFrame(payload, len, rssi, currentChannel);
    } else if (frameSubtype == 4) { // Probe Request
      // Probe requests carry the SSIDs that clients are looking for. Useful
      // for PROBE_SCAN; PINESCAN also wants them as the "what is being asked".
      if (s_instance->mode_ == SNIFF_MODE_PROBE_SCAN ||
          s_instance->mode_ == SNIFF_MODE_PINESCAN) {
        s_instance->processProbeRequestFrame(payload, len, rssi);
      }
    } else if (frameSubtype == 5) { // Probe Response
      // A probe response advertises an SSID from a specific BSSID. An AP that
      // answers for MANY different SSIDs is the karma/Pineapple tell.
      if (s_instance->mode_ == SNIFF_MODE_PINESCAN) {
        s_instance->processProbeResponseFrame(payload, len, rssi,
                                              currentChannel);
      }
    }
  }

  // Process data frames for Station Scan and the combined AP+STA view.
  if (frameType == 2 && (s_instance->mode_ == SNIFF_MODE_STATION_SCAN ||
                         s_instance->mode_ == SNIFF_MODE_AP_STA)) {
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
                                          int8_t rssi, uint8_t rxChannel) {
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
  // Fall back to the channel we received on if the DS Parameter Set tag is
  // absent (common while channel hopping).
  if (!channel)
    channel = rxChannel ? rxChannel : 1;
  int apIdx = findApByBSSID(bssid);
  if (apIdx >= 0) {
    // Update existing AP
    apList_[apIdx].rssi = rssi;
    apList_[apIdx].channel = channel;
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
    apList_[apCount_].channel = channel;
    apList_[apCount_].hasEapol = false;
    apList_[apCount_].staCount = 0;
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
    // For AP+STA: bump the per-AP associated-station tally (saturating).
    if (apIdx >= 0 && apList_[apIdx].staCount < 255)
      apList_[apIdx].staCount++;
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

void WifiSniffManager::processProbeResponseFrame(uint8_t *payload, size_t len,
                                                 int8_t rssi,
                                                 uint8_t rxChannel) {
  // Probe Response layout matches a beacon's fixed header: BSSID at offset 16,
  // tagged params (incl. SSID, tag 0) start at offset 36 (after the 12-byte
  // fixed body of timestamp/interval/capabilities).
  if (len < 38)
    return;
  uint8_t *bssid = payload + 16;
  uint8_t *body = payload + 24;
  size_t pos = 12;
  char ssid[WIFISNIFF_SSID_LEN];
  ssid[0] = '\0';
  uint8_t channel = 0;
  while (pos + 2 <= len - 24) {
    uint8_t tagId = body[pos];
    uint8_t tagLen = body[pos + 1];
    pos += 2;
    if (pos + tagLen > len - 24)
      break;
    if (tagId == 0 && tagLen > 0 && tagLen < WIFISNIFF_SSID_LEN) {
      memcpy(ssid, body + pos, tagLen);
      ssid[tagLen] = '\0';
    } else if (tagId == 3 && tagLen >= 1) {
      channel = body[pos];
    }
    pos += tagLen;
  }
  if (!channel)
    channel = rxChannel ? rxChannel : 1;
  // A hidden/empty SSID in a probe response is not interesting for karma.
  if (ssid[0] == '\0')
    return;
  recordKarma(bssid, ssid, rssi, channel);
}

void WifiSniffManager::recordKarma(uint8_t *bssid, const char *ssid,
                                   int8_t rssi, uint8_t channel) {
  int idx = findKarmaByBSSID(bssid);
  if (idx < 0) {
    if (karmaCount_ >= WIFISNIFF_AP_MAX)
      return; // table full
    idx = karmaCount_++;
    memset(&karma_[idx], 0, sizeof(KarmaSuspect));
    memcpy(karma_[idx].bssid, bssid, 6);
    snprintf(karma_[idx].bssidStr, WIFISNIFF_MAC_LEN,
             "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5]);
  }
  karma_[idx].rssi = rssi;
  karma_[idx].channel = channel;
  // Add this SSID to the suspect's distinct-SSID set if not already present.
  for (int i = 0; i < karma_[idx].ssidCount; i++) {
    if (strncmp(karma_[idx].ssids[i], ssid, WIFISNIFF_SSID_LEN - 1) == 0)
      return; // already counted
  }
  if (karma_[idx].ssidCount < WIFISNIFF_KARMA_SSID_MAX) {
    strncpy(karma_[idx].ssids[karma_[idx].ssidCount], ssid,
            WIFISNIFF_SSID_LEN - 1);
    karma_[idx].ssids[karma_[idx].ssidCount][WIFISNIFF_SSID_LEN - 1] = '\0';
    karma_[idx].ssidCount++;
  }
}

void WifiSniffManager::recordRawFrame(uint8_t *payload, size_t len, int8_t rssi,
                                      uint8_t type, uint8_t subtype,
                                      uint8_t channel) {
  RawFrame &f = rawFrames_[rawHead_];
  f.type = type;
  f.subtype = subtype;
  f.len = (uint16_t)len;
  f.rssi = rssi;
  f.channel = channel;
  uint8_t copy = (len < WIFISNIFF_RAW_HEX_LEN) ? (uint8_t)len
                                               : WIFISNIFF_RAW_HEX_LEN;
  memcpy(f.hdr, payload, copy);
  f.hdrLen = copy;
  rawHead_ = (rawHead_ + 1) % WIFISNIFF_RAW_MAX;
  if (rawCount_ < WIFISNIFF_RAW_MAX)
    rawCount_++;
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
  memset(rawFrames_, 0, sizeof(rawFrames_));
  memset(karma_, 0, sizeof(karma_));
  stats_.minRssi = 0;
  stats_.maxRssi = -128;
  mode_ = SNIFF_MODE_AP;
  probeCount_ = 0;
  packetsPerSecond_ = 0;
  lastPacketRateCalc_ = 0;
  packetCountAtLastCalc_ = 0;
  rawCount_ = 0;
  rawHead_ = 0;
  karmaCount_ = 0;
  channelHop_ = false;
  currentHopChannel_ = WIFISNIFF_CH_MIN;
  lastHopMs_ = 0;
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
  rawCount_ = 0;
  rawHead_ = 0;
  karmaCount_ = 0;
  memset(&stats_, 0, sizeof(stats_));
  memset(channelActivity_, 0, sizeof(channelActivity_));
  memset(rawFrames_, 0, sizeof(rawFrames_));
  memset(karma_, 0, sizeof(karma_));
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

  // Channel hopping default: enabled for survey-style modes that benefit from
  // seeing the whole band, off for modes that lock onto one channel/target.
  // PASSIVE only -- this just retunes the listening radio, never transmits.
  switch (mode) {
  case SNIFF_MODE_AP:
  case SNIFF_MODE_STATION_SCAN:
  case SNIFF_MODE_PROBE_SCAN:
  case SNIFF_MODE_PINESCAN:
  case SNIFF_MODE_MULTISSID:
  case SNIFF_MODE_SIGNAL_STRENGTH:
  case SNIFF_MODE_RAW_CAPTURE:
  case SNIFF_MODE_AP_STA:
    channelHop_ = true;
    break;
  default:
    // PACKET_MONITOR / CHANNEL_* / PACKET_RATE / EAPOL: stay put so per-channel
    // counters and handshake capture remain coherent.
    channelHop_ = false;
    break;
  }
  currentHopChannel_ = WIFISNIFF_CH_MIN;
  lastHopMs_ = millis();
  esp_wifi_set_channel(currentHopChannel_, WIFI_SECOND_CHAN_NONE);

  // Bounded, branch-free mode name lookup (avoids out-of-range indexing).
  const char *modeName;
  switch (mode) {
  case SNIFF_MODE_AP: modeName = "AP"; break;
  case SNIFF_MODE_PACKET_MONITOR: modeName = "Packet Monitor"; break;
  case SNIFF_MODE_EAPOL_CAPTURE: modeName = "EAPOL Capture"; break;
  case SNIFF_MODE_STATION_SCAN: modeName = "Station Scan"; break;
  case SNIFF_MODE_PROBE_SCAN: modeName = "Probe Scan"; break;
  case SNIFF_MODE_CHANNEL_ANALYZER: modeName = "Channel Analyzer"; break;
  case SNIFF_MODE_CHANNEL_ACTIVITY: modeName = "Channel Activity"; break;
  case SNIFF_MODE_PACKET_RATE: modeName = "Packet Rate"; break;
  case SNIFF_MODE_PINESCAN: modeName = "Pinescan"; break;
  case SNIFF_MODE_MULTISSID: modeName = "MultiSSID"; break;
  case SNIFF_MODE_SIGNAL_STRENGTH: modeName = "Signal Strength"; break;
  case SNIFF_MODE_RAW_CAPTURE: modeName = "Raw Capture"; break;
  case SNIFF_MODE_AP_STA: modeName = "AP+STA"; break;
  default: modeName = "?"; break;
  }
  Serial.printf("[SNIFF] Promiscuous ACTIVE (mode: %s, hop: %s).\n", modeName,
                channelHop_ ? "on" : "off");
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

  // --- Channel hopping (passive listen retune only, never transmits) ---
  // Timer-driven cycle across channels 1..13 so a single radio surveys the
  // whole 2.4 GHz band. Toggleable via setChannelHop(); when off we stay on
  // whatever channel was last selected.
  if (channelHop_ && active_) {
    if (now - lastHopMs_ >= WIFISNIFF_CH_HOP_INTERVAL_MS) {
      currentHopChannel_++;
      if (currentHopChannel_ > WIFISNIFF_CH_MAX)
        currentHopChannel_ = WIFISNIFF_CH_MIN;
      esp_wifi_set_channel(currentHopChannel_, WIFI_SECOND_CHAN_NONE);
      lastHopMs_ = now;
    }
  }

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

void WifiSniffManager::setChannelHop(bool enabled) {
  channelHop_ = enabled;
  lastHopMs_ = millis();
  if (enabled && active_) {
    // Resume hopping from the current channel immediately on next tick.
    esp_wifi_set_channel(currentHopChannel_, WIFI_SECOND_CHAN_NONE);
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

int WifiSniffManager::findKarmaByBSSID(uint8_t *bssid) {
  for (int i = 0; i < karmaCount_; i++) {
    if (memcmp(karma_[i].bssid, bssid, 6) == 0)
      return i;
  }
  return -1;
}

const WifiSniffAp *WifiSniffManager::getAp(int index) const {
  if (index < 0 || index >= apCount_)
    return nullptr;
  return &apList_[index];
}

const RawFrame *WifiSniffManager::getRawFrame(int index) const {
  // index 0 == most recent. rawHead_ points one past the newest entry.
  if (index < 0 || index >= rawCount_)
    return nullptr;
  int slot = (rawHead_ - 1 - index + WIFISNIFF_RAW_MAX * 2) % WIFISNIFF_RAW_MAX;
  return &rawFrames_[slot];
}

const KarmaSuspect *WifiSniffManager::getKarmaSuspect(int index) const {
  if (index < 0 || index >= karmaCount_)
    return nullptr;
  return &karma_[index];
}

#endif // NOCT_FEATURE_HACKER
