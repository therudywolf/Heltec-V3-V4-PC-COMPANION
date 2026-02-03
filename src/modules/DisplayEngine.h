/*
 * NOCTURNE_OS — DisplayEngine: Grid-based 128x64 OLED. Cyberpunk aesthetic.
 * Zero-overlap: drawRightAligned logic; fonts TINY / MID / HUGE.
 */
#ifndef NOCTURNE_DISPLAY_ENGINE_H
#define NOCTURNE_DISPLAY_ENGINE_H

#include "../../include/nocturne/config.h"
#include "RollingGraph.h"
#include <U8g2lib.h>

// --- NOCTURNE Design System: Forest OS v3 — Terminal / Cyberpunk ---
#define HEADER_FONT u8g2_font_t0_11_tr    // Terminal-style headers
#define LABEL_FONT u8g2_font_profont10_mr // Tiny monospaced labels
#define VALUE_FONT u8g2_font_helvB10_tr   // Clean bold values
#define HUGE_FONT u8g2_font_logisoso24_tr // Hero: main values (87°, 99%)
#define SECONDARY_FONT VALUE_FONT
#define CYRILLIC_FONT u8g2_font_6x10_tf
#define TINY_FONT LABEL_FONT
#define MID_FONT SECONDARY_FONT
#define FONT_TINY TINY_FONT
#define FONT_LABEL LABEL_FONT
#define FONT_SECONDARY SECONDARY_FONT
#define FONT_HEADER HEADER_FONT

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
  /** Full-screen CRT scanlines: black line every 2 (or 4 if subtle) pixels. */
  void drawScanlines(bool everyFourth = false);
  /** Right-aligned text: draw at (x_anchor - width). font = TINY_FONT etc. */
  void drawRightAligned(int x_anchor, int y, const uint8_t *font,
                        const char *text);
  /** Right-aligned text; if width > available_space, switch to LABEL_FONT. */
  void drawSafeRightAligned(int x_anchor, int y, int available_space,
                            const uint8_t *font, const char *text);
  /** Segmented progress bar: blocks [|||| ] with 1px gap. percent 0..100. */
  void drawProgressBar(int x, int y, int w, int h, int percent);
  void drawSegmentedBar(int x, int y, int w, int h, int percent) {
    drawProgressBar(x, y, w, h, percent);
  }

  /** Chamfered box (cut corners). chamferPx = corner cut size. */
  void drawChamferBox(int x, int y, int w, int h, int chamferPx);
  /** Dotted horizontal line from (x0,y) to (x1,y). Pattern 0x55. */
  void drawDottedHLine(int x0, int x1, int y);
  /** Dotted vertical line from (x,y0) to (x,y1). */
  void drawDottedVLine(int x, int y0, int y1);
  /** Scroll indicator on right edge: totalItems, visibleItems, firstVisible. */
  void drawScrollIndicator(int y, int h, int totalItems, int visibleItems,
                           int firstVisible);
  /** Small static greebles (tiny squares/crosses) in content area corners. */
  void drawGreebles();

  // --- Global header: 10px bar, [ SCENE_NAME ] left, WIFI + HH:MM right,
  // dotted at Y=10 ---
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
  /** Single static splash for full boot: "Forest OS" + "By RudyWolf". */
  void drawSplash();
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
  bool drawXBMArtFromBase64(int x, int y, int w, int h, const String &base64);

  int sdaPin_;
  int sclPin_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_;
};

#endif
