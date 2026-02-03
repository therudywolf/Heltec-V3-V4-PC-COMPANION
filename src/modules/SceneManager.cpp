/*
 * NOCTURNE_OS — SceneManager: Grid Law. HUB/MEDIA/ATMOS layouts, null handling.
 * Content starts below HUD (Y=12). 2px padding between text and lines.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <mbedtls/base64.h>

// Content area: below dotted line at Y=11
#define CONTENT_Y0 12
#define PAD 2

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
// SCENE_HUB: Split at X=64. Left CPU, Right GPU, Bottom RAM.
// Null: temp 0 or invalid -> "N/A". Empty strings -> "---".
// ---------------------------------------------------------------------------
void SceneManager::drawHub(unsigned long bootTime) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)bootTime;

  const int LEFT_X = 2;
  const int RIGHT_X = 66;
  const int LABEL_Y = 20;
  const int TEMP_Y = 35;
  const int BAR_Y = 45;
  const int BAR_W = 60;
  const int BAR_H = 6;
  const int RAM_LABEL_Y = 58;
  const int RAM_BAR_X0 = 25;
  const int RAM_BAR_X1 = 126;
  const int RAM_BAR_W = RAM_BAR_X1 - RAM_BAR_X0 + 1;
  const int RAM_BAR_H = 4;

  // Left: CPU
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(LEFT_X, LABEL_Y, "CPU");
  String cpuTempStr = (hw.ct <= 0) ? "N/A" : String(hw.ct);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(LEFT_X, TEMP_Y, cpuTempStr.c_str());
  if (hw.ct > 0) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(LEFT_X + 18, TEMP_Y - 1, "C");
  }
  float cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? (float)hw.cl : 0.0f;
  disp_.drawProgressBar(LEFT_X, BAR_Y, BAR_W, BAR_H, cpuLoad);

  // Right: GPU
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(RIGHT_X, LABEL_Y, "GPU");
  String gpuTempStr = (hw.gt <= 0) ? "N/A" : String(hw.gt);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(RIGHT_X, TEMP_Y, gpuTempStr.c_str());
  if (hw.gt > 0) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(RIGHT_X + 18, TEMP_Y - 1, "C");
  }
  float gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? (float)hw.gl : 0.0f;
  disp_.drawProgressBar(RIGHT_X, BAR_Y, BAR_W, BAR_H, gpuLoad);

  // Bottom: RAM segmented bar from X=25 to X=126
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(LEFT_X, RAM_LABEL_Y, "RAM");
  float ramPct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  if (ramPct > 100.0f)
    ramPct = 100.0f;
  if (ramPct < 0.0f)
    ramPct = 0.0f;
  int segments = 16;
  disp_.drawSegmentedBar(RAM_BAR_X0, RAM_LABEL_Y - 3, RAM_BAR_W, RAM_BAR_H,
                         ramPct, segments);
}

// ---------------------------------------------------------------------------
// CPU: rolling graph, stats right-aligned, 2px padding
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(unsigned long bootTime, int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)bootTime;

  int gx = NOCT_MARGIN, gy = NOCT_FOOTER_Y, gw = NOCT_DISP_W - 2 * NOCT_MARGIN,
      gh = NOCT_GRAPH_HEIGHT;
  disp_.drawDottedHLine(gx, gx + gw, gy + gh / 2);
  disp_.drawDottedHLine(gx, gx + gw, gy);
  disp_.drawRollingGraph(gx, gy, gw, gh, disp_.cpuGraph, 100);

  disp_.drawFanIcon(NOCT_DISP_W - 14, CONTENT_Y0 + 8, fanFrame);

  int y = CONTENT_Y0 + PAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + 6, "GHz");
  String ghzStr = (hw.cc <= 0) ? "N/A" : String(hw.cc / 1000.0f, 1);
  disp_.drawRightAligned(NOCT_DISP_W - NOCT_MARGIN, y + 6, ghzStr);
  y += 10 + PAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + 6, "PWR");
  String pwrStr = (hw.pw < 0) ? "N/A" : (String(hw.pw) + "W");
  disp_.drawRightAligned(NOCT_DISP_W - NOCT_MARGIN, y + 6, pwrStr);
  y += 10 + PAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + 6, "FAN");
  String fanStr = (hw.cf < 0) ? "N/A" : String(hw.cf);
  disp_.drawRightAligned(NOCT_DISP_W - NOCT_MARGIN, y + 6, fanStr);
}

// ---------------------------------------------------------------------------
// GPU: same layout, right-aligned values
// ---------------------------------------------------------------------------
void SceneManager::drawGpu(int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int gx = NOCT_MARGIN, gy = NOCT_FOOTER_Y, gw = NOCT_DISP_W - 2 * NOCT_MARGIN,
      gh = NOCT_GRAPH_HEIGHT;
  disp_.drawDottedHLine(gx, gx + gw, gy + gh / 2);
  disp_.drawDottedHLine(gx, gx + gw, gy);
  disp_.drawRollingGraph(gx, gy, gw, gh, disp_.gpuGraph, 100);

  disp_.drawFanIcon(NOCT_DISP_W - 14, CONTENT_Y0 + 8, fanFrame);

  int y = CONTENT_Y0 + PAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + 6, "HOT");
  String hotStr = (hw.gh <= 0) ? "N/A" : (String(hw.gh) + "C");
  disp_.drawRightAligned(NOCT_DISP_W - NOCT_MARGIN, y + 6, hotStr);
  y += 10 + PAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + 6, "VRAM");
  String vramStr = (hw.gv < 0) ? "N/A" : (String(hw.gv) + "%");
  disp_.drawRightAligned(NOCT_DISP_W - NOCT_MARGIN, y + 6, vramStr);
  y += 10 + PAD;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, y + 6, "FAN");
  String fanStr = (hw.gf < 0) ? "N/A" : String(hw.gf);
  disp_.drawRightAligned(NOCT_DISP_W - NOCT_MARGIN, y + 6, fanStr);
}

// ---------------------------------------------------------------------------
// NET: graph, DL in inverted box, safe strings
// ---------------------------------------------------------------------------
void SceneManager::drawNet() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int gx = NOCT_MARGIN, gy = NOCT_FOOTER_Y, gw = NOCT_DISP_W - 2 * NOCT_MARGIN,
      gh = NOCT_GRAPH_HEIGHT;
  disp_.netDownGraph.setMax(2048);
  disp_.drawRollingGraph(gx, gy, gw, gh, disp_.netDownGraph, 2048);

  int scanX = gx + (millis() / 50) % (gw + 1);
  disp_.drawDottedVLine(scanX, gy, gy + gh - 1);

  char buf[24];
  float dlMb = (hw.nd >= 1024) ? (hw.nd / 1024.0f) : 0.0f;
  if (hw.nd >= 1024)
    snprintf(buf, sizeof(buf), "DL: %.1f", dlMb);
  else
    snprintf(buf, sizeof(buf), "DL: %d", hw.nd);
  u8g2.setFont(FONT_VAL);
  int tw = u8g2.getUTF8Width(buf);
  int bx = NOCT_DISP_W - NOCT_MARGIN - tw - 4;
  int by = CONTENT_Y0 + PAD;
  u8g2.setDrawColor(1);
  u8g2.drawBox(bx, by, tw + 4, 12 + PAD);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(bx + 2, by + 11, buf);
  u8g2.setDrawColor(1);

  int y = CONTENT_Y0 + 18 + PAD;
  if (hw.nu >= 1024)
    snprintf(buf, sizeof(buf), "UP %.1f MB/s", hw.nu / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "UP %d KB/s", hw.nu);
  disp_.drawSafeStr(NOCT_MARGIN, y, String(buf));
  y += 12 + PAD;
  snprintf(buf, sizeof(buf), "PING %d ms", hw.pg >= 0 ? hw.pg : 0);
  disp_.drawSafeStr(NOCT_MARGIN, y, String(buf));
}

// ---------------------------------------------------------------------------
// SCENE_ATMOS: Icon at (5,20). Data at X=45. Temp big Y=30, city Y=45, desc
// Y=55. If weather missing: ICON_DISCONNECT + "OFFLINE".
// ---------------------------------------------------------------------------
void SceneManager::drawAtmos() {
  WeatherData &w = state_.weather;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int ICON_X = 5;
  const int ICON_Y = 20;
  const int DATA_X = 45;
  const int TEMP_Y = 30;
  const int CITY_Y = 45;
  const int DESC_Y = 55;

  bool hasWeather =
      state_.weatherReceived && (w.temp != 0 || w.desc.length() > 0);

  if (!hasWeather) {
    disp_.drawDisconnectIcon(ICON_X, ICON_Y);
    u8g2.setFont(FONT_VAL);
    u8g2.drawUTF8(DATA_X, TEMP_Y + 2, "OFFLINE");
    return;
  }

  drawWmoIconXbm(ICON_X, ICON_Y, w.wmoCode);

  String tempStr = (w.temp == 0) ? "N/A" : (String(w.temp) + "C");
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(DATA_X, TEMP_Y + 2, tempStr.c_str());

  u8g2.setFont(FONT_TINY);
  String cityStr = "MSK";
  u8g2.drawStr(DATA_X, CITY_Y, cityStr.c_str());

  String descStr = w.desc.length() > 0 ? w.desc : "---";
  if (descStr.length() > 10)
    descStr = descStr.substring(0, 10);
  u8g2.drawStr(DATA_X, DESC_Y, descStr.c_str());
}

// ---------------------------------------------------------------------------
// SCENE_MEDIA: Cover box (0,12,52,52). If no cover: "NO DATA" diagonal cross.
// Track info at X=56: Artist Y=25 (truncate 12), Track Y=40 (marquee/truncate),
// Time/Status Y=55.
// ---------------------------------------------------------------------------
void SceneManager::drawMedia(bool blinkState) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int COVER_X = 0;
  const int COVER_Y = 12;
  const int COVER_W = 52;
  const int COVER_H = 52;
  const int INFO_X = 56;
  const int ARTIST_Y = 25;
  const int TRACK_Y = 40;
  const int STATUS_Y = 55;
  const int MAX_ARTIST_CHARS = 12;
  const int RAW_BYTES = (52 * 52) / 8;

  disp_.drawChamferBox(COVER_X, COVER_Y, COVER_W, COVER_H, 2);

  if (media.coverB64.length() > 0) {
    unsigned char raw[512];
    size_t olen = 0;
    int ret = mbedtls_base64_decode(
        raw, sizeof(raw), &olen, (const unsigned char *)media.coverB64.c_str(),
        media.coverB64.length());
    if (ret == 0 && olen == (size_t)RAW_BYTES) {
      for (int row = 0; row < COVER_H; row++) {
        for (int col = 0; col < COVER_W; col++) {
          int byteIdx = row * (COVER_W / 8) + col / 8;
          int bit = col % 8;
          if (raw[byteIdx] & (1 << (7 - bit)))
            u8g2.drawPixel(COVER_X + col, COVER_Y + row);
        }
      }
    } else {
      drawNoDataCross(COVER_X, COVER_Y, COVER_W, COVER_H);
    }
  } else {
    drawNoDataCross(COVER_X, COVER_Y, COVER_W, COVER_H);
  }

  u8g2.setFont(FONT_LABEL);
  String artistStr = media.artist.length() > 0 ? media.artist : "---";
  if (artistStr.length() > (unsigned int)MAX_ARTIST_CHARS)
    artistStr = artistStr.substring(0, MAX_ARTIST_CHARS);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(INFO_X, ARTIST_Y, artistStr.c_str());

  String trackStr = media.track.length() > 0 ? media.track : "---";
  u8g2.setFont(FONT_VAL);
  int maxW = NOCT_DISP_W - INFO_X - PAD;
  int tw = u8g2.getUTF8Width(trackStr.c_str());
  if (tw > maxW) {
    int scrollLen = tw + 24;
    if (scrollLen < 1)
      scrollLen = 1;
    int scroll = (millis() / 50) % scrollLen;
    int offset = maxW - scroll;
    u8g2.drawUTF8(INFO_X + offset, TRACK_Y, trackStr.c_str());
  } else {
    u8g2.drawUTF8(INFO_X, TRACK_Y, trackStr.c_str());
  }

  u8g2.setFont(FONT_TINY);
  if (!media.isPlaying && !media.isIdle)
    u8g2.drawStr(INFO_X, STATUS_Y, "STANDBY");
  else if (media.isIdle)
    u8g2.drawStr(INFO_X, STATUS_Y, blinkState ? "IDLE" : "zzz");
  else
    u8g2.drawStr(INFO_X, STATUS_Y, "PLAY");
}

// Diagonal cross "NO DATA" inside box
void SceneManager::drawNoDataCross(int x, int y, int w, int h) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.drawLine(x, y, x + w - 1, y + h - 1);
  u8g2.drawLine(x + w - 1, y, x, y + h - 1);
  u8g2.setFont(FONT_TINY);
  String nd = "NO DATA";
  int tw = u8g2.getUTF8Width(nd.c_str());
  u8g2.drawUTF8(x + (w - tw) / 2, y + h / 2 - 2, nd.c_str());
}

// ---------------------------------------------------------------------------
// WMO -> XBM weather icon
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
  u8g2.setFont(FONT_VAL);
  disp_.drawCentered(0, NOCT_DISP_H / 2 - 10, NOCT_DISP_W, "SEARCH_MODE");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 4, "Scanning for host...");
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
  disp_.drawCentered(0, NOCT_DISP_H / 2 - 6, NOCT_DISP_W, "NO SIGNAL");
  u8g2.setFont(FONT_TINY);
  if (!wifiOk)
    u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 10, "WiFi: DISCONNECTED");
  else if (!tcpOk)
    u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 10, "TCP: CONNECTING...");
  else
    u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 10,
                 "WAITING FOR DATA...");
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
  u8g2.setFont(FONT_VAL);
  disp_.drawCentered(0, NOCT_DISP_H / 2 - 6, NOCT_DISP_W, "LINKING...");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 10,
               "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, NOCT_DISP_H / 2 + 8, 3, 3);
  disp_.drawWiFiIconXbm(NOCT_DISP_W - ICON_WIFI_W - NOCT_MARGIN, NOCT_MARGIN,
                        rssi, false);
}
