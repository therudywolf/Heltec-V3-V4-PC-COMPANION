/*
 * NOCTURNE_OS — NetManager: WiFi, TCP, JSON parse. WiFi.setSleep(false) after
 * connect.
 */
#include "NetManager.h"
#include "../../include/nocturne/Types.h"
#include "../../include/nocturne/config.h"
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <string.h>

NetManager::NetManager()
    : serverIp_(nullptr), serverPort_(0), lastTcpAttempt_(0),
      tcpConnectTime_(0), lastUpdate_(0), lastWifiRetry_(0),
      wifiConnected_(false), tcpConnected_(false), firstDataReceived_(false),
      searchMode_(false), rssi_(0), lastSentScreen_(-1) {}

void NetManager::begin(const char *ssid, const char *pass) {
  if (ssid && strlen(ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // CRITICAL FIX FOR HELTEC V3 DROPOUTS
    WiFi.begin(ssid, pass);
  }
}

void NetManager::setServer(const char *ip, uint16_t port) {
  serverIp_ = ip;
  serverPort_ = port;
}

void NetManager::tick(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected_) {
      wifiConnected_ = true;
      WiFi.setSleep(false); // MANDATORY: keep ping < 10ms for real-time graphs
      esp_wifi_set_ps(WIFI_PS_NONE); // V4: disable aggressive S3 power saving
    }
    if (now % 5000 < 100)
      rssi_ = WiFi.RSSI();

    if (!client_.connected()) {
      if (tcpConnected_) {
        disconnectTcp();
        searchMode_ = true;
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

void NetManager::disconnectTcp() {
  if (client_.connected())
    client_.stop();
  lineBuffer_ = "";
  lastSentScreen_ = -1;
  tcpConnected_ = false;
  tcpConnectTime_ = 0;
}

void NetManager::markDataReceived(unsigned long now) {
  lastUpdate_ = now;
  firstDataReceived_ = true;
}

bool NetManager::tryTcpConnect(unsigned long now) {
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

bool NetManager::isSignalLost(unsigned long now) const {
  if (tcpConnected_ && tcpConnectTime_ > 0) {
    unsigned long since = now - tcpConnectTime_;
    if (since < NOCT_SIGNAL_GRACE_MS)
      return false;
  }
  if (!firstDataReceived_ && tcpConnected_)
    return (now - tcpConnectTime_ > NOCT_SIGNAL_GRACE_MS);
  return (now - lastUpdate_ > NOCT_SIGNAL_TIMEOUT_MS);
}

void NetManager::appendLineBuffer(char c) {
  lineBuffer_ += c;
  if (lineBuffer_.length() >= NOCT_TCP_LINE_MAX)
    lineBuffer_ = "";
}

void NetManager::clearLineBuffer() { lineBuffer_ = ""; }

bool NetManager::parsePayload(const String &line, AppState *state) {
  if (!state)
    return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err)
    return false;

  HardwareData &hw = state->hw;
  hw.ct = doc["ct"] | 0;
  hw.gt = doc["gt"] | 0;
  hw.cl = doc["cl"] | 0;
  hw.gl = doc["gl"] | 0;
  hw.cc = doc["cc"] | 0;
  hw.pw = doc["pw"] | 0;
  hw.gh = doc["gh"] | 0;
  hw.gv = doc["gv"] | 0;
  hw.gclock = doc["gclock"] | 0;
  hw.vclock = doc["vclock"] | 0;
  hw.gtdp = doc["gtdp"] | 0;
  hw.ru = doc["ru"] | 0.0f;
  hw.ra = doc["ra"] | 0.0f;
  hw.nd = doc["nd"] | 0;
  hw.nu = doc["nu"] | 0;
  hw.pg = doc["pg"] | 0;
  hw.cf = doc["cf"] | 0;
  hw.s1 = doc["s1"] | 0;
  hw.s2 = doc["s2"] | 0;
  hw.gf = doc["gf"] | 0;
  JsonArray fansArr = doc["fans"];
  for (int i = 0; i < NOCT_FAN_COUNT && i < (int)fansArr.size(); i++)
    hw.fans[i] = fansArr[i] | 0;
  JsonArray fanControlsArr = doc["fan_controls"];
  for (int i = 0; i < NOCT_FAN_COUNT && i < (int)fanControlsArr.size(); i++)
    hw.fan_controls[i] = fanControlsArr[i] | 0;
  JsonArray hddArr = doc["hdd"];
  for (int i = 0; i < NOCT_HDD_COUNT && i < (int)hddArr.size(); i++) {
    const char *n = hddArr[i]["n"];
    if (n && n[0]) {
      hw.hdd[i].name[0] = n[0];
      hw.hdd[i].name[1] = '\0';
    } else {
      hw.hdd[i].name[0] = (char)('C' + i);
      hw.hdd[i].name[1] = '\0';
    }
    hw.hdd[i].used_gb = hddArr[i]["u"] | 0.0f;
    hw.hdd[i].total_gb = hddArr[i]["tot"] | 0.0f;
    hw.hdd[i].temp = hddArr[i]["t"] | 0;
  }
  for (int i = (int)hddArr.size(); i < NOCT_HDD_COUNT; i++) {
    hw.hdd[i].name[0] = (char)('C' + i);
    hw.hdd[i].name[1] = '\0';
    hw.hdd[i].used_gb = 0.0f;
    hw.hdd[i].total_gb = 0.0f;
    hw.hdd[i].temp = 0;
  }
  hw.vu = doc["vu"] | 0.0f;
  hw.vt = doc["vt"] | 0.0f;
  hw.ch = doc["ch"] | 0;
  hw.mb_sys = doc["mb_sys"] | 0;
  hw.mb_vsoc = doc["mb_vsoc"] | 0;
  hw.mb_vrm = doc["mb_vrm"] | 0;
  hw.mb_chipset = doc["mb_chipset"] | 0;
  hw.dr = doc["dr"] | 0;
  hw.dw = doc["dw"] | 0;

  if (!doc["wt"].isNull()) {
    state->weather.temp = doc["wt"] | 0;
    const char *wd = doc["wd"];
    state->weather.desc = String(wd ? wd : "");
    state->weather.wmoCode = doc["wi"] | 0;
    if (state->weather.desc.length() > 0 || state->weather.temp != 0 ||
        state->weather.wmoCode != 0)
      state->weatherReceived = true;
  }

  JsonArray tp = doc["tp"];
  for (int i = 0; i < 3; i++) {
    if (i < (int)tp.size()) {
      const char *n = tp[i]["n"];
      state->process.cpuNames[i] = String(n ? n : "");
      state->process.cpuPercent[i] = tp[i]["c"] | 0;
    } else {
      state->process.cpuNames[i] = "";
      state->process.cpuPercent[i] = 0;
    }
  }

  JsonArray tr = doc["tr"];
  for (int i = 0; i < 2; i++) {
    if (i < (int)tr.size()) {
      const char *n = tr[i]["n"];
      state->process.ramNames[i] = String(n ? n : "");
      state->process.ramMb[i] = tr[i]["r"] | 0;
    } else {
      state->process.ramNames[i] = "";
      state->process.ramMb[i] = 0;
    }
  }

  const char *art = doc["art"];
  const char *trk = doc["trk"];
  state->media.artist = String(art ? art : "");
  state->media.track = String(trk ? trk : "");
  state->media.isPlaying = doc["mp"] | false;
  state->media.isIdle = doc["idle"] | false;
  const char *ms = doc["media_status"];
  state->media.mediaStatus =
      String(ms && strcmp(ms, "PLAYING") == 0 ? "PLAYING" : "PAUSED");

  const char *alert = doc["alert"];
  const char *target = doc["target_screen"];
  const char *metric = doc["alert_metric"];
  state->alertActive = (alert && strcmp(alert, "CRITICAL") == 0);
  if (state->alertActive && target) {
    if (strcmp(target, "MAIN") == 0)
      state->alertTargetScene = NOCT_SCENE_MAIN;
    else if (strcmp(target, "CPU") == 0)
      state->alertTargetScene = NOCT_SCENE_CPU;
    else if (strcmp(target, "GPU") == 0)
      state->alertTargetScene = NOCT_SCENE_GPU;
    else if (strcmp(target, "RAM") == 0)
      state->alertTargetScene = NOCT_SCENE_RAM;
    else if (strcmp(target, "DISKS") == 0)
      state->alertTargetScene = NOCT_SCENE_DISKS;
    else if (strcmp(target, "MEDIA") == 0)
      state->alertTargetScene = NOCT_SCENE_MEDIA;
    else if (strcmp(target, "FANS") == 0)
      state->alertTargetScene = NOCT_SCENE_FANS;
    else if (strcmp(target, "MOTHERBOARD") == 0)
      state->alertTargetScene = NOCT_SCENE_MOTHERBOARD;
    else
      state->alertTargetScene = NOCT_SCENE_MAIN;
  }
  if (state->alertActive && metric) {
    if (strcmp(metric, "ct") == 0)
      state->alertMetric = NOCT_ALERT_CT;
    else if (strcmp(metric, "gt") == 0)
      state->alertMetric = NOCT_ALERT_GT;
    else if (strcmp(metric, "cl") == 0)
      state->alertMetric = NOCT_ALERT_CL;
    else if (strcmp(metric, "gl") == 0)
      state->alertMetric = NOCT_ALERT_GL;
    else if (strcmp(metric, "gv") == 0)
      state->alertMetric = NOCT_ALERT_GV;
    else if (strcmp(metric, "ram") == 0)
      state->alertMetric = NOCT_ALERT_RAM;
    else
      state->alertMetric = -1;
  }
  if (!state->alertActive) {
    state->alertTargetScene = NOCT_SCENE_MAIN;
    state->alertMetric = -1;
  }

  return true;
}
