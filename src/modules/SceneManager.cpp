/*
 * NOCTURNE_OS — SceneManager: Grid-based layouts. 128x64, zero overlap.
 * Coordinates as per spec; TINY / MID / HUGE fonts.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <math.h>

// --- Grid: header Y=0..8, content Y=10..63. Vertical split at X=64 ---
#define HEADER_H 9
#define CONTENT_Y 10
#define SPLIT_X 64

const char *SceneManager::sceneNames_[] = {"HUB",   "CPU",  "GPU",   "RAM",
                                           "DISKS", "FANS", "ATMOS", "MEDIA"};

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
    drawCpuDetail(bootTime);
    break;
  case NOCT_SCENE_GPU:
    drawGpuDetail(fanFrame);
    break;
  case NOCT_SCENE_RAM:
    drawRam();
    break;
  case NOCT_SCENE_DISKS:
    drawDisks();
    break;
  case NOCT_SCENE_FANS:
    drawFans(fanFrame);
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
// SCREEN 1: HUB — Split vertical. Left=CPU, Right=GPU. Vertical line X=64.
// ---------------------------------------------------------------------------
void SceneManager::drawHub(unsigned long bootTime) {
  HardwareData &hw = state_.hw;
  (void)bootTime;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  // Vertical divider: solid line X=64, Y=10..64
  for (int yy = CONTENT_Y; yy < NOCT_DISP_H; yy++)
    u8g2.drawPixel(SPLIT_X, yy);

  // --- LEFT (CPU) ---
  u8g2.setFont(TINY_FONT);
  u8g2.drawUTF8(2, 20, "CPU");
  u8g2.setFont(HUGE_FONT);
  String cpuTemp = (hw.ct <= 0) ? "---" : (String(hw.ct) + "\xC2\xB0");
  disp_.drawRightAligned(62, 38, HUGE_FONT, cpuTemp.c_str());
  u8g2.setFont(TINY_FONT);
  String cpuFreq =
      (hw.cc > 0)
          ? (String(hw.cc / 100) + "." + String((hw.cc % 100) / 10) + "GHz")
          : "4.2GHz";
  u8g2.drawUTF8(2, 48, cpuFreq.c_str());
  String cpuPwr = (hw.pw > 0) ? (String(hw.pw) + "W") : "65W";
  u8g2.drawUTF8(34, 48, cpuPwr.c_str());
  int cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
  disp_.drawProgressBar(2, 58, 60, 4, cpuLoad);

  // --- RIGHT (GPU) ---
  u8g2.setFont(TINY_FONT);
  u8g2.drawUTF8(66, 20, "GPU");
  u8g2.setFont(HUGE_FONT);
  String gpuTemp = (hw.gt <= 0) ? "---" : (String(hw.gt) + "\xC2\xB0");
  disp_.drawRightAligned(126, 38, HUGE_FONT, gpuTemp.c_str());
  u8g2.setFont(TINY_FONT);
  int vramPct = (hw.gv >= 0 && hw.gv <= 100) ? hw.gv : 0;
  u8g2.drawUTF8(66, 48, (String("VRAM: ") + String(vramPct) + "%").c_str());
  int gpuPwr = (hw.gtdp > 0) ? hw.gtdp : 0;
  disp_.drawRightAligned(126, 48, TINY_FONT, (String(gpuPwr) + "W").c_str());
  int gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
  disp_.drawProgressBar(66, 58, 60, 4, gpuLoad);
}

// ---------------------------------------------------------------------------
// SCREEN 2: CPU DETAIL — Top: HUGE temp "55°". Middle: 4.6GHz | 65W. Bottom:
// graph 14px
// ---------------------------------------------------------------------------
void SceneManager::drawCpuDetail(unsigned long bootTime) {
  HardwareData &hw = state_.hw;
  (void)bootTime;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(HUGE_FONT);
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%d\xC2\xB0", (hw.ct > 0) ? hw.ct : 0);
  int tw = u8g2.getUTF8Width(tempBuf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, 32, tempBuf);

  u8g2.setFont(MID_FONT);
  char buf[32];
  float ghz = (hw.cc > 0) ? (hw.cc / 1000.0f) : 4.6f;
  snprintf(buf, sizeof(buf), "%.1fGHz", ghz);
  u8g2.drawUTF8(2, 44, buf);
  snprintf(buf, sizeof(buf), "%dW", (hw.pw > 0) ? hw.pw : 65);
  disp_.drawRightAligned(128, 44, MID_FONT, buf);

  disp_.drawRollingGraph(0, 50, NOCT_DISP_W, 14, disp_.cpuGraph, 100);
}

// ---------------------------------------------------------------------------
// SCREEN 3: GPU DETAIL — Top: HUGE temp. Middle: 1920MHz | VRAM: 80%. Bottom:
// graph 14px
// ---------------------------------------------------------------------------
void SceneManager::drawGpuDetail(int fanFrame) {
  HardwareData &hw = state_.hw;
  (void)fanFrame;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(HUGE_FONT);
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%d\xC2\xB0", (hw.gt > 0) ? hw.gt : 0);
  int tw = u8g2.getUTF8Width(tempBuf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, 32, tempBuf);

  u8g2.setFont(MID_FONT);
  char buf[32];
  int coreMhz = (hw.gclock > 0) ? hw.gclock : 1920;
  snprintf(buf, sizeof(buf), "%dMHz", coreMhz);
  u8g2.drawUTF8(2, 44, buf);
  int vramPct = (hw.gv >= 0 && hw.gv <= 100) ? hw.gv : 0;
  snprintf(buf, sizeof(buf), "VRAM: %d%%", vramPct);
  disp_.drawRightAligned(128, 44, MID_FONT, buf);

  disp_.drawRollingGraph(0, 50, NOCT_DISP_W, 14, disp_.gpuGraph, 100);
}

// ---------------------------------------------------------------------------
// SCREEN 4: RAM — Large segmented bar; center "16.4 GB" (HUGE); bottom "TOTAL:
// 32 GB" (Tiny)
// ---------------------------------------------------------------------------
void SceneManager::drawRam() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  float pct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;

  float usedGb = hw.ru / 1024.0f;
  float totalGb = (hw.ra > 0) ? (hw.ra / 1024.0f) : 32.0f;
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f GB", usedGb);

  u8g2.setFont(HUGE_FONT);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, 28, buf);

  int barW = 100;
  int barH = 8;
  int barX = (NOCT_DISP_W - barW) / 2;
  int barY = 38;
  disp_.drawProgressBar(barX, barY, barW, barH, (int)(pct + 0.5f));

  u8g2.setFont(TINY_FONT);
  snprintf(buf, sizeof(buf), "TOTAL: %.0f GB", totalGb);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, 58, buf);
}

// ---------------------------------------------------------------------------
// SCREEN 5: DISKS — 2x2 grid. Zones: (0,10), (64,10), (0,38), (64,38).
// Cell: Tiny "C:" top-left, Mid "45°" top-right, progress bar bottom.
// ---------------------------------------------------------------------------
void SceneManager::drawDisks() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const char *labels[] = {"C:", "D:", "E:", "F:"};
  const int cellW = 64;
  const int cellH = 28;
  const int row0 = 10;
  const int row1 = 38;
  const int col0 = 0;
  const int col1 = 64;
  const int barH = 4;
  const int barYOffset = 20;

  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int cx = (i % 2 == 0) ? col0 : col1;
    int cy = (i < 2) ? row0 : row1;
    int load =
        (hw.hdd[i].load >= 0 && hw.hdd[i].load <= 100) ? hw.hdd[i].load : 0;
    int temp = hw.hdd[i].temp;

    u8g2.setFont(TINY_FONT);
    u8g2.drawUTF8(cx + 2, cy + 8, labels[i]);
    u8g2.setFont(MID_FONT);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", temp);
    disp_.drawRightAligned(cx + cellW - 2, cy + 8, MID_FONT, buf);
    disp_.drawProgressBar(cx + 2, cy + barYOffset, cellW - 4, barH, load);
  }
}

// ---------------------------------------------------------------------------
// SCREEN 6: FANS — List: CPU FAN, PUMP, GPU FAN, CASE with RPM; rotating icon
// ---------------------------------------------------------------------------
void SceneManager::drawFanIconSmall(int x, int y, int frame) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int cx = x + 4;
  const int cy = y + 4;
  const int r = 3;
  float angle = (frame % 8) * 3.14159f * 2.0f / 8.0f;
  for (int i = 0; i < 4; i++) {
    float a = angle + i * 3.14159f / 2.0f;
    int x1 = cx + (int)(r * cos(a));
    int y1 = cy + (int)(r * sin(a));
    u8g2.drawLine(cx, cy, x1, y1);
  }
  u8g2.drawPixel(cx, cy);
}

void SceneManager::drawFans(int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const char *names[] = {"CPU FAN:", "PUMP:", "GPU FAN:", "CASE:"};
  const int rowH = 14;
  const int startY = CONTENT_Y + 4;
  const int iconX = 2;

  for (int i = 0; i < NOCT_FAN_COUNT; i++) {
    int y = startY + i * rowH;
    int rpm = (i < NOCT_FAN_COUNT) ? hw.fans[i] : 0;
    bool active = (rpm > 0);
    if (active)
      drawFanIconSmall(iconX, y - 2, fanFrame);
    u8g2.setFont(TINY_FONT);
    u8g2.drawUTF8(iconX + 12, y + 2, names[i]);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d RPM", rpm);
    disp_.drawRightAligned(NOCT_DISP_W - 2, y + 2, TINY_FONT, buf);
  }
}

// ---------------------------------------------------------------------------
// SCREEN 7: WEATHER — Left: 32x32 icon (X=0..32). Right: -12°C (HUGE), MOSCOW
// (Tiny), Snowfall (Tiny)
// ---------------------------------------------------------------------------
void SceneManager::drawAtmos() {
  WeatherData &w = state_.weather;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int ICON_X = 0;
  const int ICON_Y = 16;
  const int RIGHT_X = 45;

  bool hasWeather =
      state_.weatherReceived && (w.temp != 0 || w.desc.length() > 0);
  if (!hasWeather) {
    u8g2.setFont(HUGE_FONT);
    u8g2.drawUTF8(RIGHT_X, 32, "---");
    u8g2.setFont(TINY_FONT);
    u8g2.drawUTF8(RIGHT_X, 44, "NO DATA");
    return;
  }

  drawWeatherIcon32(ICON_X, ICON_Y, w.wmoCode);

  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf),
           "%d"
           "\xC2"
           "\xB0"
           "C",
           w.temp);
  u8g2.setFont(HUGE_FONT);
  u8g2.drawUTF8(RIGHT_X, 32, tempBuf);

  u8g2.setFont(TINY_FONT);
  const char *city = "MOSCOW";
  u8g2.drawUTF8(RIGHT_X, 44, city);
  String desc = w.desc.length() > 0 ? w.desc : "Snowfall";
  if (desc.length() > 12)
    desc = desc.substring(0, 12);
  u8g2.drawUTF8(RIGHT_X, 54, desc.c_str());
}

// ---------------------------------------------------------------------------
// SCREEN 8: MEDIA — Cassette animation (no cover art); scrolling track below
// ---------------------------------------------------------------------------
void SceneManager::drawMedia(bool blinkState) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)blinkState;

  const int CASS_X = 0;
  const int CASS_Y = CONTENT_Y;
  const int CASS_SZ = 44;
  const int TRACK_Y = 56;
  bool playing = (media.mediaStatus == "PLAYING");

  disp_.drawCassetteAnimation(CASS_X, CASS_Y, CASS_SZ, CASS_SZ, playing,
                              (unsigned long)millis());

  u8g2.setFont(TINY_FONT);
  String track = media.track;
  if (track.length() == 0)
    track = media.isIdle ? "IDLE" : (playing ? "PLAY" : "PAUSED");
  int trackW = u8g2.getUTF8Width(track.c_str());
  int maxW = NOCT_DISP_W - 4;
  if (trackW <= maxW) {
    u8g2.drawUTF8(2, TRACK_Y, track.c_str());
  } else {
    int offset = ((int)(millis() / 200) % (trackW + 24)) - 12;
    if (offset > 0)
      offset = 0;
    if (offset + trackW < maxW)
      offset = maxW - trackW;
    u8g2.drawUTF8(2 + offset, TRACK_Y, track.c_str());
  }
}

// ---------------------------------------------------------------------------
// 32x32 weather icon by WMO code
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
  u8g2.setFont(TINY_FONT);
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
  u8g2.setFont(TINY_FONT);
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
  u8g2.setFont(TINY_FONT);
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
  u8g2.setFont(TINY_FONT);
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
  u8g2.setFont(TINY_FONT);
  u8g2.drawStr(2, NOCT_DISP_H / 2 + 10, "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, NOCT_DISP_H / 2 + 8, 3, 3);
  disp_.drawWiFiIcon(NOCT_DISP_W - 14, 2, rssi);
}
