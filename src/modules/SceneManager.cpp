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

const char *SceneManager::sceneNames_[] = {"MAIN",  "CPU",   "GPU",  "RAM",
                                           "DISKS", "MEDIA", "FANS", "MB",
                                           "WTHR",  "FIRE",  "HUNT"};

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
  case NOCT_SCENE_FIRE:
    drawFire(xOffset);
    break;
  case NOCT_SCENE_HUNT:
    drawWolfHunt(xOffset);
    break;
  default:
    drawMain(blinkState, xOffset);
    break;
  }
}

// ---------------------------------------------------------------------------
// SCENE 1: MAIN — Predator dashboard. Tech brackets CPU/GPU, segmented bar
// Y=44, chamfer RAM box. Hex stream corners, cyber claw watermark.
// ---------------------------------------------------------------------------
#define MAIN_BRACKET_TOP 14
#define MAIN_BRACKET_H 28
#define MAIN_CPU_BRACKET_X 0
#define MAIN_CPU_BRACKET_W 63
#define MAIN_GPU_BRACKET_X 65
#define MAIN_GPU_BRACKET_W 63
#define MAIN_BRACKET_LEN 4
#define MAIN_BAR_Y 44
#define MAIN_BAR_H 6
#define MAIN_CPU_BAR_X 4
#define MAIN_CPU_BAR_W 55
#define MAIN_GPU_BAR_X 69
#define MAIN_GPU_BAR_W 55
#define MAIN_RAM_BOX_Y MAIN_RAM_BOX_TOP
#define MAIN_RAM_BOX_CHAMFER 3
#define MAIN_RAM_H 15

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

  disp_.drawCyberClaw(X(100, xOff), 16);

  disp_.drawTechBracket(X(MAIN_CPU_BRACKET_X, xOff), MAIN_BRACKET_TOP,
                        MAIN_CPU_BRACKET_W, MAIN_BRACKET_H, MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(MAIN_CPU_BRACKET_X + 4, xOff), MAIN_BRACKET_TOP + 7, "CPU");
  if (!blinkCpu) {
    u8g2.setFont(VALUE_FONT);
    snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0", cpuTemp);
    int tw = u8g2.getUTF8Width(tbuf);
    int cx = MAIN_CPU_BRACKET_X + MAIN_CPU_BRACKET_W / 2 - tw / 2;
    u8g2.drawUTF8(X(cx, xOff), MAIN_BRACKET_TOP + 22, tbuf);
  }

  disp_.drawTechBracket(X(MAIN_GPU_BRACKET_X, xOff), MAIN_BRACKET_TOP,
                        MAIN_GPU_BRACKET_W, MAIN_BRACKET_H, MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(MAIN_GPU_BRACKET_X + 4, xOff), MAIN_BRACKET_TOP + 7, "GPU");
  if (!blinkGpu) {
    u8g2.setFont(VALUE_FONT);
    snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0", gpuTemp);
    int tw = u8g2.getUTF8Width(tbuf);
    int cx = MAIN_GPU_BRACKET_X + MAIN_GPU_BRACKET_W / 2 - tw / 2;
    u8g2.drawUTF8(X(cx, xOff), MAIN_BRACKET_TOP + 22, tbuf);
  }

  if (!blinkCpu)
    disp_.drawProgressBar(X(MAIN_CPU_BAR_X, xOff), MAIN_BAR_Y, MAIN_CPU_BAR_W,
                          MAIN_BAR_H, cpuLoad);
  if (!blinkGpu)
    disp_.drawProgressBar(X(MAIN_GPU_BAR_X, xOff), MAIN_BAR_Y, MAIN_GPU_BAR_W,
                          MAIN_BAR_H, gpuLoad);

  disp_.drawChamferBox(X(0, xOff), MAIN_RAM_BOX_Y, NOCT_DISP_W, MAIN_RAM_H,
                       MAIN_RAM_BOX_CHAMFER);
  float ru = hw.ru, ra = hw.ra;
  float raShow = ra > 0 ? ra : 0.0f;
  int pct = (raShow > 0) ? (int)((ru / raShow) * 100.0f) : 0;
  if (pct > 100)
    pct = 100;
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(4, xOff), MAIN_RAM_TEXT_Y, "RAM:");
  int barX = X(28, xOff);
  disp_.drawProgressBar(barX, MAIN_RAM_BOX_Y + 4, 72, 6, pct);
  if (!blinkRam) {
    snprintf(tbuf, sizeof(tbuf), "%.1fG", ru);
    u8g2.drawUTF8(barX + 76, MAIN_RAM_TEXT_Y, tbuf);
  }

  disp_.drawHexStream(X(0, xOff), NOCT_CONTENT_TOP, 2);
  disp_.drawHexStream(X(NOCT_DISP_W - 54, xOff), NOCT_CONTENT_TOP, 2);

  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 2: CPU — Row1 Temp/MHz Y=28; Dotted HLine Y=38; Row2 Load/Pwr Y=54.
// drawRightAligned for values.
// ---------------------------------------------------------------------------
#define CPU_ROW1_Y 28
#define CPU_SEP_Y 38
#define CPU_ROW2_Y 54
#define CPU_LEFT_ANCHOR 62
#define CPU_RIGHT_ANCHOR 126

void SceneManager::drawCpu(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawDottedVLine(X(SPLIT_X, xOff), NOCT_CONTENT_TOP, NOCT_DISP_H - 1);
  disp_.drawDottedHLine(X(NOCT_CARD_LEFT, xOff),
                        X(NOCT_DISP_W - NOCT_CARD_LEFT, xOff), CPU_SEP_Y);

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
  u8g2.drawUTF8(X(leftX, xOff), CPU_ROW2_Y, "ACT");
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
// SCENE 3: GPU — Simplified. Row1 Temp/Clock Y=28; Dotted HLine Y=38;
// Row2 TDP/VRAM Y=54. Left: TDP/120W; Right: VM/4.2 (no G suffix).
// ---------------------------------------------------------------------------
#define GPU_ROW1_Y 28
#define GPU_SEP_Y 38
#define GPU_ROW2_Y 54
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
  u8g2.drawUTF8(X(rightX, xOff), GPU_ROW2_Y, "VM");
  snprintf(buf, sizeof(buf), "%dW", gtdp);
  disp_.drawRightAligned(X(GPU_LEFT_ANCHOR, xOff), GPU_ROW2_Y, VALUE_FONT, buf);
  snprintf(buf, sizeof(buf), "%.1f", vu);
  disp_.drawRightAligned(X(GPU_RIGHT_ANCHOR, xOff), GPU_ROW2_Y, VALUE_FONT,
                         buf);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 4: RAM — List Y=20 (Row0), Y=34 (Row1). Summary Y=58. Text only, no
// graph.
// ---------------------------------------------------------------------------
#define RAM_ROW0_Y 20
#define RAM_ROW1_Y 34
#define RAM_SUMMARY_Y 58
#define RAM_NAME_X NOCT_CARD_LEFT
#define RAM_VALUE_ANCHOR (NOCT_DISP_W - 2)
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
  int rowY[] = {RAM_ROW0_Y, RAM_ROW1_Y};

  u8g2.setFont(LABEL_FONT);
  for (int i = 0; i < 2; i++) {
    int y = rowY[i];
    String name = proc.ramNames[i];
    int mb = proc.ramMb[i];
    if (name.length() > 0) {
      if (name.length() > (unsigned)RAM_MAX_NAMELEN)
        name = name.substring(0, RAM_MAX_NAMELEN);
      u8g2.drawUTF8(X(RAM_NAME_X, xOff), y, name.c_str());
      snprintf(buf, sizeof(buf), "%dM", mb);
      disp_.drawRightAligned(X(RAM_VALUE_ANCHOR, xOff), y, LABEL_FONT, buf);
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
// SCENE 5: DISKS — "DRIVE" header, 2x2 grid. Quadrants: 0:(0,14), 1:(64,14),
// 2:(0,39), 3:(64,39). Drive letter + temp. NO BARS.
// ---------------------------------------------------------------------------
#define DISK_QUAD_W 64
#define DISK_QUAD_H 25
#define DISK_Q0_X 0
#define DISK_Q0_Y 14
#define DISK_Q1_X 64
#define DISK_Q1_Y 14
#define DISK_Q2_X 0
#define DISK_Q2_Y 39
#define DISK_Q3_X 64
#define DISK_Q3_Y 39
#define DISK_HEADER_Y (NOCT_CONTENT_TOP + 2)

void SceneManager::drawDisks(int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(NOCT_CARD_LEFT, xOff), DISK_HEADER_Y, "DRIVE");

  static const int qx[] = {DISK_Q0_X, DISK_Q1_X, DISK_Q2_X, DISK_Q3_X};
  static const int qy[] = {DISK_Q0_Y, DISK_Q1_Y, DISK_Q2_Y, DISK_Q3_Y};
  static char buf[16];
  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int cellX = X(qx[i], xOff);
    int cellY = qy[i];
    int cellW = DISK_QUAD_W;
    int cellH = (i < 2) ? (qy[2] - qy[0]) : (NOCT_DISP_H - qy[2]);

    char letter = hw.hdd[i].name[0] ? hw.hdd[i].name[0] : (char)('C' + i);
    int t = hw.hdd[i].temp;

    u8g2.setFont(LABEL_FONT);
    snprintf(buf, sizeof(buf), "%c:", letter);
    u8g2.drawUTF8(cellX + 4, cellY + 10, buf);

    u8g2.setFont(VALUE_FONT);
    if (t > 0)
      snprintf(buf, sizeof(buf), "%d", t);
    else
      snprintf(buf, sizeof(buf), "-");
    int tw = u8g2.getUTF8Width(buf);
    int tx = cellX + (cellW - tw) / 2;
    int ty = cellY + cellH / 2;
    u8g2.drawUTF8(tx, ty, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 7: FANS — Monospaced profont10. Col1 Icon X=2, Name X=14, RPM X=60
// [ 1500 ], % X=110 "45%".
// ---------------------------------------------------------------------------
#define FAN_ROW_Y0 14
#define FAN_ROW_DY 12
#define FAN_ICON_X 2
#define FAN_NAME_X 14
#define FAN_RPM_X 60
#define FAN_PCT_X 110

void SceneManager::drawFans(int fanFrame, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(LABEL_FONT); /* u8g2_font_profont10_mr */
  const char *labels[] = {"CPU", "Pump", "GPU", "Case"};
  int rpms[] = {hw.cf, hw.s1, hw.gf, hw.s2};
  static char rpmBuf[16];
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
    int iconFrame = (rpm > 0) ? (fanFrame % 4) : 0;
    drawFanIconSmall(X(FAN_ICON_X, xOff), rowY, iconFrame);
    u8g2.drawUTF8(X(FAN_NAME_X, xOff), rowY + 6, labels[i]);
    snprintf(rpmBuf, sizeof(rpmBuf), "[ %d ]", rpm);
    u8g2.drawUTF8(X(FAN_RPM_X, xOff), rowY + 6, rpmBuf);
    snprintf(rpmBuf, sizeof(rpmBuf), "%d%%", pct);
    u8g2.drawUTF8(X(FAN_PCT_X, xOff), rowY + 6, rpmBuf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 8: MOTHERBOARD — 2 col x 2 row. Grid Y=18..63 (breathing room).
// ---------------------------------------------------------------------------
#define MB_GRID_TOP 18
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
// SCENE 9: WEATHER — Geometric fallback (sun=disc, cloud=box). helvB10 temp
// baseline Y >= 30 (no header overlap).
// ---------------------------------------------------------------------------
#define WEATHER_ICON_X (NOCT_CARD_LEFT + 4)
#define WEATHER_ICON_Y (NOCT_CONTENT_TOP + 4)
#define WEATHER_TEMP_BASELINE_Y 38
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

  /* Geometric fallback: drawWeatherPrimitive uses drawDisc (sun), drawBox
   * (cloud) when icons fail */
  disp_.drawWeatherPrimitive(X(WEATHER_ICON_X, xOff), WEATHER_ICON_Y,
                             weather.wmoCode);

  static char buf[16];
  snprintf(buf, sizeof(buf), "%+d\xC2\xB0", weather.temp);
  u8g2.setFont(VALUE_FONT); /* helvB10, baseline >= 30 */
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
// SCENE 10: FIRE — Doom-style fire effect. fire[32], propagate heat upward,
// draw vertical bars; overlay "IGNITION" / "BURNOUT".
// ---------------------------------------------------------------------------
#define FIRE_W 32
#define FIRE_BOTTOM_Y 64
#define FIRE_COOLING 2

void SceneManager::drawFire(int xOff) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  static uint8_t fire[FIRE_W];
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < FIRE_W; i++)
      fire[i] = 0;
    initialized = true;
  }
  /* Ignite bottom row randomly */
  for (int x = 0; x < FIRE_W; x++)
    if (random(3) == 0)
      fire[x] = (uint8_t)((fire[x] + random(200, 256)) / 2);
  /* Propagate heat (smooth + cool); use temp buffer */
  uint8_t next[FIRE_W];
  for (int x = 0; x < FIRE_W; x++)
    next[x] = fire[x];
  for (int x = 1; x < FIRE_W - 1; x++) {
    int v = (int)next[x] + (int)next[x - 1] + (int)next[x + 1];
    v = v / 3 - FIRE_COOLING;
    fire[x] = (v < 0) ? 0 : (uint8_t)v;
  }
  /* Render: vertical lines at bottom. Height = fire[x]/4, scaled to screen */
  const int barW = 4;
  const int baseY = NOCT_DISP_H - 1;
  for (int x = 0; x < FIRE_W; x++) {
    int h = fire[x] / 4;
    if (h > NOCT_DISP_H - NOCT_HEADER_H)
      h = NOCT_DISP_H - NOCT_HEADER_H;
    if (h > 0) {
      int px = X(x * barW, xOff);
      if (px + barW > 0 && px < NOCT_DISP_W)
        u8g2.drawBox(px, baseY - h, barW, h);
    }
  }
  const char *label = (millis() / 2000) & 1 ? "BURNOUT" : "IGNITION";
  u8g2.setFont(VALUE_FONT);
  int tw = u8g2.getUTF8Width(label);
  int tx = X((NOCT_DISP_W - tw) / 2, xOff);
  int ty = NOCT_CONTENT_TOP + 12;
  if (tx >= X(0, xOff) && tx + tw <= X(NOCT_DISP_W, xOff))
    u8g2.drawUTF8(tx, ty, label);
}

// ---------------------------------------------------------------------------
// SCENE 11: HUNT — Pixel wolf chases pixel rabbit. "TARGET LOCK" blinking.
// ---------------------------------------------------------------------------
void SceneManager::drawWolfHunt(int xOff) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  static int wolfX = 0, wolfY = 32;
  static int rabbitX = 100, rabbitY = 32;
  int dx = rabbitX - wolfX;
  int dy = rabbitY - wolfY;
  int dist = (dx == 0 && dy == 0) ? 0 : (int)sqrt((double)(dx * dx + dy * dy));
  if (dist < 5) {
    rabbitX = 20 + random(80);
    rabbitY = NOCT_HEADER_H + 8 + random(40);
  } else if (dist > 0) {
    wolfX += (dx * 2) / dist;
    wolfY += (dy * 2) / dist;
    if (wolfX < 0)
      wolfX = 0;
    if (wolfX >= NOCT_DISP_W)
      wolfX = NOCT_DISP_W - 1;
    if (wolfY < NOCT_HEADER_H)
      wolfY = NOCT_HEADER_H;
    if (wolfY >= NOCT_DISP_H)
      wolfY = NOCT_DISP_H - 1;
  }
  /* Wolf: two triangles (ears) + square (head) */
  int wx = X(wolfX, xOff);
  int wy = wolfY;
  u8g2.drawLine(wx, wy - 4, wx - 2, wy);
  u8g2.drawLine(wx - 2, wy, wx, wy - 2);
  u8g2.drawLine(wx + 4, wy - 4, wx + 6, wy);
  u8g2.drawLine(wx + 6, wy, wx + 4, wy - 2);
  u8g2.drawBox(wx, wy - 2, 4, 4);
  /* Rabbit: flashing cross/dot */
  int rx = X(rabbitX, xOff);
  int ry = rabbitY;
  bool flash = (millis() / 150) & 1;
  if (flash) {
    u8g2.drawPixel(rx, ry);
    u8g2.drawPixel(rx + 1, ry);
    u8g2.drawPixel(rx, ry + 1);
    u8g2.drawPixel(rx - 1, ry);
    u8g2.drawPixel(rx, ry - 1);
  } else {
    u8g2.drawBox(rx - 1, ry - 1, 3, 3);
  }
  u8g2.setFont(LABEL_FONT);
  if ((millis() / 400) & 1)
    u8g2.drawUTF8(X(2, xOff), NOCT_DISP_H - 4, "TARGET LOCK");
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
