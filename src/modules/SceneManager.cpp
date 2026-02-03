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

// Unified V4 2x2 grid (CPU, GPU, MB): from config.h
#define GRID_ROW1_Y NOCT_GRID_ROW1_Y
#define GRID_ROW2_Y NOCT_GRID_ROW2_Y
#define GRID_COL1_X NOCT_GRID_COL1_X
#define GRID_COL2_X NOCT_GRID_COL2_X
#define GRID_CELL_W 63
#define GRID_CELL_H 22
#define GRID_BRACKET_LEN 3
#define GRID_LBL_INSET 3
#define GRID_BASELINE_Y_OFF 8

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

// 32x32 XBM Weather Icons
static const unsigned char icon_sun_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00,
    0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0x00, 0x02, 0x18, 0x18, 0x40, 0x04, 0x30, 0x0c, 0x20,
    0x08, 0x60, 0x06, 0x10, 0x00, 0xc0, 0x03, 0x00, 0x10, 0x80, 0x01, 0x08,
    0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x70, 0x00, 0x00, 0x0e,
    0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x10, 0x80, 0x01, 0x08,
    0x00, 0xc0, 0x03, 0x00, 0x08, 0x60, 0x06, 0x10, 0x04, 0x30, 0x0c, 0x20,
    0x02, 0x18, 0x18, 0x40, 0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char icon_cloud_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x18, 0x06, 0x00,
    0x00, 0x04, 0x08, 0x00, 0x00, 0x04, 0x10, 0x00, 0x00, 0x02, 0x20, 0x1c,
    0x00, 0x02, 0x40, 0x22, 0x00, 0x01, 0x80, 0x41, 0x80, 0x00, 0x80, 0x80,
    0x40, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x80, 0x20, 0x00, 0x00, 0x80,
    0x20, 0x00, 0x00, 0x80, 0x10, 0x00, 0x00, 0xc0, 0x18, 0x00, 0x00, 0x40,
    0x0c, 0x00, 0x00, 0x40, 0x06, 0x00, 0x00, 0x60, 0x02, 0x00, 0x00, 0x20,
    0x03, 0x00, 0x00, 0x30, 0xfe, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
// SCENE 1: MAIN (Tech-Wolf 2x2 Grid) — CPU/GPU brackets; RAM ChamferBox.
// CPU: 0,CONTENT_TOP,63,32; GPU: 65,CONTENT_TOP,63,32; Lifebar Y+26; RAM:
// 0,50,128,14.
// ---------------------------------------------------------------------------
#define MAIN_CPU_X 0
#define MAIN_CPU_Y NOCT_CONTENT_TOP
#define MAIN_CPU_W 63
#define MAIN_CPU_H 32
#define MAIN_GPU_X 65
#define MAIN_GPU_Y NOCT_CONTENT_TOP
#define MAIN_GPU_W 63
#define MAIN_GPU_H 32
#define MAIN_BRACKET_LEN 4
#define MAIN_LIFEBAR_Y (NOCT_CONTENT_TOP + 26)
#define MAIN_LIFEBAR_H 2
#define MAIN_CPU_LIFEBAR_X 4
#define MAIN_CPU_LIFEBAR_W 55
#define MAIN_GPU_LIFEBAR_X 69
#define MAIN_GPU_LIFEBAR_W 55
#define MAIN_SCENE_RAM_Y 50
#define MAIN_SCENE_RAM_H 14
#define MAIN_SCENE_RAM_CHAMFER 2
#define MAIN_SCENE_RAM_TEXT_Y (MAIN_SCENE_RAM_Y + MAIN_SCENE_RAM_H - 4)

void SceneManager::drawMain(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

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

  /* CPU box: X=0, Y=16, W=63, H=32, bracket corners, value big center */
  disp_.drawTechBrackets(X(MAIN_CPU_X, xOff), MAIN_CPU_Y, MAIN_CPU_W,
                         MAIN_CPU_H, MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(MAIN_CPU_X + 4, xOff), MAIN_CPU_Y + 8, "CPU");
  if (!blinkCpu) {
    u8g2.setFont(VALUE_FONT);
    snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0", cpuTemp);
    int tw = u8g2.getUTF8Width(tbuf);
    int cx = MAIN_CPU_X + MAIN_CPU_W / 2 - tw / 2;
    u8g2.drawUTF8(X(cx, xOff), MAIN_CPU_Y + 22, tbuf);
    /* Lifebar inside bracket at Y=42 */
    int filled = (cpuLoad * MAIN_CPU_LIFEBAR_W + 50) / 100;
    if (filled > 0)
      u8g2.drawBox(X(MAIN_CPU_LIFEBAR_X, xOff), MAIN_LIFEBAR_Y, filled,
                   MAIN_LIFEBAR_H);
  }

  /* GPU box: X=65, Y=16, W=63, H=32 */
  disp_.drawTechBrackets(X(MAIN_GPU_X, xOff), MAIN_GPU_Y, MAIN_GPU_W,
                         MAIN_GPU_H, MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(X(MAIN_GPU_X + 4, xOff), MAIN_GPU_Y + 8, "GPU");
  if (!blinkGpu) {
    u8g2.setFont(VALUE_FONT);
    snprintf(tbuf, sizeof(tbuf), "%d\xC2\xB0", gpuTemp);
    int tw = u8g2.getUTF8Width(tbuf);
    int cx = MAIN_GPU_X + MAIN_GPU_W / 2 - tw / 2;
    u8g2.drawUTF8(X(cx, xOff), MAIN_GPU_Y + 22, tbuf);
    /* Lifebar inside bracket at Y=42 */
    int filled = (gpuLoad * MAIN_GPU_LIFEBAR_W + 50) / 100;
    if (filled > 0)
      u8g2.drawBox(X(MAIN_GPU_LIFEBAR_X, xOff), MAIN_LIFEBAR_Y, filled,
                   MAIN_LIFEBAR_H);
  }

  /* RAM: ChamferBox, text "RAM: 4.2G" left/center, 6× (3x6px) segments right */
  disp_.drawChamferBox(X(0, xOff), MAIN_SCENE_RAM_Y, NOCT_DISP_W,
                       MAIN_SCENE_RAM_H, MAIN_SCENE_RAM_CHAMFER);
  if (!blinkRam) {
    u8g2.setFont(LABEL_FONT);
    snprintf(tbuf, sizeof(tbuf), "RAM: %.1fG", hw.ru);
    const int textX = 4;
    int textW = u8g2.getUTF8Width(tbuf);
    u8g2.drawUTF8(X(textX, xOff), MAIN_SCENE_RAM_TEXT_Y, tbuf);
    const int segW = 23; /* 6 segments × 3px + 5× 1px gap */
    const int segH = 6;
    const int segStartX = textX + textW + 4;
    const int segY = MAIN_SCENE_RAM_Y + (MAIN_SCENE_RAM_H - segH) / 2;
    int ramPct = 0;
    if (hw.ra > 0.0f)
      ramPct = (int)((hw.ru / hw.ra) * 100.0f);
    if (ramPct > 100)
      ramPct = 100;
    disp_.drawSegmentedBar(X(segStartX, xOff), segY, segW, segH, ramPct);
  }

  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// drawGridCell: unified 2x2 cell — bracket (63x22, len 3), label top-left,
// value right-aligned. (x,y) = top-left of cell (use X(col, xOff) for x).
// ---------------------------------------------------------------------------
/* Value sits +6px lower in bracket (heavy/grounded look) */
#define GRID_VALUE_Y_EXTRA 6

void SceneManager::drawGridCell(int x, int y, const char *label,
                                const char *value, int valueYOffset) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  disp_.drawTechBracket(x, y, GRID_CELL_W, GRID_CELL_H, GRID_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(x + GRID_LBL_INSET, y + GRID_BASELINE_Y_OFF, label);
  int valueY = y + GRID_BASELINE_Y_OFF + valueYOffset + GRID_VALUE_Y_EXTRA;
  disp_.drawRightAligned(x + GRID_CELL_W - GRID_LBL_INSET, valueY, VALUE_FONT,
                         value);
}

// ---------------------------------------------------------------------------
// SCENE 2: CPU — 2x2 grid (TEMP | CLOCK / LOAD | PWR). Same layout as GPU/MB.
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

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

  static char buf[16];
  const int VALUE_DOWN = 6;
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", ct);
  drawGridCell(X(GRID_COL1_X, xOff), GRID_ROW1_Y, "TEMP", blinkTemp ? "" : buf,
               VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d", cc);
  drawGridCell(X(GRID_COL2_X, xOff), GRID_ROW1_Y, "CLOCK", buf, VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d%%", cl);
  drawGridCell(X(GRID_COL1_X, xOff), GRID_ROW2_Y, "LOAD", blinkLoad ? "" : buf,
               VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%dW", pw);
  drawGridCell(X(GRID_COL2_X, xOff), GRID_ROW2_Y, "PWR", buf, VALUE_DOWN);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 3: GPU — 2x2 grid (TEMP | CLOCK / LOAD | VRAM). Same layout as CPU/MB.
// VRAM = used only (e.g. 4.2G), no total.
// ---------------------------------------------------------------------------
void SceneManager::drawGpu(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  int gh = (hw.gh > 0) ? hw.gh : 0;
  int gclock = (hw.gclock >= 0) ? hw.gclock : 0;
  int gl = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
  float vu = hw.vu;
  bool onAlertGpu =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_GPU;
  bool blinkHot =
      onAlertGpu && state_.alertMetric == NOCT_ALERT_GT && blinkState;

  static char buf[24];
  const int VALUE_DOWN = 6;
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", gh);
  drawGridCell(X(GRID_COL1_X, xOff), GRID_ROW1_Y, "TEMP", blinkHot ? "" : buf,
               VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d", gclock);
  drawGridCell(X(GRID_COL2_X, xOff), GRID_ROW1_Y, "CLOCK", buf, VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d%%", gl);
  drawGridCell(X(GRID_COL1_X, xOff), GRID_ROW2_Y, "LOAD", buf, VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%.1fG", vu);
  drawGridCell(X(GRID_COL2_X, xOff), GRID_ROW2_Y, "VM", buf, VALUE_DOWN);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 4: RAM (Protocol Alpha Wolf) — Top 2 processes only. Start Y=16, Total
// box Y=50 (up 4px). Process names: VALUE_FONT, truncate 6 chars.
// ---------------------------------------------------------------------------
#define RAM_ROW0_Y 16
#define RAM_ROW1_Y 34
#define RAM_ROW_H 16
#define RAM_ROW_X NOCT_CARD_LEFT
#define RAM_ROW_W (NOCT_DISP_W - 2 * NOCT_CARD_LEFT)
#define RAM_BRACKET_LEN 3
#define RAM_NAME_X (NOCT_CARD_LEFT + 4)
#define RAM_VALUE_ANCHOR (NOCT_DISP_W - 4)
#define RAM_MAX_NAMELEN 6
#define RAM_SUMMARY_Y 50
#define RAM_SUMMARY_H (NOCT_DISP_H - RAM_SUMMARY_Y)
#define RAM_SUMMARY_CHAMFER 2

void SceneManager::drawRam(bool blinkState, int xOff) {
  HardwareData &hw = state_.hw;
  ProcessData &proc = state_.process;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  bool onAlertRam =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_RAM;
  bool blinkRam =
      onAlertRam && state_.alertMetric == NOCT_ALERT_RAM && blinkState;

  static char buf[24];
  static const int rowYs[] = {RAM_ROW0_Y, RAM_ROW1_Y};

  /* Process list: exactly 2 items with drawTechBracket around each */
  for (int i = 0; i < 2; i++) {
    int rowY = rowYs[i];
    int bx = X(RAM_ROW_X, xOff);
    int by = rowY;
    disp_.drawTechBracket(bx, by, RAM_ROW_W, RAM_ROW_H, RAM_BRACKET_LEN);

    int baselineY = rowY + 11; /* Text baseline within bracket (6x12) */
    String name = proc.ramNames[i];
    int mb = proc.ramMb[i];
    if (name.length() > (unsigned)RAM_MAX_NAMELEN)
      name = name.substring(0, RAM_MAX_NAMELEN);
    name.toUpperCase();

    u8g2.setFont(VALUE_FONT);
    const char *nameStr = name.length() > 0 ? name.c_str() : "-";
    u8g2.drawUTF8(X(RAM_NAME_X, xOff), baselineY, nameStr);
    int nameW = u8g2.getUTF8Width(nameStr);

    /* Size value: VALUE_FONT (HelvB10) if it fits, else LABEL_FONT */
    snprintf(buf, sizeof(buf), "%dM", mb);
    u8g2.setFont(VALUE_FONT);
    int valueW = u8g2.getUTF8Width(buf);
    int availW = RAM_VALUE_ANCHOR - RAM_NAME_X - nameW - 8;
    if (valueW <= availW) {
      disp_.drawRightAligned(X(RAM_VALUE_ANCHOR, xOff), baselineY, VALUE_FONT,
                             buf);
    } else {
      u8g2.setFont(LABEL_FONT);
      valueW = u8g2.getUTF8Width(buf);
      disp_.drawRightAligned(X(RAM_VALUE_ANCHOR, xOff), baselineY, LABEL_FONT,
                             buf);
    }

    /* Dots from end of name to value */
    int dotStartX = RAM_NAME_X + nameW + 2;
    int dotEndX = RAM_VALUE_ANCHOR - valueW - 4;
    for (int px = dotStartX; px < dotEndX && px < NOCT_DISP_W; px += 3)
      u8g2.drawPixel(X(px, xOff), baselineY);
  }

  /* Summary bar: shifted UP 4px; "TOTAL: ..." centered vertically in box */
  const int summaryX = X(0, xOff);
  const int summaryY = RAM_SUMMARY_Y;
  const int summaryW = NOCT_DISP_W;
  const int summaryH = RAM_SUMMARY_H;
  disp_.drawChamferBox(summaryX, summaryY, summaryW, summaryH,
                       RAM_SUMMARY_CHAMFER);
  if (!blinkRam) {
    float ru = hw.ru, ra = hw.ra > 0 ? hw.ra : 0.0f;
    snprintf(buf, sizeof(buf), "TOTAL: %.1f / %.0f GB", ru, ra);
    u8g2.setFont(LABEL_FONT);
    /* Center text vertically: font ascent ~7, so baseline = center - 3 */
    int textY = summaryY + (summaryH / 2) + 4;
    u8g2.drawUTF8(X(4, xOff), textY, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 5: DISKS (Tech-Wolf 2x2 Grid) — Lines at X=64, Y=40. Cell: Drive
// letter (Tiny top-left), Temp (Big center). No progress bars.
// ---------------------------------------------------------------------------
#define DISK_GRID_X_SPLIT 64
#define DISK_GRID_Y_SPLIT 40
#define DISK_CELL_W 64
#define DISK_CELL_H 40
#define DISK_LBL_X 4
#define DISK_LBL_Y 4
#define DISK_TEMP_OFFSET 20

void SceneManager::drawDisks(int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  /* Grid lines: vertical at X=64, horizontal at Y=40 */
  disp_.drawDottedVLine(X(DISK_GRID_X_SPLIT, xOff), NOCT_CONTENT_TOP,
                        NOCT_DISP_H - 1);
  disp_.drawDottedHLine(X(0, xOff), X(NOCT_DISP_W - 1, xOff),
                        DISK_GRID_Y_SPLIT);

  static const int qx[] = {0, DISK_GRID_X_SPLIT, 0, DISK_GRID_X_SPLIT};
  static const int qy[] = {NOCT_CONTENT_TOP, NOCT_CONTENT_TOP,
                           DISK_GRID_Y_SPLIT, DISK_GRID_Y_SPLIT};
  static char buf[16];
  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int cellX = X(qx[i], xOff);
    int cellY = qy[i];
    int cellW = DISK_CELL_W;
    int cellH = (i < 2) ? DISK_GRID_Y_SPLIT - NOCT_CONTENT_TOP
                        : NOCT_DISP_H - DISK_GRID_Y_SPLIT;

    char letter = hw.hdd[i].name[0] ? hw.hdd[i].name[0] : (char)('C' + i);
    int t = hw.hdd[i].temp;

    /* Drive letter: Tiny, top-left */
    u8g2.setFont(LABEL_FONT);
    snprintf(buf, sizeof(buf), "%c:", letter);
    u8g2.drawUTF8(cellX + DISK_LBL_X, cellY + DISK_LBL_Y + 6, buf);

    /* Temp: Big, center */
    u8g2.setFont(VALUE_FONT);
    if (t > 0)
      snprintf(buf, sizeof(buf), "%d", t);
    else
      snprintf(buf, sizeof(buf), "-");
    int tw = u8g2.getUTF8Width(buf);
    int tx = cellX + (cellW - tw) / 2;
    int ty = cellY + cellH / 2 + 4;
    u8g2.drawUTF8(tx, ty, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 7: FANS (Tech-Wolf Monospace) — LABEL_FONT (ProFont10) for everything.
// Format: "CPU  [1200] 45%" aligned columns.
// ---------------------------------------------------------------------------
#define FAN_ROW_Y0 NOCT_CONTENT_TOP
#define FAN_ROW_DY 12
#define FAN_NAME_X 2
#define FAN_RPM_X 28
#define FAN_PCT_X 90

void SceneManager::drawFans(int fanFrame, int xOff) {
  (void)fanFrame;
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  /* FORCE LABEL_FONT (ProFont10) for everything */
  u8g2.setFont(LABEL_FONT);
  const char *labels[] = {"CPU", "PUMP", "GPU", "CASE"};
  int rpms[] = {hw.cf, hw.s1, hw.gf, hw.s2};
  static char lineBuf[32];
  for (int i = 0; i < 4; i++) {
    int rowY = FAN_ROW_Y0 + i * FAN_ROW_DY + 6;
    if (rowY + 6 > NOCT_DISP_H)
      break;
    int rpm = (rpms[i] >= 0) ? rpms[i] : 0;
    int pct = (hw.fan_controls[i] >= 0 && hw.fan_controls[i] <= 100)
                  ? hw.fan_controls[i]
                  : -1;
    snprintf(lineBuf, sizeof(lineBuf), "%s  [%d]  ", labels[i], rpm);
    u8g2.drawUTF8(X(FAN_NAME_X, xOff), rowY, lineBuf);
    if (pct >= 0) {
      snprintf(lineBuf, sizeof(lineBuf), "%d%%", pct);
      u8g2.drawUTF8(X(FAN_PCT_X, xOff), rowY, lineBuf);
    } else {
      u8g2.drawUTF8(X(FAN_PCT_X, xOff), rowY, "--");
    }
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 8: MOTHERBOARD — 2x2 grid (SYS | VRM / SOC | CHIP). Same layout as
// CPU/GPU.
// ---------------------------------------------------------------------------
void SceneManager::drawMotherboard(int xOff) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  int sys = (hw.mb_sys > 0) ? hw.mb_sys : 0;
  int vrm = (hw.mb_vrm > 0) ? hw.mb_vrm : 0;
  int soc = (hw.mb_vsoc > 0) ? hw.mb_vsoc : 0;
  int chip = (hw.mb_chipset > 0) ? hw.mb_chipset : 0;
  static char buf[16];
  const int VALUE_DOWN = 6;

  snprintf(buf, sizeof(buf), "%d\xC2\xB0", sys);
  drawGridCell(X(GRID_COL1_X, xOff), GRID_ROW1_Y, "SYS", buf, VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", vrm);
  drawGridCell(X(GRID_COL2_X, xOff), GRID_ROW1_Y, "VRM", buf, VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", soc);
  drawGridCell(X(GRID_COL1_X, xOff), GRID_ROW2_Y, "SOC", buf, VALUE_DOWN);
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", chip);
  drawGridCell(X(GRID_COL2_X, xOff), GRID_ROW2_Y, "CHIP", buf, VALUE_DOWN);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 9: WEATHER — XBM icons left (32x32), temp right (helvB24 Y=52).
// ---------------------------------------------------------------------------
#define WTHR_LEFT_PCT 40
#define WTHR_BOX_H (NOCT_DISP_H - NOCT_CONTENT_TOP - 2)
#define WTHR_ICON_X 8
#define WTHR_ICON_SIZE 32
#define WTHR_TEMP_Y 52

void SceneManager::drawWeather(int xOff) {
  WeatherData &weather = state_.weather;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  const int boxX = X(NOCT_CARD_LEFT, xOff);
  const int boxY = NOCT_CONTENT_TOP; /* Y=17 */
  const int boxW = NOCT_DISP_W - 2 * NOCT_CARD_LEFT;
  const int boxH = WTHR_BOX_H;
  const int leftW = (boxW * WTHR_LEFT_PCT) / 100;
  const int rightX = boxX + leftW;
  const int rightW = boxW - leftW;

  disp_.drawChamferBox(boxX, boxY, boxW, boxH, 2);

  if (!state_.weatherReceived && weather.temp == 0 && weather.wmoCode == 0) {
    drawNoDataCross(boxX + 4, boxY + 4, boxW - 8, boxH - 8);
    disp_.drawGreebles();
    return;
  }

  /* Left: 32x32 XBM icon at boxX+8, boxY+8 */
  const unsigned char *iconBits = icon_cloud_bits;
  if (weather.wmoCode <= 3)
    iconBits = icon_sun_bits;
  else if (weather.wmoCode >= 50)
    iconBits = icon_weather_rain_32_bits;
  else
    iconBits = icon_cloud_bits;
  u8g2.drawXBMP(boxX + WTHR_ICON_X, boxY + WTHR_ICON_X, WTHR_ICON_SIZE,
                WTHR_ICON_SIZE, iconBits);

  /* Right: temp in WEATHER_TEMP_FONT, Y=52 (baseline), right-aligned */
  static char buf[16];
  snprintf(buf, sizeof(buf), "%+d\xC2\xB0", weather.temp);
  u8g2.setFont(WEATHER_TEMP_FONT);
  int tw = u8g2.getUTF8Width(buf);
  int tempX = rightX + rightW - tw - 4;
  u8g2.drawUTF8(tempX, WTHR_TEMP_Y, buf);

  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 6: MEDIA (Protocol Alpha Wolf) — Clip Y=NOCT_CONTENT_TOP(16) to
// NOCT_DISP_H(64). Artist Y=32, Track Y=52.
// ---------------------------------------------------------------------------
#define PLAYER_ARTIST_Y 32
#define PLAYER_TRACK_Y 52

void SceneManager::drawPlayer(int xOff) {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

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
  u8g2.setClipWindow(boxX + 4, NOCT_CONTENT_TOP, boxX + boxW - 4, NOCT_DISP_H);
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

// ---------------------------------------------------------------------------
// Wolf Menu: centered overlay. Items: CAROUSEL: 5s, CAROUSEL: 10s,
// CAROUSEL: OFF, EXIT. Short press = cycle, long press = select/exit.
// ---------------------------------------------------------------------------
void SceneManager::drawMenu(int menuItem, bool carouselOn, int carouselSec) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int boxW = 100;
  const int boxH = 44;
  const int boxX = (NOCT_DISP_W - boxW) / 2;
  const int boxY = (NOCT_DISP_H - boxH) / 2;
  const int menuRowDy = 10;
  u8g2.drawFrame(boxX, boxY, boxW, boxH);
  u8g2.setFont(LABEL_FONT);
  const char *lines[] = {"CAROUSEL: 5s", "CAROUSEL: 10s", "CAROUSEL: OFF",
                         "EXIT"};
  int startY = boxY + 8;
  for (int i = 0; i < 4; i++) {
    int y = startY + i * menuRowDy;
    if (y + 8 > boxY + boxH - 2)
      break;
    if (i == menuItem) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(boxX + 4, y - 6, boxW - 8, 9);
      u8g2.setDrawColor(0);
    }
    u8g2.drawUTF8(boxX + 6, y, lines[i]);
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
