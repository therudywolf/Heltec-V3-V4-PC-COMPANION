/*
 * NOCTURNE_OS — VaultManager: TOTP (RFC 6238), LittleFS vault.json.
 */
#include "VaultManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <mbedtls/md.h>
#include <string.h>
#include <time.h>

static const char *VAULT_FILE = "/vault.json";

VaultManager::VaultManager()
    : accountCount_(0), currentIndex_(0), timeOffset_(0), ntpSynced_(false) {
  memset(accounts_, 0, sizeof(accounts_));
}

void VaultManager::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[VAULT] LittleFS mount failed.");
    return;
  }
  load();
}

void VaultManager::tick() { trySyncNtp(); }

int VaultManager::base32Decode(const char *in, uint8_t *out, size_t outMax) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int outLen = 0;
  int bits = 0;
  int buf = 0;
  for (; *in && outLen < (int)outMax; in++) {
    if (*in == ' ' || *in == '=')
      continue;
    const char *p = strchr(alphabet, toupper((unsigned char)*in));
    if (!p)
      continue;
    int v = (int)(p - alphabet);
    buf = (buf << 5) | v;
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out[outLen++] = (uint8_t)((buf >> bits) & 0xFF);
    }
  }
  return outLen;
}

void VaultManager::computeTotp(const uint8_t *secret, size_t secretLen,
                               long unixTime, char *out, size_t outLen) {
  if (outLen < 7)
    return;
  long step = unixTime / TOTP_PERIOD_SEC;
  uint8_t timeBuf[8];
  for (int i = 7; i >= 0; i--) {
    timeBuf[i] = (uint8_t)(step & 0xFF);
    step >>= 8;
  }
  uint8_t hmac[20];
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (!md) {
    strncpy(out, "------", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, md, 1) != 0) {
    mbedtls_md_free(&ctx);
    strncpy(out, "------", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  mbedtls_md_hmac_starts(&ctx, secret, secretLen);
  mbedtls_md_hmac_update(&ctx, timeBuf, 8);
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);
  int offset = hmac[19] & 0x0F;
  int bin = ((hmac[offset] & 0x7F) << 24) | ((hmac[offset + 1] & 0xFF) << 16) |
            ((hmac[offset + 2] & 0xFF) << 8) | (hmac[offset + 3] & 0xFF);
  int code = bin % 1000000;
  snprintf(out, outLen, "%06d", code);
}

long VaultManager::getUnixTime() const {
  time_t now = time(nullptr);
  if (ntpSynced_ && now > 1600000000)
    return (long)now;
  return timeOffset_ + (long)(millis() / 1000);
}

void VaultManager::getCurrentCode(char *out, size_t outLen) {
  if (currentIndex_ < 0 || currentIndex_ >= accountCount_) {
    if (outLen > 0)
      strncpy(out, "------", outLen - 1), out[outLen - 1] = '\0';
    return;
  }
  uint8_t key[64];
  int keyLen =
      base32Decode(accounts_[currentIndex_].secretB32, key, sizeof(key));
  if (keyLen <= 0) {
    strncpy(out, "------", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  computeTotp(key, (size_t)keyLen, getUnixTime(), out, outLen);
}

int VaultManager::getCountdownSeconds() const {
  long t = getUnixTime();
  return (int)(TOTP_PERIOD_SEC - (t % TOTP_PERIOD_SEC));
}

const char *VaultManager::getAccountName(int index) const {
  if (index < 0 || index >= accountCount_)
    return "";
  return accounts_[index].name;
}

void VaultManager::setCurrentIndex(int index) {
  if (index >= 0 && index < accountCount_)
    currentIndex_ = index;
}

void VaultManager::trySyncNtp() {
  if (ntpSynced_ || WiFi.status() != WL_CONNECTED)
    return;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  int retries = 0;
  while (now < 1600000000 && retries++ < 20) {
    delay(100);
    now = time(nullptr);
  }
  if (now >= 1600000000) {
    timeOffset_ = (long)now - (long)(millis() / 1000);
    ntpSynced_ = true;
    Serial.println("[VAULT] NTP synced.");
  }
}

void VaultManager::setTimeOffset(long unixTime) {
  timeOffset_ = unixTime - (long)(millis() / 1000);
  ntpSynced_ = false;
  Serial.println("[VAULT] Time set manually.");
}

bool VaultManager::load() {
  if (!LittleFS.exists(VAULT_FILE)) {
    accountCount_ = 0;
    return true;
  }
  File f = LittleFS.open(VAULT_FILE, "r");
  if (!f)
    return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.println("[VAULT] JSON read error.");
    return false;
  }
  if (doc["to"].is<long>())
    timeOffset_ = doc["to"].as<long>();
  JsonArray arr = doc["accounts"].as<JsonArray>();
  accountCount_ = 0;
  for (JsonObject o : arr) {
    if (accountCount_ >= VAULT_MAX_ACCOUNTS)
      break;
    strncpy(accounts_[accountCount_].name, o["n"].as<const char *>() ?: "",
            VAULT_ACCOUNT_NAME_LEN - 1);
    accounts_[accountCount_].name[VAULT_ACCOUNT_NAME_LEN - 1] = '\0';
    strncpy(accounts_[accountCount_].secretB32, o["s"].as<const char *>() ?: "",
            VAULT_SECRET_B32_LEN - 1);
    accounts_[accountCount_].secretB32[VAULT_SECRET_B32_LEN - 1] = '\0';
    accountCount_++;
  }
  currentIndex_ = 0;
  return true;
}

bool VaultManager::save() {
  File f = LittleFS.open(VAULT_FILE, "w");
  if (!f)
    return false;
  JsonDocument doc;
  doc["to"] = timeOffset_;
  JsonArray arr = doc["accounts"].to<JsonArray>();
  for (int i = 0; i < accountCount_; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["n"] = accounts_[i].name;
    o["s"] = accounts_[i].secretB32;
  }
  serializeJson(doc, f);
  f.close();
  return true;
}
