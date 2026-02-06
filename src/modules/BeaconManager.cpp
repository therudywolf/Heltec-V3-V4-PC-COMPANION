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

  // Rick Roll list
  rickRollList_[0] = "01 Never gonna give you up";
  rickRollList_[1] = "02 Never gonna let you down";
  rickRollList_[2] = "03 Never gonna run around";
  rickRollList_[3] = "04 and desert you";
  rickRollList_[4] = "05 Never gonna make you cry";
  rickRollList_[5] = "06 Never gonna say goodbye";
  rickRollList_[6] = "07 Never gonna tell a lie";
  rickRollList_[7] = "08 and hurt you";

  // Funny beacon list
  funnyList_[0] = "Abraham Linksys";
  funnyList_[1] = "Benjamin FrankLAN";
  funnyList_[2] = "Dora the Internet Explorer";
  funnyList_[3] = "FBI Surveillance Van 4";
  funnyList_[4] = "Get Off My LAN";
  funnyList_[5] = "Loading...";
  funnyList_[6] = "Martin Router King";
  funnyList_[7] = "404 Wi-Fi Unavailable";
  funnyList_[8] = "Test Wi-Fi Please Ignore";
  funnyList_[9] = "This LAN is My LAN";
  funnyList_[10] = "Titanic Syncing";
  funnyList_[11] = "Winternet is Coming";

  memset(customSSIDs_, 0, sizeof(customSSIDs_));
  customSSIDCount_ = 0;
  mode_ = BEACON_MODE_DEFAULT;
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

const char **BeaconManager::getCurrentSSIDList() const {
  switch (mode_) {
  case BEACON_MODE_RICK_ROLL:
    return rickRollList_;
  case BEACON_MODE_FUNNY:
    return funnyList_;
  case BEACON_MODE_CUSTOM_LIST:
    return (const char **)customSSIDs_;
  default:
    return ssidList_;
  }
}

int BeaconManager::getCurrentListSize() const {
  switch (mode_) {
  case BEACON_MODE_RICK_ROLL:
    return BEACON_RICKROLL_SIZE;
  case BEACON_MODE_FUNNY:
    return BEACON_FUNNY_SIZE;
  case BEACON_MODE_CUSTOM_LIST:
    return customSSIDCount_;
  default:
    return BEACON_LIST_SIZE;
  }
}

const char *BeaconManager::getCurrentSSID() const {
  const char **list = getCurrentSSIDList();
  int listSize = getCurrentListSize();
  if (listSize == 0)
    return "";
  int idx = currentIndex_ % listSize;
  return list[idx];
}

void BeaconManager::setMode(BeaconMode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;
  currentIndex_ = 0;
  if (active_) {
    int listSize = getCurrentListSize();
    if (listSize > 0) {
      buildBeacon(getCurrentSSID(), 6);
    }
  }
}

void BeaconManager::addCustomSSID(const char *ssid) {
  if (!ssid || customSSIDCount_ >= BEACON_LIST_SIZE)
    return;
  size_t len = strlen(ssid);
  if (len == 0 || len >= BEACON_SSID_MAX)
    return;
  strncpy(customSSIDs_[customSSIDCount_], ssid, BEACON_SSID_MAX - 1);
  customSSIDs_[customSSIDCount_][BEACON_SSID_MAX - 1] = '\0';
  customSSIDCount_++;
  if (mode_ != BEACON_MODE_CUSTOM_LIST)
    mode_ = BEACON_MODE_CUSTOM_LIST;
}

void BeaconManager::sendBeacon() {
  if (WiFi.getMode() == WIFI_OFF || beaconLen_ < 36)
    return;
  const char **list = getCurrentSSIDList();
  int listSize = getCurrentListSize();
  if (listSize == 0)
    return;
  uint8_t ch = 1 + (currentIndex_ % 13);
  if (ch > 13)
    ch = 6;
  buildBeacon(list[currentIndex_ % listSize], ch);
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
  int listSize = getCurrentListSize();
  if (listSize == 0)
    return;
  currentIndex_ = (currentIndex_ + 1) % listSize;
}

void BeaconManager::prevSSID() {
  int listSize = getCurrentListSize();
  if (listSize == 0)
    return;
  currentIndex_--;
  if (currentIndex_ < 0)
    currentIndex_ = listSize - 1;
}
