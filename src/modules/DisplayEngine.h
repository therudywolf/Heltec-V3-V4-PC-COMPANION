/*
 * NOCTURNE_OS — DisplayEngine: Nocturne Style (High-Tech Low-Life).
 * Typography: profont10 (labels), t0_11/haxrcorp4089 (values). Chamfered boxes,
 * segmented bars, dotted grid, XBM icons.
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "../../include/nocturne/config.h"
#include "RollingGraph.h"
#include <U8g2lib.h>

// --- Typography (Nocturne: no proportional/Arial) ---
#define FONT_SMALL u8g2_font_profont10_mr
#define FONT_LABEL u8g2_font_profont10_mr
#define FONT_TINY u8g2_font_profont10_mr
#define FONT_HEAD u8g2_font_t0_11_tr
#define FONT_VAL u8g2_font_t0_11_tr
#define FONT_BIG u8g2_font_haxrcorp4089_tr

// --- XBM Icons (1 = pixel on). Width in bytes = (w+7)/8 ---
#define ICON_WIFI_W 12
#define ICON_WIFI_H 8
#define ICON_WOLF_W 32
#define ICON_WOLF_H 32
#define ICON_LOCK_W 8
#define ICON_LOCK_H 8
#define ICON_WARN_W 12
#define ICON_WARN_H 12
#define ICON_WEATHER_W 16
#define ICON_WEATHER_H 16

extern const uint8_t icon_wifi_bits[];
extern const uint8_t icon_wolf_bits[];
extern const uint8_t icon_lock_bits[];
extern const uint8_t icon_warning_bits[];
extern const uint8_t icon_weather_sun_bits[];
extern const uint8_t icon_weather_cloud_bits[];
extern const uint8_t icon_weather_rain_bits[];
extern const uint8_t icon_weather_snow_bits[];

class DisplayEngine {
public:
  DisplayEngine(int rstPin, int sdaPin, int sclPin);
  void begin();
  void clearBuffer();
  void sendBuffer();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2() { return u8g2_; }

  // Global HUD: solid black header with inverted scene name, dotted footer,
  // WiFi icon
  void drawOverlay(const char *sceneName, int rssi, bool linkOk);

  void drawBiosPost(unsigned long now, unsigned long bootTime, bool wifiOk,
                    int rssi);
  bool biosPostDone(unsigned long now, unsigned long bootTime);

  void drawMetric(int x, int y, const char *label, const char *value);
  void drawMetricStr(int x, int y, const char *label, const String &value);
  void drawProgressBar(int x, int y, int w, int h, float pct);
  void drawChamferBox(int x, int y, int w, int h, int chamfer);
  void drawSegmentedBar(int x, int y, int w, int h, float pct, int segments);
  void drawDottedHLine(int x0, int x1, int y);
  void drawDottedVLine(int x, int y0, int y1);

  void drawWiFiIcon(int x, int y, int rssi);
  void drawWiFiIconXbm(int x, int y, int rssi, bool blink);
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
