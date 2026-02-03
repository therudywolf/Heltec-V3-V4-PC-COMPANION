/*
 * NOCTURNE_OS — DisplayEngine: Grid Law. 128x64 OLED.
 * Labels: profont10. Values: t0_11. Big temp: helvB10 only.
 * Helpers: drawRightAligned, drawCentered, drawSafeStr.
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "../../include/nocturne/config.h"
#include "RollingGraph.h"
#include <U8g2lib.h>

// --- Font strategy (Grid Law): Labels uppercase profont10, Values helvB10 ---
#define FONT_LABEL                                                             \
  u8g2_font_profont10_mr // Labels (static), 6px cap, always uppercase
#define FONT_VAL u8g2_font_helvB10_tr    // Values (dynamic), bold sans-serif
#define FONT_BIG u8g2_font_helvB10_tr    // Main temperature / big values
#define FONT_TINY u8g2_font_profont10_mr // Tiny (city, desc)

// --- XBM Icons ---
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
#define ICON_DISCONNECT_W 16
#define ICON_DISCONNECT_H 16

extern const uint8_t icon_wifi_bits[];
extern const uint8_t icon_wolf_bits[];
extern const uint8_t icon_lock_bits[];
extern const uint8_t icon_warning_bits[];
extern const uint8_t icon_weather_sun_bits[];
extern const uint8_t icon_weather_cloud_bits[];
extern const uint8_t icon_weather_rain_bits[];
extern const uint8_t icon_weather_snow_bits[];
extern const uint8_t icon_disconnect_bits[];

class DisplayEngine {
public:
  DisplayEngine(int rstPin, int sdaPin, int sclPin);
  void begin();
  void clearBuffer();
  void sendBuffer();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2() { return u8g2_; }

  // --- Grid Law helpers (MANDATORY): x_end = right edge for right-align ---
  void drawRightAligned(int x_end, int y, const String &text);
  void drawCentered(int x_center, int y, const String &text);
  void drawCenteredInRect(int x, int y, int w, int h, const String &text);
  void drawSafeStr(int x, int y, const String &text);
  /** If value is 0 or empty, draw small static noise patch; else draw value. */
  void drawValueOrNoise(int x_end, int y, const String &value);

  // --- HUD: Top bar Y 0–10, dotted line Y=11 ---
  void drawOverlay(const char *sceneName, int rssi, bool linkOk,
                   const char *timeStr = nullptr);

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
  void drawDisconnectIcon(int x, int y);

  void setDataSpike(bool spike);
  void drawGlitchEffect();
  /** Decode Base64 to XBM and draw. w×h must match buffer size (e.g. 64×64 =
   * 512 bytes). */
  bool drawXBMArtFromBase64(int x, int y, int w, int h, const String &base64);

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
