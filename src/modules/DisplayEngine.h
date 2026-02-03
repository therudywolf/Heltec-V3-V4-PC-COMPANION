/*
 * NOCTURNE_OS — DisplayEngine: U8g2, buffer, glitch, rolling graphs.
 * Fonts: u8g2_font_profont10_mr (small), u8g2_font_helvB10_tr
 * (headers/numbers).
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "../../include/nocturne/config.h"
#include "RollingGraph.h"
#include <U8g2lib.h>


#define FONT_SMALL u8g2_font_profont10_mr
#define FONT_HEAD u8g2_font_helvB10_tr
#define FONT_LABEL u8g2_font_profont10_mr
#define FONT_VAL u8g2_font_helvB10_tr
#define FONT_TINY u8g2_font_profont10_mr
#define FONT_BIG u8g2_font_helvB10_tr

class DisplayEngine {
public:
  DisplayEngine(int rstPin, int sdaPin, int sclPin);
  void begin();
  void clearBuffer();
  void sendBuffer();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2() { return u8g2_; }

  void drawBiosPost(unsigned long now, unsigned long bootTime, bool wifiOk,
                    int rssi);
  bool biosPostDone(unsigned long now, unsigned long bootTime);

  void drawMetric(int x, int y, const char *label, const char *value);
  void drawMetricStr(int x, int y, const char *label, const String &value);
  void drawProgressBar(int x, int y, int w, int h, float pct);
  void drawWiFiIcon(int x, int y, int rssi);
  void drawCornerCrosshairs();
  void drawFanIcon(int x, int y, int frame);
  void drawLinkStatus(int x, int y, bool linked);

  void setDataSpike(bool spike);
  void drawGlitchEffect();

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
  unsigned long lastGlitchTrigger_;
  unsigned long glitchUntil_;
};

#endif
