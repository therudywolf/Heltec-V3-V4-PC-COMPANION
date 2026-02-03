/*
 * NOCTURNE_OS — DisplayEngine: Grid-based 128x64 OLED. Cyberpunk aesthetic.
 * Zero-overlap: drawRightAligned logic; fonts TINY / MID / HUGE.
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "../../include/nocturne/config.h"
#include "RollingGraph.h"
#include <U8g2lib.h>

// --- Font strategy: 5m rule — HUGE = main values, LABEL = readable labels ---
#define HUGE_FONT u8g2_font_logisoso24_tr   // Main values (~24px)
#define LABEL_FONT u8g2_font_profont12_tr   // Labels (~9px)
#define STORAGE_FONT u8g2_font_profont11_tr // Storage list
#define TINY_FONT LABEL_FONT
#define MID_FONT LABEL_FONT
#define FONT_TINY TINY_FONT
#define FONT_LABEL LABEL_FONT

// --- XBM Icons ---
#define ICON_WIFI_W 12
#define ICON_WIFI_H 8
#define ICON_WOLF_W 32
#define ICON_WOLF_H 32
#define WEATHER_ICON_W 32
#define WEATHER_ICON_H 32

extern const uint8_t icon_wifi_bits[];
extern const uint8_t icon_wolf_bits[];
// 32x32 pixel-art weather (Sun, Cloud, Rain, Snow)
extern const uint8_t icon_weather_sun_32_bits[];
extern const uint8_t icon_weather_cloud_32_bits[];
extern const uint8_t icon_weather_rain_32_bits[];
extern const uint8_t icon_weather_snow_32_bits[];

class DisplayEngine {
public:
  DisplayEngine(int rstPin, int sdaPin, int sclPin);
  void begin();
  void clearBuffer();
  void sendBuffer();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2() { return u8g2_; }

  // --- Cyberpunk helpers (implement first) ---
  /** Random horizontal slice shift + optional 10x10 invert. intensity 0..3. */
  void drawGlitch(int intensity);
  /** Dotted line every 2nd pixel row at y (CRT scanline). */
  void drawScanline(int y);
  /** Right-aligned text: draw at (x_anchor - width). font = TINY_FONT etc. */
  void drawRightAligned(int x_anchor, int y, const uint8_t *font,
                        const char *text);
  /** Segmented progress bar: blocks [|||| ] with 1px gap. percent 0..100. */
  void drawProgressBar(int x, int y, int w, int h, int percent);

  // --- Global header (screens 1–5): black bar 0,0,128,9; dotted line Y=9 ---
  void drawGlobalHeader(const char *sceneTitle, const char *timeStr, int rssi);

  // --- Rolling sparkline (with scanlines in background) ---
  void drawRollingGraph(int x, int y, int w, int h, RollingGraph &g,
                        int maxVal);

  // --- WiFi / media ---
  void drawWiFiIcon(int x, int y, int rssi);
  /** Cassette tape procedural animation (spools + moving pixels when PLAYING).
   */
  void drawCassetteAnimation(int x, int y, int w, int h, bool playing,
                             unsigned long nowMs);

  // --- Startup sequence ---
  void drawBiosPost(unsigned long now, unsigned long bootTime, bool wifiOk,
                    int rssi);
  bool biosPostDone(unsigned long now, unsigned long bootTime);
  void drawConfigLoaded(unsigned long now, unsigned long bootTime,
                        const char *message);

  // --- Alert: thick border box + strobe handled in main loop ---
  void drawHazardBorder();
  /** Thick (3px) border around screen edges for RED ALERT. */
  void drawAlertBorder();

  RollingGraph cpuGraph;
  RollingGraph gpuGraph;
  RollingGraph netDownGraph;
  RollingGraph netUpGraph;

private:
  void drawDottedHLine(int x0, int x1, int y);
  bool drawXBMArtFromBase64(int x, int y, int w, int h, const String &base64);

  int sdaPin_;
  int sclPin_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_;
};

#endif
