/*
 * NOCTURNE_OS — SceneManager: Strict 128x64 grid. Zero-overlap zones.
 * All draw calls use bounding-box constants; no magic numbers.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"

// --- Strict grid (128x64): non-overlapping zones ---
#define LAYOUT_HEADER_Y0 0
#define LAYOUT_HEADER_H NOCT_HEADER_H // 10px
#define LAYOUT_ZONE_A_X 0
#define LAYOUT_ZONE_A_Y 11
#define LAYOUT_ZONE_A_W 64
#define LAYOUT_ZONE_A_H 26
#define LAYOUT_ZONE_B_X 64
#define LAYOUT_ZONE_B_Y 11
#define LAYOUT_ZONE_B_W 64
#define LAYOUT_ZONE_B_H 26
#define LAYOUT_ZONE_C_X 0
#define LAYOUT_ZONE_C_Y 37
#define LAYOUT_ZONE_C_W NOCT_DISP_W
#define LAYOUT_ZONE_C_H 12
#define LAYOUT_ZONE_D_X 0
#define LAYOUT_ZONE_D_Y 49
#define LAYOUT_ZONE_D_W NOCT_DISP_W
#define LAYOUT_ZONE_D_H 15

#define PAD 2
#define CONTENT_Y0 LAYOUT_ZONE_A_Y
#define GRID_MID_X (NOCT_DISP_W / 2)
#define FOOTER_Y0 54

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
// SCENE_HUB: Strict zones A (CPU), B (GPU), C (RAM bar), D (graph).
// ---------------------------------------------------------------------------
void SceneManager::drawHub(unsigned long bootTime) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)bootTime;

  String cpuVal = (hw.ct <= 0) ? "" : (String(hw.ct) + "C");
  bool cpuWarn = (hw.ct >= CPU_TEMP_ALERT);
  disp_.drawBlock(LAYOUT_ZONE_A_X, LAYOUT_ZONE_A_Y, LAYOUT_ZONE_A_W,
                  LAYOUT_ZONE_A_H, "CPU", cpuVal, cpuWarn);

  String gpuVal = (hw.gt <= 0) ? "" : (String(hw.gt) + "C");
  bool gpuWarn = (hw.gt >= GPU_TEMP_ALERT);
  disp_.drawBlock(LAYOUT_ZONE_B_X, LAYOUT_ZONE_B_Y, LAYOUT_ZONE_B_W,
                  LAYOUT_ZONE_B_H, "GPU", gpuVal, gpuWarn);

  float ramPct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  if (ramPct > 100.0f)
    ramPct = 100.0f;
  if (ramPct < 0.0f)
    ramPct = 0.0f;
  disp_.drawProgressBar(LAYOUT_ZONE_C_X, LAYOUT_ZONE_C_Y, LAYOUT_ZONE_C_W,
                        LAYOUT_ZONE_C_H, ramPct);
  int ramGb = (int)(hw.ra / 1024.0f);
  if (ramGb < 0)
    ramGb = 0;
  String ramStr = "RAM: " + String(ramGb) + "GB";
  u8g2.setFont(FONT_TINY);
  int tw = u8g2.getUTF8Width(ramStr.c_str());
  u8g2.setDrawColor(2);
  u8g2.drawUTF8(LAYOUT_ZONE_C_X + (LAYOUT_ZONE_C_W - tw) / 2,
                LAYOUT_ZONE_C_Y + 8, ramStr.c_str());
  u8g2.setDrawColor(1);

  disp_.drawRollingGraph(LAYOUT_ZONE_D_X, LAYOUT_ZONE_D_Y, LAYOUT_ZONE_D_W,
                         LAYOUT_ZONE_D_H, disp_.cpuGraph, 100);
}

// ---------------------------------------------------------------------------
// CPU: stats in content area (11–48), rolling graph in Zone D.
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(unsigned long bootTime, int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)bootTime;

  disp_.drawRollingGraph(LAYOUT_ZONE_D_X, LAYOUT_ZONE_D_Y, LAYOUT_ZONE_D_W,
                         LAYOUT_ZONE_D_H, disp_.cpuGraph, 100);

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
// GPU: stats in content area (11–48), rolling graph in Zone D.
// ---------------------------------------------------------------------------
void SceneManager::drawGpu(int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawRollingGraph(LAYOUT_ZONE_D_X, LAYOUT_ZONE_D_Y, LAYOUT_ZONE_D_W,
                         LAYOUT_ZONE_D_H, disp_.gpuGraph, 100);

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
// NET: graph in Zone D, DL box and strings in content area.
// ---------------------------------------------------------------------------
void SceneManager::drawNet() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.netDownGraph.setMax(2048);
  disp_.drawRollingGraph(LAYOUT_ZONE_D_X, LAYOUT_ZONE_D_Y, LAYOUT_ZONE_D_W,
                         LAYOUT_ZONE_D_H, disp_.netDownGraph, 2048);

  int scanX = LAYOUT_ZONE_D_X + (millis() / 50) % (LAYOUT_ZONE_D_W + 1);
  disp_.drawDottedVLine(scanX, LAYOUT_ZONE_D_Y,
                        LAYOUT_ZONE_D_Y + LAYOUT_ZONE_D_H - 1);

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
// SCENE_MEDIA: 64x64 XBM art at (0,0) left half. Text right of 64.
// No cover -> static noise patch. Labels uppercase, values right-aligned.
// ---------------------------------------------------------------------------
void SceneManager::drawMedia(bool blinkState) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int ART_W = 64;
  const int ART_H = 64;
  const int INFO_X = 66;
  const int INFO_END = NOCT_DISP_W - NOCT_MARGIN;
  const int ARTIST_Y = 22;
  const int TRACK_Y = 36;
  const int STATUS_Y = 52;
  const int MAX_CHARS = 14;

  if (media.coverB64.length() > 0 &&
      disp_.drawXBMArtFromBase64(0, 0, ART_W, ART_H, media.coverB64)) {
  } else {
    drawNoisePattern(0, CONTENT_Y0, ART_W, ART_H - CONTENT_Y0);
    drawNoDataCross(8, CONTENT_Y0 + 8, ART_W - 16, ART_H - CONTENT_Y0 - 16);
  }

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(INFO_X, ARTIST_Y - 6, "ARTIST");
  u8g2.setFont(FONT_VAL);
  String artistStr = media.artist.length() > 0 ? media.artist : "";
  if (artistStr.length() > (unsigned int)MAX_CHARS)
    artistStr = artistStr.substring(0, MAX_CHARS);
  disp_.drawSafeStr(INFO_X, ARTIST_Y, artistStr);

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(INFO_X, TRACK_Y - 6, "TRACK");
  u8g2.setFont(FONT_VAL);
  String trackStr = media.track.length() > 0 ? media.track : "---";
  int maxW = INFO_END - INFO_X;
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
  disp_.drawCentered(NOCT_DISP_W / 2, NOCT_DISP_H / 2 - 10, "SEARCH_MODE");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 4, "Scanning for host...");
  int cx = NOCT_DISP_W / 2, cy = NOCT_DISP_H - 16, r = 14;
  u8g2.drawCircle(cx, cy, r);
  int angle = (scanPhase * 30) % 360;
  float rad = angle * 3.14159f / 180.0f;
  u8g2.drawLine(cx, cy, cx + (int)(r * cos(rad)), cy + (int)(r * sin(rad)));
}

void SceneManager::drawMenu(int menuItem, bool carouselOn, bool screenOff) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int boxX = 8;
  const int boxY = 14;
  const int boxW = NOCT_DISP_W - 16;
  const int boxH = NOCT_DISP_H - 28;
  disp_.drawChamferBox(boxX, boxY, boxW, boxH, 4);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(boxX + 6, boxY + 10, "QUICK MENU");
  u8g2.setFont(FONT_TINY);
  const char *carouselStr = carouselOn ? "CAROUSEL: ON " : "CAROUSEL: OFF";
  const char *screenStr = screenOff ? "SCREEN: OFF" : "SCREEN: ON ";
  const char *lines[] = {carouselStr, screenStr, "EXIT"};
  int startY = boxY + 18;
  for (int i = 0; i < 3; i++) {
    int y = startY + i * 12;
    if (y + 10 > boxY + boxH - 4)
      break;
    if (i == menuItem) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(boxX + 6, y - 6, boxW - 12, 10);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(boxX + 8, y, lines[i]);
    if (i == menuItem)
      u8g2.setDrawColor(1);
  }
}

void SceneManager::drawNoSignal(bool wifiOk, bool tcpOk, int rssi,
                                bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_BIG);
  disp_.drawCentered(NOCT_DISP_W / 2, NOCT_DISP_H / 2 - 6, "NO SIGNAL");
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
  disp_.drawCentered(NOCT_DISP_W / 2, NOCT_DISP_H / 2 - 6, "LINKING...");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + PAD, NOCT_DISP_H / 2 + 10,
               "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, NOCT_DISP_H / 2 + 8, 3, 3);
  disp_.drawWiFiIconXbm(NOCT_DISP_W - ICON_WIFI_W - NOCT_MARGIN, NOCT_MARGIN,
                        rssi, false);
}
