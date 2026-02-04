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

void DisplayEngine::flipScreen() {
  screenRotated_ = !screenRotated_;
  u8g2_.setFlipMode(screenRotated_ ? 1 : 0);
}

void DisplayEngine::setScreenFlipped(bool flipped) {
  screenRotated_ = flipped;
  u8g2_.setFlipMode(screenRotated_ ? 1 : 0);
}

void DisplayEngine::begin() {
  Wire.begin(sdaPin_, sclPin_);
  Wire.setClock(800000); // V4 overclock: 800kHz for fluid frames (if artifacts,
                         // use 600000)
  u8g2_.begin();
  u8g2_.enableUTF8Print();
}

void DisplayEngine::clearBuffer() { u8g2_.clearBuffer(); }
void DisplayEngine::sendBuffer() { u8g2_.sendBuffer(); }

// ---------------------------------------------------------------------------
// drawScanlines: REMOVED for Clean Cyberpunk v4 — crisp, bright display.
// ---------------------------------------------------------------------------
void DisplayEngine::drawScanlines(bool everyFourth) { (void)everyFourth; }

// ---------------------------------------------------------------------------
// drawGlitch: horizontal slice offset 2–5px + random invert block. intensity
// 1–3.
// ---------------------------------------------------------------------------
void DisplayEngine::drawGlitch(int intensity) {
  if (intensity <= 0)
    return;
  uint8_t *buf = u8g2_.getBufferPtr();
  if (!buf)
    return;
  const int sliceH = 6;
  int shift = 2 + (intensity >= 2 ? random(2, 6) : 2); // 2–5 px
  int y0 = NOCT_HEADER_H + random(NOCT_DISP_H - sliceH - NOCT_HEADER_H - 4);
  if (y0 < NOCT_HEADER_H)
    y0 = NOCT_HEADER_H;
  if (y0 + sliceH >= NOCT_DISP_H)
    y0 = NOCT_DISP_H - sliceH - 1;
  int p0 = y0 / 8;
  int p1 = (y0 + sliceH - 1) / 8;
  if (p1 >= 8)
    p1 = 7;
  uint8_t tmp[8 * 128];
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
  // Invert a random rectangular block
  int bx = 4 + random(NOCT_DISP_W - 24);
  int by = NOCT_HEADER_H + 2 + random(NOCT_DISP_H - NOCT_HEADER_H - 14);
  int bw = 10 + random(20);
  int bh = 6 + random(8);
  if (bx + bw > NOCT_DISP_W)
    bw = NOCT_DISP_W - bx;
  if (by + bh > NOCT_DISP_H)
    bh = NOCT_DISP_H - by;
  int rowStart = by / 8;
  int rowEnd = (by + bh - 1) / 8;
  for (int row = rowStart; row <= rowEnd; row++) {
    for (int col = bx; col < bx + bw && col < 128; col++)
      buf[row * 128 + col] ^= 0xFF;
  }
}

// ---------------------------------------------------------------------------
// drawScanline: REMOVED for Clean Cyberpunk v4.
// ---------------------------------------------------------------------------
void DisplayEngine::drawScanline(int y) { (void)y; }

// ---------------------------------------------------------------------------
// drawCentered: center text horizontally at given Y. Uses current font.
// ---------------------------------------------------------------------------
void DisplayEngine::drawCentered(int y, const char *text) {
  if (!text || !text[0])
    return;
  int w = u8g2_.getUTF8Width(text);
  int x = (NOCT_DISP_W - w) / 2;
  if (x < 0)
    x = 0;
  u8g2_.drawUTF8(x, y, text);
}

// ---------------------------------------------------------------------------
// drawRightAligned: width = getUTF8Width; draw at x_anchor - width (grid law).
// Safety: if x - w < 0 (left overflow), switch to TINY_FONT and redraw.
// ---------------------------------------------------------------------------
void DisplayEngine::drawRightAligned(int x_anchor, int y, const uint8_t *font,
                                     const char *text) {
  if (!text || !text[0])
    return;
  u8g2_.setFont(font);
  int w = u8g2_.getUTF8Width(text);
  if (x_anchor - w < 0) {
    u8g2_.setFont(TINY_FONT);
    w = u8g2_.getUTF8Width(text);
  }
  u8g2_.drawUTF8(x_anchor - w, y, text);
}

// ---------------------------------------------------------------------------
// drawSafeRightAligned: if text width > available_space, use LABEL_FONT (Tiny)
// ---------------------------------------------------------------------------
void DisplayEngine::drawSafeRightAligned(int x_anchor, int y,
                                         int available_space,
                                         const uint8_t *font,
                                         const char *text) {
  if (!text || !text[0])
    return;
  u8g2_.setFont(font);
  int w = u8g2_.getUTF8Width(text);
  if (w > available_space) {
    u8g2_.setFont(LABEL_FONT);
    w = u8g2_.getUTF8Width(text);
  }
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
// drawTechFrame: frame with gaps at corners (chamfered / cut corners)
// ---------------------------------------------------------------------------
void DisplayEngine::drawTechFrame(int x, int y, int w, int h) {
  const int c = 2; // chamfer size
  if (w < c * 2 + 2 || h < c * 2 + 2) {
    u8g2_.drawFrame(x, y, w, h);
    return;
  }
  // Top: left gap, line, right gap
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
// drawTechBracket (x,y,w,h,len): corner bracket frame [  ] / viewfinder
// ---------------------------------------------------------------------------
void DisplayEngine::drawTechBracket(int x, int y, int w, int h, int len) {
  u8g2_.drawHLine(x, y, len);
  u8g2_.drawVLine(x, y, len);

  u8g2_.drawHLine(x + w - len, y, len);
  u8g2_.drawVLine(x + w - 1, y, len);

  u8g2_.drawHLine(x, y + h - 1, len);
  u8g2_.drawVLine(x, y + h - len, len);

  u8g2_.drawHLine(x + w - len, y + h - 1, len);
  u8g2_.drawVLine(x + w - 1, y + h - len, len);
}

// ---------------------------------------------------------------------------
// drawTechBracket: vertical bracket [ or ] to group text
// ---------------------------------------------------------------------------
void DisplayEngine::drawTechBracket(int x, int y, int h, bool facingLeft) {
  if (h < 4)
    return;
  if (facingLeft) { // [
    u8g2_.drawPixel(x, y);
    u8g2_.drawPixel(x, y + h - 1);
    u8g2_.drawLine(x, y, x + 2, y);
    u8g2_.drawLine(x, y + h - 1, x + 2, y + h - 1);
    u8g2_.drawLine(x, y + 1, x, y + h - 2);
  } else { // ]
    int xr = x + 2;
    u8g2_.drawPixel(xr, y);
    u8g2_.drawPixel(xr, y + h - 1);
    u8g2_.drawLine(x, y, xr, y);
    u8g2_.drawLine(x, y + h - 1, xr, y + h - 1);
    u8g2_.drawLine(xr, y + 1, xr, y + h - 2);
  }
}

// ---------------------------------------------------------------------------
// drawCrosshair: small + at (x,y)
// ---------------------------------------------------------------------------
void DisplayEngine::drawCrosshair(int x, int y) {
  u8g2_.drawLine(x - 2, y, x + 2, y);
  u8g2_.drawLine(x, y - 2, x, y + 2);
}

// ---------------------------------------------------------------------------
// drawBracketedBar: [ ||||||||     ] style progress bar
// ---------------------------------------------------------------------------
void DisplayEngine::drawBracketedBar(int x, int y, int w, int h, int percent) {
  if (percent > 100)
    percent = 100;
  if (percent < 0)
    percent = 0;
  const int bracketW = 3; // each bracket is 3px wide
  const int innerW = w - 2 * bracketW;
  if (innerW < 2)
    return;
  // Draw brackets [ ]
  drawTechBracket(x, y, h, true);
  drawTechBracket(x + w - bracketW, y, h, false);
  // Inner bar fill
  int filled = (percent * innerW + 50) / 100;
  if (filled > innerW)
    filled = innerW;
  int bx = x + bracketW;
  int by = y + 1;
  int bh = h > 2 ? h - 2 : 1;
  if (bh > 0 && filled > 0)
    u8g2_.drawBox(bx, by, filled, bh);
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
// drawTechBrackets: four L-shaped corners only (viewfinder / targeting frame).
// bracketLen = length of each leg of the L at every corner.
// ---------------------------------------------------------------------------
void DisplayEngine::drawTechBrackets(int x, int y, int w, int h,
                                     int bracketLen) {
  if (w < 2 || h < 2)
    return;
  int L = bracketLen;
  if (L > (w - 2) / 2)
    L = (w - 2) / 2;
  if (L > (h - 2) / 2)
    L = (h - 2) / 2;
  if (L < 1)
    L = 1;
  int x2 = x + w - 1;
  int y2 = y + h - 1;
  // Top-left: horizontal right, vertical down
  u8g2_.drawLine(x, y, x + L, y);
  u8g2_.drawLine(x, y, x, y + L);
  // Top-right: horizontal left, vertical down
  u8g2_.drawLine(x2 - L, y, x2, y);
  u8g2_.drawLine(x2, y, x2, y + L);
  // Bottom-right: horizontal left, vertical up
  u8g2_.drawLine(x2 - L, y2, x2, y2);
  u8g2_.drawLine(x2, y2 - L, x2, y2);
  // Bottom-left: horizontal right, vertical up
  u8g2_.drawLine(x, y2, x + L, y2);
  u8g2_.drawLine(x, y2 - L, x, y2);
}

// ---------------------------------------------------------------------------
// drawCircuitTrace: 1px line between (x1,y1) and (x2,y2). If connectDot,
// draw a 2x2 filled square centered at the end point (PCB node).
// ---------------------------------------------------------------------------
void DisplayEngine::drawCircuitTrace(int x1, int y1, int x2, int y2,
                                     bool connectDot) {
  u8g2_.drawLine(x1, y1, x2, y2);
  if (connectDot) {
    // 2x2 node at end point (sharp, minimal)
    u8g2_.drawBox(x2 - 1, y2 - 1, 2, 2);
  }
}

// ---------------------------------------------------------------------------
// Netrunner decrypt: reveal finalStr from left; unrevealed = random hex.
// speedMsPerChar = ms per character to reveal (e.g. 30 = ~500ms for 16 chars).
// ---------------------------------------------------------------------------
char DisplayEngine::scrambleBuf_[32];

void DisplayEngine::drawDecryptedText(int x, int y, const char *finalStr,
                                      int speedMsPerChar) {
  if (!finalStr)
    return;
  static const char *s_lastStr = nullptr;
  static unsigned long s_startMs = 0;
  if (finalStr != s_lastStr) {
    s_lastStr = finalStr;
    s_startMs = millis();
  }
  size_t len = 0;
  while (finalStr[len] != '\0' && len < sizeof(scrambleBuf_) - 1)
    len++;
  if (len == 0)
    return;
  unsigned long elapsed = millis() - s_startMs;
  int revealed =
      (int)(elapsed / (unsigned long)(speedMsPerChar > 0 ? speedMsPerChar : 1));
  if (revealed > (int)len)
    revealed = (int)len;
  static const char hex[] = "0123456789ABCDEF#X";
  for (size_t i = 0; i < len; i++) {
    if (i < (size_t)revealed)
      scrambleBuf_[i] = finalStr[i];
    else
      scrambleBuf_[i] = hex[random(0, (int)(sizeof(hex) - 1))];
  }
  scrambleBuf_[len] = '\0';
  u8g2_.setFont(LABEL_FONT);
  u8g2_.drawUTF8(x, y, scrambleBuf_);
}

// ---------------------------------------------------------------------------
// drawCircuitLine: line + solid circle at start (edge), hollow diamond at end.
// ---------------------------------------------------------------------------
void DisplayEngine::drawCircuitLine(int x_start, int y_start, int x_end,
                                    int y_end) {
  u8g2_.drawLine(x_start, y_start, x_end, y_end);
  const int r = 2;
  u8g2_.drawDisc(x_start, y_start, r);
  const int d = 2;
  u8g2_.drawLine(x_end, y_end - d, x_end + d, y_end);
  u8g2_.drawLine(x_end + d, y_end, x_end, y_end + d);
  u8g2_.drawLine(x_end, y_end + d, x_end - d, y_end);
  u8g2_.drawLine(x_end - d, y_end, x_end, y_end - d);
}

// ---------------------------------------------------------------------------
// drawHexDecoration: 2 tiny rows of hex at bottom corner. corner 0=BL, 1=BR.
// Values change every ~100ms (millis()/100).
// ---------------------------------------------------------------------------
void DisplayEngine::drawHexDecoration(int corner) {
  u8g2_.setFont(HEXDECOR_FONT);
  unsigned long seed = millis() / 100;
  const char hex[] = "0123456789ABCDEF";
  char line1[12], line2[12];
  for (int i = 0; i < 4; i++) {
    line1[i * 2] = '0';
    line1[i * 2 + 1] = hex[(seed + i * 7) % 16];
  }
  line1[8] = '\0';
  for (int i = 0; i < 4; i++) {
    line2[i * 2] = hex[(seed + 11 + i) % 16];
    line2[i * 2 + 1] = hex[(seed + 13 + i * 3) % 16];
  }
  line2[8] = '\0';
  int y1 = NOCT_DISP_H - 14;
  int y2 = NOCT_DISP_H - 6;
  if (corner == 0) {
    u8g2_.drawUTF8(2, y1, line1);
    u8g2_.drawUTF8(2, y2, line2);
  } else {
    int w1 = u8g2_.getUTF8Width(line1);
    int w2 = u8g2_.getUTF8Width(line2);
    u8g2_.drawUTF8(NOCT_DISP_W - 2 - w1, y1, line1);
    u8g2_.drawUTF8(NOCT_DISP_W - 2 - w2, y2, line2);
  }
}

// ---------------------------------------------------------------------------
// drawWeatherPrimitive: geometric fallback (sun/cloud/rain). (x,y)=top-left
// of icon area; center used for primitives.
// ---------------------------------------------------------------------------
void DisplayEngine::drawWeatherPrimitive(int x, int y, int wmoCode) {
  const int cx = x + 12;
  const int cy = y + 12;
  if (wmoCode >= 0 && wmoCode <= 3) {
    // Sun: disc radius 6
    u8g2_.drawDisc(cx, cy, 6);
  } else if (wmoCode >= 45 && wmoCode <= 48) {
    // Cloud: box + overlapping disc
    u8g2_.drawBox(cx - 6, cy - 2, 12, 6);
    u8g2_.drawDisc(cx - 2, cy - 4, 3);
  } else if (wmoCode >= 50) {
    // Rain: cloud + 3 dotted lines below
    u8g2_.drawBox(cx - 6, cy - 2, 12, 6);
    u8g2_.drawDisc(cx - 2, cy - 4, 3);
    for (int i = -1; i <= 1; i++)
      for (int dy = 0; dy < 6; dy += 2)
        u8g2_.drawPixel(cx + i * 4, cy + 4 + dy);
  } else {
    u8g2_.drawBox(cx - 6, cy - 2, 12, 6);
    u8g2_.drawDisc(cx - 2, cy - 4, 3);
  }
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
// drawClawMark: 3 diagonal parallel scratches (wolf identity)
// ---------------------------------------------------------------------------
void DisplayEngine::drawClawMark(int x, int y) {
  for (int i = 0; i < 3; i++)
    u8g2_.drawLine(x + i * 2, y, x + 6 + i * 2, y + 6);
}

// ---------------------------------------------------------------------------
// drawPawIcon: 5x5 pixel paw (wolf vibe next to WiFi)
// ---------------------------------------------------------------------------
void DisplayEngine::drawPawIcon(int x, int y) {
  u8g2_.drawPixel(x + 2, y);
  u8g2_.drawPixel(x, y + 2);
  u8g2_.drawPixel(x + 4, y + 2);
  u8g2_.drawPixel(x + 1, y + 3);
  u8g2_.drawPixel(x + 3, y + 3);
  u8g2_.drawPixel(x + 2, y + 4);
}

// ---------------------------------------------------------------------------
// drawHexStream: 2–3 columns of random small hex (A4, 0F, 9C). Update every
// ~100ms via millis()/100 seed.
// ---------------------------------------------------------------------------
void DisplayEngine::drawHexStream(int x, int y, int rows) {
  u8g2_.setFont(HEXSTREAM_FONT);
  const char hex[] = "0123456789ABCDEF";
  unsigned long seed = millis() / 100;
  const int colW = 18;
  const int rowH = 8;
  for (int row = 0; row < rows && (y + row * rowH) < NOCT_DISP_H; row++) {
    for (int col = 0; col < 3; col++) {
      int cx = x + col * colW;
      if (cx + 14 > NOCT_DISP_W)
        break;
      char pair[4];
      pair[0] = hex[(seed + row * 7 + col * 11) % 16];
      pair[1] = hex[(seed + row * 13 + col * 17 + 3) % 16];
      pair[2] = '\0';
      u8g2_.drawUTF8(cx, y + row * rowH + 6, pair);
    }
  }
}

// ---------------------------------------------------------------------------
// drawCyberClaw: three parallel diagonal jagged lines (scratches / watermark).
// ---------------------------------------------------------------------------
void DisplayEngine::drawCyberClaw(int x, int y) {
  const int len = 20;
  const int dx = 4;
  const int dy = 6;
  for (int line = 0; line < 3; line++) {
    int lx = x + line * 3;
    int ly = y + line * 2;
    for (int i = 0; i < len; i += 2) {
      int nx = lx + dx;
      int ny = ly + dy;
      u8g2_.drawLine(lx, ly, nx, ny);
      lx = nx;
      ly = ny;
      if (i + 1 < len) {
        nx = lx + 1;
        ny = ly - 1;
        u8g2_.drawLine(lx, ly, nx, ny);
        lx = nx;
        ly = ny;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// drawActiveIndicator: 3x3 filled box blinks every 1000ms (heartbeat). Place
// in header next to scene name.
// ---------------------------------------------------------------------------
void DisplayEngine::drawActiveIndicator(int x, int y) {
  bool on = ((millis() / 1000) & 1) == 0;
  if (on)
    u8g2_.drawBox(x, y, 3, 3);
  else
    u8g2_.drawFrame(x, y, 3, 3);
}

// ---------------------------------------------------------------------------
// drawWolfEye: pixel art eye. Open: < o > shape. Closed: - - shape.
// ---------------------------------------------------------------------------
void DisplayEngine::drawWolfEye(int x, int y, bool open) {
  if (open) {
    // < (left angle)
    u8g2_.drawPixel(x, y);
    u8g2_.drawPixel(x + 1, y - 1);
    u8g2_.drawPixel(x + 1, y + 1);
    // o (center dot)
    u8g2_.drawPixel(x + 3, y);
    // > (right angle)
    u8g2_.drawPixel(x + 5, y);
    u8g2_.drawPixel(x + 4, y - 1);
    u8g2_.drawPixel(x + 4, y + 1);
  } else {
    // - - (two horizontal dashes)
    u8g2_.drawPixel(x, y);
    u8g2_.drawPixel(x + 1, y);
    u8g2_.drawPixel(x + 3, y);
    u8g2_.drawPixel(x + 4, y);
  }
}

// ---------------------------------------------------------------------------
// Global header (Tech-Wolf): Y=0..14, baseline Y=11. Scene name (X=4) + 3x3
// blink | NET:OK (X=124 right). Separator at Y=13 (physical bezel fix).
// ---------------------------------------------------------------------------
void DisplayEngine::drawGlobalHeader(const char *sceneTitle,
                                     const char *timeStr, int rssi,
                                     bool wifiConnected) {
  (void)timeStr;
  (void)rssi;
  const int barH = NOCT_HEADER_H;
  const int baselineY = 11;
  const int nameX = 4;
  /* Leave space for battery HUD (icon + %) to the right of WOOF! */
  const int rightAnchor = 76;

  u8g2_.setDrawColor(1);
  u8g2_.drawBox(0, 0, NOCT_DISP_W, barH);
  u8g2_.setDrawColor(0);

  u8g2_.setFont(HEADER_FONT);
  const char *raw = sceneTitle && sceneTitle[0] ? sceneTitle : "HUB";
  char titleBuf[24];
  snprintf(titleBuf, sizeof(titleBuf), "%s", raw);
  u8g2_.drawUTF8(nameX, baselineY, titleBuf);

  /* Blinking 3x3 pixel square next to scene name (heartbeat) */
  int titleW = u8g2_.getUTF8Width(titleBuf);
  drawActiveIndicator(nameX + titleW + 2, baselineY - 2);

  /* Right side: WOOF! (connected) or NET:-- (offline), anchored at X=124 */
  const char *statusStr = wifiConnected ? "WOOF!" : "NET:--";
  u8g2_.setFont(LABEL_FONT);
  int statusW = u8g2_.getUTF8Width(statusStr);
  u8g2_.drawUTF8(rightAnchor - statusW, baselineY, statusStr);

  /* Separator at Y=13 */
  u8g2_.setDrawColor(1);
  const int sepY = 13;
  u8g2_.drawHLine(0, sepY, NOCT_DISP_W / 2);
  u8g2_.drawHLine(NOCT_DISP_W / 2 + 1, sepY, NOCT_DISP_W / 2 - 1);
  u8g2_.setDrawColor(1);
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

// ===========================================================================
// BLADE RUNNER VFX: Analog noise (pixel dust), V-Sync loss (screen roll),
// Chromatic shift (text tearing). No grid/scanlines.
// ===========================================================================
void DisplayEngine::applyGlitch() {
  unsigned long now = millis();

  // 1. ANALOG NOISE (Pixel Dust) — "Film Grain"
  if (random(100) > 60) {
    u8g2_.setDrawColor(2); // XOR
    for (int i = 0; i < 10; i++) {
      u8g2_.drawPixel(random(128), random(64));
    }
    u8g2_.setDrawColor(1);
  }

  // 2. V-SYNC FAILURE (Screen Roll) — "Cyberpunk" look
  static int vShift = 0;
  static unsigned long lastGlitch = 0;
  if (random(1000) > 990 && vShift == 0) {
    vShift = random(-5, 5);
    lastGlitch = now;
  }

  if (vShift != 0) {
    u8g2_.setDrawColor(0);
    int y = (millis() / 5) % 70;
    u8g2_.drawBox(0, y, 128, 2);
    u8g2_.setDrawColor(1);
    if (now - lastGlitch > 200)
      vShift = 0;
  }

  // 3. CHROMATIC SHIFT (Text Tearing)
  if (random(100) > 95) {
    u8g2_.setDrawColor(2); // XOR
    int y = random(64);
    u8g2_.drawBox(random(10), y, 100, 1);
    u8g2_.setDrawColor(1);
  }
}
