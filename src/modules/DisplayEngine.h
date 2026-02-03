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
#define LABEL_FONT u8g2_font_profont10_mr // Tiny tech labels (profont10)
#define VALUE_FONT u8g2_font_helvB10_tr   // Bold readable values (no resize)
#define HUGE_FONT u8g2_font_logisoso24_tr // Hero only (weather etc)
#define SECONDARY_FONT VALUE_FONT
#define CYRILLIC_FONT u8g2_font_6x10_tf
#define TINY_FONT LABEL_FONT
#define MID_FONT SECONDARY_FONT
#define FONT_TINY TINY_FONT
#define FONT_LABEL LABEL_FONT
#define FONT_SECONDARY SECONDARY_FONT
#define FONT_HEADER HEADER_FONT
#define HEXDECOR_FONT u8g2_font_5x7_tf

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
  /** (DEPRECATED v4: no-op) Was CRT scanline — removed for clean Cyberpunk. */
  void drawScanline(int y);
  /** (DEPRECATED v4: no-op) Was CRT scanlines — removed for clean Cyberpunk. */
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
  /** Tech brackets: four L-shaped corners only (viewfinder / targeting frame).
   */
  void drawTechBrackets(int x, int y, int w, int h, int bracketLen);
  /** Circuit trace: 1px line (x1,y1)->(x2,y2); optional 2x2 dot at end. */
  void drawCircuitTrace(int x1, int y1, int x2, int y2, bool connectDot);
  /** Tech frame: corners NOT connected (gaps) or chamfered. */
  void drawTechFrame(int x, int y, int w, int h);
  /** Vertical bracket [ or ] to group text. facingLeft: true = [ */
  void drawTechBracket(int x, int y, int h, bool facingLeft);
  /** Small + crosshair at (x,y). */
  void drawCrosshair(int x, int y);
  /** Bracketed progress bar: [ ||||||||     ] percent 0..100. */
  void drawBracketedBar(int x, int y, int w, int h, int percent);
  /** Dotted horizontal line from (x0,y) to (x1,y). Pattern 0x55. */
  void drawDottedHLine(int x0, int x1, int y);
  /** Dotted vertical line from (x,y0) to (x,y1). */
  void drawDottedVLine(int x, int y0, int y1);
  /** Scroll indicator on right edge: totalItems, visibleItems, firstVisible. */
  void drawScrollIndicator(int y, int h, int totalItems, int visibleItems,
                           int firstVisible);
  /** Small static greebles (tiny squares/crosses) in content area corners. */
  void drawGreebles();

  // --- Netrunner / Cyberpunk mechanics ---
  /** Decrypt animation: reveal finalStr from left; rest = random hex. speed =
   * ms per char. */
  void drawDecryptedText(int x, int y, const char *finalStr,
                         int speedMsPerChar);
  /** PCB-style line: solid circle at start, hollow diamond at end (near text).
   */
  void drawCircuitLine(int x_start, int y_start, int x_end, int y_end);
  /** Hex data decor at bottom corners. corner: 0 = bottom-left, 1 =
   * bottom-right. */
  void drawHexDecoration(int corner);
  /** Fallback weather: 0-3=circle (sun), 45-48=rounded box (cloud),
   * 50+=box+dotted (rain). */
  void drawWeatherPrimitive(int x, int y, int wmoCode);

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
  static char scrambleBuf_[32];

  int sdaPin_;
  int sclPin_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_;
};

#endif
