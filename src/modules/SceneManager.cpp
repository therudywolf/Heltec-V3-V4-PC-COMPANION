/*
 * NOCTURNE_OS — SceneManager: 9 screens. 128x64,
 * MAIN/CPU/GPU/RAM/DISKS/MEDIA/FANS/MB/NET. Unified: LABEL_FONT only for
 * content; Y from NOCT_CONTENT_TOP, NOCT_ROW_DY.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include <math.h>

#define SPLIT_X 64
#define CONTENT_Y NOCT_CONTENT_TOP
#define STORAGE_ROW_DY 15
#define DISK_NAME_X 2
#define DISK_BAR_X0 15
#define DISK_BAR_X1 90
#define DISK_VALUE_X 126

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
    "MAIN", "CPU", "GPU", "RAM", "DISKS", "MEDIA", "FANS", "MB", "NET"};

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
    drawCpu(blinkState);
    break;
  case NOCT_SCENE_GPU:
    drawGpu(blinkState);
    break;
  case NOCT_SCENE_RAM:
    drawRam(blinkState);
    break;
  case NOCT_SCENE_DISKS:
    drawDisks();
    break;
  case NOCT_SCENE_MEDIA:
    drawPlayer();
    break;
  case NOCT_SCENE_FANS:
    drawFans(fanFrame);
    break;
  case NOCT_SCENE_MOTHERBOARD:
    drawMotherboard();
    break;
  case NOCT_SCENE_NETWORK:
    drawNetwork();
    break;
  default:
    drawMain(blinkState);
    break;
  }
}

// ---------------------------------------------------------------------------
// SCENE 1: MAIN — CPU/GPU temp+load, RAM/VRAM. Dotted divider, hero centered.
// ---------------------------------------------------------------------------
void SceneManager::drawMain(bool blinkState) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawDottedVLine(SPLIT_X, CONTENT_Y, NOCT_DISP_H - 1);

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
  bool blinkVram =
      onAlertMain && state_.alertMetric == NOCT_ALERT_GV && blinkState;

  const int y0 = NOCT_CARD_TOP;
  const int dy = NOCT_CARD_ROW_DY;
  const int leftCenter = SPLIT_X / 2;
  const int rightCenter = SPLIT_X + (NOCT_DISP_W - SPLIT_X) / 2;

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0, "CPU");
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0, "GPU");

  char buf[24];
  if (!blinkCpu) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", cpuTemp);
    u8g2.setFont(HUGE_FONT);
    int w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(leftCenter - w / 2, y0 + dy + 14, buf);
    u8g2.setFont(LABEL_FONT);
    snprintf(buf, sizeof(buf), "%d%%", cpuLoad);
    w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(leftCenter - w / 2, y0 + dy + 26, buf);
  }
  if (!blinkGpu) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", gpuTemp);
    u8g2.setFont(HUGE_FONT);
    int w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(rightCenter - w / 2, y0 + dy + 14, buf);
    u8g2.setFont(LABEL_FONT);
    snprintf(buf, sizeof(buf), "%d%%", gpuLoad);
    w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(rightCenter - w / 2, y0 + dy + 26, buf);
  }

  u8g2.setFont(LABEL_FONT);
  float ru = hw.ru, ra = hw.ra;
  float vu = hw.vu, vt = hw.vt;
  if (!blinkRam) {
    snprintf(buf, sizeof(buf), "RAM %.1f/%.1f GB", ru, ra > 0 ? ra : 0.0f);
    u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 3 * dy, buf);
  }
  if (!blinkVram) {
    snprintf(buf, sizeof(buf), "VRAM %.1f/%.1f", vu, vt > 0 ? vt : 0.0f);
    disp_.drawRightAligned(SPLIT_X + 62, y0 + 3 * dy, LABEL_FONT, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 2: CPU — Temperature, frequency, load %, watts. Dotted divider, hero
// centered.
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(bool blinkState) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawDottedVLine(SPLIT_X, CONTENT_Y, NOCT_DISP_H - 1);

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

  const int y0 = NOCT_CARD_TOP;
  const int dy = NOCT_CARD_ROW_DY;
  const int leftCenter = SPLIT_X / 2;
  const int rightCenter = SPLIT_X + (NOCT_DISP_W - SPLIT_X) / 2;

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0, "TEMP");
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0, "MHZ");
  char buf[16];
  if (!blinkTemp) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", ct);
    u8g2.setFont(HUGE_FONT);
    int w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(leftCenter - w / 2, y0 + dy + 14, buf);
  }
  snprintf(buf, sizeof(buf), "%d", cc);
  u8g2.setFont(HUGE_FONT);
  int w = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(rightCenter - w / 2, y0 + dy + 14, buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + 2 * dy, "LOAD");
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0 + 2 * dy, "PWR");
  if (!blinkLoad) {
    snprintf(buf, sizeof(buf), "%d%%", cl);
    u8g2.setFont(HUGE_FONT);
    w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(leftCenter - w / 2, y0 + 3 * dy + 14, buf);
  }
  snprintf(buf, sizeof(buf), "%dW", pw);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(SPLIT_X + NOCT_CARD_LEFT, y0 + 3 * dy, buf);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 3: GPU — Temperature, Hot Spot, core MHz, power, load %, VRAM. Dotted
// divider, hero centered.
// ---------------------------------------------------------------------------
void SceneManager::drawGpu(bool blinkState) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  disp_.drawDottedVLine(SPLIT_X, CONTENT_Y, NOCT_DISP_H - 1);

  int gt = (hw.gt > 0) ? hw.gt : 0;
  int gh = (hw.gh > 0) ? hw.gh : 0;
  int gl = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
  int gclock = (hw.gclock >= 0) ? hw.gclock : 0;
  int gtdp = (hw.gtdp >= 0) ? hw.gtdp : 0;
  float vu = hw.vu, vt = hw.vt;
  bool onAlertGpu =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_GPU;
  bool blinkTemp =
      onAlertGpu && state_.alertMetric == NOCT_ALERT_GT && blinkState;
  bool blinkLoad =
      onAlertGpu && state_.alertMetric == NOCT_ALERT_GL && blinkState;

  const int y0 = NOCT_CARD_TOP;
  const int dy = NOCT_CARD_ROW_DY;
  const int left = NOCT_CARD_LEFT;
  const int rightCol = SPLIT_X + left;
  const int leftCenter = SPLIT_X / 2;
  const int rightCenter = SPLIT_X + (NOCT_DISP_W - SPLIT_X) / 2;
  char buf[24];
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(left, y0, "TEMP");
  u8g2.drawUTF8(rightCol, y0, "HOT");
  if (!blinkTemp) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", gt);
    u8g2.setFont(HUGE_FONT);
    int w = u8g2.getUTF8Width(buf);
    u8g2.drawUTF8(leftCenter - w / 2, y0 + dy + 14, buf);
  }
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", gh);
  u8g2.setFont(HUGE_FONT);
  int w = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8(rightCenter - w / 2, y0 + dy + 14, buf);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(left, y0 + 2 * dy, "CORE");
  u8g2.drawUTF8(rightCol, y0 + 2 * dy, "PWR");
  snprintf(buf, sizeof(buf), "%d", gclock);
  u8g2.setFont(SECONDARY_FONT);
  u8g2.drawUTF8(left, y0 + 3 * dy, buf);
  snprintf(buf, sizeof(buf), "%dW", gtdp);
  u8g2.drawUTF8(rightCol, y0 + 3 * dy, buf);

  u8g2.drawUTF8(left, y0 + 4 * dy, "LOAD");
  if (!blinkLoad) {
    snprintf(buf, sizeof(buf), "%d%%", gl);
    u8g2.setFont(SECONDARY_FONT);
    u8g2.drawUTF8(left, y0 + 4 * dy + 8, buf);
  }
  snprintf(buf, sizeof(buf), "VRAM %.1f/%.1f", vu, vt > 0 ? vt : 0.0f);
  u8g2.setFont(LABEL_FONT);
  disp_.drawRightAligned(NOCT_DISP_W - 2, y0 + 4 * dy + 8, LABEL_FONT, buf);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 4: RAM — Used/total GB + top 2 processes. Typography + greebles.
// ---------------------------------------------------------------------------
void SceneManager::drawRam(bool blinkState) {
  HardwareData &hw = state_.hw;
  ProcessData &proc = state_.process;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  float ru = hw.ru, ra = hw.ra;
  bool onAlertRam =
      state_.alertActive && state_.alertTargetScene == NOCT_SCENE_RAM;
  bool blinkRam =
      onAlertRam && state_.alertMetric == NOCT_ALERT_RAM && blinkState;

  const int y0 = NOCT_CARD_TOP;
  const int dy = NOCT_CARD_ROW_DY;
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_CARD_LEFT, y0, "RAM");
  char buf[32];
  if (!blinkRam) {
    snprintf(buf, sizeof(buf), "%.1f / %.1f GB", ru, ra > 0 ? ra : 0.0f);
    u8g2.setFont(SECONDARY_FONT);
    u8g2.drawUTF8(NOCT_CARD_LEFT, y0 + dy, buf);
  }

  u8g2.setFont(LABEL_FONT);
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
      disp_.drawRightAligned(NOCT_DISP_W - 2, y, LABEL_FONT, line);
      if (name.length() > (unsigned)maxNameLen)
        name = name.substring(0, maxNameLen);
      u8g2.drawUTF8(NOCT_CARD_LEFT, y, name.c_str());
    }
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 5: DISKS — Grid: Name (X=2), Segmented bar (15–90), Value right (126).
// 15px row.
// ---------------------------------------------------------------------------
void SceneManager::drawDisks() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  u8g2.setFont(LABEL_FONT);
  const int y0 = NOCT_CARD_TOP;
  char buf[16];
  for (int i = 0; i < NOCT_HDD_COUNT; i++) {
    int y = y0 + i * STORAGE_ROW_DY;
    if (y + 12 > NOCT_DISP_H)
      break;
    char letter = hw.hdd[i].name[0] ? hw.hdd[i].name[0] : (char)('C' + i);
    float used = hw.hdd[i].used_gb >= 0.0f ? hw.hdd[i].used_gb : 0.0f;
    float total = hw.hdd[i].total_gb > 0.0f ? hw.hdd[i].total_gb : 0.0f;
    int pct = (total > 0.0f) ? (int)((used / total) * 100.0f) : 0;
    if (pct > 100)
      pct = 100;

    snprintf(buf, sizeof(buf), "%c:", letter);
    u8g2.drawUTF8(DISK_NAME_X, y + 8, buf);

    disp_.drawSegmentedBar(DISK_BAR_X0, y + 4, DISK_BAR_X1 - DISK_BAR_X0, 8,
                           pct);

    snprintf(buf, sizeof(buf), "%d%%", pct);
    disp_.drawRightAligned(DISK_VALUE_X, y + 8, LABEL_FONT, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 7: FANS — Table with dotted separators, small spinning fan icon when
// RPM > 0.
// ---------------------------------------------------------------------------
void SceneManager::drawFans(int fanFrame) {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int y0 = NOCT_CARD_TOP;
  const int rowH = 13;
  const char *labels[] = {"CPU", "Pump", "GPU", "Case"};
  int rpms[] = {hw.cf, hw.s1, hw.gf, hw.s2};
  u8g2.setFont(LABEL_FONT);
  for (int i = 0; i < 4; i++) {
    int y = y0 + i * rowH + 8;
    if (i > 0)
      disp_.drawDottedHLine(NOCT_CARD_LEFT, NOCT_DISP_W - NOCT_CARD_LEFT,
                            y0 + i * rowH - 1);
    int rpm = (rpms[i] >= 0) ? rpms[i] : 0;
    int pct = (hw.fan_controls[i] >= 0 && hw.fan_controls[i] <= 100)
                  ? hw.fan_controls[i]
                  : 0;
    char buf[24];
    if (pct > 0)
      snprintf(buf, sizeof(buf), "%s %d (%d%%)", labels[i], rpm, pct);
    else
      snprintf(buf, sizeof(buf), "%s %d", labels[i], rpm);
    int iconFrame = (rpm > 0) ? (fanFrame % 4) : 0;
    drawFanIconSmall(NOCT_CARD_LEFT, y - 7, iconFrame);
    u8g2.drawUTF8(NOCT_CARD_LEFT + 11, y, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 8: MOTHERBOARD — System, VSoC MOS, VRM MOS, Chipset. Dotted separators,
// greebles.
// ---------------------------------------------------------------------------
void SceneManager::drawMotherboard() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int y0 = NOCT_CARD_TOP;
  const int dy = NOCT_CARD_ROW_DY;
  const char *labels[] = {"System", "VSoC MOS", "VRM MOS", "Chipset"};
  int temps[] = {hw.mb_sys, hw.mb_vsoc, hw.mb_vrm, hw.mb_chipset};
  for (int i = 0; i < 4; i++) {
    int y = y0 + i * dy + 8;
    if (i > 0)
      disp_.drawDottedHLine(NOCT_CARD_LEFT, NOCT_DISP_W - NOCT_CARD_LEFT,
                            y - 6);
    u8g2.setFont(LABEL_FONT);
    u8g2.drawUTF8(NOCT_CARD_LEFT, y - 2, labels[i]);
    u8g2.setFont(SECONDARY_FONT);
    char buf[16];
    int t = (temps[i] > 0) ? temps[i] : 0;
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", t);
    disp_.drawRightAligned(NOCT_DISP_W - 2, y - 2, SECONDARY_FONT, buf);
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 9: NETWORK — Upload/download (KB/s or MB/s). Chamfer box, greebles.
// ---------------------------------------------------------------------------
void SceneManager::drawNetwork() {
  HardwareData &hw = state_.hw;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int boxX = NOCT_CARD_LEFT;
  const int boxY = NOCT_CARD_TOP;
  const int boxW = NOCT_DISP_W - 2 * NOCT_CARD_LEFT;
  const int boxH = 38;
  disp_.drawChamferBox(boxX, boxY, boxW, boxH, 3);
  disp_.drawDottedHLine(boxX + 4, boxX + boxW - 4, boxY + boxH / 2);

  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(boxX + 4, boxY + 8, "UP");
  u8g2.drawUTF8(boxX + 4, boxY + boxH / 2 + 10, "DOWN");
  char buf[24];
  int up_kb = (hw.nu >= 0) ? hw.nu : 0;
  int down_kb = (hw.nd >= 0) ? hw.nd : 0;
  if (up_kb >= 1024) {
    float up_mb = up_kb / 1024.0f;
    snprintf(buf, sizeof(buf), "%.1f MB/s", up_mb);
  } else {
    snprintf(buf, sizeof(buf), "%d KB/s", up_kb);
  }
  u8g2.setFont(SECONDARY_FONT);
  disp_.drawRightAligned(boxX + boxW - 4, boxY + 8, SECONDARY_FONT, buf);
  if (down_kb >= 1024) {
    float down_mb = down_kb / 1024.0f;
    snprintf(buf, sizeof(buf), "%.1f MB/s", down_mb);
  } else {
    snprintf(buf, sizeof(buf), "%d KB/s", down_kb);
  }
  disp_.drawRightAligned(boxX + boxW - 4, boxY + boxH / 2 + 10, SECONDARY_FONT,
                         buf);
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 6: PLAYER — Cassette container, status, artist/track scroll inside box.
// ---------------------------------------------------------------------------
void SceneManager::drawPlayer() {
  MediaData &media = state_.media;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();

  const int boxX = NOCT_CARD_LEFT;
  const int boxY = NOCT_CARD_TOP + 2;
  const int boxW = NOCT_DISP_W - 2 * NOCT_CARD_LEFT;
  const int boxH = 44;
  disp_.drawChamferBox(boxX, boxY, boxW, boxH, 3);

  bool playing = (media.mediaStatus == "PLAYING");
  disp_.drawCassetteAnimation(boxX + boxW / 2 - 20, boxY + 2, 40, 18, playing,
                              (unsigned long)millis());

  u8g2.setFont(LABEL_FONT);
  const char *status = playing ? "PLAYING" : "PAUSED";
  int sw = u8g2.getUTF8Width(status);
  u8g2.drawUTF8(boxX + boxW - sw - 2, boxY + 2, status);

  int textY = boxY + 22;
  int maxW = boxW - 8;
  String artist = media.artist;
  String track = media.track;
  if (artist.length() == 0)
    artist = " ";
  if (track.length() == 0)
    track = " ";
  u8g2.setFont(SECONDARY_FONT);
  int aw = u8g2.getUTF8Width(artist.c_str());
  int tw = u8g2.getUTF8Width(track.c_str());
  unsigned long t = (unsigned long)millis() / 40;
  int scrollA = (aw > maxW) ? (int)(t % (unsigned long)(aw + 32)) : 0;
  int scrollT = (tw > maxW) ? (int)((t + 50) % (unsigned long)(tw + 32)) : 0;
  u8g2.setClipWindow(boxX + 4, textY - 8, boxX + boxW - 4, textY + 20);
  u8g2.drawUTF8(boxX + 4 - scrollA, textY - 2, artist.c_str());
  u8g2.drawUTF8(boxX + 4 - scrollT, textY + 10, track.c_str());
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
  drawNoisePattern(0, NOCT_CONTENT_TOP, NOCT_DISP_W,
                   NOCT_DISP_H - NOCT_CONTENT_TOP);
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
