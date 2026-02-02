/*
 * NOCTURNE_OS — NetLink: WiFi (non-blocking reconnect) + TCP stream
 * SEARCH_MODE: scanning animation when PC disconnects (no plain "NO SIGNAL").
 */
#ifndef NOCTURNE_NETLINK_H
#define NOCTURNE_NETLINK_H

#include <Arduino.h>
#include <String.h>
#include <WiFi.h>
#include <WiFiClient.h>


class NetLink {
public:
  NetLink();
  void begin(const char *ssid, const char *pass);
  void tick(unsigned long now);

  bool isWifiConnected() const { return wifiConnected_; }
  bool isTcpConnected() const { return tcpConnected_; }
  bool hasReceivedData() const { return firstDataReceived_; }
  int rssi() const { return rssi_; }

  // TCP stream
  bool tcpConnected() const { return client_.connected(); }
  int available() { return client_.available(); }
  int read() { return client_.read(); }
  size_t print(const String &s) { return client_.print(s); }
  size_t print(const char *s) { return client_.print(s); }

  void disconnectTcp();
  void markDataReceived(unsigned long now);

  // SEARCH_MODE: true when we're in "scanning for host" state (show radar
  // animation)
  bool isSearchMode() const { return searchMode_; }
  unsigned long lastUpdate() const { return lastUpdate_; }
  bool isSignalLost(unsigned long now) const;

  void setServer(const char *ip, uint16_t port);
  void appendLineBuffer(char c);
  void clearLineBuffer();
  String &getLineBuffer() { return lineBuffer_; }
  int getLastSentScreen() const { return lastSentScreen_; }
  void setLastSentScreen(int s) { lastSentScreen_ = s; }

private:
  bool tryTcpConnect(unsigned long now);

  WiFiClient client_;
  String lineBuffer_;
  const char *serverIp_;
  uint16_t serverPort_;
  unsigned long lastTcpAttempt_;
  unsigned long tcpConnectTime_;
  unsigned long lastUpdate_;
  unsigned long lastWifiRetry_;
  bool wifiConnected_;
  bool tcpConnected_;
  bool firstDataReceived_;
  bool searchMode_;
  int rssi_;
  int lastSentScreen_;
};

#endif
