/*
 * NOCTURNE_OS — DisplayEngine: U8g2, buffering, glitch 2.0, BIOS POST,
 * typography Full frame buffer: clear -> draw -> send. No per-cell black-box
 * anti-ghosting.
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "RollingGraph.h"
#include "Types.h"
#include "config.h"
#include <U8g2lib.h>

// Typography: system logs vs values
#define FONT_LOG u8g2_font_tom_thumb_4x6_tr
#define FONT_VAL u8g2_font_6x12_tf
#define FONT_LABEL u8g2_font_micro_tr
#define FONT_HEAD u8g2_font_helvB08_tf
#define FONT_BIG u8g2_font_helvB12_tf
#define FONT_TINY u8g2_font_tom_thumb_4x6_tr

class DisplayEngine {
public:
  DisplayEngine(int rstPin, int sdaPin, int sclPin);
  void begin();
  void clearBuffer();
  void sendBuffer();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2() { return u8g2_; }

  // --- Boot: fake BIOS POST ---
  void drawBiosPost(unsigned long now, unsigned long bootTime, bool wifiOk,
                    int rssi);
  bool biosPostDone(unsigned long now, unsigned long bootTime);

  // --- Primitives (no black-box clear; full buffer clear only) ---
  void drawMetric(int x, int y, const char *label, const char *value);
  void drawMetricStr(int x, int y, const char *label, const String &value);
  void drawProgressBar(int x, int y, int w, int h, float pct);
  void drawWiFiIcon(int x, int y, int rssi);
  void drawCornerCrosshairs();
  void drawFanIcon(int x, int y, int frame);
  void drawLinkStatus(int x, int y, bool linked);

  // --- Glitch: horizontal shift 2–4 px every ~45 s for 100 ms ---
  void setDataSpike(bool spike);
  void drawGlitchEffect(); // Timed glitch (45s interval, 100ms duration)

  // --- Rolling graphs (sparklines) ---
  RollingGraph cpuGraph;
  RollingGraph gpuGraph;
  RollingGraph netDownGraph;
  RollingGraph netUpGraph;

  void drawRollingGraph(int x, int y, int w, int h, RollingGraph &g,
                        int maxVal);

private:
  int sdaPin_;
  int sclPin_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_;
  bool dataSpike_;
  unsigned long lastGlitchTrigger_; // Last time we started a glitch
  unsigned long glitchUntil_; // End of current glitch window (0 = inactive)
};

#endif
