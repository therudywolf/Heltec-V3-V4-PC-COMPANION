#include "modules/storage/CaptureExport.h"
#if NOCT_FEATURE_HACKER

#include "modules/network/WifiSniffManager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

static const char *kPcapPath = "/capture.pcap";
static const char *kCsvPath = "/wardrive.csv";

// ESP32 is little-endian; pcap on-disk fields are native LE here.
static void wr32(File &f, uint32_t v) { f.write((const uint8_t *)&v, 4); }
static void wr16(File &f, uint16_t v) { f.write((const uint8_t *)&v, 2); }

int CaptureExport::writePcap(const char *path, WifiSniffManager &sniff) {
  File f = LittleFS.open(path, "w");
  if (!f) return 0;
  // pcap global header
  wr32(f, 0xa1b2c3d4);          // magic
  wr16(f, 2); wr16(f, 4);       // version 2.4
  wr32(f, 0);                   // thiszone
  wr32(f, 0);                   // sigfigs
  wr32(f, WIFISNIFF_CAP_SNAP);  // snaplen
  wr32(f, 105);                 // LINKTYPE_IEEE802_11
  int n = sniff.getCapCount();
  for (int i = 0; i < n; i++) {
    const CapFrame *cf = sniff.getCapFrame(i);
    if (!cf) continue;
    wr32(f, (uint32_t)i);       // ts_sec (synthetic — no RTC; monotonic order)
    wr32(f, 0);                 // ts_usec
    wr32(f, cf->capLen);        // incl_len
    wr32(f, cf->origLen);       // orig_len (true on-air length)
    f.write(cf->data, cf->capLen);
  }
  pcapBytes_ = (uint32_t)f.size();
  f.close();
  return n;
}

int CaptureExport::writeCsv(const char *path, WifiSniffManager &sniff) {
  File f = LittleFS.open(path, "w");
  if (!f) return 0;
  f.print("BSSID,SSID,RSSI,Channel\n");
  int n = sniff.getApCount();
  for (int i = 0; i < n; i++) {
    const WifiSniffAp *ap = sniff.getAp(i);
    if (!ap) continue;
    // SSID may contain commas/quotes — wrap + escape minimally.
    f.print(ap->bssidStr);
    f.print(",\"");
    for (const char *p = ap->ssid; *p; p++) {
      if (*p == '"') f.print('"'); // CSV-escape quotes
      f.print(*p);
    }
    f.print("\",");
    f.print((int)ap->rssi);
    f.print(",");
    f.print((int)ap->channel);
    f.print("\n");
  }
  f.close();
  return n;
}

bool CaptureExport::begin(WifiSniffManager &sniff) {
  if (active_) return true;
  if (!LittleFS.begin(true)) {
    Serial.println("[CAP] LittleFS mount failed");
    return false;
  }
  pcapFrames_ = writePcap(kPcapPath, sniff);
  csvAps_ = writeCsv(kCsvPath, sniff);
  Serial.printf("[CAP] wrote %d frames (%lu B) + %d APs\n", pcapFrames_,
                (unsigned long)pcapBytes_, csvAps_);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid(), apPass());
  strncpy(ipStr_, WiFi.softAPIP().toString().c_str(), sizeof(ipStr_) - 1);
  ipStr_[sizeof(ipStr_) - 1] = '\0';

  server_ = new WebServer(80);
  server_->on("/", [this]() {
    String h = "<!doctype html><meta name=viewport content='width=device-width'>";
    h += "<style>body{font-family:monospace;background:#111;color:#0f0;padding:1em}";
    h += "a{color:#0ff;font-size:1.2em;display:block;margin:.6em 0}</style>";
    h += "<h2>NOCTURNE // capture</h2>";
    h += "<a href='/capture.pcap'>capture.pcap (" + String(pcapFrames_) + " frames, " +
         String(pcapBytes_) + " B)</a>";
    h += "<a href='/wardrive.csv'>wardrive.csv (" + String(csvAps_) + " APs)</a>";
    h += "<p>pcap linktype 105 (802.11) - open in Wireshark.</p>";
    server_->send(200, "text/html", h);
  });
  server_->on("/capture.pcap", [this]() {
    File f = LittleFS.open(kPcapPath, "r");
    if (!f) { server_->send(404, "text/plain", "no pcap"); return; }
    server_->streamFile(f, "application/vnd.tcpdump.pcap");
    f.close();
  });
  server_->on("/wardrive.csv", [this]() {
    File f = LittleFS.open(kCsvPath, "r");
    if (!f) { server_->send(404, "text/plain", "no csv"); return; }
    server_->streamFile(f, "text/csv");
    f.close();
  });
  server_->begin();
  active_ = true;
  Serial.printf("[CAP] SoftAP '%s' @ %s\n", apSsid(), ipStr_);
  return true;
}

void CaptureExport::tick() {
  if (active_ && server_) server_->handleClient();
}

void CaptureExport::end() {
  if (!active_) return;
  if (server_) { server_->stop(); delete server_; server_ = nullptr; }
  WiFi.softAPdisconnect(true);
  active_ = false;
  Serial.println("[CAP] export stopped");
}

#endif // NOCT_FEATURE_HACKER
