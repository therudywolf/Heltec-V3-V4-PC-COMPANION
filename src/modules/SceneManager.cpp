/*
 * NOCTURNE_OS — SceneManager: Nocturne Style. 2x2 grid HUB, chamfered boxes,
 * segmented bars, dotted grid, XBM weather icons, sonar NET, cassette MEDIA.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <mbedtls/base64.h>

#define LINE_H_DATA NOCT_LINE_H_DATA
#define LINE_H_LABEL NOCT_LINE_H_LABEL
#define LINE_H_HEAD NOCT_LINE_H_HEAD
#define LINE_H_BIG NOCT_LINE_H_BIG

// Content area below overlay header
#define CONTENT_Y0 (NOCT_HEADER_H + NOCT_MARGIN)
#define CONTENT_H (NOCT_DISP_H - NOCT_HEADER_H - NOCT_MARGIN - 2)

const char *SceneManager::sceneNames_[] = {"HUB", "CPU",   "GPU",
                                           "NET", "ATMOS", "MEDIA"};

SceneManager::SceneManager(DisplayEngine &disp, AppState &state)
    : disp_(disp), state_(state) {}

const char *SceneManager::getSceneName(int sceneIndex) const {
  if (sceneIndex < 0 || sceneIndex >= NOCT_TOTAL_SCENES)
    return "HUB";
  return sceneNames_[sceneIndex];
}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame) {
  switch (sceneIndex) {
  case NOCT_SCENE_HUB:
    drawHub(bootTime);
    break;
  case NOCT_SCENE_CPU:
    drawCpu(bootTime, fanFrame);
    break;
  case NOCT_SCENE_GPU:
    drawGpu(fanFrame);
    break;
  case NOCT_SCENE_NET:
    drawNet();
    break;
  case NOCT_SCENE_ATMOS:
    drawAtmos();
    break;
  case NOCT_SCENE_MEDIA:
    drawMedia(blinkState);
    break;
  default:
    drawHub(bootTime);
    break;
  }
}

// ---------------------------------------------------------------------------
// HUB: 2x2 grid (dotted), CPU temp+load, GPU temp+load, RAM segmented bar +
// VRAM
// ---------------------------------------------------------------------------
void SceneManager::drawHub(unsigned long bootTime) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  int mx = NOCT_DISP_W / 2;
  int my = CONTENT_Y0 + CONTENT_H / 2;

  disp_.drawDottedVLine(mx, CONTENT_Y0, CONTENT_Y0 + CONTENT_H - 1);
  disp_.drawDottedHLine(NOCT_MARGIN, NOCT_DISP_W - NOCT_MARGIN, my);

  char buf[24];
  int cellW = mx - NOCT_MARGIN - 2;
  int cellH = my - CONTENT_Y0 - 2;

  // Top-left: CPU temp (big) + load (tiny segmented bar)
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, CONTENT_Y0 + LINE_H_LABEL, "CPU");
  snprintf(buf, sizeof(buf), "%d", hw.ct);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(NOCT_MARGIN + 2, CONTENT_Y0 + LINE_H_LABEL + LINE_H_BIG, buf);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + 2, CONTENT_Y0 + LINE_H_LABEL + LINE_H_BIG + 4,
               "C");
  float cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? (float)hw.cl : 0.0f;
  disp_.drawSegmentedBar(NOCT_MARGIN + 2, CONTENT_Y0 + cellH - 6, cellW - 4, 4,
                         cpuLoad, 10);

  // Top-right: GPU temp (big) + load (tiny segmented bar)
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(mx + 2, CONTENT_Y0 + LINE_H_LABEL, "GPU");
  snprintf(buf, sizeof(buf), "%d", hw.gt);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(mx + 2, CONTENT_Y0 + LINE_H_LABEL + LINE_H_BIG, buf);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(mx + 2, CONTENT_Y0 + LINE_H_LABEL + LINE_H_BIG + 4, "C");
  float gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? (float)hw.gl : 0.0f;
  disp_.drawSegmentedBar(mx + 2, CONTENT_Y0 + cellH - 6, cellW - 4, 4, gpuLoad,
                         10);

  // Bottom: RAM segmented bar + VRAM label
  int by = my + NOCT_MARGIN;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, by + LINE_H_LABEL, "RAM");
  float ramPct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  int barW = NOCT_DISP_W - 2 * NOCT_MARGIN - 4;
  disp_.drawSegmentedBar(NOCT_MARGIN + 2, by + LINE_H_LABEL + 2, barW, 5,
                         ramPct, 16);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_DISP_W - NOCT_MARGIN - 28, by + LINE_H_LABEL + 2, "VRAM");
  snprintf(buf, sizeof(buf), "%d%%", hw.gv);
  u8g2.drawStr(NOCT_DISP_W - NOCT_MARGIN - 18, by + LINE_H_LABEL + 2, buf);
  (void)bootTime;
}

// ---------------------------------------------------------------------------
// CPU: rolling graph + dotted ref lines at 50% and 100%; stats right-aligned
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(unsigned long bootTime, int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)bootTime;
  char buf[24];

  int gx = NOCT_MARGIN, gy = NOCT_FOOTER_Y, gw = NOCT_DISP_W - 2 * NOCT_MARGIN,
      gh = NOCT_GRAPH_HEIGHT;
  disp_.drawDottedHLine(gx, gx + gw, gy + gh / 2);
  disp_.drawDottedHLine(gx, gx + gw, gy);
  disp_.drawRollingGraph(gx, gy, gw, gh, disp_.cpuGraph, 100);

  disp_.drawFanIcon(NOCT_DISP_W - 14, CONTENT_Y0 + LINE_H_HEAD - 2, fanFrame);

  int y = CONTENT_Y0 + LINE_H_HEAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + LINE_H_LABEL, "GHz");
  snprintf(buf, sizeof(buf), "%.1f", hw.cc / 1000.0f);
  u8g2.setFont(FONT_VAL);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - tw, y + LINE_H_LABEL, buf);
  y += LINE_H_DATA + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + LINE_H_LABEL, "PWR");
  snprintf(buf, sizeof(buf), "%dW", hw.pw);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - tw, y + LINE_H_LABEL, buf);
  y += LINE_H_DATA + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + LINE_H_LABEL, "FAN");
  snprintf(buf, sizeof(buf), "%d", hw.cf);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - tw, y + LINE_H_LABEL, buf);
}

// ---------------------------------------------------------------------------
// GPU: same as CPU, dotted ref lines, stats right-aligned
// ---------------------------------------------------------------------------
void SceneManager::drawGpu(int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  char buf[24];

  int gx = NOCT_MARGIN, gy = NOCT_FOOTER_Y, gw = NOCT_DISP_W - 2 * NOCT_MARGIN,
      gh = NOCT_GRAPH_HEIGHT;
  disp_.drawDottedHLine(gx, gx + gw, gy + gh / 2);
  disp_.drawDottedHLine(gx, gx + gw, gy);
  disp_.drawRollingGraph(gx, gy, gw, gh, disp_.gpuGraph, 100);

  disp_.drawFanIcon(NOCT_DISP_W - 14, CONTENT_Y0 + LINE_H_HEAD - 2, fanFrame);

  int y = CONTENT_Y0 + LINE_H_HEAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + LINE_H_LABEL, "HOT");
  snprintf(buf, sizeof(buf), "%dC", hw.gh);
  u8g2.setFont(FONT_VAL);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - tw, y + LINE_H_LABEL, buf);
  y += LINE_H_DATA + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + LINE_H_LABEL, "VRAM");
  snprintf(buf, sizeof(buf), "%d%%", hw.gv);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - tw, y + LINE_H_LABEL, buf);
  y += LINE_H_DATA + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + LINE_H_LABEL, "FAN");
  snprintf(buf, sizeof(buf), "%d", hw.gf);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - tw, y + LINE_H_LABEL, buf);
}

// ---------------------------------------------------------------------------
// NET: graph + scanning radar line (vertical, moving L-to-R); DL in inverted
// box
// ---------------------------------------------------------------------------
void SceneManager::drawNet() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  char buf[24];

  int gx = NOCT_MARGIN, gy = NOCT_FOOTER_Y, gw = NOCT_DISP_W - 2 * NOCT_MARGIN,
      gh = NOCT_GRAPH_HEIGHT;
  disp_.netDownGraph.setMax(2048);
  disp_.drawRollingGraph(gx, gy, gw, gh, disp_.netDownGraph, 2048);

  int scanX = gx + (millis() / 50) % (gw + 1);
  disp_.drawDottedVLine(scanX, gy, gy + gh - 1);

  float dlMb = hw.nd >= 1024 ? (hw.nd / 1024.0f) : 0.0f;
  if (hw.nd >= 1024)
    snprintf(buf, sizeof(buf), "DL: %.1f", dlMb);
  else
    snprintf(buf, sizeof(buf), "DL: %d", hw.nd);
  u8g2.setFont(FONT_VAL);
  int tw = u8g2.getUTF8Width(buf);
  int bx = NOCT_DISP_W - NOCT_MARGIN - tw - 4;
  int by = CONTENT_Y0 + 2;
  u8g2.setDrawColor(1);
  u8g2.drawBox(bx, by, tw + 4, LINE_H_DATA + 2);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(bx + 2, by + LINE_H_DATA - 1, buf);
  u8g2.setDrawColor(1);

  int y = CONTENT_Y0 + LINE_H_HEAD + 4;
  if (hw.nu >= 1024)
    snprintf(buf, sizeof(buf), "UP %.1f MB/s", hw.nu / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "UP %d KB/s", hw.nu);
  disp_.drawMetricStr(NOCT_MARGIN, y + LINE_H_DATA, "", String(buf));
  y += LINE_H_DATA + 2;
  snprintf(buf, sizeof(buf), "PING %d ms", hw.pg);
  disp_.drawMetricStr(NOCT_MARGIN, y + LINE_H_DATA, "", String(buf));
}

// ---------------------------------------------------------------------------
// ATMOS: XBM weather icons (SUN, CLOUD, RAIN, SNOW) from WMO codes
// ---------------------------------------------------------------------------
void SceneManager::drawAtmos() {
  WeatherData &w = state_.weather;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int y = CONTENT_Y0 + 2;
  if (!state_.weatherReceived || (w.temp == 0 && w.desc.length() == 0)) {
    u8g2.setFont(FONT_BIG);
    u8g2.drawStr(NOCT_MARGIN + 2, y + LINE_H_BIG, "NO DATA");
    return;
  }
  drawWmoIconXbm(NOCT_MARGIN, y, w.wmoCode);
  char buf[24];
  snprintf(buf, sizeof(buf), "%dC", w.temp);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(NOCT_MARGIN + ICON_WEATHER_W + 4, y + 12, buf);
  u8g2.setFont(FONT_TINY);
  String desc = w.desc;
  if (desc.length() > 14)
    desc = desc.substring(0, 14);
  u8g2.drawStr(NOCT_MARGIN + ICON_WEATHER_W + 4, y + 24, desc.c_str());
}

// ---------------------------------------------------------------------------
// MEDIA: 64x64 art or noise/cassette; track marquee scroll
// ---------------------------------------------------------------------------
void SceneManager::drawMedia(bool blinkState) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int ART_W = 64, ART_H = 64, RAW_BYTES = (ART_W * ART_H) / 8;

  const int ART_Y = NOCT_HEADER_H;
  const int ART_H_VIS = NOCT_DISP_H - NOCT_HEADER_H;
  if (media.coverB64.length() > 0) {
    unsigned char raw[512];
    size_t olen = 0;
    int ret = mbedtls_base64_decode(
        raw, sizeof(raw), &olen, (const unsigned char *)media.coverB64.c_str(),
        media.coverB64.length());
    if (ret == 0 && olen == (size_t)RAW_BYTES) {
      for (int row = 0; row < ART_H_VIS && row < ART_H; row++) {
        for (int col = 0; col < ART_W; col++) {
          int byteIdx = row * (ART_W / 8) + col / 8;
          int bit = col % 8;
          if (raw[byteIdx] & (1 << (7 - bit)))
            u8g2.drawPixel(col, ART_Y + row);
        }
      }
    } else {
      drawNoisePattern(0, ART_Y, ART_W, ART_H_VIS);
    }
  } else {
    drawCassetteIcon(0, ART_Y);
  }

  int rx = 65, ry = CONTENT_Y0;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(rx, ry + LINE_H_LABEL, "TRACK");
  String trk = media.track.length() > 0 ? media.track : "-";
  u8g2.setFont(FONT_VAL);
  int maxW = NOCT_DISP_W - rx - NOCT_MARGIN;
  int tw = u8g2.getUTF8Width(trk.c_str());
  if (tw > maxW) {
    int scrollLen = tw + 24;
    if (scrollLen < 1)
      scrollLen = 1;
    int scroll = (millis() / 50) % scrollLen;
    int offset = maxW - scroll;
    u8g2.drawUTF8(rx + offset, ry + LINE_H_LABEL + LINE_H_DATA, trk.c_str());
  } else {
    u8g2.drawUTF8(rx, ry + LINE_H_LABEL + LINE_H_DATA, trk.c_str());
  }
  ry += LINE_H_DATA + 8;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(rx, ry + LINE_H_LABEL, "ARTIST");
  String art = media.artist.length() > 0 ? media.artist : "-";
  if (art.length() > 14)
    art = art.substring(0, 14);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(rx, ry + LINE_H_LABEL + LINE_H_DATA, art.c_str());

  if (!media.isPlaying && !media.isIdle) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(rx, NOCT_DISP_H - NOCT_MARGIN, "STANDBY");
  } else if (media.isIdle) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(rx, NOCT_DISP_H - NOCT_MARGIN, blinkState ? "IDLE" : "zzz");
  }
}

// ---------------------------------------------------------------------------
// WMO -> XBM weather icon (SUN, CLOUD, RAIN, SNOW)
// ---------------------------------------------------------------------------
void SceneManager::drawWmoIconXbm(int x, int y, int wmoCode) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const uint8_t *bits = icon_weather_cloud_bits;
  if (wmoCode == 0)
    bits = icon_weather_sun_bits;
  else if (wmoCode >= 1 && wmoCode <= 3)
    bits = icon_weather_sun_bits;
  else if (wmoCode >= 45 && wmoCode <= 48)
    bits = icon_weather_cloud_bits;
  else if (wmoCode >= 51 && wmoCode <= 67)
    bits = icon_weather_rain_bits;
  else if (wmoCode >= 71 && wmoCode <= 77)
    bits = icon_weather_snow_bits;
  else if (wmoCode >= 80 && wmoCode <= 82)
    bits = icon_weather_rain_bits;
  else if (wmoCode >= 85 && wmoCode <= 86)
    bits = icon_weather_snow_bits;
  else if (wmoCode >= 95 && wmoCode <= 99)
    bits = icon_weather_rain_bits;
  u8g2.drawXBM(x, y, ICON_WEATHER_W, ICON_WEATHER_H, bits);
}

void SceneManager::drawCassetteIcon(int x, int y) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  int cx = x + 32, cy = y + 32;
  disp_.drawChamferBox(x + 8, y + 12, 48, 40, 4);
  u8g2.drawDisc(cx - 10, cy - 6, 6);
  u8g2.drawDisc(cx + 10, cy - 6, 6);
  u8g2.drawDisc(cx - 10, cy - 6, 2);
  u8g2.drawDisc(cx + 10, cy - 6, 2);
  u8g2.drawFrame(cx - 6, cy - 2, 12, 8);
}

void SceneManager::drawNoisePattern(int x, int y, int w, int h) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  for (int i = 0; i < 200; i++)
    u8g2.drawPixel(x + random(w), y + random(h));
}

// ---------------------------------------------------------------------------
void SceneManager::drawSearchMode(int scanPhase) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr((NOCT_DISP_W - 90) / 2, NOCT_DISP_H / 2 - 8, "SEARCH_MODE");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 4, "Scanning for host...");
  int cx = NOCT_DISP_W / 2, cy = NOCT_DISP_H - 16, r = 14;
  u8g2.drawCircle(cx, cy, r);
  int angle = (scanPhase * 30) % 360;
  float rad = angle * 3.14159f / 180.0f;
  u8g2.drawLine(cx, cy, cx + (int)(r * cos(rad)), cy + (int)(r * sin(rad)));
}

void SceneManager::drawMenu(int menuItem) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  disp_.drawChamferBox(NOCT_MARGIN + 4, NOCT_MARGIN + 4,
                       NOCT_DISP_W - 2 * NOCT_MARGIN - 8,
                       NOCT_DISP_H - 2 * NOCT_MARGIN - 8, 3);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 8, NOCT_MARGIN + 14, "SETTINGS");
  const char *labels[] = {"Carousel", "WiFi Reset", "Exit"};
  int startY = NOCT_MARGIN + 20;
  for (int i = 0; i < 3; i++) {
    int y = startY + i * 10;
    if (y + 10 > NOCT_DISP_H - NOCT_MARGIN - 4)
      break;
    if (i == menuItem) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(NOCT_MARGIN + 8, y - 6, NOCT_DISP_W - 2 * NOCT_MARGIN - 16,
                   10);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(NOCT_MARGIN + 10, y, labels[i]);
    if (i == menuItem)
      u8g2.setDrawColor(1);
  }
}

void SceneManager::drawNoSignal(bool wifiOk, bool tcpOk, int rssi,
                                bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_BIG);
  u8g2.drawStr((NOCT_DISP_W - 72) / 2, NOCT_DISP_H / 2 - 4, "NO SIGNAL");
  u8g2.setFont(FONT_TINY);
  if (!wifiOk)
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "WiFi: DISCONNECTED");
  else if (!tcpOk)
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "TCP: CONNECTING...");
  else
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "WAITING FOR DATA...");
  drawNoisePattern(0, 0, NOCT_DISP_W, NOCT_DISP_H);
  if (wifiOk)
    disp_.drawWiFiIconXbm(NOCT_DISP_W - ICON_WIFI_W - NOCT_MARGIN, NOCT_MARGIN,
                          rssi, (rssi < -70));
  if (blinkState)
    u8g2.drawBox(NOCT_DISP_W / 2 + 40, NOCT_DISP_H / 2 - 8, 6, 12);
}

void SceneManager::drawConnecting(int rssi, bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)blinkState;
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr((NOCT_DISP_W - 72) / 2, NOCT_DISP_H / 2 - 4, "LINKING...");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, NOCT_DISP_H / 2 + 8, 3, 3);
  disp_.drawWiFiIconXbm(NOCT_DISP_W - ICON_WIFI_W - NOCT_MARGIN, NOCT_MARGIN,
                        rssi, false);
}
