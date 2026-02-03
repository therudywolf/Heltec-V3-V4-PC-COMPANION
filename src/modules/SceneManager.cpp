/*
 * NOCTURNE_OS — SceneManager: 9 screens. 128x64,
 * MAIN/CPU/GPU/RAM/DISKS/MEDIA/FANS/MB/NET. Unified: LABEL_FONT only for
 * content; Y from NOCT_CONTENT_TOP, NOCT_ROW_DY.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <math.h>

#define SPLIT_X 64
#define X(x, off) ((x) + (off))

// 8x8 fan icon, 4 frames (0..3) for spin
static const uint8_t icon_fan_8x8_f0[] = {0x18, 0x3C, 0x3C, 0xFF,
                                          0xFF, 0x3C, 0x3C, 0x18};
static const uint8_t icon_fan_8x8_f1[] = {0x08, 0x0C, 0x7E, 0xFF,
                                          0x7E, 0x0C, 0x08, 0x00};
static const uint8_t icon_fan_8x8_f2[] = {0x00, 0x18, 0x3C, 0xFF,
                                          0xFF, 0x3C, 0x18, 0x00};
static const uint8_t icon_fan_8x8_f3[] = {0x00, 0x08, 0x0C, 0x7E,
                                          0xFF, 0x7E, 0x0C, 0x08};
static const uint8_t *icon_fan_frames[] = {icon_fan_8x8_f0, icon_fan_8x8_f1,
                                           icon_fan_8x8_f2, icon_fan_8x8_f3};

const char *SceneManager::sceneNames_[] = {
    "MAIN", "CPU", "GPU", "RAM", "DISKS", "MEDIA", "FANS", "MB", "WTHR"};

SceneManager::SceneManager(DisplayEngine &disp, AppState &state)
    : disp_(disp), state_(state) {}

const char *SceneManager::getSceneName(int sceneIndex) const {
  if (sceneIndex < 0 || sceneIndex >= NOCT_TOTAL_SCENES)
    return "MAIN";
  return sceneNames_[sceneIndex];
}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame) {
  drawWithOffset(sceneIndex, 0, bootTime, blinkState, fanFrame);
}

void SceneManager::drawWithOffset(int sceneIndex, int xOffset,
                                  unsigned long bootTime, bool blinkState,
                                  int fanFrame) {
  (void)bootTime;
  (void)fanFrame;
  switch (sceneIndex) {
  case NOCT_SCENE_MAIN:
    drawMain(blinkState, xOffset);
    break;
  case NOCT_SCENE_CPU:
    drawCpu(blinkState, xOffset);
    break;
  case NOCT_SCENE_GPU:
    drawGpu(blinkState, xOffset);
    break;
  case NOCT_SCENE_RAM:
    drawRam(blinkState, xOffset);
    break;
  case NOCT_SCENE_DISKS:
    drawDisks(xOffset);
    break;
  case NOCT_SCENE_MEDIA:
    drawPlayer(xOffset);
    break;
  case NOCT_SCENE_FANS:
    drawFans(fanFrame, xOffset);
    break;
  case NOCT_SCENE_MOTHERBOARD:
    drawMotherboard(xOffset);
    break;
  case NOCT_SCENE_WEATHER:
    drawWeather(xOffset);
    break;
  default:
    drawMain(blinkState, xOffset);
    break;
  }
}

// ---------------------------------------------------------------------------
// SCENE 1: MAIN — 3 sections. Left CPU (0..63), Right GPU (64..128), Bottom
// RAM. CPU/GPU: label (Tiny) + value (Big) centered; Y=35 progress bar Load.
// RAM: Y=50.
// ---------------------------------------------------------------------------
#define MAIN_CPU_LEFT_W 64
#define MAIN_CPU_LBL_Y 16
#define MAIN_CPU_VAL_Y 26
#define MAIN_LOAD_BAR_Y 35
#define MAIN_LOAD_BAR_W 50
#define MAIN_LOAD_BAR_H 6
#define MAIN_RAM_LBL_Y 50
#define MAIN_RAM_BAR_Y 52
#define MAIN_RAM_BAR_H 6
#define MAIN_RAM_TEXT_Y 58
#define MAIN_RAM_SEG 10

void SceneManager::drawMain(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  int cpuTemp = (hw.ct > 0) ? hw.ct : 0;
  int gpuTemp = (hw.gt > 0) ? hw.gt : 0;
  int cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
  int gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
  bool onAlertMain =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_MAIN;
  bool blinkCpu = onAlertMain &&
                  (state_.alertMetric == NOCT_ALERT_CT ||
                   state_.alertMetric == NOCT_ALERT_CL) &&
                  blinkState;
  bool blinkGpu = onAlertMain &&
                  (state_.alertMetric == NOCT_ALERT_GT ||
                   state_.alertMetric == NOCT_ALERT_GL) &&
                  blinkState;
  bool blinkRam =
      onAlertMain && state_.alertMetric == NOCT_ALERT_RAM && blinkState;

  static char tbuf[16];
  int half = MAIN_CPU_LEFT_W / 2;

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(half - 6, xOff), MAIN_CPU_LBL_Y, "CPU");
  u8g2.setFont(VALUE_FONT);
  if (!blinkCpu) {
    snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0", cpuTemp);
    int tw = u8g2.getUTF8Width(tbuf);
    u8g2.drawUTF8(X(half - tw / 2, xOff), MAIN_CPU_VAL_Y, tbuf);
  }
  int barLeft = X(half - MAIN_LOAD_BAR_W / 2, xOff);
  if (!blinkCpu)
    disp_.drawProgressBar(barLeft, MAIN_LOAD_BAR_Y, MAIN_LOAD_BAR_W,
                          MAIN_LOAD_BAR_H, cpuLoad);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(64 + half - 6, xOff), MAIN_CPU_LBL_Y, "GPU");
  u8g2.setFont(VALUE_FONT);
  if (!blinkGpu) {
    snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0", gpuTemp);
    int tw = u8g2.getUTF8Width(tbuf);
    u8g2.drawUTF8(X(64 + half - tw / 2, xOff), MAIN_CPU_VAL_Y, tbuf);
  }
  barLeft = X(64 + half - MAIN_LOAD_BAR_W / 2, xOff);
  if (!blinkGpu)
    disp_.drawProgressBar(barLeft, MAIN_LOAD_BAR_Y, MAIN_LOAD_BAR_W,
                          MAIN_LOAD_BAR_H, gpuLoad);

  float ru = hw.ru, ra = hw.ra;
  float raShow = ra > 0 ? ra : 0.0f;
  int pct = (raShow > 0) ? (int)((ru / raShow) * 100.0f) : 0;
  if (pct > 100)
    pct = 100;

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(2, xOff), MAIN_RAM_LBL_Y, "RAM");
  if (!blinkRam) {
    int ramW = NOCT_DISP_W - 4;
    int nSeg = ramW / MAIN_RAM_SEG;
    int filled = (pct * nSeg + 50) / 100;
    if (filled > nSeg)
      filled = nSeg;
    for (int i = 0; i < nSeg; i++) {
      int sx = X(2, xOff) + i * MAIN_RAM_SEG;
      if (i < filled)
        u8g2.drawBox(sx, MAIN_RAM_BAR_Y, MAIN_RAM_SEG - 1, MAIN_RAM_BAR_H);
      else
        u8g2.drawFrame(sx, MAIN_RAM_BAR_Y, MAIN_RAM_SEG - 1, MAIN_RAM_BAR_H);
    }
    snprintf(tbuf, sizeof(tbuf), "%.1f GB", ru);
    u8g2.setFont(LABEL_FONT);
    int tw = u8g2.getUTF8Width(tbuf);
    u8g2.drawUTF8(X(NOCT_DISP_W - 2 - tw, xOff), MAIN_RAM_TEXT_Y, tbuf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 2: CPU — Temp/MHz Row1 Y=24; 10px gap; Load/Pwr Row2 Y=46. Labels:
// Temp, Load, Pwr.
// ---------------------------------------------------------------------------
#define CPU_ROW1_Y 24
#define CPU_ROW2_Y 46
#define CPU_LEFT_ANCHOR 62
#define CPU_RIGHT_ANCHOR 126

void SceneManager::drawCpu(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawDottedVLine(X(SPLIT_X, xOff), NOCT_CONTENT_TOP, NOCT_DISP_H - 1);

  int ct = (hw.ct > 0) ? hw.ct : 0;
  int cc = (hw.cc >= 0) ? hw.cc : 0;
  int cl = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
  int pw = (hw.pw >= 0) ? hw.pw : 0;
  bool onAlertCpu =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_CPU;
  bool blinkTemp =
      onAlertCpu && state_.alertMetric == NOCT_ALERT_CT && blinkState;
  bool blinkLoad =
      onAlertCpu && state_.alertMetric == NOCT_ALERT_CL && blinkState;

  const int leftX = NOCT_CARD_LEFT;
  const int rightX = SPLIT_X + 2;
  static char buf[16];

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(leftX, xOff), CPU_ROW1_Y, "Temp");
  u8g2.drawUTF8(X(rightX, xOff), CPU_ROW1_Y, "MHz");
  if (!blinkTemp) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", ct);
    disp_.drawRightAligned(X(CPU_LEFT_ANCHOR, xOff), CPU_ROW1_Y, VALUE_FONT,
                           buf);
  }
  snprintf(buf, sizeof(buf), "%d", cc);
  disp_.drawRightAligned(X(CPU_RIGHT_ANCHOR, xOff), CPU_ROW1_Y, VALUE_FONT,
                         buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(leftX, xOff), CPU_ROW2_Y, "Load");
  u8g2.drawUTF8(X(rightX, xOff), CPU_ROW2_Y, "Pwr");
  if (!blinkLoad) {
    snprintf(buf, sizeof(buf), "%d%%", cl);
    disp_.drawRightAligned(X(CPU_LEFT_ANCHOR, xOff), CPU_ROW2_Y, VALUE_FONT,
                           buf);
  }
  snprintf(buf, sizeof(buf), "%dW", pw);
  disp_.drawRightAligned(X(CPU_RIGHT_ANCHOR, xOff), CPU_ROW2_Y, VALUE_FONT,
                         buf);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 3: GPU — Row1 Temp/Clock; separator Y=32; Row2 TDP; Row3 VM (VRAM, no
// G).
// ---------------------------------------------------------------------------
#define GPU_ROW1_Y 24
#define GPU_SEP_Y 32
#define GPU_ROW2_Y 38
#define GPU_ROW3_Y 52
#define GPU_LEFT_ANCHOR 62
#define GPU_RIGHT_ANCHOR 126

void SceneManager::drawGpu(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawDottedVLine(X(SPLIT_X, xOff), NOCT_CONTENT_TOP, NOCT_DISP_H - 1);
  disp_.drawDottedHLine(X(NOCT_CARD_LEFT, xOff),
                        X(NOCT_DISP_W - NOCT_CARD_LEFT, xOff), GPU_SEP_Y);

  int gh = (hw.gh > 0) ? hw.gh : 0;
  int gclock = (hw.gclock >= 0) ? hw.gclock : 0;
  int gtdp = (hw.gtdp >= 0) ? hw.gtdp : 0;
  float vu = hw.vu;
  bool onAlertGpu =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_GPU;
  bool blinkHot =
      onAlertGpu && state_.alertMetric == NOCT_ALERT_GT && blinkState;

  const int leftX = NOCT_CARD_LEFT;
  const int rightX = SPLIT_X + 2;
  static char buf[24];

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(leftX, xOff), GPU_ROW1_Y, "Temp");
  u8g2.drawUTF8(X(rightX, xOff), GPU_ROW1_Y, "Clock");
  u8g2.setFont(VALUE_FONT);
  if (!blinkHot) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", gh);
    disp_.drawRightAligned(X(GPU_LEFT_ANCHOR, xOff), GPU_ROW1_Y, VALUE_FONT,
                           buf);
  }
  snprintf(buf, sizeof(buf), "%d", gclock);
  disp_.drawRightAligned(X(GPU_RIGHT_ANCHOR, xOff), GPU_ROW1_Y, VALUE_FONT,
                         buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(leftX, xOff), GPU_ROW2_Y, "TDP");
  snprintf(buf, sizeof(buf), "%dW", gtdp);
  disp_.drawRightAligned(X(GPU_LEFT_ANCHOR, xOff), GPU_ROW2_Y, VALUE_FONT, buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(leftX, xOff), GPU_ROW3_Y, "VM");
  snprintf(buf, sizeof(buf), "%.0f", vu);
  disp_.drawRightAligned(X(GPU_LEFT_ANCHOR, xOff), GPU_ROW3_Y, VALUE_FONT, buf);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 4: RAM — List from Y=18. Header "16/32 GB" at Y=58 (bottom). No graphs.
// ---------------------------------------------------------------------------
#define RAM_LIST_Y 18
#define RAM_ROW_DY 12
#define RAM_NAME_X NOCT_CARD_LEFT
#define RAM_VALUE_ANCHOR (NOCT_DISP_W - 2)
#define RAM_SUMMARY_Y 58
#define RAM_MAX_NAMELEN 8

void SceneManager::drawRam(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  ProcessData &proc = state_.process;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  float ru = hw.ru, ra = hw.ra;
  bool onAlertRam =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_RAM;
  bool blinkRam =
      onAlertRam && state_.alertMetric == NOCT_ALERT_RAM && blinkState;

  char buf[24];

  u8g2.setFont(LABEL_FONT);
  for (int i = 0; i < 2; i++) {
    int y = RAM_LIST_Y + i * RAM_ROW_DY;
    if (y + 10 > RAM_SUMMARY_Y - 4)
      break;
    String name = proc.ramNames[i];
    int mb = proc.ramMb[i];
    if (name.length() > 0) {
      if (name.length() > (unsigned)RAM_MAX_NAMELEN)
        name = name.substring(0, RAM_MAX_NAMELEN);
      u8g2.drawUTF8(X(RAM_NAME_X, xOff), y + 8, name.c_str());
      snprintf(buf, sizeof(buf), "%dM", mb);
      disp_.drawRightAligned(X(RAM_VALUE_ANCHOR, xOff), y + 8, LABEL_FONT, buf);
    }
  }

  if (!blinkRam) {
    snprintf(buf, sizeof(buf), "%.0f/%.0f GB", ru, ra > 0 ? ra : 0.0f);
    u8g2.setFont(LABEL_FONT);
    disp_.drawRightAligned(X(RAM_VALUE_ANCHOR, xOff), RAM_SUMMARY_Y, LABEL_FONT,
                           buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 5: DISKS — 2x2 grid. 64x32 quadrants. Drive letter top-left, temp
// centered (getUTF8Width). No graphs. Big bold numbers only.
// ---------------------------------------------------------------------------
#define DISK_QUAD_W 64
#define DISK_QUAD_H 25
#define DISK_GRID_TOP NOCT_CONTENT_TOP

void SceneManager::drawDisks(int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  static char buf[16];
  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int col = i % 2;
    int row = i / 2;
    int cellLeft = X(col * DISK_QUAD_W + 4, xOff);
    int cellTop = DISK_GRID_TOP + row * DISK_QUAD_H;
    int cellW = DISK_QUAD_W - 8;
    int cellH = DISK_QUAD_H - 2;

    char letter = hw.hdd[i].name[0] ? hw.hdd[i].name[0] : (char)('C' + i);
    int t = hw.hdd[i].temp;

    u8g2.setFont(LABEL_FONT);
    snprintf(buf, sizeof(buf), "%c:", letter);
    u8g2.drawUTF8(cellLeft, cellTop + 6, buf);

    u8g2.setFont(VALUE_FONT);
    if (t > 0)
      snprintf(buf, sizeof(buf), "%d", t);
    else
      snprintf(buf, sizeof(buf), "-");
    int tw = u8g2.getUTF8Width(buf);
    int tx = cellLeft + (cellW - tw) / 2;
    int ty = cellTop + cellH / 2 + 6;
    u8g2.drawUTF8(tx, ty, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 7: FANS — RPM in SECONDARY_FONT. Avoid %% (font may lack %); use "pct"
// or number only.
// ---------------------------------------------------------------------------
#define FAN_ROW_Y0 14
#define FAN_ROW_DY 12
#define FAN_ICON_X 2
#define FAN_TEXT_X 16

void SceneManager::drawFans(int fanFrame, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const char *labels[] = {"CPU", "Pump", "GPU", "Case"};
  int rpms[] = {hw.cf, hw.s1, hw.gf, hw.s2};
  for (int i = 0; i < 4; i++) {
    int rowY = FAN_ROW_Y0 + i * FAN_ROW_DY;
    if (rowY + FAN_ROW_DY > NOCT_DISP_H)
      break;
    if (i > 0)
      disp_.drawDottedHLine(X(NOCT_CARD_LEFT, xOff),
                            X(NOCT_DISP_W - NOCT_CARD_LEFT, xOff), rowY - 1);
    int rpm = (rpms[i] >= 0) ? rpms[i] : 0;
    int pct = (hw.fan_controls[i] >= 0 && hw.fan_controls[i] <= 100)
                  ? hw.fan_controls[i]
                  : 0;
    char buf[28];
    int iconFrame = (rpm > 0) ? (fanFrame % 4) : 0;
    drawFanIconSmall(X(FAN_ICON_X, xOff), rowY, iconFrame);
    u8g2.setFont(LABEL_FONT);
    u8g2.drawUTF8(X(FAN_TEXT_X, xOff), rowY + 6, labels[i]);
    u8g2.setFont(SECONDARY_FONT);
    if (pct > 0)
      snprintf(buf, sizeof(buf), "%d %d pct", rpm, pct);
    else
      snprintf(buf, sizeof(buf), "%d", rpm);
    u8g2.drawUTF8(X(FAN_TEXT_X + 24, xOff), rowY + 8, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 8: MOTHERBOARD — 2 col x 2 row like CPU. SYS, VRM, SOC, CHIP. Clean
// grid.
// ---------------------------------------------------------------------------
#define MB_GRID_TOP 16
#define MB_CELL_DX 64
#define MB_CELL_DY 22
#define MB_LBL_X_OFF 4
#define MB_VAL_ANCHOR_OFF 58

void SceneManager::drawMotherboard(int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const char *labels[] = {"SYS", "VRM", "SOC", "CHIP"};
  int temps[] = {hw.mb_sys, hw.mb_vrm, hw.mb_vsoc, hw.mb_chipset};
  static char buf[16];

  for (int i = 0; i < 4; i++) {
    int col = i % 2;
    int row = i / 2;
    int cx = X(col * MB_CELL_DX + MB_LBL_X_OFF, xOff);
    int cy = MB_GRID_TOP + row * MB_CELL_DY + 4;
    int anchorX = X((col + 1) * MB_CELL_DX - 4, xOff);

    u8g2.setFont(LABEL_FONT);
    u8g2.drawUTF8(cx, cy + 6, labels[i]);
    int t = (temps[i] > 0) ? temps[i] : 0;
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", t);
    u8g2.setFont(VALUE_FONT);
    disp_.drawRightAligned(anchorX, cy + 6, VALUE_FONT, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 9: WEATHER — Icon 32x32 left. Temp HUGE top-right baseline Y=40.
// Desc bottom (CYRILLIC/TINY), no overlap with temp descenders.
// ---------------------------------------------------------------------------
#define WEATHER_ICON_X (NOCT_CARD_LEFT + 4)
#define WEATHER_ICON_Y (NOCT_CONTENT_TOP + 4)
#define WEATHER_TEMP_BASELINE_Y 40
#define WEATHER_DESC_Y 58

void SceneManager::drawWeather(int xOff) {
  WeatherData &weather = state_.weather;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int boxX = X(NOCT_CARD_LEFT, xOff);
  const int boxY = NOCT_CONTENT_TOP;
  const int boxW = NOCT_DISP_W - 2 * NOCT_CARD_LEFT;
  const int boxH = NOCT_DISP_H - boxY - 2;
  disp_.drawChamferBox(boxX, boxY, boxW, boxH, 2);

  if (!state_.weatherReceived && weather.temp == 0 && weather.wmoCode == 0) {
    drawNoDataCross(boxX + 4, boxY + 4, boxW - 8, boxH - 8);
    disp_.drawGreebles();
    return;
  }

  disp_.drawWeatherPrimitive(X(WEATHER_ICON_X, xOff), WEATHER_ICON_Y,
                             weather.wmoCode);

  static char buf[16];
  snprintf(buf, sizeof(buf), "%+d\xC2\xB0", weather.temp);
  u8g2.setFont(HUGE_FONT);
  int tw = u8g2.getUTF8Width(buf);
  if (boxX + boxW - 4 - tw >= X(WEATHER_ICON_X, xOff) + WEATHER_ICON_W)
    u8g2.drawUTF8(boxX + boxW - 4 - tw, WEATHER_TEMP_BASELINE_Y, buf);

  if (weather.desc.length() > 0) {
    u8g2.setFont(LABEL_FONT);
    static char descBuf[24];
    size_t len = weather.desc.length();
    if (len >= sizeof(descBuf))
      len = sizeof(descBuf) - 1;
    strncpy(descBuf, weather.desc.c_str(), len);
    descBuf[len] = '\0';
    int dw = u8g2.getUTF8Width(descBuf);
    if (dw > (int)(boxW - 8) && len > 12)
      descBuf[12] = '\0';
    dw = u8g2.getUTF8Width(descBuf);
    int dx = boxX + (boxW - dw) / 2;
    if (dx + dw < NOCT_DISP_W + xOff)
      u8g2.drawUTF8(dx, WEATHER_DESC_Y, descBuf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 6: PLAYER — Artist/Track block shifted down 4–6px to clear header.
// ---------------------------------------------------------------------------
#define PLAYER_ARTIST_Y 36
#define PLAYER_TRACK_Y 54

void SceneManager::drawPlayer(int xOff) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int boxX = X(NOCT_CARD_LEFT, xOff);
  const int boxY = NOCT_CONTENT_TOP;
  const int boxW = NOCT_DISP_W - 2 * NOCT_CARD_LEFT;
  const int boxH = NOCT_DISP_H - boxY - 2;
  disp_.drawChamferBox(boxX, boxY, boxW, boxH, 2);

  bool playing = (media.mediaStatus == "PLAYING");
  u8g2.setFont(LABEL_FONT);
  const char *status = playing ? "PLAY" : "PAUSED";
  int sw = u8g2.getUTF8Width(status);
  u8g2.drawUTF8(boxX + boxW - sw - 4, boxY + 7, status);

  u8g2.setFont(VALUE_FONT);
  static char artistBuf[64];
  static char trackBuf[64];
  strncpy(artistBuf, media.artist.length() ? media.artist.c_str() : "-",
          sizeof(artistBuf) - 1);
  artistBuf[sizeof(artistBuf) - 1] = '\0';
  strncpy(trackBuf, media.track.length() ? media.track.c_str() : "-",
          sizeof(trackBuf) - 1);
  trackBuf[sizeof(trackBuf) - 1] = '\0';

  int maxW = boxW - 8;
  int aw = u8g2.getUTF8Width(artistBuf);
  int tw = u8g2.getUTF8Width(trackBuf);
  unsigned long t = (unsigned long)millis() / 90;
  int scrollA = (aw > maxW) ? (int)(t % (unsigned long)(aw + 48)) : 0;
  int scrollT = (tw > maxW) ? (int)((t + 60) % (unsigned long)(tw + 48)) : 0;
  u8g2.setClipWindow(boxX + 4, PLAYER_ARTIST_Y - 10, boxX + boxW - 4,
                     PLAYER_TRACK_Y + 12);
  u8g2.drawUTF8(boxX + 4 - scrollA, PLAYER_ARTIST_Y, artistBuf);
  u8g2.drawUTF8(boxX + 4 - scrollT, PLAYER_TRACK_Y, trackBuf);
  u8g2.setMaxClipWindow();
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// Small fan icon (8x8), frame 0..3 for spin when RPM > 0
// ---------------------------------------------------------------------------
void SceneManager::drawFanIconSmall(int x, int y, int frame) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const uint8_t *bits = icon_fan_frames[frame % 4];
  u8g2.drawXBM(x, y, 8, 8, bits);
}

// ---------------------------------------------------------------------------
// 32x32 weather icon by WMO code. Fallback: Cloud or Sun (never blank).
// ---------------------------------------------------------------------------
void SceneManager::drawWeatherIcon32(int x, int y, int wmoCode) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const uint8_t *bits = icon_weather_cloud_32_bits; /* default fallback */
  if (wmoCode == 0)
    bits = icon_weather_sun_32_bits;
  else if (wmoCode >= 1 && wmoCode <= 3)
    bits = icon_weather_sun_32_bits;
  else if (wmoCode >= 4 && wmoCode <= 49)
    bits = icon_weather_cloud_32_bits;
  else if (wmoCode >= 50 && wmoCode <= 67)
    bits = icon_weather_rain_32_bits;
  else if (wmoCode >= 68 && wmoCode <= 70)
    bits = icon_weather_rain_32_bits;
  else if (wmoCode >= 71 && wmoCode <= 77)
    bits = icon_weather_snow_32_bits;
  else if (wmoCode >= 80 && wmoCode <= 82)
    bits = icon_weather_rain_32_bits;
  else if (wmoCode >= 85 && wmoCode <= 86)
    bits = icon_weather_snow_32_bits;
  else if (wmoCode >= 95 && wmoCode <= 99)
    bits = icon_weather_rain_32_bits;
  else
    bits = icon_weather_cloud_32_bits; /* unknown WMO → cloud */
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
  int y0 = NOCT_CONTENT_TOP;
  int dy = NOCT_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  int tw = u8g2.getUTF8Width("SEARCH_MODE");
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, y0 + dy - 2, "SEARCH_MODE");
  u8g2.drawStr(NOCT_CARD_LEFT, y0 + 2 * dy, "Scanning for host...");
  int cx = NOCT_DISP_W / 2;
  int cy = NOCT_DISP_H - 16;
  int r = 14;
  u8g2.drawCircle(cx, cy, r);
  int angle = (scanPhase * 30) % 360;
  float rad = angle * 3.14159f / 180.0f;
  u8g2.drawLine(cx, cy, cx + (int)(r * cos(rad)), cy + (int)(r * sin(rad)));
}

void SceneManager::drawMenu(int menuItem, bool carouselOn, bool screenOff) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int boxX = NOCT_MARGIN * 4;
  const int boxY = NOCT_CONTENT_TOP;
  const int boxW = NOCT_DISP_W - 2 * boxX;
  const int boxH = NOCT_DISP_H - boxY - NOCT_MARGIN;
  const int menuRowDy = NOCT_ROW_DY;
  u8g2.drawFrame(boxX, boxY, boxW, boxH);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawStr(boxX + 6, boxY + 8, "QUICK MENU");
  const char *carouselStr = carouselOn ? "CAROUSEL: ON " : "CAROUSEL: OFF";
  const char *screenStr = screenOff ? "SCREEN: OFF" : "SCREEN: ON ";
  const char *lines[] = {carouselStr, screenStr, "EXIT"};
  int startY = boxY + 8 + NOCT_ROW_DY;
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
  int y0 = NOCT_CONTENT_TOP;
  int dy = NOCT_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  int tw = u8g2.getUTF8Width("NO SIGNAL");
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, y0 + dy - 2, "NO SIGNAL");
  if (!wifiOk)
    u8g2.drawStr(NOCT_CARD_LEFT, y0 + 2 * dy, "WiFi: DISCONNECTED");
  else if (!tcpOk)
    u8g2.drawStr(NOCT_CARD_LEFT, y0 + 2 * dy, "TCP: CONNECTING...");
  else
    u8g2.drawStr(NOCT_CARD_LEFT, y0 + 2 * dy, "WAITING FOR DATA...");
  disp_.drawGreebles();
  if (blinkState)
    u8g2.drawBox(NOCT_DISP_W / 2 + 40, y0 + 2 * dy - 8, 6, 12);
}

void SceneManager::drawConnecting(int rssi, bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)rssi;
  (void)blinkState;
  int y0 = NOCT_CONTENT_TOP;
  int dy = NOCT_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  int tw = u8g2.getUTF8Width("LINKING...");
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, y0 + dy - 2, "LINKING...");
  u8g2.drawStr(NOCT_CARD_LEFT, y0 + 2 * dy, "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, y0 + 2 * dy + 2, 3, 3);
}
