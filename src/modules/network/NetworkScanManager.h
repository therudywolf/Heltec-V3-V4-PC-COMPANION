/*
 * NOCTURNE_OS — NetworkScanManager: Network scanning (ARP, Port, DNS, HTTP,
 * etc.) Adapted from esp32_marauder/WiFiScan.cpp
 */
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include <WiFi.h>

enum NetworkScanType {
  NETWORK_SCAN_NONE = 0,
  NETWORK_SCAN_ARP,
  NETWORK_SCAN_PORT,
  NETWORK_SCAN_PING,
  NETWORK_SCAN_DNS,
  NETWORK_SCAN_HTTP,
  NETWORK_SCAN_HTTPS,
  NETWORK_SCAN_SMTP,
  NETWORK_SCAN_RDP,
  NETWORK_SCAN_TELNET,
  NETWORK_SCAN_SSH
};

struct NetworkHost {
  IPAddress ip;
  uint8_t mac[6];
  char macStr[18];
  bool isAlive;
  uint16_t openPorts[16];
  int openPortCount;
  uint32_t lastSeen;
};

class NetworkScanManager {
public:
  NetworkScanManager();
  void begin(NetworkScanType scanType);
  void stop();
  void tick();

  bool isActive() const { return active_; }
  NetworkScanType getScanType() const { return scanType_; }

  // Set scan target
  void setTargetIP(IPAddress ip);
  void setSubnet(IPAddress subnet);
  void setTargetPort(uint16_t port);

  // Get results
  int getHostCount() const { return hostCount_; }
  const NetworkHost *getHost(int index) const;

  bool isScanComplete() const { return scanComplete_; }

private:
  void doPingScan();
  void doPortScan();
  void doArpScan();
  void doServiceScan();

  bool isHostAlive(IPAddress ip);
  bool checkHostPort(IPAddress ip, uint16_t port, uint16_t timeout = 100);
  bool readARP(IPAddress targ_ip);
  bool singleARP(IPAddress ip_addr);

  IPAddress getNextIP(IPAddress current, IPAddress subnet);
  uint16_t getNextPort(uint16_t current);

  bool active_ = false;
  NetworkScanType scanType_ = NETWORK_SCAN_NONE;
  NetworkHost hosts_[32];
  int hostCount_ = 0;
  bool scanComplete_ = false;

  IPAddress currentScanIP_;
  IPAddress subnet_;
  IPAddress targetIP_;
  uint16_t targetPort_ = 0;
  uint16_t currentScanPort_ = 0;

  unsigned long lastScanMs_ = 0;
  static const unsigned long SCAN_INTERVAL_MS;
};
