/*
 * NOCTURNE_OS — NetManager: WiFi (reconnect), TCP stream, JSON parsing
 * (ArduinoJson) MANDATORY: WiFi.setSleep(false) after connection for ping <
 * 10ms.
 */
#ifndef NOCTURNE_NET_MANAGER_H
#define NOCTURNE_NET_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

struct AppState;

class NetManager {
public:
  NetManager();
  void begin(const char *ssid, const char *pass);
  void setServer(const char *ip, uint16_t port);
  void tick(unsigned long now);

  bool isWifiConnected() const { return wifiConnected_; }
  bool isTcpConnected() const { return tcpConnected_; }
  bool hasReceivedData() const { return firstDataReceived_; }
  int rssi() const { return rssi_; }
  bool isSearchMode() const { return searchMode_; }
  bool isSignalLost(unsigned long now) const;

  int available() { return client_.available(); }
  int read() { return client_.read(); }
  size_t print(const String &s) { return client_.print(s); }
  size_t print(const char *s) { return client_.print(s); }

  void disconnectTcp();
  void markDataReceived(unsigned long now);
  void appendLineBuffer(char c);
  void clearLineBuffer();
  String &getLineBuffer() { return lineBuffer_; }
  int getLastSentScreen() const { return lastSentScreen_; }
  void setLastSentScreen(int s) { lastSentScreen_ = s; }

  /** Parse one newline-terminated JSON line into AppState. Returns true on
   * success. */
  bool parsePayload(const String &line, AppState *state);

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
