/*
 * NOCTURNE_OS — NetLink: WiFi non-blocking reconnect + TCP, SEARCH_MODE
 */
#include "nocturne/NetLink.h"
#include "nocturne/config.h"

NetLink::NetLink()
    : serverIp_(nullptr), serverPort_(0), lastTcpAttempt_(0),
      tcpConnectTime_(0), lastUpdate_(0), lastWifiRetry_(0),
      wifiConnected_(false), tcpConnected_(false), firstDataReceived_(false),
      searchMode_(false), rssi_(0), lastSentScreen_(-1) {}

void NetLink::begin(const char *ssid, const char *pass) {
  if (ssid && strlen(ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
  }
}

void NetLink::setServer(const char *ip, uint16_t port) {
  serverIp_ = ip;
  serverPort_ = port;
}

void NetLink::tick(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected_)
      wifiConnected_ = true;
    if (now % 5000 < 100)
      rssi_ = WiFi.RSSI();

    if (!client_.connected()) {
      if (tcpConnected_) {
        disconnectTcp();
        searchMode_ = true; // Enter SEARCH_MODE when we lose TCP
      }
      tryTcpConnect(now);
    } else {
      tcpConnected_ = true;
      searchMode_ = false;
    }
  } else {
    wifiConnected_ = false;
    if (tcpConnected_)
      disconnectTcp();
    searchMode_ = true;
    if (now - lastWifiRetry_ > NOCT_WIFI_RETRY_INTERVAL_MS) {
      WiFi.disconnect();
      WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());
      lastWifiRetry_ = now;
    }
  }
}

void NetLink::disconnectTcp() {
  if (client_.connected())
    client_.stop();
  lineBuffer_ = "";
  lastSentScreen_ = -1;
  tcpConnected_ = false;
  tcpConnectTime_ = 0;
}

void NetLink::markDataReceived(unsigned long now) {
  lastUpdate_ = now;
  firstDataReceived_ = true;
}

bool NetLink::tryTcpConnect(unsigned long now) {
  if (!serverIp_ || serverPort_ == 0)
    return false;
  if (now - lastTcpAttempt_ < NOCT_TCP_RECONNECT_INTERVAL_MS)
    return false;
  if (client_.connected())
    return true;

  lastTcpAttempt_ = now;
  client_.setTimeout(NOCT_TCP_CONNECT_TIMEOUT_MS / 1000);

  if (client_.connect(serverIp_, serverPort_)) {
    lineBuffer_ = "";
    lastSentScreen_ = -1;
    tcpConnected_ = true;
    tcpConnectTime_ = now;
    lastUpdate_ = now;
    client_.print("HELO\n");
    return true;
  }
  return false;
}

bool NetLink::isSignalLost(unsigned long now) const {
  if (tcpConnected_ && tcpConnectTime_ > 0) {
    unsigned long since = now - tcpConnectTime_;
    if (since < NOCT_SIGNAL_GRACE_MS)
      return false;
  }
  if (!firstDataReceived_ && tcpConnected_)
    return (now - tcpConnectTime_ > NOCT_SIGNAL_GRACE_MS);
  return (now - lastUpdate_ > NOCT_SIGNAL_TIMEOUT_MS);
}

void NetLink::appendLineBuffer(char c) {
  lineBuffer_ += c;
  if (lineBuffer_.length() >= NOCT_TCP_LINE_MAX)
    lineBuffer_ = "";
}

void NetLink::clearLineBuffer() { lineBuffer_ = ""; }
