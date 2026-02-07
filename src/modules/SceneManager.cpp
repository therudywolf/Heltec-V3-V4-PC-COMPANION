/*
 * NOCTURNE_OS — SceneManager: 9 screens. 128x64,
 * MAIN/CPU/GPU/RAM/DISKS/MEDIA/FANS/MB/NET. Unified: LABEL_FONT only for
 * content; Y from NOCT_CONTENT_TOP, NOCT_ROW_DY.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include "BleManager.h"
#include "ForzaManager.h"
#include "KickManager.h"
#include "WifiSniffManager.h"
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
// SCENE 9: WEATHER — Stacked icon + desc left, temp right.
// ---------------------------------------------------------------------------
#define WTHR_LEFT_PCT 40
#define WTHR_BOX_H (NOCT_DISP_H - NOCT_CONTENT_TOP - 2)
#define WTHR_ICON_W 22
#define WTHR_ICON_H 22
#define WTHR_DESC_PAD 4
#define WTHR_DESC_BASELINE 6
#define WTHR_TEMP_Y 52

static bool readXbmBit(const uint8_t *bits, int w, int x, int y) {
  int byteIndex = (x + y * w) / 8;
  int bitIndex = x & 7;
  return (bits[byteIndex] & (1 << bitIndex)) != 0;
}

static void drawXbmScaled(U8G2 &u8g2, int x, int y, int srcW, int srcH,
                          const uint8_t *bits, int dstW, int dstH) {
  for (int dy = 0; dy < dstH; dy++) {
    int sy = (dy * srcH + (dstH / 2)) / dstH;
    for (int dx = 0; dx < dstW; dx++) {
      int sx = (dx * srcW + (dstW / 2)) / dstW;
      if (readXbmBit(bits, srcW, sx, sy))
        u8g2.drawPixel(x + dx, y + dy);
    }
  }
}

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

// Battery HUD: far right of top bar. Uses NOCT_BAT_* and
// NOCT_HEADER_BASELINE_Y. onWhiteHeader: true = draw in black (visible on white
// header); false = white.
void SceneManager::drawPowerStatus(int pct, bool isCharging,
                                   float batteryVoltage, bool onWhiteHeader) {
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

  if (batteryConnected && pct > 0) {
    u8g2.setDrawColor(ink);
    /* Smooth fill: one continuous bar by percentage (inner padding 2px) */
    const int innerW = frameW - 4;
    const int innerH = frameH - 4;
    const int fillW = (pct * innerW + 50) / 100;
    if (fillW > 0)
      u8g2.drawBox(x + 2, y - frameH + 2, fillW, innerH);

    if (isCharging) {
      unsigned long t = millis() / 300;
      if (t % 2 == 0) {
        int chargeX = x + frameW + termW + NOCT_MARGIN;
        int chargeY = y - frameH / 2;
        u8g2.setDrawColor(ink);
        u8g2.drawLine(chargeX, chargeY + 2, chargeX, chargeY - 2);
        u8g2.drawLine(chargeX - 1, chargeY - 1, chargeX, chargeY - 2);
        u8g2.drawLine(chargeX + 1, chargeY - 1, chargeX, chargeY - 2);
      }
    }
  } else {
    u8g2.setDrawColor(ink);
    u8g2.drawLine(x + 2, y - frameH + 2, x + frameW - 2, y - 2);
    u8g2.drawLine(x + frameW - 2, y - frameH + 2, x + 2, y - 2);
  }

  static char buf[8];
  if (!batteryConnected) {
    snprintf(buf, sizeof(buf), "NO BAT");
  } else if (isCharging) {
    snprintf(buf, sizeof(buf), "CHG");
  } else {
    snprintf(buf, sizeof(buf), "%d%%", pct);
  }
  u8g2.setDrawColor(ink);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.setCursor(x - tw - 2, y);
  u8g2.print(buf);
  u8g2.setDrawColor(1);
}

void SceneManager::drawNoDataCross(int x, int y, int w, int h) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.drawLine(x, y, x + w - 1, y + h - 1);
  u8g2.drawLine(x + w - 1, y, x, y + h - 1);
  u8g2.setFontMode(1);
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
static int submenuCountForCategory(int cat) {
  switch (cat) {
  case 0:
    return 5;  // Config: AUTO, FLIP, GLITCH, LED, DIM
  case 1:
    return 20; // WiFi: Full Marauder menu
  case 2:
    return 22; // Tools: BLE + Network + Other tools
  case 3:
    return 3;  // REBOOT, VERSION, EXIT
  case 4:
    return 1;  // Games: RACING (Forza)
  default:
    return 3;
  }
}

void SceneManager::drawMenu(int menuLevel, int menuCategory, int mainIndex,
                            bool carouselOn, int carouselSec,
                            bool screenRotated, bool glitchEnabled,
                            bool ledEnabled, bool lowBrightnessDefault,
                            bool rebootConfirmed) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int boxX = NOCT_MENU_BOX_X;
  const int boxY = NOCT_MENU_BOX_Y;
  const int boxW = NOCT_MENU_BOX_W;
  const int boxH = NOCT_MENU_BOX_H;

  u8g2.setDrawColor(0);
  u8g2.drawBox(boxX, boxY, boxW, boxH);
  u8g2.setDrawColor(1);
  disp_.drawTechFrame(boxX, boxY, boxW, boxH);

  static const char *categoryNames[] = {"Config", "WiFi", "Tools", "System",
                                        "Games"};
  static char items[25][20]; // Increased for expanded menus
  int count;
  const char *headerStr;

  if (menuLevel == 0) {
    count = 5;
    headerStr = "// MENU";
    for (int i = 0; i < 5; i++) {
      strncpy(items[i], categoryNames[i], sizeof(items[i]) - 1);
      items[i][sizeof(items[i]) - 1] = '\0';
    }
  } else {
    count = submenuCountForCategory(menuCategory);
    headerStr = categoryNames[menuCategory];
    if (menuCategory == 0) {
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
    } else if (menuCategory == 1) {
      // WiFi Scans
      strncpy(items[0], "AP SCAN", sizeof(items[0]) - 1);
      strncpy(items[1], "PROBE SCAN", sizeof(items[1]) - 1);
      strncpy(items[2], "EAPOL CAPTURE", sizeof(items[2]) - 1);
      strncpy(items[3], "STATION SCAN", sizeof(items[3]) - 1);
      strncpy(items[4], "PACKET MONITOR", sizeof(items[4]) - 1);
      strncpy(items[5], "CHAN ANALYZER", sizeof(items[5]) - 1);
      strncpy(items[6], "CHAN ACTIVITY", sizeof(items[6]) - 1);
      strncpy(items[7], "PACKET RATE", sizeof(items[7]) - 1);
      strncpy(items[8], "PINESCAN", sizeof(items[8]) - 1);
      strncpy(items[9], "MULTISSID", sizeof(items[9]) - 1);
      strncpy(items[10], "SIG STRENGTH", sizeof(items[10]) - 1);
      strncpy(items[11], "RAW CAPTURE", sizeof(items[11]) - 1);
      strncpy(items[12], "AP+STA SCAN", sizeof(items[12]) - 1);
      // WiFi Attacks
      strncpy(items[13], "DEAUTH", sizeof(items[13]) - 1);
      strncpy(items[14], "DEAUTH TARGET", sizeof(items[14]) - 1);
      strncpy(items[15], "DEAUTH MANUAL", sizeof(items[15]) - 1);
      strncpy(items[16], "BEACON SPAM", sizeof(items[16]) - 1);
      strncpy(items[17], "BEACON RICKROLL", sizeof(items[17]) - 1);
      strncpy(items[18], "AUTH ATTACK", sizeof(items[18]) - 1);
      strncpy(items[19], "EVIL PORTAL", sizeof(items[19]) - 1);
      for (int i = 0; i < 20; i++)
        items[i][sizeof(items[0]) - 1] = '\0';
    } else if (menuCategory == 2) {
      // BLE Scans
      strncpy(items[0], "BLE SCAN", sizeof(items[0]) - 1);
      strncpy(items[1], "BLE SKIMMERS", sizeof(items[1]) - 1);
      strncpy(items[2], "BLE AIRTAG", sizeof(items[2]) - 1);
      strncpy(items[3], "BLE AIRTAG MON", sizeof(items[3]) - 1);
      strncpy(items[4], "BLE FLIPPER", sizeof(items[4]) - 1);
      strncpy(items[5], "BLE ANALYZER", sizeof(items[5]) - 1);
      // BLE Attacks
      strncpy(items[6], "BLE SPAM", sizeof(items[6]) - 1);
      strncpy(items[7], "SOUR APPLE", sizeof(items[7]) - 1);
      strncpy(items[8], "SWIFTPAIR MS", sizeof(items[8]) - 1);
      strncpy(items[9], "SWIFTPAIR GOOGLE", sizeof(items[9]) - 1);
      strncpy(items[10], "SWIFTPAIR SAMSUNG", sizeof(items[10]) - 1);
      strncpy(items[11], "FLIPPER SPAM", sizeof(items[11]) - 1);
      // Network Scans
      strncpy(items[12], "ARP SCAN", sizeof(items[12]) - 1);
      strncpy(items[13], "PORT SCAN", sizeof(items[13]) - 1);
      strncpy(items[14], "PING SCAN", sizeof(items[14]) - 1);
      strncpy(items[15], "DNS SCAN", sizeof(items[15]) - 1);
      strncpy(items[16], "HTTP SCAN", sizeof(items[16]) - 1);
      strncpy(items[17], "HTTPS SCAN", sizeof(items[17]) - 1);
      strncpy(items[18], "SMTP SCAN", sizeof(items[18]) - 1);
      strncpy(items[19], "RDP SCAN", sizeof(items[19]) - 1);
      strncpy(items[20], "TELNET SCAN", sizeof(items[20]) - 1);
      strncpy(items[21], "SSH SCAN", sizeof(items[21]) - 1);
      // Tools
      strncpy(items[22], "USB HID", sizeof(items[22]) - 1);
      // Removed: VAULT, FAKE LOGIN, QR, DAEMON (moved to submenu or removed)
      for (int i = 0; i < 23; i++)
        items[i][sizeof(items[0]) - 1] = '\0';
    } else if (menuCategory == 4) {
      strncpy(items[0], "RACING", sizeof(items[0]) - 1);
      items[0][sizeof(items[0]) - 1] = '\0';
    } else {
      if (rebootConfirmed)
        strncpy(items[0], "REBOOT [OK]", sizeof(items[0]) - 1);
      else
        strncpy(items[0], "REBOOT", sizeof(items[0]) - 1);
      items[0][sizeof(items[0]) - 1] = '\0';
      strncpy(items[1], "VERSION", sizeof(items[1]) - 1);
      items[1][sizeof(items[1]) - 1] = '\0';
      strncpy(items[2], "EXIT", sizeof(items[2]) - 1);
      items[2][sizeof(items[2]) - 1] = '\0';
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

  for (int r = 0; r < NOCT_MENU_VISIBLE_ROWS; r++) {
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

    if (i == selected) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(NOCT_MENU_LIST_LEFT, y - 6, NOCT_MENU_LIST_W,
                   NOCT_MENU_ROW_H);
      u8g2.setDrawColor(0);
      u8g2.drawUTF8(NOCT_MENU_LIST_LEFT + 2, y, ">");
      u8g2.drawUTF8(NOCT_MENU_LIST_LEFT + NOCT_MENU_LIST_W - 6, y, "<");
      u8g2.drawUTF8(textX, y, itemText);
      u8g2.setDrawColor(1);
    } else {
      u8g2.setDrawColor(1);
      u8g2.drawUTF8(textX, y, itemText);
    }
  }

  disp_.drawScrollIndicator(startY - 2,
                            NOCT_MENU_VISIBLE_ROWS * NOCT_MENU_ROW_H, count,
                            NOCT_MENU_VISIBLE_ROWS, firstVisible);

  /* Hint: how to use the menu */
  u8g2.setFont(UNIT_FONT);
  const char *hint = "1x next  2s ok  2x back";
  int hintW = u8g2.getUTF8Width(hint);
  int hintX = boxX + (boxW - hintW) / 2;
  if (hintX < NOCT_MENU_LIST_LEFT)
    hintX = NOCT_MENU_LIST_LEFT;
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(hintX, boxY + boxH - 2, hint);
}

void SceneManager::drawToast(const char *msg) {
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
  if (x < 2)
    x = 2;
  u8g2.drawUTF8(x, barY + barH - 3, msg);
  u8g2.setDrawColor(1);
}

void SceneManager::drawNoSignal(bool wifiOk, bool tcpOk, int rssi,
                                bool blinkState) {
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

void SceneManager::drawConnecting(int rssi, bool blinkState) {
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
void SceneManager::drawIdleScreensaver(unsigned long now) {
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
                              bool tcpConnected, int rssi) {
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

  if (hasData) {
    cpuTemp = (hw.ct > 0) ? hw.ct : 0;
    gpuTemp = (hw.gt > 0) ? hw.gt : 0;
    cpuLoad = (hw.cl >= 0 && hw.cl <= 100) ? hw.cl : 0;
    gpuLoad = (hw.gl >= 0 && hw.gl <= 100) ? hw.gl : 0;
    ramUsage = (hw.ra > 0) ? (hw.ru / hw.ra) * 100.0f : 0.0f;

    // Обновление кэша только при изменении или раз в секунду
    unsigned long now = millis();
    if (now - lastUpdate > 1000 || cpuTemp != lastCpuTemp ||
        gpuTemp != lastGpuTemp || cpuLoad != lastCpuLoad ||
        gpuLoad != lastGpuLoad || abs(ramUsage - lastRamUsage) > 0.5f) {
      lastCpuTemp = cpuTemp;
      lastGpuTemp = gpuTemp;
      lastCpuLoad = cpuLoad;
      lastGpuLoad = gpuLoad;
      lastRamUsage = ramUsage;
      lastUpdate = now;
    } else {
      // Использование кэшированных значений
      cpuTemp = lastCpuTemp;
      gpuTemp = lastGpuTemp;
      cpuLoad = lastCpuLoad;
      gpuLoad = lastGpuLoad;
      ramUsage = lastRamUsage;
    }
  } else {
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

  if (criticalAlert) {
    // Критическое состояние - всегда агрессивная анимация
    wolfSprite = wolf_aggressive;
  } else if (alert) {
    // HUNTING режим - чередование агрессивной и обычной анимации
    int huntPhase = (int)((t / 400) % 6);
    if (huntPhase < 3) {
      wolfSprite = wolf_aggressive;
    } else {
      wolfSprite = wolf_idle;
    }
  } else if (!hasData) {
    // Моргание при отсутствии данных
    int blinkPhase = (int)((t / 300) % 4);
    if (blinkPhase == 0 || blinkPhase == 2) {
      wolfSprite = wolf_blink;
    } else {
      wolfSprite = wolf_idle;
    }
  } else {
    // Плавная анимация: idle -> blink -> funny -> idle
    int phase = (int)((t / 600) % 16); // Более медленная смена фаз
    if (phase == 10) {
      wolfSprite = wolf_blink;
    } else if (phase >= 11 && phase <= 13) {
      wolfSprite = wolf_funny;
    } else {
      wolfSprite = wolf_idle;
    }
  }

  u8g2.setDrawColor(1);
  u8g2.drawXBMP(4, 16, 32, 32, wolfSprite);
  u8g2.drawFrame(0, 10, 42, 44);
  u8g2.drawBox(40, 20, 4, 2);

  const int dataX = 48;
  const int barW = 70;
  const int rowH = 14;
  int y = 16;

  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setCursor(dataX, y);
  if (!hasData) {
    u8g2.print("STATUS: NO DATA");
  } else {
    if (criticalAlert) {
      u8g2.print("STATUS: CRITICAL");
    } else if (alert) {
      u8g2.print("STATUS: HUNTING");
    } else {
      u8g2.print("STATUS: IDLE");
    }
  }

  // Индикация сетевой активности справа от статуса
  if (wifiConnected) {
    int netX = dataX + barW - 20;
    if (tcpConnected) {
      u8g2.setCursor(netX, y);
      u8g2.print("[TCP]");
    } else {
      u8g2.setCursor(netX, y);
      u8g2.print("[WiFi]");
    }
  }

  y += rowH;

  // Статистика времени работы системы (если есть данные)
  if (hasData && bootTime > 0) {
    unsigned long uptimeSec = (millis() - bootTime) / 1000;
    unsigned long hours = uptimeSec / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    u8g2.setCursor(dataX, y);
    u8g2.printf("UPTIME: %lu:%02lu", hours, minutes);
    y += rowH;
  }

  if (!hasData) {
    // Отображение сообщения об отсутствии данных
    u8g2.setCursor(dataX, y);
    u8g2.print("Waiting for");
    y += rowH;
    u8g2.setCursor(dataX, y);
    u8g2.print("telemetry...");
  } else {
    // Отображение данных с оптимизированными вычислениями прогресс-баров
    u8g2.setCursor(dataX, y);
    u8g2.printf("CPU: %d C %d%%", cpuTemp, cpuLoad);
    u8g2.drawFrame(dataX, y + 2, barW, 3);
    int cpuBarWidth = (cpuLoad * (barW - 2) + 50) / 100;
    if (cpuBarWidth > 0) {
      u8g2.drawBox(dataX + 1, y + 3, cpuBarWidth, 1);
    }
    y += rowH;

    u8g2.setCursor(dataX, y);
    u8g2.printf("GPU: %d C %d%%", gpuTemp, gpuLoad);
    u8g2.drawFrame(dataX, y + 2, barW, 3);
    int gpuBarWidth = (gpuLoad * (barW - 2) + 50) / 100;
    if (gpuBarWidth > 0) {
      u8g2.drawBox(dataX + 1, y + 3, gpuBarWidth, 1);
    }
    y += rowH;

    u8g2.setCursor(dataX, y);
    u8g2.printf("RAM: %0.1f%%", ramUsage);
    u8g2.drawFrame(dataX, y + 2, barW, 3);
    int ramBarWidth = ((int)(ramUsage + 0.5f) * (barW - 2) + 50) / 100;
    if (ramBarWidth > 0) {
      u8g2.drawBox(dataX + 1, y + 3, ramBarWidth, 1);
    }
  }
}

// ===========================================================================
// NETRUNNER MODE: WiFi Scanner — more networks, fast scroll, JAMMER status
// ===========================================================================
void SceneManager::drawWiFiScanner(int selectedIndex, int pageOffset,
                                   int *sortedIndices, int filteredCount,
                                   const char *footerOverride) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);

  // If footerOverride is provided and no WiFi scan data, show simple status
  if (footerOverride && filteredCount == 0 && sortedIndices == nullptr) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, NOCT_DISP_W, 10);
    u8g2.setDrawColor(0);
    u8g2.setCursor(2, 7);
    u8g2.print("ACTIVE");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 10, NOCT_DISP_W, 10);
    u8g2.setCursor(2, 20);
    char footer[48];
    strncpy(footer, footerOverride, sizeof(footer) - 1);
    footer[sizeof(footer) - 1] = '\0';
    u8g2.print(footer);
    u8g2.setCursor(2, 30);
    u8g2.print("2x BACK to exit");
    u8g2.setCursor(2, 40);
    u8g2.print("Mode running...");
    return;
  }

  int n = WiFi.scanComplete();
  bool useFiltered = (sortedIndices != nullptr && filteredCount > 0);
  int displayCount = useFiltered ? filteredCount : n;

  // If no WiFi scan data and no footer, show a default message
  if (n == 0 && displayCount == 0 && !footerOverride) {
    disp_.drawCentered(32, "NO DATA");
    return;
  }

  if (n == -2) {
    disp_.drawCentered(32, "INIT RADAR...");
    return;
  }
  if (n == -1) {
    disp_.drawCentered(25, "SCANNING...");
    u8g2.drawFrame(34, 35, 60, 4);
    int w = (millis() / 10) % 60;
    u8g2.drawBox(34, 35, w, 4);
    return;
  }
  if (displayCount == 0 && !footerOverride) {
    disp_.drawCentered(32, "NO SIGNALS");
    disp_.drawCentered(45, "[PRESS TO RESCAN]");
    return;
  }

  // If we have footer but no scan data, skip WiFi scan display logic
  if (displayCount == 0 && footerOverride) {
    // Already handled above
    return;
  }

  // --- LIST RENDER (full screen, no header) ---
  u8g2.setCursor(2, 2);
  if (useFiltered && filteredCount < n) {
    u8g2.printf("TARGETS: %d/%d", filteredCount, n);
  } else {
    u8g2.printf("TARGETS: %d", displayCount);
  }
  u8g2.drawLine(0, 10, 128, 10);

  int yStart = 12;
  int h = 10;

  for (int i = pageOffset;
       i < (pageOffset + 5 < displayCount ? pageOffset + 5 : displayCount);
       i++) {
    int actualIndex = useFiltered ? sortedIndices[i] : i;
    int y = yStart + ((i - pageOffset) * h);

    if (i == selectedIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y - 8, 128, h);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }

    // Copy SSID to local String so c_str() remains valid; then to buffer
    String ssidStr = WiFi.SSID(actualIndex);
    const char *ssid = ssidStr.c_str();
    char ssidBuf[12]; // Max 10 chars + "." + null terminator
    if (!ssid || ssidStr.length() == 0) {
      strncpy(ssidBuf, "[HIDDEN]", sizeof(ssidBuf) - 1);
    } else {
      size_t len = ssidStr.length();
      if (len > 10) {
        strncpy(ssidBuf, ssid, 9);
        ssidBuf[9] = '.';
        ssidBuf[10] = '\0';
      } else {
        strncpy(ssidBuf, ssid, sizeof(ssidBuf) - 1);
        ssidBuf[sizeof(ssidBuf) - 1] = '\0';
      }
    }
    u8g2.setCursor(2, y);
    u8g2.print(ssidBuf);

    // RSSI и канал
    int rssi = WiFi.RSSI(actualIndex);
    int channel = WiFi.channel(actualIndex);
    u8g2.setCursor(70, y);
    u8g2.printf("%d CH%d", rssi, channel);

    // Индикация шифрования
    wifi_auth_mode_t auth = WiFi.encryptionType(actualIndex);
    if (auth != WIFI_AUTH_OPEN) {
      u8g2.setCursor(120, y);
      if (auth == WIFI_AUTH_WPA3_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK) {
        u8g2.print("3"); // WPA3
      } else {
        u8g2.print("*"); // WPA/WPA2
      }
    }
    u8g2.setDrawColor(1);
  }

  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 56, 128, 56);
  u8g2.setCursor(2, 62);
  if (footerOverride && footerOverride[0] != '\0') {
    u8g2.print(footerOverride);
  } else if (WiFi.status() == WL_CONNECTED) {
    u8g2.print("LINK: ONLINE");
  } else {
    u8g2.print("LINK: DISCONNECTED");
  }
}

// ---------------------------------------------------------------------------
// BLE PHANTOM SPAMMER: status bar, pulsing BT icon, packet count, glitch in
// main
// ---------------------------------------------------------------------------
#define PHANTOM_HEADER_H 10
#define PHANTOM_BT_CX 64
#define PHANTOM_BT_CY 32
#define PHANTOM_BT_R 12

static void drawBtIcon(U8G2 &u8g2, int cx, int cy, int r, int pulse) {
  int R = r + (pulse / 2);
  if (R < 4)
    R = 4;
  u8g2.drawCircle(cx, cy, R);
  u8g2.drawCircle(cx, cy, R - 2);
  int tip = 4;
  u8g2.drawTriangle(cx - tip, cy - R + 2, cx - tip, cy + R - 2, cx + tip, cy);
  u8g2.drawLine(cx - tip, cy - R + 2, cx + R - 2, cy - 4);
  u8g2.drawLine(cx - tip, cy + R - 2, cx + R - 2, cy + 4);
}

void SceneManager::drawBleSpammer(int packetCount) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  // Top bar: STATUS: PHANTOM ACTIVE
  u8g2.drawBox(0, 0, NOCT_DISP_W, PHANTOM_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("BLE SPAM");
  u8g2.setDrawColor(1);

  // Pulsing Bluetooth icon (center)
  unsigned long t = millis() / 100;
  int pulse = (int)(t % 8) - 4;
  if (pulse < 0)
    pulse = -pulse;
  drawBtIcon(u8g2, PHANTOM_BT_CX, PHANTOM_BT_CY, PHANTOM_BT_R, pulse);

  // Packet counter
  u8g2.setCursor(20, 54);
  u8g2.print("PACKETS SENT: [");
  u8g2.print(packetCount);
  u8g2.print("]");

  disp_.drawGreebles();
}

// ---------------------------------------------------------------------------
// BADWOLF USB HID: skull with wolf ears, ARMED, Short=Matrix / Long=Sniffer
// ---------------------------------------------------------------------------
#define BADWOLF_HEADER_H 10
#define BADWOLF_ICON_CX 64
#define BADWOLF_ICON_CY 28
#define BADWOLF_SKULL_R 10
#define BADWOLF_FOOTER_Y 52

static const char *const badWolfScriptNames[] = {"Matrix", "Sniffer", "CMD",
                                                 "Process", "Backdoor"};

void SceneManager::drawBadWolf(int scriptIndex) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  u8g2.drawBox(0, 0, NOCT_DISP_W, BADWOLF_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("USB HID");
  u8g2.setDrawColor(1);

  int cx = BADWOLF_ICON_CX;
  int cy = BADWOLF_ICON_CY;
  u8g2.drawCircle(cx, cy, BADWOLF_SKULL_R);
  u8g2.drawCircle(cx - 4, cy - 2, 2);
  u8g2.drawCircle(cx + 4, cy - 2, 2);
  u8g2.drawTriangle(cx - 10, cy - 8, cx - 6, cy - 14, cx - 2, cy - 8);
  u8g2.drawTriangle(cx + 2, cy - 8, cx + 6, cy - 14, cx + 10, cy - 8);

  if (scriptIndex >= 0 && scriptIndex <= 4) {
    u8g2.setCursor(2, 42);
    u8g2.print("Script: ");
    u8g2.print(badWolfScriptNames[scriptIndex]);
  }

  u8g2.drawLine(0, BADWOLF_FOOTER_Y - 1, NOCT_DISP_W, BADWOLF_FOOTER_Y - 1);
  u8g2.setCursor(2, BADWOLF_FOOTER_Y + 6);
  u8g2.print("1x next | 2s run | 3x Out");

  disp_.drawGreebles();
}

// --- SILENCE (868 MHz Jammer) ---
#define SILENCE_HEADER_H 10
#define SILENCE_FOOTER_Y 56

void SceneManager::drawSilenceMode(int8_t power) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  u8g2.drawBox(0, 0, NOCT_DISP_W, SILENCE_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("JAM 868");
  u8g2.setDrawColor(1);

  // Static noise: intensity increases with time (jammer run duration)
  unsigned long t = millis() / 500;
  int noisePixels = 80 + (int)(t % 120);
  for (int i = 0; i < noisePixels; i++)
    u8g2.drawPixel(
        2 + (esp_random() % (NOCT_DISP_W - 4)),
        SILENCE_HEADER_H + 2 +
            (esp_random() % (SILENCE_FOOTER_Y - SILENCE_HEADER_H - 6)));

  // Muted speaker (cone + X) + wolf silhouette
  int sx = 28;
  int sy = 28;
  u8g2.drawLine(sx - 10, sy, sx + 2, sy - 8);
  u8g2.drawLine(sx - 10, sy, sx + 2, sy + 8);
  u8g2.drawLine(sx + 2, sy - 8, sx + 10, sy - 4);
  u8g2.drawLine(sx + 2, sy + 8, sx + 10, sy + 4);
  u8g2.drawLine(sx + 10, sy - 4, sx + 10, sy + 4);
  u8g2.drawLine(sx - 12, sy - 6, sx + 12, sy + 6);
  u8g2.drawLine(sx - 12, sy + 6, sx + 12, sy - 6);
  int wx = 88;
  int wy = 28;
  u8g2.drawDisc(wx, wy, 7);
  u8g2.drawTriangle(wx - 6, wy - 6, wx - 2, wy - 10, wx + 2, wy - 6);
  u8g2.drawTriangle(wx + 2, wy - 6, wx + 6, wy - 10, wx + 6, wy - 4);

  u8g2.drawLine(0, SILENCE_FOOTER_Y - 1, NOCT_DISP_W, SILENCE_FOOTER_Y - 1);
  u8g2.setCursor(2, SILENCE_FOOTER_Y + 5);
  static char powerBuf[16];
  snprintf(powerBuf, sizeof(powerBuf), "+%ddBm | 3x Out", power);
  u8g2.print(powerBuf);

  disp_.drawGreebles();
}

// --- TRAP (Evil Twin / Captive Portal) ---
#define TRAP_HEADER_H 10
#define TRAP_WEB_CX 64
#define TRAP_WEB_CY 28
#define TRAP_FOOTER_Y 56

void SceneManager::drawTrapMode(int clientCount, int logsCaptured,
                                const char *lastPassword,
                                unsigned long passwordShowUntil,
                                const char *clonedSSID) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  u8g2.drawBox(0, 0, NOCT_DISP_W, TRAP_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("PORTAL");
  if (clonedSSID && clonedSSID[0] != '\0') {
    u8g2.setCursor(60, 7);
    u8g2.print("[CLONE]");
  }
  u8g2.setDrawColor(1);

  unsigned long now = millis();
  bool showBite =
      lastPassword && lastPassword[0] != '\0' && now < passwordShowUntil;

  if (showBite) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, TRAP_HEADER_H, NOCT_DISP_W, NOCT_DISP_H - TRAP_HEADER_H);
    u8g2.setDrawColor(1);
    u8g2.drawXBM(48, 12, 32, 32, wolf_aggressive);
    u8g2.setCursor(2, 50);
    u8g2.print("BITE:");
    u8g2.setCursor(2, 58);
    u8g2.print(lastPassword);
  } else {
    int cx = TRAP_WEB_CX;
    int cy = TRAP_WEB_CY;
    for (int i = 0; i < 8; i++) {
      double a = (i * 2.0 * 3.14159265359) / 8.0;
      int x2 = cx + (int)(24 * cos(a));
      int y2 = cy + (int)(24 * sin(a));
      u8g2.drawLine(cx, cy, x2, y2);
    }
    u8g2.drawCircle(cx, cy, 8);
    u8g2.drawCircle(cx, cy, 16);
    u8g2.drawCircle(cx, cy, 24);

    static char buf[24];
    snprintf(buf, sizeof(buf), "CLIENTS: %d", clientCount);
    u8g2.setCursor(2, 22);
    u8g2.print(buf);
    snprintf(buf, sizeof(buf), "LOGS: %d", logsCaptured);
    u8g2.setCursor(2, 32);
    u8g2.print(buf);
  }

  u8g2.drawLine(0, TRAP_FOOTER_Y - 1, NOCT_DISP_W, TRAP_FOOTER_Y - 1);
  u8g2.setCursor(2, TRAP_FOOTER_Y + 5);
  u8g2.print("3x Out");

  disp_.drawGreebles();
}

// --- KICK (WiFi Deauth) ---
#define KICK_ROW_DY 10
#define KICK_Y_TITLE 2
#define KICK_Y_TARGET 12
#define KICK_Y_BSSID 22
#define KICK_Y_STATUS 32
#define KICK_Y_PKTS 42
#define KICK_Y_WARN 52

void SceneManager::drawKickMode(KickManager &kick) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  bool attacking = kick.isAttacking();

  u8g2.setCursor(2, KICK_Y_TITLE);
  u8g2.print("DEAUTH");
  u8g2.drawLine(0, 9, NOCT_DISP_W, 9);

  static char targetBuf[44];
  static char bssidBuf[20];
  kick.getTargetSSID(targetBuf, sizeof(targetBuf));
  kick.getTargetBSSIDStr(bssidBuf, sizeof(bssidBuf));
  const char *targetStr = targetBuf[0]  ? targetBuf
                          : bssidBuf[0] ? bssidBuf
                                        : "(none)";
  char targetShort[18];
  strncpy(targetShort, targetStr, sizeof(targetShort) - 1);
  targetShort[sizeof(targetShort) - 1] = '\0';
  if (strlen(targetStr) > sizeof(targetShort) - 1)
    targetShort[sizeof(targetShort) - 2] = '.';

  u8g2.setCursor(2, KICK_Y_TARGET);
  u8g2.print("TGT: ");
  u8g2.print(targetShort);

  u8g2.setCursor(2, KICK_Y_BSSID);
  u8g2.print(bssidBuf);

  u8g2.setCursor(2, KICK_Y_STATUS);
  u8g2.print(attacking ? "STATUS: INJECTING" : "STATUS: IDLE");

  u8g2.setCursor(2, KICK_Y_PKTS);
  u8g2.print("PKTS: ");
  u8g2.print(kick.getPacketCount());

  if (kick.isTargetProtected()) {
    u8g2.setCursor(2, KICK_Y_WARN);
    u8g2.print("WPA3/PMF - may fail");
  }
}

// --- VAULT (TOTP 2FA) ---
#define VAULT_HEADER_H 10
#define VAULT_CHEST_X 24
#define VAULT_CHEST_Y 22
#define VAULT_FOOTER_Y 56

void SceneManager::drawVaultMode(const char *accountName, const char *code6,
                                 int countdownSec) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  u8g2.drawBox(0, 0, NOCT_DISP_W, VAULT_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("VAULT");
  u8g2.setDrawColor(1);

  int cx = VAULT_CHEST_X;
  int cy = VAULT_CHEST_Y;
  u8g2.drawRFrame(cx - 2, cy - 4, 20, 14, 2);
  u8g2.drawRFrame(cx + 2, cy - 2, 12, 10, 1);
  u8g2.drawCircle(cx + 8, cy + 2, 2);
  u8g2.drawDisc(cx + 8, cy + 2, 1);
  u8g2.drawTriangle(cx + 28, cy - 2, cx + 36, cy + 2, cx + 28, cy + 6);
  u8g2.drawTriangle(cx + 32, cy, cx + 38, cy + 2, cx + 32, cy + 4);

  u8g2.setCursor(2, 14);
  u8g2.print(accountName && accountName[0] ? accountName : "(no account)");
  u8g2.setFont(u8g2_font_logisoso24_tr);
  int codeW = code6 ? u8g2.getUTF8Width(code6) : 0;
  u8g2.setCursor((NOCT_DISP_W - codeW) / 2, 44);
  u8g2.print(code6 && code6[0] ? code6 : "------");
  u8g2.setFont(TINY_FONT);
  int pct = (countdownSec >= 0 && countdownSec <= 30)
                ? (int)((countdownSec * 100) / 30)
                : 100;
  disp_.drawProgressBar(4, 48, NOCT_DISP_W - 8, 4, 100 - pct);
  u8g2.setCursor(2, VAULT_FOOTER_Y + 5);
  u8g2.print("Short: next account | 3x Out");

  disp_.drawGreebles();
}

// --- BEACON SPAM ---
void SceneManager::drawBeaconMode(const char *ssid, int beaconCount, int index,
                                  int total) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, NOCT_DISP_W, 10);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("BEACON");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, NOCT_DISP_W, 10);
  u8g2.setCursor(2, 18);
  u8g2.print(ssid && ssid[0] ? ssid : "(none)");
  u8g2.setCursor(2, 28);
  u8g2.print("PKTS: ");
  u8g2.print(beaconCount);
  u8g2.setCursor(2, 38);
  u8g2.print("SSID ");
  u8g2.print(index + 1);
  u8g2.print("/");
  u8g2.print(total);
  u8g2.setCursor(2, 54);
  u8g2.print("1x next SSID | 2x back");
  disp_.drawGreebles();
}

// --- WIFI SNIFF ---
void SceneManager::drawWifiSniffMode(int selected, WifiSniffManager &mgr) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, NOCT_DISP_W, 10);
  u8g2.setDrawColor(0);

  // Show mode name in header
  const char *modeName = "SNIFF";
  SniffMode mode = mgr.getMode();
  switch (mode) {
  case SNIFF_MODE_PROBE_SCAN:
    modeName = "PROBE";
    break;
  case SNIFF_MODE_EAPOL_CAPTURE:
    modeName = "EAPOL";
    break;
  case SNIFF_MODE_STATION_SCAN:
    modeName = "STATION";
    break;
  case SNIFF_MODE_PACKET_MONITOR:
    modeName = "PKT MON";
    break;
  case SNIFF_MODE_CHANNEL_ANALYZER:
    modeName = "CH ANALYZ";
    break;
  case SNIFF_MODE_CHANNEL_ACTIVITY:
    modeName = "CH ACT";
    break;
  case SNIFF_MODE_PACKET_RATE:
    modeName = "PKT RATE";
    break;
  case SNIFF_MODE_PINESCAN:
    modeName = "PINESCAN";
    break;
  case SNIFF_MODE_MULTISSID:
    modeName = "MULTISSID";
    break;
  case SNIFF_MODE_SIGNAL_STRENGTH:
    modeName = "SIG STR";
    break;
  case SNIFF_MODE_RAW_CAPTURE:
    modeName = "RAW CAP";
    break;
  case SNIFF_MODE_AP_STA:
    modeName = "AP+STA";
    break;
  default:
    break;
  }

  u8g2.setCursor(2, 7);
  u8g2.print(modeName);
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, NOCT_DISP_W, 10);

  int n = mgr.getApCount();
  int pkts = mgr.getPacketCount();
  int eapol = mgr.getEapolCount();

  // Show different stats based on mode
  if (mode == SNIFF_MODE_PROBE_SCAN) {
    int probeCount = mgr.getProbeCount();
    u8g2.setCursor(2, 18);
    u8g2.print("Probes: ");
    u8g2.print(probeCount);
  } else if (mode == SNIFF_MODE_PACKET_RATE) {
    uint32_t pps = mgr.getPacketsPerSecond();
    u8g2.setCursor(2, 18);
    u8g2.print("PPS: ");
    u8g2.print(pps);
  } else if (mode == SNIFF_MODE_CHANNEL_ACTIVITY ||
             mode == SNIFF_MODE_CHANNEL_ANALYZER) {
    uint32_t channels[14];
    mgr.getChannelActivity(channels, 14);
    u8g2.setCursor(2, 18);
    u8g2.print("Ch Activity");
  } else {
    u8g2.setCursor(2, 18);
    u8g2.print("PKTS:");
    u8g2.print(pkts);
    u8g2.print(" EAPOL:");
    u8g2.print(eapol);
  }

  if (n > 0 && selected < n) {
    const WifiSniffAp *ap = mgr.getAp(selected);
    if (ap) {
      u8g2.setCursor(2, 28);
      char line[22];
      strncpy(line, ap->ssid, 18);
      line[18] = '\0';
      if (strlen(ap->ssid) > 18)
        line[16] = '.';
      u8g2.print(line);
      u8g2.setCursor(2, 38);
      u8g2.print(ap->bssidStr);
      u8g2.setCursor(2, 48);
      u8g2.print("CH:");
      u8g2.print((int)ap->channel);
      u8g2.print(" RSSI:");
      u8g2.print(ap->rssi);
      if (ap->hasEapol)
        u8g2.print(" [EAPOL]");
    }
  }
  u8g2.setCursor(2, 58);
  u8g2.print("1x next | 2x back");
  disp_.drawGreebles();
}

// --- BLE SCAN ---
void SceneManager::drawBleScanMode(int selected, BleManager &mgr) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, NOCT_DISP_W, 10);
  u8g2.setDrawColor(0);

  // Show scan type in header
  const char *scanTypeName = "BLE SCAN";
  BleScanType scanType = mgr.getScanType();
  switch (scanType) {
  case BLE_SCAN_SKIMMERS:
    scanTypeName = "SKIMMERS";
    break;
  case BLE_SCAN_AIRTAG:
    scanTypeName = "AIRTAG";
    break;
  case BLE_SCAN_FLIPPER:
    scanTypeName = "FLIPPER";
    break;
  case BLE_SCAN_FLOCK:
    scanTypeName = "FLOCK";
    break;
  case BLE_SCAN_ANALYZER:
    scanTypeName = "ANALYZER";
    break;
  case BLE_SCAN_SIMPLE:
    scanTypeName = "SIMPLE";
    break;
  case BLE_SCAN_SIMPLE_TWO:
    scanTypeName = "SIMPLE2";
    break;
  default:
    break;
  }

  u8g2.setCursor(2, 7);
  u8g2.print(scanTypeName);
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, NOCT_DISP_W, 10);

  // Show count based on scan type
  int n = 0;
  if (scanType == BLE_SCAN_SKIMMERS) {
    n = mgr.getSkimmerCount();
  } else if (scanType == BLE_SCAN_AIRTAG || mgr.isAirTagMonitoring()) {
    n = mgr.getAirTagCount();
  } else if (scanType == BLE_SCAN_FLIPPER) {
    n = mgr.getFlipperCount();
  } else if (scanType == BLE_SCAN_FLOCK) {
    n = mgr.getFlockCount();
  } else if (scanType == BLE_SCAN_ANALYZER) {
    n = mgr.getAnalyzerValue();
  } else {
    n = mgr.getScanCount();
  }

  u8g2.setCursor(2, 18);
  u8g2.print("Count: ");
  u8g2.print(n);

  if (mgr.isCloning()) {
    u8g2.setCursor(2, 28);
    u8g2.print("CLONE MODE");
  } else if (n > 0 && selected < n) {
    const BleScanDevice *dev = nullptr;
    if (scanType == BLE_SCAN_SKIMMERS) {
      dev = mgr.getSkimmer(selected);
    } else if (scanType == BLE_SCAN_AIRTAG || mgr.isAirTagMonitoring()) {
      dev = mgr.getAirTag(selected);
    } else if (scanType == BLE_SCAN_FLIPPER) {
      dev = mgr.getFlipper(selected);
    } else if (scanType == BLE_SCAN_FLOCK) {
      dev = mgr.getFlock(selected);
    } else {
      dev = mgr.getScanDevice(selected);
    }
    if (dev) {
      u8g2.setCursor(2, 28);
      char line[20];
      strncpy(line, dev->name, 18);
      line[18] = '\0';
      u8g2.print(line);
      u8g2.setCursor(2, 38);
      u8g2.print(dev->addr);
      u8g2.setCursor(2, 48);
      u8g2.print("RSSI: ");
      u8g2.print(dev->rssi);
    }
  }
  u8g2.setCursor(2, 58);
  u8g2.print("1x next | L=clone | 2x back");
  disp_.drawGreebles();
}

// --- FAKE LOGIN ---
void SceneManager::drawFakeLoginMode() {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, NOCT_DISP_W, NOCT_DISP_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(20, 14);
  u8g2.print("Windows Security");
  u8g2.drawLine(10, 18, NOCT_DISP_W - 10, 18);
  u8g2.setCursor(14, 30);
  u8g2.print("Enter password");
  u8g2.setCursor(14, 42);
  u8g2.print("for NOCTURNE:");
  u8g2.drawRFrame(14, 48, NOCT_DISP_W - 28, 12, 1);
  u8g2.setCursor(18, 57);
  u8g2.print("********");
  disp_.drawGreebles();
}

// --- QR / TEXT ---
void SceneManager::drawQrMode(const char *text) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, NOCT_DISP_W, 10);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("QR / TEXT");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, NOCT_DISP_W, 10);
  u8g2.setCursor(2, 22);
  u8g2.print(text && text[0] ? text : "NOCTURNE_OS");
  u8g2.setCursor(2, 36);
  u8g2.print("https://nocturne.local");
  u8g2.setCursor(2, 50);
  u8g2.print("2x back");
  disp_.drawGreebles();
}

// --- FORZA DASHBOARD ---
#define FORZA_TOP_H 10
#define FORZA_LEFT_W 62
#define FORZA_RIGHT_X 64

static void drawTireIcon(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2, int x,
                         int y, float fl, float fr, float rl, float rr) {
  int cx = x + 14;
  int cy = y + 12;
  u8g2.drawFrame(x, y, 28, 24);
  if (fl > 40.0f)
    u8g2.drawBox(cx - 5, cy - 7, 3, 3);
  else
    u8g2.drawPixel(cx - 4, cy - 6);
  if (fr > 40.0f)
    u8g2.drawBox(cx + 2, cy - 7, 3, 3);
  else
    u8g2.drawPixel(cx + 4, cy - 6);
  if (rl > 40.0f)
    u8g2.drawBox(cx - 5, cy + 4, 3, 3);
  else
    u8g2.drawPixel(cx - 4, cy + 6);
  if (rr > 40.0f)
    u8g2.drawBox(cx + 2, cy + 4, 3, 3);
  else
    u8g2.drawPixel(cx + 4, cy + 6);
}

void SceneManager::drawForzaDash(ForzaManager &forza, bool showSplash,
                                 uint32_t localIp) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(0);

  if (showSplash) {
    u8g2.setFont(LABEL_FONT);
    char ipBuf[20];
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", (int)((localIp >> 24) & 0xFF),
             (int)((localIp >> 16) & 0xFF), (int)((localIp >> 8) & 0xFF),
             (int)(localIp & 0xFF));
    u8g2.drawUTF8(2, 10, "IP:");
    u8g2.drawUTF8(20, 10, ipBuf);
    u8g2.drawUTF8(2, 22, "PORT: 5300");
    u8g2.drawUTF8(2, 34, "WAITING...");
    u8g2.drawUTF8(2, 50, "2x back to exit");
    disp_.drawGreebles();
    return;
  }

  const ForzaState &s = forza.getState();
  float maxRpm = (s.maxRpm > 0.0f) ? s.maxRpm : 10000.0f;
  float rpmPct = (maxRpm > 0.0f) ? (s.currentRpm / maxRpm) : 0.0f;
  if (rpmPct > 1.0f)
    rpmPct = 1.0f;
  int speedKmh = forza.getSpeedKmh();

  disp_.drawTechFrame(0, 0, NOCT_DISP_W, NOCT_DISP_H);

  u8g2.drawCircle(16, 5, 3);
  u8g2.drawCircle(40, 5, 3);
  u8g2.drawCircle(64, 5, 3);
  u8g2.drawCircle(88, 5, 3);
  u8g2.drawCircle(112, 5, 3);
  int pos = forza.getRacePosition();
  if (pos >= 1 && pos <= 5) {
    u8g2.drawDisc(16 + (pos - 1) * 24, 5, 3);
  }
  u8g2.setFont(LABEL_FONT);
  static char posBuf[8];
  snprintf(posBuf, sizeof(posBuf), "P%d", pos > 0 ? pos : 1);
  u8g2.drawUTF8(54, 8, posBuf);
  snprintf(posBuf, sizeof(posBuf), "L%d", (int)forza.getLapNumber());
  u8g2.drawUTF8(90, 8, posBuf);

  disp_.drawTechBrackets(0, FORZA_TOP_H, FORZA_LEFT_W, NOCT_DISP_H - FORZA_TOP_H, 3);
  u8g2.setFont(VALUE_FONT);
  static char rpmBuf[8];
  snprintf(rpmBuf, sizeof(rpmBuf), "%d", (int)(s.currentRpm + 0.5f));
  u8g2.drawUTF8(4, FORZA_TOP_H + 12, rpmBuf);
  u8g2.drawFrame(4, FORZA_TOP_H + 18, FORZA_LEFT_W - 10, 6);
  int fillW = (int)(rpmPct * (FORZA_LEFT_W - 12) + 0.5f);
  if (fillW > 0) {
    u8g2.drawBox(5, FORZA_TOP_H + 19, fillW, 4);
  }
  drawTireIcon(u8g2, 4, FORZA_TOP_H + 28, s.tireFL, s.tireFR, s.tireRL, s.tireRR);

  disp_.drawTechBrackets(FORZA_RIGHT_X, FORZA_TOP_H,
                         NOCT_DISP_W - FORZA_RIGHT_X, NOCT_DISP_H - FORZA_TOP_H, 3);
  char gearStr[2] = {forza.getGearChar(), '\0'};
  u8g2.setFont(u8g2_font_logisoso34_tn);
  int gearW = u8g2.getUTF8Width(gearStr);
  u8g2.drawUTF8(FORZA_RIGHT_X + (NOCT_DISP_W - FORZA_RIGHT_X) / 2 - gearW / 2,
                FORZA_TOP_H + 32, gearStr);
  u8g2.setFont(VALUE_FONT);
  static char spdBuf[12];
  snprintf(spdBuf, sizeof(spdBuf), "%d", speedKmh);
  int sw = u8g2.getUTF8Width(spdBuf);
  u8g2.drawUTF8(FORZA_RIGHT_X + (NOCT_DISP_W - FORZA_RIGHT_X) / 2 - sw / 2,
                FORZA_TOP_H + 48, spdBuf);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(FORZA_RIGHT_X + 2, FORZA_TOP_H + 50, "km/h");
  int fuelPct = (int)(forza.getFuelPct() * 100.0f + 0.5f);
  snprintf(spdBuf, sizeof(spdBuf), "F:%d%%", fuelPct);
  u8g2.drawUTF8(NOCT_DISP_W - u8g2.getUTF8Width(spdBuf) - 4,
                NOCT_DISP_H - 2, spdBuf);

  if (!s.connected) {
    u8g2.drawUTF8(4, NOCT_DISP_H - 2, "NO SIGNAL");
  }

  if (rpmPct >= FORZA_SHIFT_THRESHOLD && (millis() / 80) % 2 == 0) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, NOCT_DISP_W, NOCT_DISP_H);
  }

  disp_.drawGreebles();
}

// --- mDNS ---
void SceneManager::drawMdnsMode(const char *serviceName, bool active) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, NOCT_DISP_W, 10);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("mDNS");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, NOCT_DISP_W, 10);
  u8g2.setCursor(2, 22);
  u8g2.print(serviceName && serviceName[0] ? serviceName : "NOCTURNE");
  u8g2.setCursor(2, 34);
  u8g2.print(active ? "ACTIVE" : "IDLE");
  u8g2.setCursor(2, 48);
  u8g2.print("1x change name");
  u8g2.setCursor(2, 58);
  u8g2.print("2x back");
  disp_.drawGreebles();
}
