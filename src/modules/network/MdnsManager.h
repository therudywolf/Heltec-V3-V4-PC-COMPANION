/*
 * NOCTURNE_OS — MdnsManager: fake mDNS/SSDP responder.
 * Advertises a configurable name (e.g. printer, Chromecast) on the network.
 */
#pragma once
#include <Arduino.h>

class MdnsManager {
public:
  MdnsManager();
  void begin();
  void stop();
  void tick();

  bool isActive() const { return active_; }
  void setServiceName(const char *name);
  const char *getServiceName() const { return serviceName_; }

private:
  bool active_ = false;
  char serviceName_[32];
};
