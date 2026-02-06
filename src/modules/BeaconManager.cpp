/*
 * NOCTURNE_OS — BeaconManager: 802.11 beacon frame injection.
 */
#include "BeaconManager.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

static const uint8_t DEAUTH_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t RATES_TAG[10] = {0x01, 0x08, 0x82, 0x84, 0x8b,
                                      0x96, 0x24, 0x30, 0x48, 0x6c};
static const uint8_t DS_TAG[3] = {0x03, 0x01, 0x06};

const unsigned long BeaconManager::BEACON_INTERVAL_MS = 80;

BeaconManager::BeaconManager() {
  memset(beaconBuf_, 0, sizeof(beaconBuf_));
  bssid_[0] = 0x02;
  bssid_[1] = 0x12;
  bssid_[2] = 0x34;
  bssid_[3] = 0x56;
  bssid_[4] = 0x78;
  bssid_[5] = 0x9A;
  const char *defaultList[BEACON_LIST_SIZE] = {
      "Free WiFi", "Starbucks",     "Airport_Guest", "Hotel_Login",
      "NOCTURNE",  "Guest_Network", "Public_WiFi",   "Connect_Me"};
  for (int i = 0; i < BEACON_LIST_SIZE; i++)
    ssidList_[i] = defaultList[i];
}

void BeaconManager::buildBeacon(const char *ssid, uint8_t channel) {
  size_t ssidLen = strlen(ssid);
  if (ssidLen > BEACON_SSID_MAX)
    ssidLen = BEACON_SSID_MAX;

  uint8_t *p = beaconBuf_;
  *p++ = 0x80;
  *p++ = 0x00;
  *p++ = 0x00;
  *p++ = 0x00;
  memcpy(p, DEAUTH_BROADCAST, 6);
  p += 6;
  memcpy(p, bssid_, 6);
  p += 6;
  memcpy(p, bssid_, 6);
  p += 6;
  *p++ = 0x00;
  *p++ = 0x00;
  for (int i = 0; i < 8; i++)
    *p++ = 0x00;
  *p++ = 0x64;
  *p++ = 0x00;
  *p++ = 0x01;
  *p++ = 0x00;
  *p++ = 0x00;
  *p++ = (uint8_t)ssidLen;
  memcpy(p, ssid, ssidLen);
  p += ssidLen;
  memcpy(p, RATES_TAG, sizeof(RATES_TAG));
  p += sizeof(RATES_TAG);
  *p++ = 0x03;
  *p++ = 0x01;
  *p++ = channel;
  beaconLen_ = p - beaconBuf_;
}

void BeaconManager::sendBeacon() {
  if (WiFi.getMode() == WIFI_OFF || beaconLen_ < 36)
    return;
  uint8_t ch = 1 + (currentIndex_ % 13);
  if (ch > 13)
    ch = 6;
  buildBeacon(ssidList_[currentIndex_], ch);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  if (esp_wifi_80211_tx(WIFI_IF_STA, beaconBuf_, (int)beaconLen_, false) ==
      ESP_OK)
    beaconCount_++;
}

void BeaconManager::begin() {
  if (active_)
    return;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(50);
  esp_wifi_set_promiscuous(true);
  active_ = true;
  beaconCount_ = 0;
  lastSendMs_ = millis();
  buildBeacon(ssidList_[currentIndex_], 6);
  Serial.println("[BEACON] Beacon spam ACTIVE.");
}

void BeaconManager::stop() {
  if (!active_)
    return;
  esp_wifi_set_promiscuous(false);
  active_ = false;
  Serial.println("[BEACON] Beacon spam STOP.");
}

void BeaconManager::tick() {
  if (!active_)
    return;
  unsigned long now = millis();
  if (now - lastSendMs_ >= BEACON_INTERVAL_MS) {
    lastSendMs_ = now;
    sendBeacon();
  }
}

void BeaconManager::nextSSID() {
  currentIndex_ = (currentIndex_ + 1) % BEACON_LIST_SIZE;
}

void BeaconManager::prevSSID() {
  currentIndex_--;
  if (currentIndex_ < 0)
    currentIndex_ = BEACON_LIST_SIZE - 1;
}
