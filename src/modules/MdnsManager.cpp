/*
 * NOCTURNE_OS — MdnsManager: ESPmDNS fake service.
 */
#include "MdnsManager.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <string.h>

MdnsManager::MdnsManager() {
  strncpy(serviceName_, "NOCTURNE", sizeof(serviceName_) - 1);
  serviceName_[sizeof(serviceName_) - 1] = '\0';
}

void MdnsManager::begin() {
  if (active_)
    return;
  WiFi.mode(WIFI_STA);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin("", ""); // Empty = use stored or start AP
  }
  delay(200);
  if (!MDNS.begin(serviceName_)) {
    Serial.println("[MDNS] Failed to start");
    return;
  }
  MDNS.addService("_http", "_tcp", 80);
  MDNS.addService("_printer", "_tcp", 9100);
  active_ = true;
  Serial.println("[MDNS] Responder ACTIVE.");
}

void MdnsManager::stop() {
  if (!active_)
    return;
  MDNS.end();
  active_ = false;
  Serial.println("[MDNS] STOP.");
}

void MdnsManager::tick() {}

void MdnsManager::setServiceName(const char *name) {
  if (name) {
    strncpy(serviceName_, name, sizeof(serviceName_) - 1);
    serviceName_[sizeof(serviceName_) - 1] = '\0';
  }
}
