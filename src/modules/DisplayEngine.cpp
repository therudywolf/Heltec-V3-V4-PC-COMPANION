/*
 * NOCTURNE_OS — DisplayEngine: full buffer, glitch, BIOS POST. Fonts:
 * profont10, helvB10.
 */
#include "DisplayEngine.h"
#include <Wire.h>

#define LINE_H_DATA NOCT_LINE_H_DATA
#define LINE_H_LABEL NOCT_LINE_H_LABEL
#define LINE_H_HEAD NOCT_LINE_H_HEAD
#define LINE_H_BIG NOCT_LINE_H_BIG

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

void DisplayEngine::drawBiosPost(unsigned long now, unsigned long bootTime,
                                 bool wifiOk, int rssi) {
  int cx = NOCT_DISP_W / 2, cy = 14;
  u8g2_.drawTriangle(cx - 12, cy - 8, cx - 6, cy - 2, cx - 10, cy);
  u8g2_.drawTriangle(cx + 12, cy - 8, cx + 6, cy - 2, cx + 10, cy);
  u8g2_.drawRFrame(cx - 10, cy - 2, 20, 14, 2);
  u8g2_.drawPixel(cx - 4, cy + 2);
  u8g2_.drawPixel(cx + 4, cy + 2);

  u8g2_.setFont(FONT_TINY);
  int lineH = 7;
  unsigned long elapsed = now - bootTime;
  int scroll = (int)(elapsed / 80) % (6 * lineH);
  int y0 = NOCT_DISP_H + 4 - (scroll % (6 * lineH));
  const char *lines[] = {
      "SYSTEM INIT",   "MEM CHECK......... OK",           "LINK ESTABLISHED..",
      "WOLF_PROTOCOL", wifiOk ? "WIFI: OK" : "WIFI: ...", ""};
  for (int i = 0; i < 5; i++) {
    int y = y0 + i * lineH;
    if (y >= -lineH && y < NOCT_DISP_H)
      u8g2_.drawStr(NOCT_MARGIN, y + lineH - 1, lines[i]);
  }
  if (wifiOk)
    drawWiFiIcon(NOCT_DISP_W - 16, NOCT_DISP_H - 12, rssi);
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

void DisplayEngine::drawMetric(int x, int y, const char *label,
                               const char *value) {
  u8g2_.setFont(FONT_VAL);
  if (label && strlen(label) > 0) {
    u8g2_.setFont(FONT_LABEL);
    u8g2_.drawStr(x, y - LINE_H_DATA - 1, label);
    u8g2_.setFont(FONT_VAL);
  }
  if (value)
    u8g2_.drawUTF8(x, y, value);
}

void DisplayEngine::drawMetricStr(int x, int y, const char *label,
                                  const String &value) {
  drawMetric(x, y, label, value.c_str());
}

void DisplayEngine::drawProgressBar(int x, int y, int w, int h, float pct) {
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  u8g2_.drawRFrame(x, y, w, h, 1);
  int fillW = (int)((w - 2) * (pct / 100.0f));
  if (fillW > 0)
    u8g2_.drawRBox(x + 1, y + 1, fillW, h - 2, 1);
}

void DisplayEngine::drawWiFiIcon(int x, int y, int rssi) {
  int bars = (rssi >= -50)   ? 4
             : (rssi >= -60) ? 3
             : (rssi >= -70) ? 2
             : (rssi >= -80) ? 1
                             : 0;
  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 2;
    if (i < bars)
      u8g2_.drawBox(x + i * 3, y + 8 - h, 2, h);
    else
      u8g2_.drawFrame(x + i * 3, y + 8 - h, 2, h);
  }
}

void DisplayEngine::drawCornerCrosshairs() {
  int len = 4;
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

void DisplayEngine::drawGlitchEffect() {
  unsigned long now = millis();
  if (glitchUntil_ > 0 && now < glitchUntil_) {
    int shift = 2 + (random(3));
    if (random(2) == 0)
      shift = -shift;
    for (int band = 0; band < 4; band++) {
      int y = random(2, NOCT_DISP_H - 4);
      for (int x = 0; x < NOCT_DISP_W; x++) {
        int sx = (x + shift + NOCT_DISP_W) % NOCT_DISP_W;
        if (random(4) < 2)
          u8g2_.drawPixel(sx, y);
      }
    }
    return;
  }
  if (glitchUntil_ > 0 && now >= glitchUntil_)
    glitchUntil_ = 0;
  if (now - lastGlitchTrigger_ >= (unsigned long)NOCT_GLITCH_INTERVAL_MS) {
    lastGlitchTrigger_ = now;
    glitchUntil_ = now + NOCT_GLITCH_DURATION_MS;
  }
}

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
    if (px >= 0)
      u8g2_.drawLine(px, py, gx, gy);
    else
      u8g2_.drawPixel(gx, gy);
    px = gx;
    py = gy;
  }
}
