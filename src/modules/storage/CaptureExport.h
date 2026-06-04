/*
 * NOCTURNE_OS — CaptureExport: pull WiFi captures off the device (#24).
 *
 * On begin() it writes the current sniffer state to LittleFS as a real .pcap
 * (linktype 105 / IEEE 802.11, Wireshark-openable) plus a wardriving .csv of the
 * seen APs, then brings up a SoftAP + tiny web server so a laptop can download
 * them. Passive: it only serializes what the promiscuous sniffer already saw.
 */
#ifndef NOCTURNE_CAPTURE_EXPORT_H
#define NOCTURNE_CAPTURE_EXPORT_H

#include "nocturne/config.h"
#if NOCT_FEATURE_HACKER
#include <Arduino.h>

class WifiSniffManager;
class WebServer;

class CaptureExport {
public:
  bool begin(WifiSniffManager &sniff); // write files + start SoftAP + HTTP server
  void tick();                         // service HTTP clients
  void end();                          // stop server + SoftAP
  bool isActive() const { return active_; }

  static const char *apSsid() { return "Nocturne-DL"; }
  static const char *apPass() { return "nocturne1234"; } // >=8 chars for WPA2
  const char *ip() const { return ipStr_; }
  int pcapFrames() const { return pcapFrames_; }
  uint32_t pcapBytes() const { return pcapBytes_; }
  int csvAps() const { return csvAps_; }

private:
  int writePcap(const char *path, WifiSniffManager &sniff);
  int writeCsv(const char *path, WifiSniffManager &sniff);

  bool active_ = false;
  WebServer *server_ = nullptr;
  char ipStr_[16] = {0};
  int pcapFrames_ = 0;
  uint32_t pcapBytes_ = 0;
  int csvAps_ = 0;
};

#endif // NOCT_FEATURE_HACKER
#endif
