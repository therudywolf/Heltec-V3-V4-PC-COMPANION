/*
 * NOCTURNE_OS — WifiAttackManager: All WiFi attack types from Marauder.
 * Auth, Mimic, AP Spam, Bad Message, Sleep, SAE Commit attacks.
 */
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

enum WifiAttackType {
  WIFI_ATTACK_NONE = 0,
  WIFI_ATTACK_AUTH,
  WIFI_ATTACK_MIMIC_FLOOD,
  WIFI_ATTACK_AP_SPAM,
  WIFI_ATTACK_BAD_MESSAGE,
  WIFI_ATTACK_BAD_MESSAGE_TARGETED,
  WIFI_ATTACK_SLEEP,
  WIFI_ATTACK_SLEEP_TARGETED,
  WIFI_ATTACK_SAE_COMMIT
};

class WifiAttackManager {
public:
  WifiAttackManager();
  void begin(WifiAttackType attackType);
  void stop();
  void tick();

  bool isActive() const { return active_; }
  WifiAttackType getAttackType() const { return attackType_; }
  uint32_t getPacketsSent() const { return packetsSent_; }

  // For targeted attacks
  void setTargetBSSID(const uint8_t *bssid);
  void setTargetSSID(const char *ssid);
  void setTargetChannel(uint8_t channel);

private:
  void sendAuthFrame();
  void sendMimicBeacon();
  void sendAPSpamBeacon();
  void sendBadMessageFrame();
  void sendSleepFrame();
  void sendSAECommitFrame();

  void buildAuthFrame(uint8_t *frame, size_t *len, const uint8_t *bssid);
  void buildBadMessageFrame(uint8_t *frame, size_t *len, const uint8_t *bssid);
  void buildSleepFrame(uint8_t *frame, size_t *len, const uint8_t *bssid);
  void buildBeaconFrame(uint8_t *frame, size_t *len, const char *ssid,
                        uint8_t channel, const uint8_t *bssid);

  void changeChannel(uint8_t channel);
  void generateRandomMAC(uint8_t *mac);

  bool active_ = false;
  WifiAttackType attackType_ = WIFI_ATTACK_NONE;
  uint32_t packetsSent_ = 0;
  unsigned long lastSendMs_ = 0;

  uint8_t targetBSSID_[6];
  char targetSSID_[33];
  uint8_t targetChannel_ = 1;
  bool targetSet_ = false;

  uint8_t packetBuf_[256];
  uint8_t apMac_[6];

  static const unsigned long ATTACK_INTERVAL_MS;
  static const uint8_t BEACON_TEMPLATE[];
};
