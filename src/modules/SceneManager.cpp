/*
 * NOCTURNE_OS — SceneManager: 6 screens. 128x64, MAIN/CPU/GPU/RAM/DISKS/MEDIA.
 * Unified card style: NOCT_CARD_LEFT/TOP/ROW_DY, LABEL_FONT labels, HUGE_FONT
 * for main values, STORAGE_FONT for lists.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <math.h>

#define SPLIT_X 64
#define CONTENT_Y NOCT_HEADER_H

const char *SceneManager::sceneNames_[] = {"MAIN", "CPU",   "GPU",
                                           "RAM",  "DISKS", "MEDIA"};

SceneManager::SceneManager(DisplayEngine &disp, AppState &state)
    : disp_(disp), state_(state) {}

const char *SceneManager::getSceneName(int sceneIndex) const {
  if (sceneIndex < 0 || sceneIndex >= NOCT_TOTAL_SCENES)
    return "MAIN";
  return sceneNames_[sceneIndex];
}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame) {
  (void)bootTime;
  (void)fanFrame;
  switch (sceneIndex) {
  case NOCT_SCENE_MAIN:
    drawMain(blinkState);
    break;
  case NOCT_SCENE_CPU:
    drawCpu();
    break;
  case NOCT_SCENE_GPU:
    drawGpu();
    break;
  case NOCT_SCENE_RAM:
    drawRam();
    break;
  case NOCT_SCENE_DISKS:
    drawDisks();
    break;
  case NOCT_SCENE_MEDIA:
    drawPlayer();
    break;
  default:
    drawMain(blinkState);
    break;
  }
}

// ---------------------------------------------------------------------------
// SCENE 1: MAIN — CPU/GPU temp+load, RAM and VRAM used/total.
// ---------------------------------------------------------------------------
void SceneManager::drawMain(bool blinkState) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  for (int yy = CONTENT_Y; yy < NOCT_DISP_H; yy++)
    u8g2.drawPixel(SPLIT_X, yy);

  int cpuTemp = (hw.ct > 0) ? hw.ct : 0;
  int gpuTemp = (hw.gt > 0) ? hw.gt : 0;
  int cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
  int gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
  bool cpuOver = (cpuTemp >= CPU_TEMP_ALERT);
  bool gpuOver = (gpuTemp >= GPU_TEMP_ALERT);

  int y0 = NOCT_CARD_TOP;
  int dy = NOCT_CARD_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0, "CPU");
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0, "GPU");

  char buf[24];
  snprintf(buf, sizeof(buf), "%d\xC2\xB0 %d%%", cpuTemp, cpuLoad);
  if (!(cpuOver && blinkState))
    u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + dy, buf);

  snprintf(buf, sizeof(buf), "%d\xC2\xB0 %d%%", gpuTemp, gpuLoad);
  if (!(gpuOver && blinkState))
    u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0 + dy, buf);

  float ru = hw.ru, ra = hw.ra;
  float vu = hw.vu, vt = hw.vt;
  snprintf(buf, sizeof(buf), "RAM %.1f/%.1f GB", ru, ra > 0 ? ra : 0.0f);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 2 * dy, buf);
  snprintf(buf, sizeof(buf), "VRAM %.1f/%.1f GB", vu, vt > 0 ? vt : 0.0f);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 3 * dy, buf);
}

// ---------------------------------------------------------------------------
// SCENE 2: CPU — Temperature, frequency, load %, watts.
// ---------------------------------------------------------------------------
void SceneManager::drawCpu() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int ct = (hw.ct > 0) ? hw.ct : 0;
  int cc = (hw.cc >= 0) ? hw.cc : 0;
  int cl = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
  int pw = (hw.pw >= 0) ? hw.pw : 0;

  int y0 = NOCT_CARD_TOP;
  int dy = NOCT_CARD_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0, "TEMP");
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0, "MHz");
  u8g2.setFont(HUGE_FONT);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", ct);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(NOCT_CARD_LEFT + (SPLIT_X - NOCT_CARD_LEFT - tw) / 2, y0 + dy,
                buf);
  snprintf(buf, sizeof(buf), "%d", cc);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(SPLIT_X + (SPLIT_X - NOCT_CARD_LEFT - tw) / 2, y0 + dy, buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 2 * dy, "LOAD");
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0 + 2 * dy, "PWR");
  snprintf(buf, sizeof(buf), "%d%%", cl);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 3 * dy, buf);
  snprintf(buf, sizeof(buf), "%dW", pw);
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0 + 3 * dy, buf);
}

// ---------------------------------------------------------------------------
// SCENE 3: GPU — Temperature, load %, VRAM %, frequencies, power.
// ---------------------------------------------------------------------------
void SceneManager::drawGpu() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int gt = (hw.gt > 0) ? hw.gt : 0;
  int gl = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
  int gv = (hw.gv >= 0 && hw.gv <= 100) ? hw.gv : 0;
  int gclock = (hw.gclock >= 0) ? hw.gclock : 0;
  int vclock = (hw.vclock >= 0) ? hw.vclock : 0;
  int gtdp = (hw.gtdp >= 0) ? hw.gtdp : 0;

  int y0 = NOCT_CARD_TOP;
  int dy = 8;
  int left = NOCT_CARD_LEFT;
  int rightCol = SPLIT_X + left;
  char buf[20];
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(left, y0, "TEMP");
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", gt);
  u8g2.drawUTF8(left, y0 + dy, buf);
  u8g2.drawUTF8(left, y0 + 2 * dy, "LOAD");
  snprintf(buf, sizeof(buf), "%d%%", gl);
  u8g2.drawUTF8(left, y0 + 3 * dy, buf);
  u8g2.drawUTF8(left, y0 + 4 * dy, "VRAM");
  snprintf(buf, sizeof(buf), "%d%%", gv);
  u8g2.drawUTF8(left, y0 + 5 * dy, buf);

  u8g2.drawUTF8(rightCol, y0, "CORE");
  snprintf(buf, sizeof(buf), "%d", gclock);
  u8g2.drawUTF8(rightCol, y0 + dy, buf);
  u8g2.drawUTF8(rightCol, y0 + 2 * dy, "MEM");
  snprintf(buf, sizeof(buf), "%d", vclock);
  u8g2.drawUTF8(rightCol, y0 + 3 * dy, buf);
  u8g2.drawUTF8(rightCol, y0 + 4 * dy, "PWR");
  snprintf(buf, sizeof(buf), "%dW", gtdp);
  u8g2.drawUTF8(rightCol, y0 + 5 * dy, buf);
}

// ---------------------------------------------------------------------------
// SCENE 4: RAM — Used/total GB + top 2 processes by RAM.
// ---------------------------------------------------------------------------
void SceneManager::drawRam() {
  HardwareData &hw = state_.hw;
  ProcessData &proc = state_.process;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  float ru = hw.ru, ra = hw.ra;
  int y0 = NOCT_CARD_TOP;
  int dy = NOCT_CARD_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0, "RAM");
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f / %.1f GB", ru, ra > 0 ? ra : 0.0f);
  u8g2.setFont(HUGE_FONT);
  int tw = u8g2.getUTF8Width(buf);
  if (tw > NOCT_DISP_W - 4)
    u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + dy, buf);

  u8g2.setFont(STORAGE_FONT);
  const int maxNameLen = 14;
  for (int i = 0; i < 2; i++) {
    int y = y0 + 2 * dy + i * dy;
    if (y > NOCT_DISP_H - 2)
      break;
    String name = proc.ramNames[i];
    int mb = proc.ramMb[i];
    if (name.length() > 0) {
      char line[32];
      snprintf(line, sizeof(line), "%d MB", mb);
      int lw = u8g2.getUTF8Width(line);
      u8g2.drawUTF8(NOCT_DISP_W - 2 - lw, y, line);
      if (name.length() > (unsigned)maxNameLen)
        name = name.substring(0, maxNameLen);
      u8g2.drawUTF8(NOCT_CARD_LEFT, y, name.c_str());
    }
  }
}

// ---------------------------------------------------------------------------
// SCENE 5: DISKS — Letter, load %, temperature. No bars.
// ---------------------------------------------------------------------------
void SceneManager::drawDisks() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(STORAGE_FONT);
  int y0 = NOCT_CARD_TOP;
  int dy = NOCT_CARD_ROW_DY;
  char buf[32];
  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int y = y0 + i * dy;
    char letter = hw.hdd[i].name[0] ? hw.hdd[i].name[0] : (char)('C' + i);
    int u = (hw.hdd[i].load >= 0 && hw.hdd[i].load <= 100) ? hw.hdd[i].load : 0;
    int t = hw.hdd[i].temp;
    snprintf(buf, sizeof(buf), "%c: %d%% %d\xC2\xB0", letter, u, t);
    u8g2.drawUTF8(NOCT_CARD_LEFT, y, buf);
  }
}

// ---------------------------------------------------------------------------
// SCENE 6: PLAYER — Artist, track, status (PLAYING/PAUSED).
// ---------------------------------------------------------------------------
void SceneManager::drawPlayer() {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int y0 = NOCT_CARD_TOP;
  int dy = NOCT_CARD_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  const char *status = (media.mediaStatus == "PLAYING") ? "PLAYING" : "PAUSED";
  int sw = u8g2.getUTF8Width(status);
  u8g2.drawUTF8(NOCT_DISP_W - 2 - sw, y0, status);

  int maxChar = 18;
  String artist = media.artist;
  if (artist.length() > (unsigned)maxChar)
    artist = artist.substring(0, maxChar);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + dy, artist.c_str());

  String track = media.track;
  if (track.length() > (unsigned)maxChar)
    track = track.substring(0, maxChar);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 2 * dy, track.c_str());
}

// ---------------------------------------------------------------------------
// 32x32 weather icon by WMO code (kept for possible future use)
// ---------------------------------------------------------------------------
void SceneManager::drawWeatherIcon32(int x, int y, int wmoCode) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const uint8_t *bits = icon_weather_cloud_32_bits;
  if (wmoCode == 0)
    bits = icon_weather_sun_32_bits;
  else if (wmoCode >= 1 && wmoCode <= 3)
    bits = icon_weather_sun_32_bits;
  else if (wmoCode >= 45 && wmoCode <= 48)
    bits = icon_weather_cloud_32_bits;
  else if (wmoCode >= 51 && wmoCode <= 67)
    bits = icon_weather_rain_32_bits;
  else if (wmoCode >= 71 && wmoCode <= 77)
    bits = icon_weather_snow_32_bits;
  else if (wmoCode >= 80 && wmoCode <= 82)
    bits = icon_weather_rain_32_bits;
  else if (wmoCode >= 85 && wmoCode <= 86)
    bits = icon_weather_snow_32_bits;
  else if (wmoCode >= 95 && wmoCode <= 99)
    bits = icon_weather_rain_32_bits;
  u8g2.drawXBM(x, y, WEATHER_ICON_W, WEATHER_ICON_H, bits);
}

void SceneManager::drawNoDataCross(int x, int y, int w, int h) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.drawLine(x, y, x + w - 1, y + h - 1);
  u8g2.drawLine(x + w - 1, y, x, y + h - 1);
  u8g2.setFont(LABEL_FONT);
  const char *nd = "NO DATA";
  int tw = u8g2.getUTF8Width(nd);
  u8g2.drawUTF8(x + (w - tw) / 2, y + h / 2 - 2, nd);
}

void SceneManager::drawNoisePattern(int x, int y, int w, int h) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  for (int i = 0; i < 200; i++)
    u8g2.drawPixel(x + random(w), y + random(h));
}

// ---------------------------------------------------------------------------
void SceneManager::drawSearchMode(int scanPhase) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  int tw;
  u8g2.setFont(MID_FONT);
  tw = u8g2.getUTF8Width("SEARCH_MODE");
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, NOCT_DISP_H / 2 - 10, "SEARCH_MODE");
  u8g2.setFont(LABEL_FONT);
  u8g2.drawStr(2, NOCT_DISP_H / 2 + 4, "Scanning for host...");
  int cx = NOCT_DISP_W / 2, cy = NOCT_DISP_H - 16, r = 14;
  u8g2.drawCircle(cx, cy, r);
  int angle = (scanPhase * 30) % 360;
  float rad = angle * 3.14159f / 180.0f;
  u8g2.drawLine(cx, cy, cx + (int)(r * cos(rad)), cy + (int)(r * sin(rad)));
}

void SceneManager::drawMenu(int menuItem, bool carouselOn, bool screenOff) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int boxX = 8, boxY = 10, boxW = NOCT_DISP_W - 16,
            boxH = NOCT_DISP_H - 20;
  const int menuRowDy = 10;
  u8g2.drawFrame(boxX, boxY, boxW, boxH);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawStr(boxX + 6, boxY + 8, "QUICK MENU");
  const char *carouselStr = carouselOn ? "CAROUSEL: ON " : "CAROUSEL: OFF";
  const char *screenStr = screenOff ? "SCREEN: OFF" : "SCREEN: ON ";
  const char *lines[] = {carouselStr, screenStr, "EXIT"};
  int startY = boxY + 18;
  for (int i = 0; i < 3; i++) {
    int y = startY + i * menuRowDy;
    if (y + 8 > boxY + boxH - 4)
      break;
    if (i == menuItem) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(boxX + 6, y - 6, boxW - 12, 9);
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
  int tw;
  u8g2.setFont(HUGE_FONT);
  tw = u8g2.getUTF8Width("NO SIGNAL");
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, NOCT_DISP_H / 2 - 6, "NO SIGNAL");
  u8g2.setFont(LABEL_FONT);
  if (!wifiOk)
    u8g2.drawStr(2, NOCT_DISP_H / 2 + 10, "WiFi: DISCONNECTED");
  else if (!tcpOk)
    u8g2.drawStr(2, NOCT_DISP_H / 2 + 10, "TCP: CONNECTING...");
  else
    u8g2.drawStr(2, NOCT_DISP_H / 2 + 10, "WAITING FOR DATA...");
  drawNoisePattern(0, 0, NOCT_DISP_W, NOCT_DISP_H);
  if (wifiOk)
    disp_.drawWiFiIcon(NOCT_DISP_W - 14, 2, rssi);
  if (blinkState)
    u8g2.drawBox(NOCT_DISP_W / 2 + 40, NOCT_DISP_H / 2 - 8, 6, 12);
}

void SceneManager::drawConnecting(int rssi, bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)blinkState;
  int tw;
  u8g2.setFont(MID_FONT);
  tw = u8g2.getUTF8Width("LINKING...");
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, NOCT_DISP_H / 2 - 6, "LINKING...");
  u8g2.setFont(LABEL_FONT);
  u8g2.drawStr(2, NOCT_DISP_H / 2 + 10, "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, NOCT_DISP_H / 2 + 8, 3, 3);
  disp_.drawWiFiIcon(NOCT_DISP_W - 14, 2, rssi);
}
