/*
 * NOCTURNE_OS — KickManager: WiFi deauth (802.11 frame injection).
 * Uses Radar scan list; short = cycle target, long = START/STOP attack.
 * Safety: will not attack the device's current AP.
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
  /** Start/stop injection. Returns false if target is our own AP. */
  bool startAttack();
  void stopAttack();
  bool isAttacking() const { return attacking_; }

  int getPacketCount() const { return packetCount_; }
  void getTargetSSID(char *out, size_t maxLen) const;
  void getTargetBSSIDStr(char *out, size_t maxLen) const;
  /** True if current target is our connected AP (attack blocked). */
  bool isTargetOwnAP() const { return targetIsOwnAP_; }

private:
  void sendDeauthBurst();

  uint8_t targetBSSID_[6];
  char targetSSID_[KICK_TARGET_SSID_LEN];
  bool targetSet_;
  bool targetIsOwnAP_;
  bool attacking_;
  unsigned long lastBurstMs_;
  int packetCount_;
  uint8_t deauthBuf_[26];
};

#endif
