/*
 * NOCTURNE_OS тАФ NetManager: WiFi (reconnect), TCP stream, JSON parsing
 * (ArduinoJson) MANDATORY: WiFi.setSleep(false) after connection for ping <
 * 10ms.
 */
#ifndef NOCTURNE_NET_MANAGER_H
#define NOCTURNE_NET_MANAGER_H

#include "nocturne/config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>


struct AppState;

class NetManager {
public:
  NetManager();

  /** A WiFi network candidate for auto-failover (#2). */
  struct WifiCred { const char *ssid; const char *pass; };

  void begin(const char *ssid, const char *pass);
  /** Register multiple networks; connects to the strongest reachable one and
   * auto-fails-over between them (WiFiMulti). */
  void begin(const WifiCred *nets, size_t count);
  void setServer(const char *ip, uint16_t port);
  /** Select TLS transport (WiFiClientSecure + setInsecure). Use when the server
   * is reached through an HTTPS-fronted proxy / public domain on :443. The
   * line-protocol on top is identical to the raw-TCP path. */
  void setTls(bool on);
  void tick(unsigned long now);

  bool isWifiConnected() const { return wifiConnected_; }
  bool isTcpConnected() const { return tcpConnected_; }
  bool hasReceivedData() const { return firstDataReceived_; }
  int rssi() const { return rssi_; }
  bool isSearchMode() const { return searchMode_; }
  bool isSignalLost(unsigned long now) const;

  int available() { return client_->available(); }
  int read() { return client_->read(); }
  size_t print(const String &s) { return client_->print(s); }
  size_t print(const char *s) { return client_->print(s); }

  // WiFi network selection (#2): -1 = AUTO (priority failover), or a fixed index
  // into the registered networks. Forcing one reconnects to it immediately.
  void setForcedNetwork(int idx);
  int forcedNetwork() const { return forcedNet_; }
  int networkCount() const { return netCount_; }
  const char *networkName(int i) const {
    return (i >= 0 && i < netCount_) ? netSsid_[i] : "";
  }

  void setSuspend(bool suspend);
  void disconnectTcp();
  void markDataReceived(unsigned long now);
  void appendLineBuffer(char c);
  void clearLineBuffer();
  char *getLineBuffer() { return lineBuffer_; }
  size_t getLineBufferLen() const { return lineBufferLen_; }
  int getLastSentScreen() const { return lastSentScreen_; }
  void setLastSentScreen(int s) { lastSentScreen_ = s; }

  /** Parse one newline-terminated JSON line into AppState. Returns true on
   * success. */
  bool parsePayload(const char *line, size_t lineLen, AppState *state);

private:
  bool tryTcpConnect(unsigned long now);

  char storedSSID_[33]; // Max SSID length is 32 + null terminator
  char storedPass_[65]; // Max password length is 64 + null terminator
  // Transport: raw TCP by default, or TLS when setTls(true) (HTTPS-fronted
  // proxy / public domain). client_ points at the active one; the line-protocol
  // is transport-agnostic (Client base class).
  WiFiClient clientPlain_;
  WiFiClientSecure clientTls_;
  Client *client_ = &clientPlain_;
  bool useTls_ = false;
  char lineBuffer_[NOCT_TCP_LINE_MAX];
  size_t lineBufferLen_;
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
  bool suspended_ = false;
  static const int kMaxNets = 5;
  char netSsid_[kMaxNets][33];
  char netPass_[kMaxNets][65];
  int netCount_ = 0; // priority list of WiFi networks (#2)
  int netIdx_ = 0;   // index currently being attempted
  int forcedNet_ = -1; // -1 = AUTO failover; else lock to this network index
  int rssi_;
  int lastSentScreen_;
};

#endif