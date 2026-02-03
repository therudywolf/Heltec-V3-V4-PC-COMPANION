/*
 * NOCTURNE_OS — SceneManager: Brutalist 5-scene monitoring. 128x64, 5m rule.
 * MAIN VALUES: u8g2_font_logisoso24_tr. LABELS: u8g2_font_profont12_tr.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <math.h>

#define SPLIT_X 64
#define CONTENT_Y 10

const char *SceneManager::sceneNames_[] = {"THERMAL", "LOAD", "MEMORY",
                                           "STORAGE", "FANS"};

SceneManager::SceneManager(DisplayEngine &disp, AppState &state)
    : disp_(disp), state_(state) {}

const char *SceneManager::getSceneName(int sceneIndex) const {
  if (sceneIndex < 0 || sceneIndex >= NOCT_TOTAL_SCENES)
    return "THERMAL";
  return sceneNames_[sceneIndex];
}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame) {
  (void)bootTime;
  (void)fanFrame;
  switch (sceneIndex) {
  case NOCT_SCENE_THERMAL:
    drawThermal(blinkState);
    break;
  case NOCT_SCENE_LOAD:
    drawLoad();
    break;
  case NOCT_SCENE_MEMORY:
    drawMemory();
    break;
  case NOCT_SCENE_STORAGE:
    drawStorage();
    break;
  case NOCT_SCENE_FANS:
    drawFans();
    break;
  default:
    drawThermal(blinkState);
    break;
  }
}

// ---------------------------------------------------------------------------
// SCENE 1: THERMAL — Split at X=64. Left: CPU label + huge temp. Right: GPU.
// If temp >= limit, blink that value.
// ---------------------------------------------------------------------------
void SceneManager::drawThermal(bool blinkState) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  for (int yy = CONTENT_Y; yy < NOCT_DISP_H; yy++)
    u8g2.drawPixel(SPLIT_X, yy);

  int cpuTemp = (hw.ct > 0) ? hw.ct : 0;
  int gpuTemp = (hw.gt > 0) ? hw.gt : 0;
  bool cpuOver = (cpuTemp >= CPU_TEMP_ALERT);
  bool gpuOver = (gpuTemp >= GPU_TEMP_ALERT);

  // Left: CPU
  u8g2.setFont(LABEL_FONT);
  int lw = u8g2.getUTF8Width("CPU");
  u8g2.drawUTF8((SPLIT_X - lw) / 2, 18, "CPU");
  u8g2.setFont(HUGE_FONT);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", cpuTemp);
  if (!(cpuOver && blinkState)) {
    int tw = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8((SPLIT_X - tw) / 2, 44, buf);
  }
  // Right: GPU
  u8g2.setFont(LABEL_FONT);
  int rw = u8g2.getUTF8Width("GPU");
  u8g2.drawUTF8(SPLIT_X + (SPLIT_X - rw) / 2, 18, "GPU");
  u8g2.setFont(HUGE_FONT);
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", gpuTemp);
  if (!(gpuOver && blinkState)) {
    int tw = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(SPLIT_X + (SPLIT_X - tw) / 2, 44, buf);
  }
}

// ---------------------------------------------------------------------------
// SCENE 2: LOAD — Left: CPU % huge. Right: GPU % huge.
// ---------------------------------------------------------------------------
void SceneManager::drawLoad() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  for (int yy = CONTENT_Y; yy < NOCT_DISP_H; yy++)
    u8g2.drawPixel(SPLIT_X, yy);

  int cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
  int gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(2, 18, "CPU %");
  u8g2.setFont(HUGE_FONT);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", cpuLoad);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((SPLIT_X - tw) / 2, 44, buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(SPLIT_X + 2, 18, "GPU %");
  u8g2.setFont(HUGE_FONT);
  snprintf(buf, sizeof(buf), "%d", gpuLoad);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(SPLIT_X + (SPLIT_X - tw) / 2, 44, buf);
}

// ---------------------------------------------------------------------------
// SCENE 3: MEMORY — Top: RAM USED "12.4" + "GB". Bottom: VRAM % huge.
// ---------------------------------------------------------------------------
void SceneManager::drawMemory() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  float usedGb = hw.ru;
  int vramPct = (hw.gv >= 0 && hw.gv <= 100) ? hw.gv : 0;

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(2, 18, "RAM USED");
  u8g2.setFont(HUGE_FONT);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f", usedGb);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2 - 10, 38, buf);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2 - 10 + tw + 2, 38, "GB");

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(2, 52, "VRAM %");
  u8g2.setFont(HUGE_FONT);
  snprintf(buf, sizeof(buf), "%d", vramPct);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, 62, buf);
}

// ---------------------------------------------------------------------------
// SCENE 4: STORAGE — Vertical list. Row Y=15,30,45,60: "C: [||||||....] 45%"
// ---------------------------------------------------------------------------
void SceneManager::drawStorage() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(STORAGE_FONT);
  const int rowY[] = {15, 30, 45, 60};
  const int barX = 24;
  const int barW = 70;
  const int barH = 4;

  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int y = rowY[i];
    int u = (hw.hdd[i].load >= 0 && hw.hdd[i].load <= 100) ? hw.hdd[i].load : 0;
    char letter = hw.hdd[i].name[0] ? hw.hdd[i].name[0] : (char)('C' + i);
    char label[4] = {letter, ':', '\0'};
    u8g2.drawUTF8(2, y, label);
    disp_.drawProgressBar(barX, y - 3, barW, barH, u);
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", u);
    disp_.drawRightAligned(NOCT_DISP_W - 2, y, STORAGE_FONT, pct);
  }
}

// ---------------------------------------------------------------------------
// SCENE 5: FANS & POWER — Left: PUMP, CPU. Right: GPU, PWR.
// ---------------------------------------------------------------------------
void SceneManager::drawFans() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(LABEL_FONT);
  char buf[20];
  snprintf(buf, sizeof(buf), "PUMP: %d", hw.s1);
  u8g2.drawUTF8(2, 22, buf);
  snprintf(buf, sizeof(buf), "CPU: %d", hw.cf);
  u8g2.drawUTF8(2, 38, buf);
  snprintf(buf, sizeof(buf), "GPU: %d", hw.gf);
  u8g2.drawUTF8(SPLIT_X + 2, 22, buf);
  snprintf(buf, sizeof(buf), "PWR: %dW", hw.gtdp > 0 ? hw.gtdp : hw.pw);
  u8g2.drawUTF8(SPLIT_X + 2, 38, buf);
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
  const int boxX = 8, boxY = 14, boxW = NOCT_DISP_W - 16,
            boxH = NOCT_DISP_H - 28;
  u8g2.drawFrame(boxX, boxY, boxW, boxH);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawStr(boxX + 6, boxY + 10, "QUICK MENU");
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
