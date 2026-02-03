/*
 * NOCTURNE_OS — DisplayEngine: Layout Engine. 128x64 OLED.
 * Bounding-box helpers guarantee zero overlaps. Font fallback for wide values.
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "../../include/nocturne/config.h"
#include "RollingGraph.h"
#include <U8g2lib.h>

// --- Font strategy: Labels tiny, Values big with narrow fallback ---
#define FONT_LABEL u8g2_font_profont10_mr      // Labels (static), 6px cap
#define FONT_VAL u8g2_font_helvB10_tr          // Values (primary), bold
#define FONT_VAL_NARROW u8g2_font_profont12_tr // Fallback when value too wide
#define FONT_BIG u8g2_font_helvB10_tr          // Main temperature
#define FONT_TINY u8g2_font_profont10_mr       // Tiny (city, desc)

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

  // --- Layout helpers: bounding box based, no magic numbers ---
  /** Draw a block: clear rect, label (tiny) at (x+2,y+8), value right at
   * (x+w-2). If warning, invert block colors. Dotted frame. Font fallback if
   * value too wide. */
  void drawBlock(int x, int y, int w, int h, const String &label,
                 const String &value, bool warning = false);
  /** Progress bar: frame, fill with segments (1px gap). percent 0..100. */
  void drawProgressBar(int x, int y, int w, int h, float percent);

  // --- Legacy alignment helpers (use drawBlock when possible) ---
  void drawRightAligned(int x_end, int y, const String &text);
  void drawCentered(int x_center, int y, const String &text);
  void drawCenteredInRect(int x, int y, int w, int h, const String &text);
  void drawSafeStr(int x, int y, const String &text);
  void drawValueOrNoise(int x_end, int y, const String &value);

  // --- HUD: Top bar Y 0–10, dotted line Y=11 ---
  void drawOverlay(const char *sceneName, int rssi, bool linkOk,
                   const char *timeStr = nullptr);

  void drawBiosPost(unsigned long now, unsigned long bootTime, bool wifiOk,
                    int rssi);
  bool biosPostDone(unsigned long now, unsigned long bootTime);

  void drawMetric(int x, int y, const char *label, const char *value);
  void drawMetricStr(int x, int y, const char *label, const String &value);
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
