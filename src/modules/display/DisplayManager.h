/*
 * NOCTURNE_OS — DisplayManager: Terminal-style OLED for BMW Assistant (Heltec V4).
 * Single font, clearBuffer → draw full frame → sendBuffer. Rate-limited 10 Hz.
 * 128x64, 3 zones: Row1 BLE | Uptime; Row2 RX/TX/ERR; Row3–4 last I-Bus event.
 */
#ifndef NOCTURNE_DISPLAY_MANAGER_H
#define NOCTURNE_DISPLAY_MANAGER_H

#include "nocturne/config.h"

class DisplayEngine;
class BmwManager;

class DisplayManager {
 public:
  DisplayManager(DisplayEngine &display, BmwManager &bmw);

  /** Draw one frame (clear → draw all zones → send). No-op if rate limit not elapsed. Returns true if frame was sent. */
  bool update(unsigned long nowMs);

  /** Force next update() to draw (reset rate limit). */
  void requestRefresh();

 private:
  void drawFrame(unsigned long nowMs);

  DisplayEngine &display_;
  BmwManager &bmw_;
  unsigned long lastRefreshMs_ = 0;
  static const unsigned long kMinRefreshIntervalMs = 100;  /* 10 Hz max */
};

#endif
