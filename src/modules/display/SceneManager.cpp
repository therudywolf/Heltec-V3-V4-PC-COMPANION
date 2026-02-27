/*
 * NOCTURNE_OS — SceneManager: 9 screens. 128x64,
 * MAIN/CPU/GPU/RAM/DISKS/MEDIA/FANS/MB/NET. Unified: LABEL_FONT only for
 * content; Y from NOCT_CONTENT_TOP, NOCT_ROW_DY.
 */
#include "SceneManager.h"
#include "nocturne/config.h"
#include "BmwManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

// ===========================================================================
// ASSETS: 32x32 PIXEL PERFECT WOLF (High contrast for OLED)
// ===========================================================================

// 32x32 PIXEL PERFECT WOLF (Left Profile)
static const unsigned char wolf_head_side[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x03, 0x00, 0x00, 0xE0, 0x07, 0x00,
    0x00, 0xF0, 0x0F, 0x00, 0x00, 0xF8, 0x1F, 0x00, 0x00, 0xFC, 0x3F, 0x00,
    0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFF, 0x7F, 0x00, 0x80, 0xFF, 0xFF, 0x00,
    0xC0, 0xFF, 0xFF, 0x01, 0xE0, 0xFF, 0xFF, 0x03, 0xF0, 0xFF, 0xFF, 0x03,
    0xF8, 0xCF, 0xF3, 0x07, 0xFC, 0x87, 0xE1, 0x07, 0xFE, 0x03, 0xC0, 0x0F,
    0x00, 0xFF, 0x87, 0xFF, 0x03, 0x00, 0x00, 0xFF, 0x03, 0xFF, 0x07, 0x00,
    0x80, 0xFF, 0x01, 0xFE, 0x0F, 0x00, 0xC0, 0xFF, 0x00, 0xFC, 0x0F, 0x00,
    0xE0, 0x7F, 0x00, 0xF8, 0x1F, 0x00, 0xF0, 0x3F, 0x00, 0xF8, 0x1F, 0x00,
    0xF8, 0x1F, 0x00, 0xF0, 0x3F, 0x00, 0xFC, 0x0F, 0x00, 0xE0, 0x3F, 0x00,
    0xFE, 0x07, 0x00, 0xC0, 0x7F, 0x00, 0xFF, 0x03, 0x00, 0x80, 0x7F, 0x00,
    0xFF, 0x01, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x01,
    0x7F, 0x00, 0x00, 0x00, 0xFE, 0x01, 0x3F, 0x00, 0x00, 0x00, 0xFC, 0x03,
    0x1F, 0x00, 0x00, 0x00, 0xF8, 0x03, 0x0F, 0x00, 0x00, 0x00, 0xF0, 0x07,
    0x0F, 0x00, 0x00, 0x00, 0xE0, 0x07, 0x07, 0x00, 0x00, 0x00, 0xC0, 0x0F,
    0x07, 0x00, 0x00, 0x00, 0x80, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x1F,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 32x32 ALERT (Teeth Bared)
static const unsigned char wolf_head_growl[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00,
    0x00, 0x00, 0xF8, 0x07, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x1F, 0x00, 0x00,
    0x00, 0x80, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0xC0, 0xFF, 0x7F, 0x00, 0x00,
    0x00, 0xE0, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0xFF, 0x01, 0x00,
    0x00, 0xF8, 0xC7, 0xFF, 0x03, 0x00, 0x00, 0xFC, 0x03, 0xFF, 0x07, 0x00,
    0x00, 0xFE, 0x01, 0xFE, 0x0F, 0x00, 0x00, 0xFF, 0x00, 0xFC, 0x0F, 0x00,
    0x80, 0x7F, 0x00, 0xF8, 0x1F, 0x00, 0xC0, 0x3F, 0x00, 0xF0, 0x3F, 0x00,
    0xE0, 0x1F, 0x00, 0xE0, 0x3F, 0x00, 0xF0, 0x0F, 0x00, 0xC0, 0x7F, 0x00,
    0xF8, 0x07, 0x00, 0x80, 0xFF, 0x00, 0xFC, 0x03, 0x00, 0x00, 0xFF, 0x01,
    0xFE, 0x01, 0x00, 0x00, 0xFE, 0x01, 0xFF, 0x40, 0x00, 0x00, 0xFC, 0x03,
    0xFF, 0xE0, 0x00, 0x00, 0xF8, 0x03, 0x7F, 0xF0, 0x01, 0x00, 0xF0, 0x07,
    0x3F, 0xF0, 0x03, 0x00, 0xE0, 0x07, 0x1F, 0xE0, 0x03, 0x00, 0xC0, 0x0F,
    0x0F, 0xC0, 0x03, 0x00, 0x80, 0x0F, 0x07, 0x80, 0x01, 0x00, 0x00, 0x1F,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7C,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// WOLF SPRITES for Daemon (32x32 XBM, 128 bytes each)
static const unsigned char wolf_idle[] = {
    0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x06, 0xf0, 0x00, 0x00, 0x0f,
    0xb0, 0x01, 0x80, 0x0f, 0x90, 0x03, 0xc0, 0x09, 0x10, 0x07, 0xe0, 0x08,
    0x08, 0x1c, 0x30, 0x10, 0xc8, 0xf8, 0x1f, 0x13, 0x88, 0xfc, 0x3f, 0x10,
    0x88, 0x72, 0x5b, 0x13, 0x88, 0xff, 0xff, 0x13, 0x98, 0xff, 0xff, 0x1b,
    0xd0, 0x79, 0x3d, 0x0b, 0xf0, 0x47, 0x25, 0x0f, 0xf0, 0xc1, 0xc3, 0x0f,
    0xf8, 0xce, 0xf3, 0x1e, 0xc4, 0x52, 0x6b, 0x26, 0xce, 0xaa, 0x70, 0x76,
    0x1c, 0x3c, 0x38, 0x39, 0x0e, 0x10, 0x08, 0x70, 0x02, 0x00, 0x10, 0xc0,
    0x0e, 0x40, 0x00, 0xf0, 0x06, 0x00, 0x00, 0x60, 0x18, 0x00, 0x00, 0x18,
    0x3c, 0xe4, 0x27, 0x3c, 0x30, 0xe4, 0x27, 0x0d, 0xf0, 0xc4, 0x23, 0x0f,
    0xe0, 0x08, 0x90, 0x07, 0x80, 0xd5, 0xe3, 0x03, 0x00, 0xaf, 0xf7, 0x01,
    0x00, 0x1c, 0x38, 0x00, 0x00, 0xe0, 0x07, 0x00};

static const unsigned char wolf_blink[] = {
    0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x06, 0x70, 0x00, 0x00, 0x0f,
    0xf0, 0x00, 0x00, 0x0d, 0xf0, 0x01, 0x80, 0x0f, 0xf0, 0x03, 0xc0, 0x0b,
    0x88, 0x6d, 0xf4, 0x11, 0xc8, 0xfb, 0xbf, 0x11, 0x88, 0x5b, 0xbb, 0x11,
    0x08, 0x0f, 0xcf, 0x13, 0x88, 0x4f, 0x8e, 0x13, 0x88, 0x0e, 0x04, 0x11,
    0xd0, 0x30, 0x32, 0x08, 0xf0, 0x30, 0x30, 0x0c, 0x90, 0x11, 0xc7, 0x0b,
    0x18, 0x86, 0x87, 0x1b, 0x08, 0x80, 0x03, 0x13, 0x0c, 0x80, 0x03, 0x25,
    0x78, 0xc7, 0xf3, 0x14, 0x1c, 0x18, 0x08, 0x28, 0x06, 0x00, 0x00, 0x40,
    0x0f, 0x10, 0x00, 0x78, 0x0e, 0x00, 0x00, 0x70, 0x38, 0x40, 0x00, 0x1c,
    0x0c, 0x00, 0x00, 0x30, 0x70, 0xc0, 0x03, 0x0e, 0x70, 0x24, 0x26, 0x0e,
    0xe0, 0xe5, 0x27, 0x07, 0x80, 0x89, 0x91, 0x01, 0x80, 0x0f, 0xf0, 0x01,
    0x00, 0x1c, 0x38, 0x00, 0x00, 0xe0, 0x07, 0x00};

static const unsigned char wolf_aggressive[] = {
    0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x0e, 0xf0, 0x00, 0x00, 0x0e,
    0x90, 0x01, 0x80, 0x0b, 0x90, 0x07, 0xe0, 0x09, 0x08, 0x0f, 0xf0, 0x10,
    0x08, 0xfe, 0x7f, 0x12, 0x08, 0xfc, 0x3f, 0x10, 0x88, 0xff, 0xff, 0x13,
    0xc8, 0x3f, 0xfd, 0x13, 0xd8, 0xe7, 0xe7, 0x1b, 0xf0, 0xc3, 0xc7, 0x0f,
    0xf0, 0x47, 0xe3, 0x0f, 0xf8, 0x5a, 0x5a, 0x1f, 0xf8, 0xea, 0x57, 0x1f,
    0xfc, 0x3d, 0xb8, 0x3f, 0x98, 0x07, 0xe0, 0x19, 0x04, 0xe0, 0x07, 0x21,
    0x8e, 0xe0, 0x07, 0xf0, 0x02, 0xc4, 0x23, 0xc0, 0x0e, 0x0c, 0x30, 0xf0,
    0x04, 0x2c, 0x34, 0x20, 0x3e, 0xec, 0x37, 0x7c, 0x70, 0xf8, 0x1f, 0x0e,
    0x70, 0xb8, 0x1f, 0x0e, 0xe0, 0x30, 0x4e, 0x07, 0xc0, 0xd1, 0x89, 0x03,
    0xc0, 0xd7, 0xeb, 0x03, 0x00, 0x4e, 0x7a, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char wolf_funny[] = {
    0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x0f,
    0xf0, 0x01, 0x80, 0x0f, 0x50, 0x07, 0xe0, 0x0b, 0xc8, 0x6f, 0x74, 0x11,
    0xc8, 0xfd, 0xbf, 0x11, 0x88, 0x79, 0xbf, 0x13, 0x08, 0xb3, 0xcc, 0x12,
    0x88, 0x81, 0x8d, 0x13, 0xc8, 0xdf, 0x3b, 0x11, 0x50, 0xc0, 0x07, 0x0a,
    0x70, 0x02, 0x01, 0x0c, 0xd0, 0x0e, 0x79, 0x09, 0xd8, 0x11, 0x89, 0x18,
    0xcc, 0x19, 0x94, 0x38, 0x1e, 0x19, 0x98, 0x78, 0x14, 0x10, 0x08, 0x20,
    0x12, 0xc0, 0x13, 0x40, 0x03, 0x20, 0x02, 0x40, 0x0f, 0xe0, 0x63, 0x70,
    0x04, 0xc2, 0x41, 0x20, 0x02, 0x02, 0x20, 0x40, 0x10, 0x0c, 0x38, 0x08,
    0x30, 0xf8, 0x1f, 0x08, 0xf0, 0x70, 0x0c, 0x0f, 0x60, 0x81, 0x84, 0x07,
    0xc0, 0x25, 0x84, 0x03, 0x00, 0x1c, 0xf0, 0x00, 0x00, 0x10, 0x0c, 0x00,
    0x00, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00};

#define SPLIT_X (NOCT_DISP_W / 2)
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

const char *SceneManager::getSceneName(int sceneIndex) const
{
  if (sceneIndex < 0 || sceneIndex >= NOCT_TOTAL_SCENES)
    return "MAIN";
  return sceneNames_[sceneIndex];
}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame)
{
  drawWithOffset(sceneIndex, 0, bootTime, blinkState, fanFrame);
}

void SceneManager::drawWithOffset(int sceneIndex, int xOffset,
                                  unsigned long bootTime, bool blinkState,
                                  int fanFrame)
{
  (void)bootTime;
  (void)fanFrame;
  switch (sceneIndex)
  {
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
#define MAIN_CPU_LIFEBAR_X 4
#define MAIN_CPU_LIFEBAR_W 55
#define MAIN_GPU_LIFEBAR_X 69
#define MAIN_GPU_LIFEBAR_W 55
#define MAIN_SCENE_RAM_Y 50
#define MAIN_SCENE_RAM_H 14
#define MAIN_SCENE_RAM_CHAMFER 2
#define MAIN_SCENE_RAM_TEXT_Y (MAIN_SCENE_RAM_Y + MAIN_SCENE_RAM_H - 4)

void SceneManager::drawMain(bool blinkState, int xOff)
{
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
  if (!blinkCpu)
  {
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
  if (!blinkGpu)
  {
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
  if (!blinkRam)
  {
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
                                const char *value, int valueYOffset)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  disp_.drawTechBracket(x, y, GRID_CELL_W, GRID_CELL_H, GRID_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(x + GRID_LBL_INSET, y + GRID_BASELINE_Y_OFF, label);
  int valueY = y + GRID_BASELINE_Y_OFF + valueYOffset + GRID_VALUE_Y_EXTRA;
  u8g2.setFontPosBaseline();
  disp_.drawRightAligned(x + GRID_CELL_W - GRID_LBL_INSET, valueY, VALUE_FONT,
                         value);
}

// ---------------------------------------------------------------------------
// SCENE 2: CPU — 2x2 grid (TEMP | CLOCK / LOAD | PWR). Same layout as GPU/MB.
// ---------------------------------------------------------------------------
void SceneManager::drawCpu(bool blinkState, int xOff)
{
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
void SceneManager::drawGpu(bool blinkState, int xOff)
{
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

void SceneManager::drawRam(bool blinkState, int xOff)
{
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
  for (int i = 0; i < 2; i++)
  {
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
    if (valueW <= availW)
    {
      disp_.drawRightAligned(X(RAM_VALUE_ANCHOR, xOff), baselineY, VALUE_FONT,
                             buf);
    }
    else
    {
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
  if (!blinkRam)
  {
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
#define DISK_GRID_X_SPLIT (NOCT_DISP_W / 2)
#define DISK_GRID_Y_SPLIT 40
#define DISK_CELL_W (NOCT_DISP_W / 2)
#define DISK_CELL_H 40
#define DISK_LBL_X 4
#define DISK_LBL_Y 4
#define DISK_TEMP_OFFSET 20

void SceneManager::drawDisks(int xOff)
{
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
  for (int i = 0; i < NOCT_HDD_COUNT; i++)
  {
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

void SceneManager::drawFans(int fanFrame, int xOff)
{
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
  for (int i = 0; i < 4; i++)
  {
    int rowY = FAN_ROW_Y0 + i * FAN_ROW_DY + 6;
    if (rowY + 6 > NOCT_DISP_H)
      break;
    int rpm = (rpms[i] >= 0) ? rpms[i] : 0;
    int pct = (hw.fan_controls[i] >= 0 && hw.fan_controls[i] <= 100)
                  ? hw.fan_controls[i]
                  : -1;
    snprintf(lineBuf, sizeof(lineBuf), "%s  [%d]  ", labels[i], rpm);
    u8g2.drawUTF8(X(FAN_NAME_X, xOff), rowY, lineBuf);
    if (pct >= 0)
    {
      snprintf(lineBuf, sizeof(lineBuf), "%d%%", pct);
      u8g2.drawUTF8(X(FAN_PCT_X, xOff), rowY, lineBuf);
    }
    else
    {
      u8g2.drawUTF8(X(FAN_PCT_X, xOff), rowY, "--");
    }
  }
  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// SCENE 8: MOTHERBOARD — 2x2 grid (SYS | VRM / SOC | CHIP). Same layout as
// CPU/GPU.
// ---------------------------------------------------------------------------
void SceneManager::drawMotherboard(int xOff)
{
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
// SCENE 9: WEATHER — Stacked icon + desc left, temp right.
// ---------------------------------------------------------------------------
#define WTHR_LEFT_PCT 40
#define WTHR_BOX_H (NOCT_DISP_H - NOCT_CONTENT_TOP - 2)
#define WTHR_ICON_W 22
#define WTHR_ICON_H 22
#define WTHR_DESC_PAD 4
#define WTHR_DESC_BASELINE 6
#define WTHR_TEMP_Y 52

static bool readXbmBit(const uint8_t *bits, int w, int x, int y)
{
  int byteIndex = (x + y * w) / 8;
  int bitIndex = x & 7;
  return (bits[byteIndex] & (1 << bitIndex)) != 0;
}

static void drawXbmScaled(U8G2 &u8g2, int x, int y, int srcW, int srcH,
                          const uint8_t *bits, int dstW, int dstH)
{
  for (int dy = 0; dy < dstH; dy++)
  {
    int sy = (dy * srcH + (dstH / 2)) / dstH;
    for (int dx = 0; dx < dstW; dx++)
    {
      int sx = (dx * srcW + (dstW / 2)) / dstW;
      if (readXbmBit(bits, srcW, sx, sy))
        u8g2.drawPixel(x + dx, y + dy);
    }
  }
}

void SceneManager::drawWeather(int xOff)
{
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

  if (!state_.weatherReceived && weather.temp == 0 && weather.wmoCode == 0)
  {
    drawNoDataCross(boxX + 4, boxY + 4, boxW - 8, boxH - 8);
    disp_.drawGreebles();
    return;
  }

  /* Left: scaled XBM icon + description stacked */
  const unsigned char *iconBits = icon_cloud_bits;
  if (weather.wmoCode <= 3)
    iconBits = icon_sun_bits;
  else if (weather.wmoCode >= 50)
    iconBits = icon_weather_rain_32_bits;
  else
    iconBits = icon_cloud_bits;
  const int stackH = WTHR_ICON_H + WTHR_DESC_PAD + WTHR_DESC_BASELINE;
  const int iconX = boxX + (leftW - WTHR_ICON_W) / 2;
  const int iconY = boxY + (boxH - stackH) / 2;
  const int descY = iconY + WTHR_ICON_H + WTHR_DESC_PAD + WTHR_DESC_BASELINE;
  u8g2.setClipWindow(boxX, boxY, boxX + leftW - 1, boxY + boxH - 1);
  drawXbmScaled(u8g2, iconX, iconY, 32, 32, iconBits, WTHR_ICON_W, WTHR_ICON_H);
  u8g2.setFont(LABEL_FONT);
  const char *descStr = weather.desc.length() ? weather.desc.c_str() : "-";
  int descW = u8g2.getUTF8Width(descStr);
  int descX = iconX + (WTHR_ICON_W / 2) - (descW / 2);
  u8g2.drawUTF8(descX, descY, descStr);
  u8g2.setMaxClipWindow();

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

void SceneManager::drawPlayer(int xOff)
{
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
void SceneManager::drawFanIconSmall(int x, int y, int frame)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const uint8_t *bits = icon_fan_frames[frame % 4];
  u8g2.drawXBM(x, y, 8, 8, bits);
}

// ---------------------------------------------------------------------------
// 32x32 weather icon by WMO code. Fallback: Cloud or Sun (never blank).
// ---------------------------------------------------------------------------
void SceneManager::drawWeatherIcon32(int x, int y, int wmoCode)
{
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

// Battery HUD: far right of top bar. Uses NOCT_BAT_* and
// NOCT_HEADER_BASELINE_Y. onWhiteHeader: true = draw in black (visible on white
// header); false = white.
void SceneManager::drawPowerStatus(int pct, bool isCharging,
                                   float batteryVoltage, bool onWhiteHeader)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(LABEL_FONT);

  const int frameW = NOCT_BAT_FRAME_W;
  const int frameH = NOCT_BAT_FRAME_H;
  const int termW = NOCT_BAT_TERM_W;
  const int termH = NOCT_BAT_TERM_H;
  const int baselineY = NOCT_HEADER_BASELINE_Y;
  int x = NOCT_DISP_W - frameW - termW;
  int y = baselineY;

  // Цвет рисования: на белом хедере — чёрный (0), на тёмном фоне — белый (1)
  int ink = onWhiteHeader ? 0 : 1;

  // Определяем, подключена ли батарея
  bool batteryConnected = (batteryVoltage >= 3.5f && batteryVoltage <= 5.0f);

  u8g2.setDrawColor(ink);
  u8g2.drawFrame(x, y - frameH, frameW, frameH);
  u8g2.drawBox(x + frameW, y - frameH + (frameH - termH) / 2, termW, termH);

  if (batteryConnected && pct > 0)
  {
    u8g2.setDrawColor(ink);
    /* Smooth fill: one continuous bar by percentage (inner padding 2px) */
    const int innerW = frameW - 4;
    const int innerH = frameH - 4;
    const int fillW = (pct * innerW + 50) / 100;
    if (fillW > 0)
      u8g2.drawBox(x + 2, y - frameH + 2, fillW, innerH);

    if (isCharging)
    {
      unsigned long t = millis() / 300;
      if (t % 2 == 0)
      {
        int chargeX = x + frameW + termW + NOCT_MARGIN;
        int chargeY = y - frameH / 2;
        u8g2.setDrawColor(ink);
        u8g2.drawLine(chargeX, chargeY + 2, chargeX, chargeY - 2);
        u8g2.drawLine(chargeX - 1, chargeY - 1, chargeX, chargeY - 2);
        u8g2.drawLine(chargeX + 1, chargeY - 1, chargeX, chargeY - 2);
      }
    }
  }
  else
  {
    u8g2.setDrawColor(ink);
    u8g2.drawLine(x + 2, y - frameH + 2, x + frameW - 2, y - 2);
    u8g2.drawLine(x + frameW - 2, y - frameH + 2, x + 2, y - 2);
  }

  static char buf[8];
  if (!batteryConnected)
  {
    snprintf(buf, sizeof(buf), "NO BAT");
  }
  else if (isCharging)
  {
    snprintf(buf, sizeof(buf), "CHG");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%d%%", pct);
  }
  u8g2.setDrawColor(ink);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.setCursor(x - tw - 2, y);
  u8g2.print(buf);
  u8g2.setDrawColor(1);
}

void SceneManager::drawChargeOnlyScreen(int pct, bool isCharging,
                                        float batteryVoltage)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  /* Same layout as MAIN: header, two brackets (BAT | VOLT), chamfer status bar. */
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(NOCT_MARGIN, NOCT_HEADER_BASELINE_Y, "CHARGE");

  /* Left bracket: BAT + percentage (like CPU in MAIN). */
  disp_.drawTechBrackets(MAIN_CPU_X, MAIN_CPU_Y, MAIN_CPU_W, MAIN_CPU_H,
                        MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(MAIN_CPU_X + 4, MAIN_CPU_Y + 8, "BAT");

  /* Right bracket: V + voltage (like GPU in MAIN). */
  disp_.drawTechBrackets(MAIN_GPU_X, MAIN_GPU_Y, MAIN_GPU_W, MAIN_GPU_H,
                        MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(MAIN_GPU_X + 4, MAIN_GPU_Y + 8, "V");

  if (batteryVoltage >= 3.5f && batteryVoltage <= 5.0f)
  {
    static char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    u8g2.setFont(VALUE_FONT);
    int pw = u8g2.getUTF8Width(pctBuf);
    int cx = MAIN_CPU_X + MAIN_CPU_W / 2 - pw / 2;
    u8g2.drawUTF8(cx, MAIN_CPU_Y + 22, pctBuf);

    static char voltBuf[16];
    snprintf(voltBuf, sizeof(voltBuf), "%.2f", batteryVoltage);
    int vw = u8g2.getUTF8Width(voltBuf);
    int vx = MAIN_GPU_X + MAIN_GPU_W / 2 - vw / 2;
    u8g2.drawUTF8(vx, MAIN_GPU_Y + 22, voltBuf);

    /* Bottom chamfer: status (like RAM bar in MAIN). */
    disp_.drawChamferBox(0, MAIN_SCENE_RAM_Y, NOCT_DISP_W, MAIN_SCENE_RAM_H,
                         MAIN_SCENE_RAM_CHAMFER);
    u8g2.setFont(LABEL_FONT);
    const char *statusStr = isCharging ? "CHARGING" : "READY";
    u8g2.drawUTF8(NOCT_MARGIN + 4, MAIN_SCENE_RAM_TEXT_Y, statusStr);
    snprintf(pctBuf, sizeof(pctBuf), "%.2f V", batteryVoltage);
    int sw = u8g2.getUTF8Width(pctBuf);
    u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - 4 - sw, MAIN_SCENE_RAM_TEXT_Y,
                  pctBuf);
  }
  else
  {
    u8g2.setFont(LABEL_FONT);
    const char *na = "N/A";
    int naw = u8g2.getUTF8Width(na);
    u8g2.drawUTF8(MAIN_CPU_X + MAIN_CPU_W / 2 - naw / 2, MAIN_CPU_Y + 22, na);
    u8g2.drawUTF8(MAIN_GPU_X + MAIN_GPU_W / 2 - naw / 2, MAIN_GPU_Y + 22, na);
    disp_.drawChamferBox(0, MAIN_SCENE_RAM_Y, NOCT_DISP_W, MAIN_SCENE_RAM_H,
                         MAIN_SCENE_RAM_CHAMFER);
    u8g2.setFont(LABEL_FONT);
    u8g2.drawUTF8(NOCT_MARGIN + 4, MAIN_SCENE_RAM_TEXT_Y, "Connect USB");
  }

  drawBottomHint("2x menu");
  disp_.drawGreebles();
}

void SceneManager::drawNoDataCross(int x, int y, int w, int h)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.drawLine(x, y, x + w - 1, y + h - 1);
  u8g2.drawLine(x + w - 1, y, x, y + h - 1);
  u8g2.setFontMode(1);
  u8g2.setFont(LABEL_FONT);
  const char *nd = "NO DATA";
  int tw = u8g2.getUTF8Width(nd);
  u8g2.drawUTF8(x + (w - tw) / 2, y + h / 2 - 2, nd);
}

void SceneManager::drawNoisePattern(int x, int y, int w, int h)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  for (int i = 0; i < 200; i++)
    u8g2.drawPixel(x + random(w), y + random(h));
}

// ---------------------------------------------------------------------------
void SceneManager::drawSearchMode(int scanPhase)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
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
// Flipper-style two-level menu: level 0 = categories, level 1 = submenu.
// Short = scroll, Long = enter/execute, Double = back/close.
// ---------------------------------------------------------------------------
static int submenuCountForCategory(int cat)
{
  switch (cat)
  {
  case 0:
    return 2; // BMW: BMW Assistant, BMW Demo
  case 1:
    return 7; // Config: AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT
  case 2:
    return 5; // System: Demo, REBOOT, CHARGE ONLY, POWER OFF, VERSION
  default:
    return 2;
  }
}

void SceneManager::drawMenu(int menuLevel, int menuCategory, int mainIndex,
                            int menuHackerGroup, bool carouselOn,
                            int carouselSec, bool screenRotated,
                            bool glitchEnabled, bool ledEnabled,
                            bool lowBrightnessDefault, bool rebootConfirmed,
                            int displayContrast, int displayTimeoutSec)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int boxX = NOCT_MENU_BOX_X;
  const int boxY = NOCT_MENU_BOX_Y;
  const int boxW = NOCT_MENU_BOX_W;
  const int boxH = NOCT_MENU_BOX_H;

  u8g2.setDrawColor(0);
  u8g2.drawBox(boxX, boxY, boxW, boxH);
  u8g2.setDrawColor(1);
  disp_.drawTechFrame(boxX, boxY, boxW, boxH);

  // 0=BMW, 1=Config, 2=System (BMW-only)
  static const char *categoryNames[] = {"BMW", "Config", "System"};
  static char items[25][20];
  int count;
  const char *headerStr;

  if (menuLevel == 0)
  {
    count = 3;
    headerStr = "// MENU";
    for (int i = 0; i < 3; i++)
    {
      strncpy(items[i], categoryNames[i], sizeof(items[i]) - 1);
      items[i][sizeof(items[i]) - 1] = '\0';
    }
  }
  else
  {
    count = submenuCountForCategory(menuCategory);
    headerStr = categoryNames[menuCategory];
    if (menuCategory == 0)
    {
      strncpy(items[0], "BMW Assistant", sizeof(items[0]) - 1);
      strncpy(items[1], "BMW Demo", sizeof(items[1]) - 1);
      items[0][sizeof(items[0]) - 1] = '\0';
      items[1][sizeof(items[1]) - 1] = '\0';
    }
    else if (menuCategory == 1)
    {
      if (!carouselOn)
        snprintf(items[0], sizeof(items[0]), "AUTO: OFF");
      else
        snprintf(items[0], sizeof(items[0]), "AUTO: %ds", carouselSec);
      snprintf(items[1], sizeof(items[1]), "FLIP: %s",
               screenRotated ? "180deg" : "0deg");
      snprintf(items[2], sizeof(items[2]), "GLITCH: %s",
               glitchEnabled ? "ON" : "OFF");
      snprintf(items[3], sizeof(items[3]), "LED: %s",
               ledEnabled ? "ON" : "OFF");
      snprintf(items[4], sizeof(items[4]), "DIM: %s",
               lowBrightnessDefault ? "ON" : "OFF");
      snprintf(items[5], sizeof(items[5]), "CONTRAST:%d", displayContrast);
      if (displayTimeoutSec == 0)
        strncpy(items[6], "TIMEOUT:OFF", sizeof(items[6]) - 1);
      else
        snprintf(items[6], sizeof(items[6]), "TIMEOUT:%ds", displayTimeoutSec);
      items[6][sizeof(items[6]) - 1] = '\0';
    }
    else if (menuCategory == 2)
    {
      strncpy(items[0], "Demo", sizeof(items[0]) - 1);
      items[0][sizeof(items[0]) - 1] = '\0';
      if (rebootConfirmed)
        snprintf(items[1], sizeof(items[1]), "REBOOT [OK]");
      else
        strncpy(items[1], "REBOOT", sizeof(items[1]) - 1);
      items[1][sizeof(items[1]) - 1] = '\0';
      strncpy(items[2], "Charge only", sizeof(items[2]) - 1);
      strncpy(items[3], "Power off", sizeof(items[3]) - 1);
      strncpy(items[4], "Version", sizeof(items[4]) - 1);
      items[2][sizeof(items[2]) - 1] = '\0';
      items[3][sizeof(items[3]) - 1] = '\0';
      items[4][sizeof(items[4]) - 1] = '\0';
    }
  }
  if (count <= 0)
    count = 1;

  u8g2.setDrawColor(1);
  u8g2.drawBox(NOCT_MENU_CONFIG_BAR_X, NOCT_MENU_CONFIG_BAR_Y,
               NOCT_MENU_CONFIG_BAR_W, NOCT_MENU_CONFIG_BAR_H);
  disp_.drawChamferBox(NOCT_MENU_CONFIG_BAR_X, NOCT_MENU_CONFIG_BAR_Y,
                       NOCT_MENU_CONFIG_BAR_W, NOCT_MENU_CONFIG_BAR_H,
                       NOCT_MENU_CONFIG_BAR_CHAMFER);
  u8g2.setDrawColor(0);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_MENU_CONFIG_BAR_X + 4,
                NOCT_MENU_CONFIG_BAR_Y + NOCT_MENU_CONFIG_BAR_H - 2, headerStr);
  u8g2.setDrawColor(1);

  int selected = mainIndex;
  if (selected < 0)
    selected = 0;
  if (selected >= count)
    selected = count - 1;

  const int startY = NOCT_MENU_START_Y;
  int firstVisible = selected - NOCT_MENU_VISIBLE_ROWS / 2;
  if (firstVisible < 0)
    firstVisible = 0;
  if (firstVisible + NOCT_MENU_VISIBLE_ROWS > count)
    firstVisible = count - NOCT_MENU_VISIBLE_ROWS;
  if (firstVisible < 0)
    firstVisible = 0; // when count < NOCT_MENU_VISIBLE_ROWS (e.g. 4 categories)

  u8g2.setFontMode(1);
  u8g2.setFont(LABEL_FONT);

  for (int r = 0; r < NOCT_MENU_VISIBLE_ROWS; r++)
  {
    int i = firstVisible + r;
    if (i >= count || i < 0)
      break;
    const char *itemText = items[i];
    if (itemText[0] == '\0')
      continue;
    int y = startY + r * NOCT_MENU_ROW_H;
    int textWidth = u8g2.getUTF8Width(itemText);
    int textX = boxX + (boxW - textWidth) / 2;
    if (textX < NOCT_MENU_LIST_LEFT + 10)
      textX = NOCT_MENU_LIST_LEFT + 10;
    if (textX + textWidth > NOCT_MENU_LIST_LEFT + NOCT_MENU_LIST_W - 2)
      textX = NOCT_MENU_LIST_LEFT + NOCT_MENU_LIST_W - 2 - textWidth;

    if (i == selected)
    {
      u8g2.setDrawColor(1);
      u8g2.drawBox(NOCT_MENU_LIST_LEFT, y - 6, NOCT_MENU_LIST_W,
                   NOCT_MENU_ROW_H);
      u8g2.setDrawColor(0);
      u8g2.drawUTF8(NOCT_MENU_LIST_LEFT + 2, y, ">");
      u8g2.drawUTF8(NOCT_MENU_LIST_LEFT + NOCT_MENU_LIST_W - 6, y, "<");
      u8g2.drawUTF8(textX, y, itemText);
      u8g2.setDrawColor(1);
    }
    else
    {
      u8g2.setDrawColor(1);
      u8g2.drawUTF8(textX, y, itemText);
    }
  }

  disp_.drawScrollIndicator(startY - 2,
                            NOCT_MENU_VISIBLE_ROWS * NOCT_MENU_ROW_H, count,
                            NOCT_MENU_VISIBLE_ROWS, firstVisible);

  /* Hint: how to use the menu */
  u8g2.setFont(UNIT_FONT);
  const char *hint = "1x next  2s ok  2x menu";
  int hintW = u8g2.getUTF8Width(hint);
  int hintX = boxX + (boxW - hintW) / 2;
  if (hintX < NOCT_MENU_LIST_LEFT)
    hintX = NOCT_MENU_LIST_LEFT;
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(hintX, boxY + boxH - 2, hint);
}

static const char DEFAULT_BOTTOM_HINT[] = "1x next  2s ok  2x menu";

void SceneManager::drawBottomHint(const char *hint)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, NOCT_FOOTER_Y, NOCT_DISP_W, NOCT_FOOTER_Y);
  u8g2.setFont(LABEL_FONT);
  u8g2.setCursor(NOCT_MARGIN, NOCT_FOOTER_TEXT_Y);
  u8g2.print(hint ? hint : DEFAULT_BOTTOM_HINT);
}

void SceneManager::drawToast(const char *msg)
{
  if (!msg || !msg[0])
    return;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int barH = 12;
  const int barY = NOCT_DISP_H - barH;
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, barY, NOCT_DISP_W, barH);
  u8g2.setDrawColor(0);
  u8g2.setFont(LABEL_FONT);
  int w = u8g2.getUTF8Width(msg);
  int x = (NOCT_DISP_W - w) / 2;
  if (x < NOCT_MARGIN)
    x = NOCT_MARGIN;
  if (x + w > NOCT_DISP_W - NOCT_MARGIN)
    x = NOCT_DISP_W - NOCT_MARGIN - w;
  /* Text top at least 54 so bottom <= 64 */
  int textY = barY + 2;
  if (textY < NOCT_FOOTER_TEXT_Y)
    textY = NOCT_FOOTER_TEXT_Y;
  u8g2.drawUTF8(x, textY, msg);
  u8g2.setDrawColor(1);
}

void SceneManager::drawNoSignal(bool wifiOk, bool tcpOk, int rssi,
                                bool blinkState)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
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

void SceneManager::drawConnecting(int rssi, bool blinkState)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
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

// ---------------------------------------------------------------------------
// IDLE SCREENSAVER: Animated wolf (Daemon sprites) + cyberpunk/Blade Runner
// vibes. Full screen, no header. Shown after NOCT_IDLE_SCREENSAVER_MS in
// no-WiFi or linking state.
// ---------------------------------------------------------------------------
void SceneManager::drawIdleScreensaver(unsigned long now)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int wolfW = 32;
  const int wolfH = 32;
  const int wolfX = (NOCT_DISP_W - wolfW) / 2;
  const int wolfY = (NOCT_DISP_H - wolfH) / 2;

  /* Cycle: idle -> blink -> funny -> idle (~500ms per phase) */
  unsigned long phase = (now / 500) % 12;
  const unsigned char *wolfSprite = wolf_idle;
  if (phase >= 2 && phase < 4)
    wolfSprite = wolf_blink;
  else if (phase >= 6 && phase < 9)
    wolfSprite = wolf_funny;

  u8g2.setDrawColor(1);
  u8g2.drawXBMP(wolfX, wolfY, wolfW, wolfH, wolfSprite);

  /* Tech bracket frame around wolf (viewfinder) */
  disp_.drawTechBrackets(wolfX - 2, wolfY - 2, wolfW + 4, wolfH + 4, 4);

  /* Hex stream columns left and right (cyberpunk data rain) */
  disp_.drawHexStream(2, NOCT_CONTENT_TOP, 3);
  disp_.drawHexStream(NOCT_DISP_W - 56, NOCT_CONTENT_TOP, 3);

  /* Diagonal scratches / Blade Runner watermark */
  disp_.drawCyberClaw(0, NOCT_DISP_H - 22);
  disp_.drawCyberClaw(NOCT_DISP_W - 24, 0);

  /* Bottom corner hex decor */
  disp_.drawHexDecoration(0);
  disp_.drawHexDecoration(1);

  /* Subtle "SLEEP" or "STANDBY" at bottom center */
  u8g2.setFont(LABEL_FONT);
  const char *standby = "STANDBY";
  int tw = u8g2.getUTF8Width(standby);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, NOCT_DISP_H - 4, standby);

  /* Occasional horizontal slice glitch (Blade Runner VFX) */
  if ((now / 200) % 7 == 0)
    disp_.drawGlitch(1);
}

// ---------------------------------------------------------------------------
// DAEMON MODE: Wolf (left) | CPU, GPU, RAM (right). Wolf sprites by mood.
// Улучшенная анимация, обработка отсутствия данных, оптимизация.
// Добавлена статистика времени работы и индикация сетевой активности.
// ---------------------------------------------------------------------------
void SceneManager::drawDaemon(unsigned long bootTime, bool wifiConnected,
                              bool tcpConnected, int rssi)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  HardwareData &hw = state_.hw;

  // Проверка наличия данных
  bool hasData = (hw.ct > 0 || hw.gt > 0 || hw.ra > 0);

  // Оптимизированные вычисления (кэширование значений)
  static int lastCpuTemp = 0, lastGpuTemp = 0;
  static int lastCpuLoad = 0, lastGpuLoad = 0;
  static float lastRamUsage = 0.0f;
  static unsigned long lastUpdate = 0;

  int cpuTemp = 0, gpuTemp = 0, cpuLoad = 0, gpuLoad = 0;
  float ramUsage = 0.0f;

  if (hasData)
  {
    cpuTemp = (hw.ct > 0) ? hw.ct : 0;
    gpuTemp = (hw.gt > 0) ? hw.gt : 0;
    cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
    gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
    ramUsage = (hw.ra > 0) ? (hw.ru / hw.ra) * 100.0f : 0.0f;

    // Обновление кэша только при изменении или раз в секунду
    unsigned long now = millis();
    if (now - lastUpdate > 1000 || cpuTemp != lastCpuTemp ||
        gpuTemp != lastGpuTemp || cpuLoad != lastCpuLoad ||
        gpuLoad != lastGpuLoad || abs(ramUsage - lastRamUsage) > 0.5f)
    {
      lastCpuTemp = cpuTemp;
      lastGpuTemp = gpuTemp;
      lastCpuLoad = cpuLoad;
      lastGpuLoad = gpuLoad;
      lastRamUsage = ramUsage;
      lastUpdate = now;
    }
    else
    {
      // Использование кэшированных значений
      cpuTemp = lastCpuTemp;
      gpuTemp = lastGpuTemp;
      cpuLoad = lastCpuLoad;
      gpuLoad = lastGpuLoad;
      ramUsage = lastRamUsage;
    }
  }
  else
  {
    // Использование последних известных значений при отсутствии данных
    cpuTemp = lastCpuTemp;
    gpuTemp = lastGpuTemp;
    cpuLoad = lastCpuLoad;
    gpuLoad = lastGpuLoad;
    ramUsage = lastRamUsage;
  }

  // Улучшенная логика определения состояния HUNTING
  bool alert = hasData && (cpuTemp > 75 || gpuTemp > 75 || ramUsage > 85);
  bool criticalAlert =
      hasData && (cpuTemp > 85 || gpuTemp > 85 || ramUsage > 95);

  // Улучшенная анимация волка с более плавными переходами
  const unsigned char *wolfSprite = wolf_idle;
  unsigned long t = millis();

  if (criticalAlert)
  {
    // Критическое состояние - всегда агрессивная анимация
    wolfSprite = wolf_aggressive;
  }
  else if (alert)
  {
    // HUNTING режим - чередование агрессивной и обычной анимации
    int huntPhase = (int)((t / 400) % 6);
    if (huntPhase < 3)
    {
      wolfSprite = wolf_aggressive;
    }
    else
    {
      wolfSprite = wolf_idle;
    }
  }
  else if (!hasData)
  {
    // Моргание при отсутствии данных
    int blinkPhase = (int)((t / 300) % 4);
    if (blinkPhase == 0 || blinkPhase == 2)
    {
      wolfSprite = wolf_blink;
    }
    else
    {
      wolfSprite = wolf_idle;
    }
  }
  else
  {
    // Плавная анимация: idle -> blink -> funny -> idle
    int phase = (int)((t / 600) % 16); // Более медленная смена фаз
    if (phase == 10)
    {
      wolfSprite = wolf_blink;
    }
    else if (phase >= 11 && phase <= 13)
    {
      wolfSprite = wolf_funny;
    }
    else
    {
      wolfSprite = wolf_idle;
    }
  }

  u8g2.setDrawColor(1);
  const int contentStartY = NOCT_CONTENT_TOP;
  u8g2.drawXBMP(4, contentStartY + 2, 32, 32, wolfSprite);
  u8g2.drawFrame(0, contentStartY, 42, 44);
  u8g2.drawBox(40, contentStartY + 10, 4, 2);

  const int dataX = 48;
  const int barW = 70;
  int y = contentStartY;

  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setCursor(dataX, y);
  if (!hasData)
  {
    u8g2.print("STATUS: NO DATA");
  }
  else
  {
    if (criticalAlert)
    {
      u8g2.print("STATUS: CRITICAL");
    }
    else if (alert)
    {
      u8g2.print("STATUS: HUNTING");
    }
    else
    {
      u8g2.print("STATUS: IDLE");
    }
  }

  // Индикация сетевой активности справа от статуса
  if (wifiConnected)
  {
    int netX = dataX + barW - 20;
    if (tcpConnected)
    {
      u8g2.setCursor(netX, y);
      u8g2.print("[TCP]");
    }
    else
    {
      u8g2.setCursor(netX, y);
      u8g2.print("[WiFi]");
    }
  }

  y += NOCT_ROW_DY;

  // Статистика времени работы системы (если есть данные)
  if (hasData && bootTime > 0)
  {
    unsigned long uptimeSec = (millis() - bootTime) / 1000;
    unsigned long hours = uptimeSec / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    u8g2.setCursor(dataX, y);
    u8g2.printf("UPTIME: %lu:%02lu", hours, minutes);
    y += NOCT_ROW_DY;
  }

  if (!hasData)
  {
    // Отображение сообщения об отсутствии данных
    u8g2.setCursor(dataX, y);
    u8g2.print("Waiting for");
    y += NOCT_ROW_DY;
    u8g2.setCursor(dataX, y);
    u8g2.print("telemetry...");
  }
  else
  {
    // Отображение данных с оптимизированными вычислениями прогресс-баров
    u8g2.setCursor(dataX, y);
    u8g2.printf("CPU: %d C %d%%", cpuTemp, cpuLoad);
    u8g2.drawFrame(dataX, y + 2, barW, 3);
    int cpuBarWidth = (cpuLoad * (barW - 2) + 50) / 100;
    if (cpuBarWidth > 0)
    {
      u8g2.drawBox(dataX + 1, y + 3, cpuBarWidth, 1);
    }
    y += NOCT_ROW_DY;

    u8g2.setCursor(dataX, y);
    u8g2.printf("GPU: %d C %d%%", gpuTemp, gpuLoad);
    u8g2.drawFrame(dataX, y + 2, barW, 3);
    int gpuBarWidth = (gpuLoad * (barW - 2) + 50) / 100;
    if (gpuBarWidth > 0)
    {
      u8g2.drawBox(dataX + 1, y + 3, gpuBarWidth, 1);
    }
    y += NOCT_ROW_DY;

    u8g2.setCursor(dataX, y);
    u8g2.printf("RAM: %0.1f%%", ramUsage);
    u8g2.drawFrame(dataX, y + 2, barW, 3);
    int ramBarWidth = ((int)(ramUsage + 0.5f) * (barW - 2) + 50) / 100;
    if (ramBarWidth > 0)
    {
      u8g2.drawBox(dataX + 1, y + 3, ramBarWidth, 1);
    }
  }
}

static const char *bmwActionNames[] = {
  "Goodbye", "FollowMe", "Park", "Hazard", "LowBeam",
  "LightsOff", "Unlock", "Lock", "Trunk", "Cluster",
  "DoorUnlk", "DoorLock"
};

/* BMW Assistant: full screen 128x64, no header. Layout fits exactly. */
#define BMW_BRACKET_Y 0
#define BMW_BRACKET_H 32
#define BMW_LEFT_X 0
#define BMW_LEFT_W 63
#define BMW_RIGHT_X 65
#define BMW_RIGHT_W 63
#define BMW_BAR_Y 50
#define BMW_BAR_H 14
#define BMW_BAR_TEXT_Y 54
#define BMW_INSET 4
#define BMW_ROW1_Y 2
#define BMW_ROW2_Y 12
#define BMW_ROW3_Y 22

void SceneManager::drawBmwAssistant(BmwManager &bmw, int selectedActionIndex)
{
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  static char buf[40];
  const int maxLeftW = BMW_LEFT_W - BMW_INSET * 2;
  const int maxRightW = BMW_RIGHT_W - BMW_INSET * 2;
  const int maxBarW = NOCT_DISP_W - 10;

  /* Left bracket: STATUS + BAT (full 128x64, Y=0) */
  disp_.drawTechBrackets(BMW_LEFT_X, BMW_BRACKET_Y, BMW_LEFT_W, BMW_BRACKET_H,
                         MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(BMW_LEFT_X + BMW_INSET, BMW_BRACKET_Y + BMW_ROW1_Y, "STATUS");
  bmw.getStatusLine(buf, sizeof(buf));
  u8g2.setFont(VALUE_FONT);
  size_t len = strlen(buf);
  while (len > 0 && (unsigned)u8g2.getUTF8Width(buf) > (unsigned)maxLeftW) {
    buf[--len] = '\0';
  }
  u8g2.drawUTF8(BMW_LEFT_X + BMW_INSET, BMW_BRACKET_Y + BMW_ROW2_Y, buf);
  float v = state_.batteryVoltage;
  bool batOk = (v >= 3.5f && v <= 5.0f);
  u8g2.setFont(LABEL_FONT);
  if (batOk) {
    if (state_.isCharging)
      snprintf(buf, sizeof(buf), "BAT %d%% CHG", state_.batteryPct);
    else
      snprintf(buf, sizeof(buf), "BAT %d%%", state_.batteryPct);
  } else {
    snprintf(buf, sizeof(buf), "BAT --");
  }
  len = strlen(buf);
  while (len > 0 && (unsigned)u8g2.getUTF8Width(buf) > (unsigned)maxLeftW) {
    buf[--len] = '\0';
  }
  u8g2.drawUTF8(BMW_LEFT_X + BMW_INSET, BMW_BRACKET_Y + BMW_ROW3_Y, buf);

  /* Right bracket: ACTION + selected */
  disp_.drawTechBrackets(BMW_RIGHT_X, BMW_BRACKET_Y, BMW_RIGHT_W, BMW_BRACKET_H,
                         MAIN_BRACKET_LEN);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(BMW_RIGHT_X + BMW_INSET, BMW_BRACKET_Y + BMW_ROW1_Y, "ACTION");
  int actIdx = selectedActionIndex;
  if (actIdx < 0) actIdx = 0;
  if (actIdx >= 12) actIdx = 11;
  snprintf(buf, sizeof(buf), "> %s", bmwActionNames[actIdx]);
  u8g2.setFont(VALUE_FONT);
  len = strlen(buf);
  while (len > 0 && (unsigned)u8g2.getUTF8Width(buf) > (unsigned)maxRightW) {
    buf[--len] = '\0';
  }
  int aw = u8g2.getUTF8Width(buf);
  int ax = BMW_RIGHT_X + (BMW_RIGHT_W - aw) / 2;
  u8g2.drawUTF8(ax, BMW_BRACKET_Y + BMW_ROW2_Y + 2, buf);

  /* Bottom bar: feedback or status, one line */
  disp_.drawChamferBox(0, BMW_BAR_Y, NOCT_DISP_W, BMW_BAR_H,
                       MAIN_SCENE_RAM_CHAMFER);
  const char *feedback = bmw.getLastActionFeedback();
  if (feedback && feedback[0]) {
    strncpy(buf, feedback, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  } else {
    buf[0] = '\0';
    if (!bmw.isIbusSynced()) {
      strncpy(buf, "I-Bus: connect", sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
    } else if (bmw.isObdConnected()) {
      int oil = bmw.getObdOilTempC();
      int cool = bmw.getObdCoolantTempC();
      if (oil >= 0 || cool >= 0)
        snprintf(buf, sizeof(buf), "OBD OIL %d COOL %d", oil >= 0 ? oil : 0, cool >= 0 ? cool : 0);
      else
        snprintf(buf, sizeof(buf), "OBD RPM %d", bmw.getObdRpm());
    } else if (bmw.getIgnitionState() >= 0) {
      snprintf(buf, sizeof(buf), "IGN %d", bmw.getIgnitionState());
    } else if (bmw.hasPdcData()) {
      int d[4];
      bmw.getPdcDistances(d, 4);
      snprintf(buf, sizeof(buf), "PDC %d %d %d %d", d[0], d[1], d[2], d[3]);
    } else if (bmw.getNowPlayingTrack()[0]) {
      const char *t = bmw.getNowPlayingTrack();
      size_t n = strlen(t);
      if (n > 18) {
        strncpy(buf, t, 15);
        buf[15] = '\0';
        strcat(buf, "...");
      } else {
        strncpy(buf, t, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
      }
    }
  }
  if (buf[0]) {
    u8g2.setFont(LABEL_FONT);
    len = strlen(buf);
    while (len > 0 && (unsigned)u8g2.getUTF8Width(buf) > (unsigned)maxBarW) {
      buf[--len] = '\0';
    }
    u8g2.drawUTF8(NOCT_MARGIN + 4, BMW_BAR_TEXT_Y, buf);
  }

  disp_.drawGreebles();
}
