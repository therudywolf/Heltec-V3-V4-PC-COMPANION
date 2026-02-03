/*
 * NOCTURNE_OS — DisplayEngine: Grid-law, glitch, scanlines, right-aligned text.
 * Resolution 128x64; zero-overlap policy.
 */
#include "DisplayEngine.h"
#include "../../include/nocturne/config.h"
#include <Arduino.h>
#include <Wire.h>
#include <mbedtls/base64.h>

// --- XBM: WiFi 12x8 ---
const uint8_t icon_wifi_bits[] = {0x00, 0x00, 0x00, 0x00, 0x24, 0x00,
                                  0x24, 0x00, 0x24, 0x00, 0x6C, 0x00,
                                  0x6C, 0x00, 0xEE, 0x0E};

// --- XBM: Wolf / BIOS 32x32 ---
const uint8_t icon_wolf_bits[] = {
    0x00, 0x18, 0x18, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00,
    0x00, 0x7E, 0x7E, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x01, 0xFF, 0xFF, 0x80, 0x01, 0xFF, 0xFF, 0x80, 0x03, 0xFF, 0xFF, 0xC0,
    0x03, 0xE7, 0xE7, 0xC0, 0x03, 0xC3, 0xC3, 0xC0, 0x03, 0xC3, 0xC3, 0xC0,
    0x03, 0xE7, 0xE7, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x01, 0xFF, 0xFF, 0x80,
    0x01, 0xFF, 0xFF, 0x80, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x7E, 0x7E, 0x00,
    0x00, 0x7E, 0x7E, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// --- 32x32 Weather: Sun (center + rays) ---
const uint8_t icon_weather_sun_32_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x7E, 0x7E, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// --- 32x32 Weather: Cloud ---
const uint8_t icon_weather_cloud_32_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xE0, 0x00,
    0x00, 0x1F, 0xF8, 0x00, 0x00, 0x38, 0x1C, 0x00, 0x00, 0x30, 0x0C, 0x00,
    0x00, 0x70, 0x0E, 0x00, 0x00, 0x7F, 0xFE, 0x00, 0x00, 0x3F, 0xFC, 0x00,
    0x00, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// --- 32x32 Weather: Rain (cloud + drops) ---
const uint8_t icon_weather_rain_32_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x1F, 0xF8, 0x00,
    0x00, 0x38, 0x1C, 0x00, 0x00, 0x30, 0x0C, 0x00, 0x00, 0x7F, 0xFE, 0x00,
    0x00, 0x3F, 0xFC, 0x00, 0x00, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00,
    0x00, 0x48, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00,
    0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// --- 32x32 Weather: Snow (snowflake) ---
const uint8_t icon_weather_snow_32_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0xDB, 0xDB, 0x00, 0x00, 0x7E, 0x7E, 0x00,
    0x00, 0x3C, 0x3C, 0x00, 0x00, 0x18, 0x18, 0x00, 0x01, 0xFF, 0xFF, 0x80,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x7E, 0x7E, 0x00,
    0x00, 0xDB, 0xDB, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// ---------------------------------------------------------------------------
DisplayEngine::DisplayEngine(int rstPin, int sdaPin, int sclPin)
    : sdaPin_(sdaPin), sclPin_(sclPin), u8g2_(U8G2_R0, rstPin) {}

void DisplayEngine::begin() {
  Wire.begin(sdaPin_, sclPin_);
  u8g2_.begin();
  u8g2_.enableUTF8Print();
}

void DisplayEngine::clearBuffer() { u8g2_.clearBuffer(); }
void DisplayEngine::sendBuffer() { u8g2_.sendBuffer(); }

// ---------------------------------------------------------------------------
// drawGlitch: subtle — 1px max shift, no invert. Call only every ~10s.
// ---------------------------------------------------------------------------
void DisplayEngine::drawGlitch(int intensity) {
  if (intensity <= 0)
    return;
  uint8_t *buf = u8g2_.getBufferPtr();
  if (!buf)
    return;
  const int sliceH = 4;
  const int shift = 1;
  int y0 = 10 + (random(NOCT_DISP_H - sliceH - 12));
  if (y0 < 10)
    y0 = 10;
  if (y0 + sliceH >= NOCT_DISP_H)
    y0 = NOCT_DISP_H - sliceH - 1;
  int p0 = y0 / 8;
  int p1 = (y0 + sliceH - 1) / 8;
  if (p1 >= 8)
    p1 = 7;
  uint8_t tmp[4 * 128];
  for (int p = p0; p <= p1; p++) {
    for (int c = 0; c < 128; c++)
      tmp[(p - p0) * 128 + c] = buf[p * 128 + c];
  }
  int dir = (random(2) == 0) ? -shift : shift;
  for (int p = p0; p <= p1; p++) {
    for (int c = 0; c < 128; c++) {
      int src = c - dir;
      if (src < 0)
        src += 128;
      if (src >= 128)
        src -= 128;
      buf[p * 128 + c] = tmp[(p - p0) * 128 + src];
    }
  }
}

// ---------------------------------------------------------------------------
// drawScanline: dotted line every 2nd pixel at row y (CRT effect)
// ---------------------------------------------------------------------------
void DisplayEngine::drawScanline(int y) {
  if (y < 0 || y >= NOCT_DISP_H)
    return;
  for (int x = 0; x < NOCT_DISP_W; x += 2)
    u8g2_.drawPixel(x, y);
}

// ---------------------------------------------------------------------------
// drawRightAligned: width = getUTF8Width; draw at x_anchor - width (grid law)
// ---------------------------------------------------------------------------
void DisplayEngine::drawRightAligned(int x_anchor, int y, const uint8_t *font,
                                     const char *text) {
  if (!text || !text[0])
    return;
  u8g2_.setFont(font);
  int w = u8g2_.getUTF8Width(text);
  u8g2_.drawUTF8(x_anchor - w, y, text);
}

// ---------------------------------------------------------------------------
// drawProgressBar: segmented [|||| ] 1px gap between blocks
// ---------------------------------------------------------------------------
void DisplayEngine::drawProgressBar(int x, int y, int w, int h, int percent) {
  if (percent > 100)
    percent = 100;
  if (percent < 0)
    percent = 0;
  const int SEG_GAP = 1;
  int blockW = 3;
  int numSegs = (w + SEG_GAP) / (blockW + SEG_GAP);
  if (numSegs < 1)
    numSegs = 1;
  int filled = (percent * numSegs + 50) / 100;
  if (filled > numSegs)
    filled = numSegs;
  for (int i = 0; i < numSegs; i++) {
    int sx = x + i * (blockW + SEG_GAP);
    if (i < filled)
      u8g2_.drawBox(sx, y, blockW, h);
    else {
      u8g2_.drawFrame(sx, y, blockW, h);
    }
  }
}

// ---------------------------------------------------------------------------
// drawDottedHLine: dotted horizontal (pattern 0x55)
// ---------------------------------------------------------------------------
void DisplayEngine::drawDottedHLine(int x0, int x1, int y) {
  if (x0 > x1) {
    int t = x0;
    x0 = x1;
    x1 = t;
  }
  for (int x = x0; x <= x1; x += 2)
    u8g2_.drawPixel(x, y);
}

// ---------------------------------------------------------------------------
// drawDottedVLine: dotted vertical
// ---------------------------------------------------------------------------
void DisplayEngine::drawDottedVLine(int x, int y0, int y1) {
  if (y0 > y1) {
    int t = y0;
    y0 = y1;
    y1 = t;
  }
  for (int y = y0; y <= y1; y += 2)
    u8g2_.drawPixel(x, y);
}

// ---------------------------------------------------------------------------
// drawChamferBox: cut corners (chamferPx from each corner)
// ---------------------------------------------------------------------------
void DisplayEngine::drawChamferBox(int x, int y, int w, int h, int chamferPx) {
  if (chamferPx <= 0 || chamferPx * 2 >= w || chamferPx * 2 >= h) {
    u8g2_.drawFrame(x, y, w, h);
    return;
  }
  int c = chamferPx;
  // Top: left chamfer cut, line, right chamfer cut
  u8g2_.drawLine(x + c, y, x + w - 1 - c, y);
  u8g2_.drawLine(x + w - 1 - c, y, x + w - 1, y + c);
  u8g2_.drawLine(x + w - 1, y + c, x + w - 1, y + h - 1 - c);
  u8g2_.drawLine(x + w - 1, y + h - 1 - c, x + w - 1 - c, y + h - 1);
  u8g2_.drawLine(x + w - 1 - c, y + h - 1, x + c, y + h - 1);
  u8g2_.drawLine(x + c, y + h - 1, x, y + h - 1 - c);
  u8g2_.drawLine(x, y + h - 1 - c, x, y + c);
  u8g2_.drawLine(x, y + c, x + c, y);
}

// ---------------------------------------------------------------------------
// drawScrollIndicator: pixel-thin bar on right edge
// ---------------------------------------------------------------------------
void DisplayEngine::drawScrollIndicator(int y, int h, int totalItems,
                                        int visibleItems, int firstVisible) {
  if (totalItems <= visibleItems || visibleItems <= 0)
    return;
  const int rx = NOCT_DISP_W - 1;
  int thumbH = (visibleItems * h + totalItems - 1) / totalItems;
  if (thumbH < 2)
    thumbH = 2;
  int maxFirst = totalItems - visibleItems;
  int thumbY =
      y + (maxFirst > 0 ? (firstVisible * (h - thumbH) / maxFirst) : 0);
  for (int dy = 0; dy < thumbH; dy++)
    u8g2_.drawPixel(rx, thumbY + dy);
}

// ---------------------------------------------------------------------------
// Global header: 10px black bar, [ SCENE_NAME ] left, WIFI + HH:MM right,
// dotted at Y=10
// ---------------------------------------------------------------------------
void DisplayEngine::drawGlobalHeader(const char *sceneTitle,
                                     const char *timeStr, int rssi) {
  const int barH = 10; /* NOCT_HEADER_H */
  const int textY = 7;

  u8g2_.setDrawColor(0);
  u8g2_.drawBox(0, 0, NOCT_DISP_W, barH);
  u8g2_.setDrawColor(1);

  u8g2_.setFont(TINY_FONT);
  const char *raw = sceneTitle && sceneTitle[0] ? sceneTitle : "HUB";
  char titleBuf[24];
  snprintf(titleBuf, sizeof(titleBuf), "[ %s ]", raw);
  u8g2_.drawUTF8(2, textY, titleBuf);

  const char *tstr = (timeStr && timeStr[0]) ? timeStr : "--:--";
  drawRightAligned(98, textY, TINY_FONT, tstr);

  int iconX = 104;
  int iconY = 1;
  if (rssi > -70)
    u8g2_.drawXBM(iconX, iconY, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);
  else if ((millis() / 200) % 2 == 0)
    u8g2_.drawXBM(iconX, iconY, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);

  drawDottedHLine(0, NOCT_DISP_W - 1, 10);
}

// ---------------------------------------------------------------------------
// drawGreebles: tiny technical decorations in content corners
// ---------------------------------------------------------------------------
void DisplayEngine::drawGreebles() {
  const int top = NOCT_CONTENT_TOP;
  const int bottom = NOCT_DISP_H - 1;
  const int left = 0;
  const int right = NOCT_DISP_W - 1;
  u8g2_.drawPixel(left + 2, top + 2);
  u8g2_.drawPixel(left + 4, top + 2);
  u8g2_.drawPixel(left + 2, top + 4);
  u8g2_.drawPixel(right - 2, top + 2);
  u8g2_.drawPixel(right - 4, top + 2);
  u8g2_.drawPixel(right - 2, top + 4);
  u8g2_.drawPixel(left + 2, bottom - 2);
  u8g2_.drawPixel(left + 2, bottom - 4);
  u8g2_.drawPixel(left + 4, bottom - 2);
  u8g2_.drawPixel(right - 2, bottom - 2);
  u8g2_.drawPixel(right - 2, bottom - 4);
  u8g2_.drawPixel(right - 4, bottom - 2);
}

// ---------------------------------------------------------------------------
// Rolling graph with scanline background
// ---------------------------------------------------------------------------
void DisplayEngine::drawRollingGraph(int x, int y, int w, int h,
                                     RollingGraph &g, int maxVal) {
  for (int ly = y; ly < y + h; ly += 2)
    drawScanline(ly);
  if (g.count < 2 || maxVal <= 0)
    return;
  float scale = (float)(h - 2) / (float)maxVal;
  int n = g.count < NOCT_GRAPH_SAMPLES ? g.count : NOCT_GRAPH_SAMPLES;
  int step = (n > 0 && w > n) ? w / n : 1;
  int px = -1, py = -1;
  for (int i = 0; i < n; i++) {
    int idx = (g.head - 1 - i + NOCT_GRAPH_SAMPLES) % NOCT_GRAPH_SAMPLES;
    float v = g.values[idx];
    int gx = x + w - 1 - i * step;
    int gy = y + h - 1 - (int)(v * scale);
    if (gy < y)
      gy = y;
    if (gy >= y + h)
      gy = y + h - 1;
    if (px >= 0) {
      u8g2_.drawLine(px, py, gx, gy);
      u8g2_.drawLine(px, py + 1, gx, gy + 1);
    } else {
      u8g2_.drawPixel(gx, gy);
      u8g2_.drawPixel(gx, gy + 1);
    }
    px = gx;
    py = gy;
  }
}

// ---------------------------------------------------------------------------
void DisplayEngine::drawWiFiIcon(int x, int y, int rssi) {
  (void)rssi;
  u8g2_.drawXBM(x, y, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);
}

// ---------------------------------------------------------------------------
// Cassette tape: two spool circles + rectangle; moving pixels when playing
// ---------------------------------------------------------------------------
void DisplayEngine::drawCassetteAnimation(int x, int y, int w, int h,
                                          bool playing, unsigned long nowMs) {
  const int cx = x + w / 2;
  const int cy = y + h / 2;
  const int r = (w < h ? w : h) / 4;
  int leftCx = cx - r - 2;
  int rightCx = cx + r + 2;
  u8g2_.drawCircle(leftCx, cy, r);
  u8g2_.drawCircle(rightCx, cy, r);
  int rectLeft = leftCx - r - 1;
  int rectW = (rightCx + r + 1) - rectLeft;
  int rectTop = cy - 2;
  u8g2_.drawFrame(rectLeft, rectTop, rectW, 5);
  if (playing) {
    int phase = (nowMs / 80) % 8;
    for (int i = 0; i < 3; i++) {
      int t = (phase + i * 3) % 8;
      int px, py;
      if (t < 4) {
        px = rectLeft + 2 + (t * (rectW - 4) / 4);
        py = cy;
      } else {
        px = rectLeft + rectW - 2 - ((t - 4) * (rectW - 4) / 4);
        py = cy;
      }
      if (px >= rectLeft && px < rectLeft + rectW && py >= rectTop &&
          py < rectTop + 5)
        u8g2_.drawPixel(px, py);
    }
  }
}

// ---------------------------------------------------------------------------
// Legacy: Base64 XBM art (unused; media uses cassette animation)
// ---------------------------------------------------------------------------
bool DisplayEngine::drawXBMArtFromBase64(int x, int y, int w, int h,
                                         const String &base64) {
  if (base64.length() == 0)
    return false;
  size_t outLen = 0;
  const size_t maxDec = (size_t)((w * h + 7) / 8);
  if (maxDec > 1024)
    return false;
  uint8_t decoded[1024];
  int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &outLen,
                                  (const unsigned char *)base64.c_str(),
                                  base64.length());
  if (ret != 0 || outLen != maxDec)
    return false;
  u8g2_.drawXBMP(x, y, (uint16_t)w, (uint16_t)h, decoded);
  return true;
}

// ---------------------------------------------------------------------------
// Startup: single static splash "Forest OS" / "By RudyWolf"
// ---------------------------------------------------------------------------
void DisplayEngine::drawSplash() {
  u8g2_.setFont(HUGE_FONT);
  const char *line1 = "Forest OS";
  int w1 = u8g2_.getUTF8Width(line1);
  int x1 = (NOCT_DISP_W - w1) / 2;
  int y1 = NOCT_DISP_H / 2 - 10;
  u8g2_.drawUTF8(x1, y1, line1);

  u8g2_.setFont(LABEL_FONT);
  const char *line2 = "By RudyWolf";
  int w2 = u8g2_.getUTF8Width(line2);
  int x2 = (NOCT_DISP_W - w2) / 2;
  int y2 = NOCT_DISP_H / 2 + 6;
  u8g2_.drawUTF8(x2, y2, line2);
}

// ---------------------------------------------------------------------------
// Legacy: Wolf BIOS (kept for compatibility; use drawSplash in main)
// ---------------------------------------------------------------------------
void DisplayEngine::drawBiosPost(unsigned long now, unsigned long bootTime,
                                 bool wifiOk, int rssi) {
  (void)now;
  (void)bootTime;
  (void)wifiOk;
  (void)rssi;
  drawSplash();
}

bool DisplayEngine::biosPostDone(unsigned long now, unsigned long bootTime) {
  return (now - bootTime) >= (unsigned long)NOCT_SPLASH_MS;
}

// ---------------------------------------------------------------------------
// Config loaded (legacy): now same as splash
// ---------------------------------------------------------------------------
void DisplayEngine::drawConfigLoaded(unsigned long now, unsigned long bootTime,
                                     const char *message) {
  (void)bootTime;
  (void)now;
  (void)message;
  drawSplash();
}

// ---------------------------------------------------------------------------
// Hazard stripe border (legacy)
// ---------------------------------------------------------------------------
void DisplayEngine::drawHazardBorder() { drawAlertBorder(); }

// ---------------------------------------------------------------------------
// Thick border box around screen edges (RED ALERT overlay)
// ---------------------------------------------------------------------------
void DisplayEngine::drawAlertBorder() {
  const int thick = 3;
  for (int t = 0; t < thick; t++) {
    u8g2_.drawFrame(t, t, NOCT_DISP_W - 2 * t, NOCT_DISP_H - 2 * t);
  }
}
