/*
 * NOCTURNE_OS — DisplayEngine: Grid Law. Alignment, glitch, XBM art.
 */
#include "DisplayEngine.h"
#include "../../include/nocturne/config.h"
#include <Arduino.h>
#include <Wire.h>
#include <mbedtls/base64.h>

// --- XBM bitmaps (1 = pixel on) ---
const uint8_t icon_wifi_bits[] = {0x00, 0x00, 0x00, 0x00, 0x24, 0x00,
                                  0x24, 0x00, 0x24, 0x00, 0x6C, 0x00,
                                  0x6C, 0x00, 0xEE, 0x0E};

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

const uint8_t icon_lock_bits[] = {0x3C, 0x66, 0x42, 0x42,
                                  0xFF, 0x42, 0x42, 0x42};

const uint8_t icon_warning_bits[] = {
    0x00, 0x80, 0x01, 0xC0, 0x01, 0xC0, 0x03, 0xE0, 0x03, 0xE0, 0x07, 0xF0,
    0x07, 0xF0, 0x0F, 0xF8, 0x0F, 0xF8, 0x1F, 0xFC, 0x1F, 0xFC, 0x3F, 0xF0};

const uint8_t icon_weather_sun_bits[] = {
    0x01, 0x80, 0x01, 0x80, 0x11, 0x88, 0x01, 0x80, 0x49, 0x92, 0x01,
    0x80, 0x01, 0x80, 0x7D, 0xBE, 0x7D, 0xBE, 0x01, 0x80, 0x01, 0x80,
    0x49, 0x92, 0x01, 0x80, 0x11, 0x88, 0x01, 0x80, 0x01, 0x80};

const uint8_t icon_weather_cloud_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x03, 0xC0, 0x0F, 0xF0, 0x1C, 0x38, 0x18, 0x18,
    0x38, 0x1C, 0x3F, 0xFC, 0x1F, 0xF8, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t icon_weather_rain_bits[] = {
    0x00, 0x00, 0x03, 0xC0, 0x0F, 0xF0, 0x1C, 0x38, 0x18, 0x18, 0x3F, 0xFC,
    0x1F, 0xF8, 0x0F, 0xF0, 0x00, 0x00, 0x12, 0x00, 0x12, 0x00, 0x48, 0x00,
    0x48, 0x00, 0x12, 0x00, 0x12, 0x00, 0x48, 0x00, 0x00, 0x00};

const uint8_t icon_weather_snow_bits[] = {
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x0D, 0xB0, 0x1D, 0xB8, 0x39,
    0x9C, 0x01, 0x80, 0x7F, 0xFE, 0x01, 0x80, 0x39, 0x9C, 0x1D, 0xB8,
    0x0D, 0xB0, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00};

// 16x16 disconnect / offline (X mark)
const uint8_t icon_disconnect_bits[] = {
    0x80, 0x01, 0x40, 0x02, 0x20, 0x04, 0x10, 0x08, 0x08, 0x10, 0x04,
    0x20, 0x02, 0x40, 0x01, 0x80, 0x01, 0x80, 0x02, 0x40, 0x04, 0x20,
    0x08, 0x10, 0x10, 0x08, 0x20, 0x04, 0x40, 0x02, 0x80, 0x01};

// ---------------------------------------------------------------------------
DisplayEngine::DisplayEngine(int rstPin, int sdaPin, int sclPin)
    : sdaPin_(sdaPin), sclPin_(sclPin), u8g2_(U8G2_R0, rstPin),
      dataSpike_(false), lastGlitchTrigger_(0), glitchUntil_(0) {}

void DisplayEngine::begin() {
  Wire.begin(sdaPin_, sclPin_);
  u8g2_.begin();
  u8g2_.enableUTF8Print();
}

void DisplayEngine::clearBuffer() { u8g2_.clearBuffer(); }
void DisplayEngine::sendBuffer() { u8g2_.sendBuffer(); }
void DisplayEngine::setDataSpike(bool spike) { dataSpike_ = spike; }

// ---------------------------------------------------------------------------
// Layout Engine: drawBlock (bounding box), drawProgressBar (segments + 1px gap)
// ---------------------------------------------------------------------------
void DisplayEngine::drawBlock(int x, int y, int w, int h, const String &label,
                              const String &value, bool warning) {
  const int PAD = 2;
  const int LABEL_Y_OFFSET = 8;

  u8g2_.setDrawColor(0);
  u8g2_.drawBox(x, y, w, h);
  u8g2_.setDrawColor(1);

  if (warning) {
    u8g2_.drawBox(x, y, w, h);
    u8g2_.setDrawColor(0);
  }

  u8g2_.setFont(FONT_TINY);
  if (label.length() > 0)
    u8g2_.drawUTF8(x + PAD, y + LABEL_Y_OFFSET, label.c_str());

  int valueRight = x + w - PAD;
  int maxValueW = w - (PAD * 2);
  if (value.length() > 0) {
    u8g2_.setFont(FONT_VAL);
    int needW = u8g2_.getUTF8Width(value.c_str());
    if (needW > maxValueW) {
      u8g2_.setFont(FONT_VAL_NARROW);
      needW = u8g2_.getUTF8Width(value.c_str());
    }
    u8g2_.drawUTF8(valueRight - needW, y + LABEL_Y_OFFSET, value.c_str());
  }

  if (warning)
    u8g2_.setDrawColor(1);

  drawDottedHLine(x, x + w - 1, y);
  drawDottedHLine(x, x + w - 1, y + h - 1);
  drawDottedVLine(x, y, y + h - 1);
  drawDottedVLine(x + w - 1, y, y + h - 1);
}

void DisplayEngine::drawProgressBar(int x, int y, int w, int h, float percent) {
  if (percent > 100.0f)
    percent = 100.0f;
  if (percent < 0.0f)
    percent = 0.0f;
  const int SEG_GAP = 1;
  const int INNER_PAD = 1;
  int iw = w - 2 * INNER_PAD;
  int ih = h - 2 * INNER_PAD;
  if (iw < 2 || ih < 1)
    return;
  u8g2_.drawFrame(x, y, w, h);
  int segW = 3;
  int numSegs = (iw + SEG_GAP) / (segW + SEG_GAP);
  if (numSegs < 1)
    numSegs = 1;
  int filled = (int)((percent / 100.0f) * numSegs + 0.5f);
  if (filled > numSegs)
    filled = numSegs;
  for (int i = 0; i < filled; i++) {
    int sx = x + INNER_PAD + i * (segW + SEG_GAP);
    u8g2_.drawBox(sx, y + INNER_PAD, segW, ih);
  }
}

// ---------------------------------------------------------------------------
// Legacy alignment helpers
// ---------------------------------------------------------------------------
void DisplayEngine::drawRightAligned(int x, int y, const String &text) {
  if (text.length() == 0)
    return;
  u8g2_.setFont(FONT_VAL);
  int tw = u8g2_.getUTF8Width(text.c_str());
  u8g2_.drawUTF8(x - tw, y, text.c_str());
}

void DisplayEngine::drawCentered(int x_center, int y, const String &text) {
  if (text.length() == 0)
    return;
  u8g2_.setFont(FONT_VAL);
  int tw = u8g2_.getUTF8Width(text.c_str());
  u8g2_.drawUTF8(x_center - tw / 2, y, text.c_str());
}

void DisplayEngine::drawCenteredInRect(int x, int y, int w, int h,
                                       const String &text) {
  if (text.length() == 0)
    return;
  u8g2_.setFont(FONT_VAL);
  int tw = u8g2_.getUTF8Width(text.c_str());
  if (tw > w)
    tw = w;
  int sx = x + (w - tw) / 2;
  int sy = y + (h - 10) / 2 + 8;
  u8g2_.drawUTF8(sx, sy, text.c_str());
}

void DisplayEngine::drawSafeStr(int x, int y, const String &text) {
  u8g2_.setFont(FONT_VAL);
  if (text.length() == 0) {
    u8g2_.drawUTF8(x, y, "---");
    return;
  }
  u8g2_.drawUTF8(x, y, text.c_str());
}

void DisplayEngine::drawValueOrNoise(int x_end, int y, const String &value) {
  if (value.length() == 0 || value == "0" || value == "N/A") {
    int nx = x_end - 12;
    if (nx < 0)
      nx = 0;
    for (int i = 0; i < 24; i++)
      u8g2_.drawPixel(nx + (random(12)), y - 2 + (random(6)));
    return;
  }
  drawRightAligned(x_end, y, value);
}

// ---------------------------------------------------------------------------
// HUD: Header 0–10px (strict grid). Title centered. Dotted line at Y=10.
// ---------------------------------------------------------------------------
void DisplayEngine::drawOverlay(const char *sceneName, int rssi, bool linkOk,
                                const char *timeStr) {
  (void)linkOk;
  const int BAR_Y0 = 0;
  const int BAR_H = NOCT_HEADER_H;
  const int DOTTED_Y = NOCT_HEADER_H;

  u8g2_.setDrawColor(0);
  u8g2_.drawBox(0, BAR_Y0, NOCT_DISP_W, BAR_H);
  u8g2_.setDrawColor(1);

  u8g2_.setFont(FONT_LABEL);
  char buf[20];
  snprintf(buf, sizeof(buf), "%s", sceneName ? sceneName : "");
  int tw = u8g2_.getUTF8Width(buf);
  u8g2_.drawUTF8(NOCT_DISP_W / 2 - tw / 2, BAR_Y0 + BAR_H - 2, buf);

  u8g2_.setFont(FONT_TINY);
  const char *tstr = (timeStr && timeStr[0]) ? timeStr : "---";
  int tw = u8g2_.getUTF8Width(tstr);
  u8g2_.drawUTF8(90 - tw, BAR_Y0 + BAR_H - 2, tstr);

  drawWiFiIconXbm(115, BAR_Y0 + 1, rssi, (rssi < -70));

  drawDottedHLine(0, NOCT_DISP_W - 1, DOTTED_Y);
}

// ---------------------------------------------------------------------------
// Chamfered box: drawFrame then cut corners with drawPixel(0)
// ---------------------------------------------------------------------------
void DisplayEngine::drawChamferBox(int x, int y, int w, int h, int chamfer) {
  u8g2_.drawFrame(x, y, w, h);
  if (chamfer <= 0 || chamfer * 2 >= w || chamfer * 2 >= h)
    return;
  u8g2_.setDrawColor(0);
  for (int c = 0; c < chamfer; c++) {
    u8g2_.drawPixel(x + c, y);
    u8g2_.drawPixel(x, y + c);
    u8g2_.drawPixel(x + w - 1 - c, y);
    u8g2_.drawPixel(x + w - 1, y + c);
    u8g2_.drawPixel(x + c, y + h - 1);
    u8g2_.drawPixel(x, y + h - 1 - c);
    u8g2_.drawPixel(x + w - 1 - c, y + h - 1);
    u8g2_.drawPixel(x + w - 1, y + h - 1 - c);
  }
  u8g2_.setDrawColor(1);
}

// ---------------------------------------------------------------------------
void DisplayEngine::drawSegmentedBar(int x, int y, int w, int h, float pct,
                                     int segments) {
  if (segments < 1)
    segments = 1;
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  int gap = 1;
  int totalGaps = (segments - 1) * gap;
  int blockW = (w - totalGaps) / segments;
  if (blockW < 1)
    blockW = 1;
  int filled = (int)((pct / 100.0f) * segments + 0.5f);
  if (filled > segments)
    filled = segments;
  for (int i = 0; i < segments; i++) {
    int bx = x + i * (blockW + gap);
    if (i < filled)
      u8g2_.drawBox(bx, y, blockW, h);
    else
      u8g2_.drawFrame(bx, y, blockW, h);
  }
}

void DisplayEngine::drawDottedHLine(int x0, int x1, int y) {
  if (x0 > x1) {
    int t = x0;
    x0 = x1;
    x1 = t;
  }
  for (int x = x0; x <= x1; x += 2)
    u8g2_.drawPixel(x, y);
}

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
void DisplayEngine::drawBiosPost(unsigned long now, unsigned long bootTime,
                                 bool wifiOk, int rssi) {
  u8g2_.setFont(FONT_TINY);
  int lineH = 7;
  unsigned long elapsed = now - bootTime;
  int scroll = (int)(elapsed / 80) % (6 * lineH);
  int y0 = NOCT_DISP_H + 4 - (scroll % (6 * lineH));
  const char *lines[] = {"MEM_CHECK......... OK",
                         "LINK.............. OK",
                         "WOLF_PROTOCOL..... OK",
                         "CORTEX........... OK",
                         wifiOk ? "WIFI.............. OK"
                                : "WIFI.............. ...",
                         ""};
  for (int i = 0; i < 5; i++) {
    int y = y0 + i * lineH;
    if (y >= -lineH && y < NOCT_DISP_H - ICON_WOLF_H - 4)
      u8g2_.drawStr(NOCT_MARGIN, y + lineH - 1, lines[i]);
  }

  int wolfX = NOCT_DISP_W - ICON_WOLF_W - NOCT_MARGIN;
  int wolfY = (NOCT_DISP_H - ICON_WOLF_H) / 2;
  u8g2_.drawXBM(wolfX, wolfY, ICON_WOLF_W, ICON_WOLF_H, icon_wolf_bits);

  if (wifiOk)
    drawWiFiIconXbm(NOCT_DISP_W - ICON_WIFI_W - NOCT_MARGIN, NOCT_MARGIN, rssi,
                    false);

  int barW = (int)((float)elapsed * (NOCT_DISP_W - 2 * NOCT_MARGIN) /
                   (float)NOCT_SPLASH_MS);
  if (barW > NOCT_DISP_W - 2 * NOCT_MARGIN)
    barW = NOCT_DISP_W - 2 * NOCT_MARGIN;
  if (barW > 0)
    u8g2_.drawBox(NOCT_MARGIN, NOCT_DISP_H - 4 - NOCT_MARGIN, barW, 3);
}

bool DisplayEngine::biosPostDone(unsigned long now, unsigned long bootTime) {
  return (now - bootTime) >= (unsigned long)NOCT_SPLASH_MS;
}

// ---------------------------------------------------------------------------
void DisplayEngine::drawMetric(int x, int y, const char *label,
                               const char *value) {
  u8g2_.setFont(FONT_LABEL);
  if (label && strlen(label) > 0)
    u8g2_.drawStr(x, y - 2, label);
  u8g2_.setFont(FONT_VAL);
  if (value)
    u8g2_.drawUTF8(x, y + 10, value);
}

void DisplayEngine::drawMetricStr(int x, int y, const char *label,
                                  const String &value) {
  drawMetric(x, y, label, value.c_str());
}

void DisplayEngine::drawWiFiIcon(int x, int y, int rssi) {
  drawWiFiIconXbm(x, y, rssi, false);
}

void DisplayEngine::drawWiFiIconXbm(int x, int y, int rssi, bool blink) {
  if (blink && (millis() / 200) % 2 == 0)
    return;
  u8g2_.drawXBM(x, y, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);
}

void DisplayEngine::drawDisconnectIcon(int x, int y) {
  u8g2_.drawXBM(x, y, ICON_DISCONNECT_W, ICON_DISCONNECT_H,
                icon_disconnect_bits);
}

void DisplayEngine::drawCornerCrosshairs() {
  int len = 4;
  u8g2_.setDrawColor(1);
  u8g2_.drawLine(0, 0, len, 0);
  u8g2_.drawLine(0, 0, 0, len);
  u8g2_.drawLine(NOCT_DISP_W - len - 1, 0, NOCT_DISP_W - 1, 0);
  u8g2_.drawLine(NOCT_DISP_W - 1, 0, NOCT_DISP_W - 1, len);
  u8g2_.drawLine(0, NOCT_DISP_H - 1, len, NOCT_DISP_H - 1);
  u8g2_.drawLine(0, NOCT_DISP_H - len - 1, 0, NOCT_DISP_H - 1);
  u8g2_.drawLine(NOCT_DISP_W - len - 1, NOCT_DISP_H - 1, NOCT_DISP_W - 1,
                 NOCT_DISP_H - 1);
  u8g2_.drawLine(NOCT_DISP_W - 1, NOCT_DISP_H - len - 1, NOCT_DISP_W - 1,
                 NOCT_DISP_H - 1);
}

void DisplayEngine::drawFanIcon(int x, int y, int frame) {
  int r = 6;
  u8g2_.drawCircle(x, y, r);
  u8g2_.drawDisc(x, y, 2);
  for (int i = 0; i < 3; i++) {
    float angle = (frame * 30 + i * 120) * 3.14159f / 180.0f;
    int x1 = x + (int)(r * 0.8f * cos(angle));
    int y1 = y + (int)(r * 0.8f * sin(angle));
    u8g2_.drawLine(x, y, x1, y1);
  }
}

void DisplayEngine::drawLinkStatus(int x, int y, bool linked) {
  u8g2_.setFont(FONT_TINY);
  if (linked)
    u8g2_.drawStr(x, y, "LINK");
  else
    u8g2_.drawStr(x, y, "----");
}

// ---------------------------------------------------------------------------
// Glitch: offset a 10px-tall horizontal slice by 3 pixels (CRT-style)
// ---------------------------------------------------------------------------
void DisplayEngine::drawGlitchEffect() {
  unsigned long now = millis();
  if (glitchUntil_ > 0 && now >= glitchUntil_)
    glitchUntil_ = 0;
  if (glitchUntil_ == 0 &&
      now - lastGlitchTrigger_ < (unsigned long)NOCT_GLITCH_INTERVAL_MS)
    return;
  if (glitchUntil_ == 0) {
    lastGlitchTrigger_ = now;
    glitchUntil_ = now + NOCT_GLITCH_DURATION_MS;
  }
  uint8_t *buf = u8g2_.getBufferPtr();
  if (!buf)
    return;
  const int sliceH = 10;
  const int shift = 3;
  int y0 = 2 + (random(NOCT_DISP_H - sliceH - 4));
  if (y0 < 2)
    y0 = 2;
  if (y0 + sliceH >= NOCT_DISP_H)
    y0 = NOCT_DISP_H - sliceH - 1;
  int p0 = y0 / 8;
  int p1 = (y0 + sliceH - 1) / 8;
  if (p1 >= 8)
    p1 = 7;
  uint8_t tmp[2 * 128];
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
// Decode Base64 to XBM buffer and draw. 64x64 = 512 bytes.
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
void DisplayEngine::drawRollingGraph(int x, int y, int w, int h,
                                     RollingGraph &g, int maxVal) {
  if (g.count < 2 || maxVal <= 0)
    return;
  for (int ly = y; ly < y + h; ly += 2)
    drawDottedHLine(x, x + w - 1, ly);
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
