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
  if (len < 24)
    return;
  uint8_t fc0 = payload[0], fc1 = payload[1];
  uint8_t frameType = (fc0 & 0x0C) >> 2;
  uint8_t frameSubtype = (fc0 & 0xF0) >> 4;
  if (frameType != 0)
    return;
  if (frameSubtype == 8) {
    if (len < 36)
      return;
    uint8_t *bssid = payload + 16;
    uint8_t *body = payload + 24;
    uint32_t ts = (uint32_t)body[0] | ((uint32_t)body[1] << 8) |
                  ((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 24);
    (void)ts;
    uint16_t interval = body[8] | (body[9] << 8);
    (void)interval;
    uint16_t capab = body[10] | (body[11] << 8);
    (void)capab;
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
    memcpy(s_instance->apList_[s_instance->apCount_].bssid, bssid, 6);
    snprintf(s_instance->apList_[s_instance->apCount_].bssidStr,
             WIFISNIFF_MAC_LEN, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0],
             bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    strncpy(s_instance->apList_[s_instance->apCount_].ssid,
            ssid[0] ? ssid : "(hidden)", WIFISNIFF_SSID_LEN - 1);
    s_instance->apList_[s_instance->apCount_].ssid[WIFISNIFF_SSID_LEN - 1] =
        '\0';
    s_instance->apList_[s_instance->apCount_].rssi = pkt->rx_ctrl.rssi;
    s_instance->apList_[s_instance->apCount_].channel = channel ? channel : 1;
    s_instance->apList_[s_instance->apCount_].hasEapol = false;
    int i = s_instance->apCount_;
    for (int j = 0; j < i; j++) {
      if (memcmp(s_instance->apList_[j].bssid, bssid, 6) == 0) {
        s_instance->apList_[j].rssi = s_instance->apList_[i].rssi;
        s_instance->apList_[j].channel = s_instance->apList_[i].channel;
        if (ssid[0])
          strncpy(s_instance->apList_[j].ssid, ssid, WIFISNIFF_SSID_LEN - 1);
        s_instance->apList_[j].ssid[WIFISNIFF_SSID_LEN - 1] = '\0';
        return;
      }
    }
    if (s_instance->apCount_ < WIFISNIFF_AP_MAX - 1)
      s_instance->apCount_++;
  }
  if (len >= 32) {
    uint8_t *snap = payload + 30;
    if (snap[0] == 0x88 && snap[1] == 0x8e) {
      s_instance->eapolCount_++;
      for (int j = 0; j < s_instance->apCount_; j++) {
        if (memcmp(payload + 16, s_instance->apList_[j].bssid, 6) == 0 ||
            memcmp(payload + 4, s_instance->apList_[j].bssid, 6) == 0)
          s_instance->apList_[j].hasEapol = true;
      }
    }
  }
}

WifiSniffManager::WifiSniffManager() { memset(apList_, 0, sizeof(apList_)); }

void WifiSniffManager::begin() {
  if (active_)
    return;
  s_instance = this;
  apCount_ = 0;
  packetCount_ = 0;
  eapolCount_ = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(50);
  esp_wifi_set_promiscuous_rx_cb(&promiscuousCb);
  esp_wifi_set_promiscuous(true);
  active_ = true;
  Serial.println("[SNIFF] Promiscuous ACTIVE.");
}

void WifiSniffManager::stop() {
  if (!active_)
    return;
  esp_wifi_set_promiscuous(false);
  s_instance = nullptr;
  active_ = false;
  Serial.println("[SNIFF] Promiscuous STOP.");
}

void WifiSniffManager::tick() { (void)lastSortMs_; }

const WifiSniffAp *WifiSniffManager::getAp(int index) const {
  if (index < 0 || index >= apCount_)
    return nullptr;
  return &apList_[index];
}
