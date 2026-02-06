/*
 * NOCTURNE_OS — NetworkScanManager: Network scanning (ARP, Port, DNS, HTTP,
 * etc.) Adapted from esp32_marauder/WiFiScan.cpp
 */
#include "NetworkScanManager.h"
#include <ESP32Ping.h>
#include <WiFiClient.h>
#include <string.h>

const unsigned long NetworkScanManager::SCAN_INTERVAL_MS = 100;

NetworkScanManager::NetworkScanManager() {
  memset(hosts_, 0, sizeof(hosts_));
  hostCount_ = 0;
  scanComplete_ = false;
  currentScanIP_ = IPAddress(0, 0, 0, 0);
  subnet_ = IPAddress(255, 255, 255, 0);
  targetIP_ = IPAddress(0, 0, 0, 0);
  targetPort_ = 0;
  currentScanPort_ = 0;
}

void NetworkScanManager::begin(NetworkScanType scanType) {
  if (active_)
    stop();

  scanType_ = scanType;
  active_ = true;
  scanComplete_ = false;
  hostCount_ = 0;
  memset(hosts_, 0, sizeof(hosts_));
  lastScanMs_ = 0;

  // Initialize scan IP from WiFi gateway
  IPAddress gateway = WiFi.gatewayIP();
  IPAddress localIP = WiFi.localIP();

  if (gateway != IPAddress(0, 0, 0, 0)) {
    // Start from gateway subnet
    uint8_t *gw = (uint8_t *)&gateway;
    currentScanIP_ = IPAddress(gw[0], gw[1], gw[2], 1);
  } else if (localIP != IPAddress(0, 0, 0, 0)) {
    // Use local IP subnet
    uint8_t *ip = (uint8_t *)&localIP;
    currentScanIP_ = IPAddress(ip[0], ip[1], ip[2], 1);
  }

  // Set default ports for service scans
  switch (scanType_) {
  case NETWORK_SCAN_SSH:
    targetPort_ = 22;
    break;
  case NETWORK_SCAN_TELNET:
    targetPort_ = 23;
    break;
  case NETWORK_SCAN_SMTP:
    targetPort_ = 25;
    break;
  case NETWORK_SCAN_DNS:
    targetPort_ = 53;
    break;
  case NETWORK_SCAN_HTTP:
    targetPort_ = 80;
    break;
  case NETWORK_SCAN_HTTPS:
    targetPort_ = 443;
    break;
  case NETWORK_SCAN_RDP:
    targetPort_ = 3389;
    break;
  default:
    break;
  }

  Serial.printf("[NETSCAN] Started scan type %d\n", scanType);
}

void NetworkScanManager::stop() {
  if (!active_)
    return;

  active_ = false;
  Serial.println("[NETSCAN] Stopped");
}

void NetworkScanManager::tick() {
  if (!active_ || scanComplete_)
    return;

  unsigned long now = millis();
  if (now - lastScanMs_ < SCAN_INTERVAL_MS)
    return;

  lastScanMs_ = now;

  switch (scanType_) {
  case NETWORK_SCAN_PING:
    doPingScan();
    break;
  case NETWORK_SCAN_PORT:
    doPortScan();
    break;
  case NETWORK_SCAN_ARP:
    doArpScan();
    break;
  case NETWORK_SCAN_DNS:
  case NETWORK_SCAN_HTTP:
  case NETWORK_SCAN_HTTPS:
  case NETWORK_SCAN_SMTP:
  case NETWORK_SCAN_RDP:
  case NETWORK_SCAN_TELNET:
  case NETWORK_SCAN_SSH:
    doServiceScan();
    break;
  default:
    break;
  }
}

void NetworkScanManager::setTargetIP(IPAddress ip) {
  targetIP_ = ip;
  if (ip != IPAddress(0, 0, 0, 0)) {
    currentScanIP_ = ip;
  }
}

void NetworkScanManager::setSubnet(IPAddress subnet) { subnet_ = subnet; }

void NetworkScanManager::setTargetPort(uint16_t port) { targetPort_ = port; }

const NetworkHost *NetworkScanManager::getHost(int index) const {
  if (index < 0 || index >= hostCount_)
    return nullptr;
  return &hosts_[index];
}

void NetworkScanManager::doPingScan() {
  if (currentScanIP_ == IPAddress(0, 0, 0, 0)) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] Ping scan complete");
    return;
  }

  // Check if host is alive
  if (isHostAlive(currentScanIP_)) {
    if (hostCount_ < 32) {
      hosts_[hostCount_].ip = currentScanIP_;
      hosts_[hostCount_].isAlive = true;
      hosts_[hostCount_].lastSeen = millis();
      hostCount_++;
      Serial.printf("[NETSCAN] Found host: %s\n",
                    currentScanIP_.toString().c_str());
    }
  }

  // Move to next IP
  currentScanIP_ = getNextIP(currentScanIP_, subnet_);

  // Check if we've scanned the whole subnet
  if (currentScanIP_[3] == 0) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] Ping scan complete");
  }
}

void NetworkScanManager::doPortScan() {
  if (targetIP_ == IPAddress(0, 0, 0, 0)) {
    scanComplete_ = true;
    return;
  }

  if (currentScanPort_ >= 65535) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] Port scan complete");
    return;
  }

  currentScanPort_ = getNextPort(currentScanPort_);

  if (checkHostPort(targetIP_, currentScanPort_, 100)) {
    // Find or add host
    int hostIdx = -1;
    for (int i = 0; i < hostCount_; i++) {
      if (hosts_[i].ip == targetIP_) {
        hostIdx = i;
        break;
      }
    }

    if (hostIdx < 0 && hostCount_ < 32) {
      hostIdx = hostCount_++;
      hosts_[hostIdx].ip = targetIP_;
      hosts_[hostIdx].isAlive = true;
      hosts_[hostIdx].openPortCount = 0;
    }

    if (hostIdx >= 0 && hosts_[hostIdx].openPortCount < 16) {
      hosts_[hostIdx].openPorts[hosts_[hostIdx].openPortCount++] =
          currentScanPort_;
      Serial.printf("[NETSCAN] Found open port %d on %s\n", currentScanPort_,
                    targetIP_.toString().c_str());
    }
  }
}

void NetworkScanManager::doArpScan() {
  // ARP scan requires lwip access, simplified version using ping
  if (currentScanIP_ == IPAddress(0, 0, 0, 0)) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] ARP scan complete");
    return;
  }

  // Use ping as ARP alternative
  if (isHostAlive(currentScanIP_)) {
    if (hostCount_ < 32) {
      hosts_[hostCount_].ip = currentScanIP_;
      hosts_[hostCount_].isAlive = true;
      hosts_[hostCount_].lastSeen = millis();
      hostCount_++;
      Serial.printf("[NETSCAN] Found host: %s\n",
                    currentScanIP_.toString().c_str());
    }
  }

  currentScanIP_ = getNextIP(currentScanIP_, subnet_);

  if (currentScanIP_[3] == 0) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] ARP scan complete");
  }
}

void NetworkScanManager::doServiceScan() {
  if (currentScanIP_ == IPAddress(0, 0, 0, 0)) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] Service scan complete");
    return;
  }

  // Check if host is alive first
  if (isHostAlive(currentScanIP_)) {
    // Check specific port
    if (checkHostPort(currentScanIP_, targetPort_, 100)) {
      if (hostCount_ < 32) {
        hosts_[hostCount_].ip = currentScanIP_;
        hosts_[hostCount_].isAlive = true;
        hosts_[hostCount_].openPorts[0] = targetPort_;
        hosts_[hostCount_].openPortCount = 1;
        hosts_[hostCount_].lastSeen = millis();
        hostCount_++;
        Serial.printf("[NETSCAN] Found service on %s:%d\n",
                      currentScanIP_.toString().c_str(), targetPort_);
      }
    }
  }

  currentScanIP_ = getNextIP(currentScanIP_, subnet_);

  if (currentScanIP_[3] == 0) {
    scanComplete_ = true;
    Serial.println("[NETSCAN] Service scan complete");
  }
}

bool NetworkScanManager::isHostAlive(IPAddress ip) {
  if (ip == IPAddress(0, 0, 0, 0))
    return false;

  // Use ESP32Ping library
  bool ret = Ping.ping(ip, 1);
  return ret;
}

bool NetworkScanManager::checkHostPort(IPAddress ip, uint16_t port,
                                       uint16_t timeout) {
  WiFiClient client;
  client.setTimeout(timeout);

  bool connected = client.connect(ip, port);
  client.stop();

  return connected;
}

bool NetworkScanManager::readARP(IPAddress targ_ip) {
  // Simplified: use ping instead of ARP table lookup
  return isHostAlive(targ_ip);
}

bool NetworkScanManager::singleARP(IPAddress ip_addr) {
  // Send ARP request and check response
  delay(250);
  return readARP(ip_addr);
}

IPAddress NetworkScanManager::getNextIP(IPAddress current, IPAddress subnet) {
  uint8_t *ip = (uint8_t *)&current;
  uint8_t *mask = (uint8_t *)&subnet;

  // Increment IP
  ip[3]++;

  // Handle overflow
  if (ip[3] == 0) {
    ip[2]++;
    if (ip[2] == 0) {
      ip[1]++;
      if (ip[1] == 0) {
        ip[0]++;
      }
    }
  }

  // Check if we've exceeded subnet
  if ((ip[0] & mask[0]) != (current[0] & mask[0]) ||
      (ip[1] & mask[1]) != (current[1] & mask[1]) ||
      (ip[2] & mask[2]) != (current[2] & mask[2])) {
    return IPAddress(0, 0, 0, 0); // End of scan
  }

  return IPAddress(ip[0], ip[1], ip[2], ip[3]);
}

uint16_t NetworkScanManager::getNextPort(uint16_t current) {
  if (current == 0)
    return 1;
  if (current >= 65535)
    return 65535;
  return current + 1;
}
