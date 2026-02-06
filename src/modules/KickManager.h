/*
 * NOCTURNE_OS — KickManager: WiFi deauth (802.11 frame injection).
 * Uses Radar scan list; short = cycle target, long = START/STOP attack.
 */
#ifndef NOCTURNE_KICK_MANAGER_H
#define NOCTURNE_KICK_MANAGER_H

#include <Arduino.h>

#define KICK_TARGET_SSID_LEN 33
#define KICK_TARGET_BSSID_LEN 18

class KickManager {
public:
  KickManager();
  void begin();
  void tick();

  /** Set target from current scan (index). Call when entering KICK or on short
   * press. */
  void setTargetFromScan(int scanIndex);
  /** Set target client MAC for targeted deauth. If mac is nullptr or all zeros,
   * use broadcast mode. */
  void setTargetClient(const uint8_t *mac);
  /** Check if targeted mode is active (specific client MAC set). */
  bool isTargetedMode() const { return targetedMode_; }
  /** Start injection. Returns false if no target set. */
  bool startAttack();
  void stopAttack();
  bool isAttacking() const { return attacking_; }
  bool isTargetSet() const { return targetSet_; }

  int getPacketCount() const { return packetCount_; }
  void getTargetSSID(char *out, size_t maxLen) const;
  void getTargetBSSIDStr(char *out, size_t maxLen) const;
  void getTargetClientStr(char *out, size_t maxLen) const;
  /** Reserved (own-AP check disabled). */
  bool isTargetOwnAP() const { return false; }
  /** Get encryption type of current target. Returns WIFI_AUTH_OPEN if not set.
   */
  wifi_auth_mode_t getTargetEncryption() const { return targetEncryption_; }
  /** Check if target uses WPA3 or PMF (Protected Management Frames).
   * Improved: checks capability info for PMF bit (0x10). */
  bool isTargetProtected() const;

private:
  void sendDeauthBurst();
  void sendTargetedDeauth();

  uint8_t targetBSSID_[6];
  uint8_t targetClientMAC_[6];
  char targetSSID_[KICK_TARGET_SSID_LEN];
  uint8_t targetChannel_;
  bool targetSet_;
  bool targetIsOwnAP_;
  bool targetedMode_;
  bool attacking_;
  unsigned long lastBurstMs_;
  int packetCount_;
  wifi_auth_mode_t targetEncryption_;
  uint16_t targetCapabilityInfo_; // For PMF check
  uint8_t deauthBuf_[26];
};

#endif
