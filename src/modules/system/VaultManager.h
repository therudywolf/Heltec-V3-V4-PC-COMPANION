/*
 * NOCTURNE_OS — VaultManager: TOTP 2FA generator.
 * LittleFS vault.json, time sync via NTP or Serial.
 */
#ifndef NOCTURNE_VAULT_MANAGER_H
#define NOCTURNE_VAULT_MANAGER_H

#include <Arduino.h>

#define VAULT_ACCOUNT_NAME_LEN 24
#define VAULT_SECRET_B32_LEN 32
#define VAULT_MAX_ACCOUNTS 8
#define TOTP_PERIOD_SEC 30

struct VaultAccount {
  char name[VAULT_ACCOUNT_NAME_LEN];
  char secretB32[VAULT_SECRET_B32_LEN];
};

class VaultManager {
public:
  VaultManager();
  void begin();
  void tick();

  int getAccountCount() const { return accountCount_; }
  const char *getAccountName(int index) const;
  void setCurrentIndex(int index);
  int getCurrentIndex() const { return currentIndex_; }

  /** Current 6-digit TOTP code for current account. Filled into out (e.g.
   * "123456"). */
  void getCurrentCode(char *out, size_t outLen);
  /** Seconds remaining in current 30s window (0..29). */
  int getCountdownSeconds() const;

  /** Call when WiFi connected to sync time from NTP (one-shot). */
  void trySyncNtp();
  /** Set time from Serial/external: unix timestamp. */
  void setTimeOffset(long unixTime);

private:
  bool load();
  bool save();
  long getUnixTime() const;
  void computeTotp(const uint8_t *secret, size_t secretLen, long unixTime,
                   char *out, size_t outLen);
  int base32Decode(const char *in, uint8_t *out, size_t outMax);

  VaultAccount accounts_[VAULT_MAX_ACCOUNTS];
  int accountCount_;
  int currentIndex_;
  long timeOffset_; // unix = timeOffset_ + millis()/1000 when no NTP
  bool ntpSynced_;
};

#endif
