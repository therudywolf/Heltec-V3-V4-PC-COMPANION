/*
 * NOCTURNE_OS — SceneManager: one topic per screen, large text, no overlap.
 * Each scene uses one content zone (full area below header) or two blocks
 * (HUB).
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"

// --- Grid: header 0–10, content 11–63 (one zone per scene) ---
#define LAYOUT_HEADER_H NOCT_HEADER_H
#define LAYOUT_CONTENT_Y 11
#define LAYOUT_CONTENT_H (NOCT_DISP_H - LAYOUT_HEADER_H)
#define LAYOUT_CONTENT_W NOCT_DISP_W
#define LAYOUT_ZONE_A_X 0
#define LAYOUT_ZONE_A_Y 11
#define LAYOUT_ZONE_A_W 64
#define LAYOUT_ZONE_A_H 26
#define LAYOUT_ZONE_B_X 64
#define LAYOUT_ZONE_B_Y 11
#define LAYOUT_ZONE_B_W 64
#define LAYOUT_ZONE_B_H 26
#define PAD 2
#define CONTENT_Y0 11

const char *SceneManager::sceneNames_[] = {"HUB",   "CPU",  "CPU_G", "GPU",
                                           "GPU_G", "RAM",  "NET",   "NET_G",
                                           "ATMOS", "MEDIA"};

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
  case NOCT_SCENE_CPU_GRAPH:
    drawCpuGraph(bootTime);
    break;
  case NOCT_SCENE_GPU:
    drawGpu(fanFrame);
    break;
  case NOCT_SCENE_GPU_GRAPH:
    drawGpuGraph(fanFrame);
    break;
  case NOCT_SCENE_RAM:
    drawRam();
    break;
  case NOCT_SCENE_NET:
    drawNet();
    break;
  case NOCT_SCENE_NET_GRAPH:
    drawNetGraph();
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
// HUB: only CPU + GPU temps (two blocks). One topic = summary.
// ---------------------------------------------------------------------------
void SceneManager::drawHub(unsigned long bootTime) {
  HardwareData &hw = state_.hw;
  (void)bootTime;
  String cpuVal = (hw.ct <= 0) ? "" : (String(hw.ct) + "C");
  disp_.drawBlock(LAYOUT_ZONE_A_X, LAYOUT_ZONE_A_Y, LAYOUT_ZONE_A_W,
                  LAYOUT_ZONE_A_H, "CPU", cpuVal, hw.ct >= CPU_TEMP_ALERT);
  String gpuVal = (hw.gt <= 0) ? "" : (String(hw.gt) + "C");
  disp_.drawBlock(LAYOUT_ZONE_B_X, LAYOUT_ZONE_B_Y, LAYOUT_ZONE_B_W,
                  LAYOUT_ZONE_B_H, "GPU", gpuVal, hw.gt >= GPU_TEMP_ALERT);
}

// ---------------------------------------------------------------------------
// CPU: one big block — temperature only (large, readable).
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(unsigned long bootTime, int fanFrame) {
  HardwareData &hw = state_.hw;
  (void)bootTime;
  (void)fanFrame;
  String v = (hw.ct <= 0) ? "—" : (String(hw.ct) + "C");
  disp_.drawBlock(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W, LAYOUT_CONTENT_H,
                  "CPU", v, hw.ct >= CPU_TEMP_ALERT);
}

// ---------------------------------------------------------------------------
// CPU_GRAPH: full-area graph only.
// ---------------------------------------------------------------------------
void SceneManager::drawCpuGraph(unsigned long bootTime) {
  (void)bootTime;
  disp_.drawRollingGraph(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W,
                         LAYOUT_CONTENT_H, disp_.cpuGraph, 100);
}

// ---------------------------------------------------------------------------
// GPU: one big block — temperature only.
// ---------------------------------------------------------------------------
void SceneManager::drawGpu(int fanFrame) {
  HardwareData &hw = state_.hw;
  (void)fanFrame;
  String v = (hw.gt <= 0) ? "—" : (String(hw.gt) + "C");
  disp_.drawBlock(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W, LAYOUT_CONTENT_H,
                  "GPU", v, hw.gt >= GPU_TEMP_ALERT);
}

// ---------------------------------------------------------------------------
// GPU_GRAPH: full-area graph only.
// ---------------------------------------------------------------------------
void SceneManager::drawGpuGraph(int fanFrame) {
  (void)fanFrame;
  disp_.drawRollingGraph(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W,
                         LAYOUT_CONTENT_H, disp_.gpuGraph, 100);
}

// ---------------------------------------------------------------------------
// RAM: one progress bar + one line "RAM: X GB" (full content area).
// ---------------------------------------------------------------------------
void SceneManager::drawRam() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  float pct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  disp_.drawProgressBar(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W, LAYOUT_CONTENT_H,
                        pct);
  int gb = (int)(hw.ra / 1024.0f);
  if (gb < 0)
    gb = 0;
  String s = "RAM: " + String(gb) + " GB";
  u8g2.setFont(FONT_TINY);
  int tw = u8g2.getUTF8Width(s.c_str());
  u8g2.setDrawColor(2);
  u8g2.drawUTF8((LAYOUT_CONTENT_W - tw) / 2,
                LAYOUT_CONTENT_Y + LAYOUT_CONTENT_H / 2 - 2, s.c_str());
  u8g2.setDrawColor(1);
}

// ---------------------------------------------------------------------------
// NET: one big block — DL speed only.
// ---------------------------------------------------------------------------
void SceneManager::drawNet() {
  HardwareData &hw = state_.hw;
  String v;
  if (hw.nd >= 1024)
    v = String((int)(hw.nd / 1024.0f)) + " MB/s";
  else
    v = String(hw.nd) + " KB/s";
  disp_.drawBlock(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W, LAYOUT_CONTENT_H, "DL",
                  v, false);
}

// ---------------------------------------------------------------------------
// NET_GRAPH: full-area net graph only.
// ---------------------------------------------------------------------------
void SceneManager::drawNetGraph() {
  disp_.netDownGraph.setMax(2048);
  disp_.drawRollingGraph(0, LAYOUT_CONTENT_Y, LAYOUT_CONTENT_W,
                         LAYOUT_CONTENT_H, disp_.netDownGraph, 2048);
}

// ---------------------------------------------------------------------------
// ATMOS: icon + big temp only (one topic, readable).
// ---------------------------------------------------------------------------
void SceneManager::drawAtmos() {
  WeatherData &w = state_.weather;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int ICON_X = 8;
  const int ICON_Y = LAYOUT_CONTENT_Y + 8;
  const int TEMP_X = 50;
  const int TEMP_Y = LAYOUT_CONTENT_Y + LAYOUT_CONTENT_H / 2 - 4;

  bool hasWeather =
      state_.weatherReceived && (w.temp != 0 || w.desc.length() > 0);
  if (!hasWeather) {
    disp_.drawDisconnectIcon(ICON_X, ICON_Y);
    u8g2.setFont(FONT_BIG);
    u8g2.drawUTF8(TEMP_X, TEMP_Y, "OFF");
    return;
  }
  drawWmoIconXbm(ICON_X, ICON_Y, w.wmoCode);
  String tempStr = (w.temp == 0) ? "—" : (String(w.temp) + "C");
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(TEMP_X, TEMP_Y, tempStr.c_str());
}

// ---------------------------------------------------------------------------
// MEDIA: cover (left) + one line — track or status (right, large).
// ---------------------------------------------------------------------------
void SceneManager::drawMedia(bool blinkState) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int ART_W = 64;
  const int ART_H = 64;
  const int TX = 66;
  const int TY = LAYOUT_CONTENT_Y + LAYOUT_CONTENT_H / 2 - 4;

  if (media.coverB64.length() > 0 &&
      disp_.drawXBMArtFromBase64(0, 0, ART_W, ART_H, media.coverB64)) {
  } else {
    drawNoisePattern(0, LAYOUT_CONTENT_Y, ART_W, LAYOUT_CONTENT_H);
    drawNoDataCross(8, LAYOUT_CONTENT_Y + 8, ART_W - 16, LAYOUT_CONTENT_H - 16);
  }

  u8g2.setFont(FONT_BIG);
  String line = media.track.length() > 0 ? media.track : "";
  if (line.length() > 18)
    line = line.substring(0, 18);
  if (line.length() == 0) {
    if (!media.isPlaying && !media.isIdle)
      line = "STANDBY";
    else if (media.isIdle)
      line = blinkState ? "IDLE" : "—";
    else
      line = "PLAY";
  }
  u8g2.drawUTF8(TX, TY, line.c_str());
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
