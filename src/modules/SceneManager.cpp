/*
 * NOCTURNE_OS — SceneManager: 9 screens. 128x64,
 * MAIN/CPU/GPU/RAM/DISKS/MEDIA/FANS/MB/NET. Unified: LABEL_FONT only for
 * content; Y from NOCT_CONTENT_TOP, NOCT_ROW_DY.
 */
#include "SceneManager.h"
#include "../../include/nocturne/config.h"
#include "KickManager.h"
#include "LoraManager.h"
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
// Tactical HUD Menu: ChamferBox with scroll. Uses NOCT_MENU_* from config.
// Short = navigate (scroll), Long = interact.
// ---------------------------------------------------------------------------
static const int MENU_MAIN = 0;

void SceneManager::drawMenu(int menuStateVal, int mainIndex, int submenuIndex,
                            bool carouselOn, int carouselSec,
                            bool screenRotated, bool glitchEnabled,
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

  u8g2.setDrawColor(1);
  u8g2.drawBox(NOCT_MENU_CONFIG_BAR_X, NOCT_MENU_CONFIG_BAR_Y,
               NOCT_MENU_CONFIG_BAR_W, NOCT_MENU_CONFIG_BAR_H);
  disp_.drawChamferBox(NOCT_MENU_CONFIG_BAR_X, NOCT_MENU_CONFIG_BAR_Y,
                       NOCT_MENU_CONFIG_BAR_W, NOCT_MENU_CONFIG_BAR_H,
                       NOCT_MENU_CONFIG_BAR_CHAMFER);
  u8g2.setDrawColor(0);
  u8g2.setFont(LABEL_FONT);
  u8g2.drawUTF8(NOCT_MENU_CONFIG_BAR_X + 8, NOCT_HEADER_BASELINE_Y + 2,
                "// CONFIG");
  u8g2.setDrawColor(1);

  static char row0Buf[16];
  static char row1Buf[16];
  static char row2Buf[16];
  if (!carouselOn)
    snprintf(row0Buf, sizeof(row0Buf), "AUTO: OFF");
  else
    snprintf(row0Buf, sizeof(row0Buf), "AUTO: %ds", carouselSec);
  snprintf(row1Buf, sizeof(row1Buf), "FLIP: %s",
           screenRotated ? "180deg" : "0deg");
  snprintf(row2Buf, sizeof(row2Buf), "GLITCH: %s",
           glitchEnabled ? "ON" : "OFF");

  // Плоское меню со всеми режимами
  static char mainItems[15][20];
  static bool itemsInitialized = false;

  // Инициализация фиксированных элементов выполняется один раз
  if (!itemsInitialized) {
    strncpy(mainItems[3], "WiFi SCAN", sizeof(mainItems[3]) - 1);
    mainItems[3][sizeof(mainItems[3]) - 1] = '\0';
    strncpy(mainItems[4], "WiFi DEAUTH", sizeof(mainItems[4]) - 1);
    mainItems[4][sizeof(mainItems[4]) - 1] = '\0';
    strncpy(mainItems[5], "WiFi PORTAL", sizeof(mainItems[5]) - 1);
    mainItems[5][sizeof(mainItems[5]) - 1] = '\0';
    strncpy(mainItems[6], "LoRa MESH", sizeof(mainItems[6]) - 1);
    mainItems[6][sizeof(mainItems[6]) - 1] = '\0';
    strncpy(mainItems[7], "LoRa JAM", sizeof(mainItems[7]) - 1);
    mainItems[7][sizeof(mainItems[7]) - 1] = '\0';
    strncpy(mainItems[8], "LoRa SENSE", sizeof(mainItems[8]) - 1);
    mainItems[8][sizeof(mainItems[8]) - 1] = '\0';
    strncpy(mainItems[9], "BLE SPAM", sizeof(mainItems[9]) - 1);
    mainItems[9][sizeof(mainItems[9]) - 1] = '\0';
    strncpy(mainItems[10], "USB HID", sizeof(mainItems[10]) - 1);
    mainItems[10][sizeof(mainItems[10]) - 1] = '\0';
    strncpy(mainItems[11], "VAULT", sizeof(mainItems[11]) - 1);
    mainItems[11][sizeof(mainItems[11]) - 1] = '\0';
    strncpy(mainItems[12], "DAEMON", sizeof(mainItems[12]) - 1);
    mainItems[12][sizeof(mainItems[12]) - 1] = '\0';
    strncpy(mainItems[14], "EXIT", sizeof(mainItems[14]) - 1);
    mainItems[14][sizeof(mainItems[14]) - 1] = '\0';
    itemsInitialized = true;
  }

  // Обновление динамических элементов (AUTO, FLIP, GLITCH) - каждый раз
  // Копируем строки из буферов в mainItems
  strncpy(mainItems[0], row0Buf, sizeof(mainItems[0]) - 1);
  mainItems[0][sizeof(mainItems[0]) - 1] = '\0';
  strncpy(mainItems[1], row1Buf, sizeof(mainItems[1]) - 1);
  mainItems[1][sizeof(mainItems[1]) - 1] = '\0';
  strncpy(mainItems[2], row2Buf, sizeof(mainItems[2]) - 1);
  mainItems[2][sizeof(mainItems[2]) - 1] = '\0';

  // REBOOT с индикацией подтверждения - обновляется каждый раз
  if (rebootConfirmed) {
    strncpy(mainItems[13], "REBOOT [OK]", sizeof(mainItems[13]) - 1);
  } else {
    strncpy(mainItems[13], "REBOOT", sizeof(mainItems[13]) - 1);
  }
  mainItems[13][sizeof(mainItems[13]) - 1] = '\0';

  const int count = 15;

  // Защита от выхода за границы массива
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

  // Установка шрифта перед отрисовкой (как в других местах кода)
  u8g2.setFontMode(1);
  u8g2.setFont(LABEL_FONT);
  u8g2.setDrawColor(1);

  // Optimized caching: кэширование вычислений ширины текста для оптимизации
  static int cachedTextWidths[15] = {-1};
  static int cachedSelected = -1;
  static bool cacheValid = false;

  // Пересчитываем ширину текста только если изменился выбранный элемент или кэш
  // невалиден
  if (selected != cachedSelected || !cacheValid) {
    cachedSelected = selected;
    cacheValid = true;
    for (int i = 0; i < count; i++) {
      if (mainItems[i][0] != '\0') {
        cachedTextWidths[i] = u8g2.getUTF8Width(mainItems[i]);
      } else {
        cachedTextWidths[i] = -1;
      }
    }
  }

  for (int r = 0; r < NOCT_MENU_VISIBLE_ROWS; r++) {
    int i = firstVisible + r;
    if (i >= count || i < 0)
      break;

    // Используем mainItems напрямую
    const char *itemText = mainItems[i];
    // Проверяем, что строка не пустая
    if (itemText[0] != '\0') {
      int y = startY + r * NOCT_MENU_ROW_H;
      int textWidth = cachedTextWidths[i] >= 0 ? cachedTextWidths[i]
                                               : u8g2.getUTF8Width(itemText);
      int textX = boxX + (boxW - textWidth) / 2;

      if (i == selected) {
        // Выделение фона для выбранного элемента
        u8g2.setDrawColor(1);
        u8g2.drawBox(NOCT_MENU_LIST_LEFT, y - 6, NOCT_MENU_LIST_W,
                     NOCT_MENU_ROW_H);
        // Белый текст на черном фоне
        u8g2.setDrawColor(0);
        u8g2.drawUTF8(NOCT_MENU_LIST_LEFT + 2, y, ">");
        u8g2.drawUTF8(NOCT_MENU_LIST_LEFT + NOCT_MENU_LIST_W - 6, y, "<");
        u8g2.drawUTF8(textX, y, itemText);
        u8g2.setDrawColor(1); // Возврат к нормальному цвету
      } else {
        // Обычный текст для невыбранных элементов
        u8g2.setDrawColor(1);
        u8g2.drawUTF8(textX, y, itemText);
      }
    }
  }

  disp_.drawScrollIndicator(startY - 2,
                            NOCT_MENU_VISIBLE_ROWS * NOCT_MENU_ROW_H, count,
                            NOCT_MENU_VISIBLE_ROWS, firstVisible);
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
                                   int *sortedIndices, int filteredCount) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);

  int n = WiFi.scanComplete();
  bool useFiltered = (sortedIndices != nullptr && filteredCount > 0);
  int displayCount = useFiltered ? filteredCount : n;

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
  if (displayCount == 0) {
    disp_.drawCentered(32, "NO SIGNALS");
    disp_.drawCentered(45, "[PRESS TO RESCAN]");
    return;
  }

  // --- LIST RENDER ---
  u8g2.setCursor(2, 6);
  if (useFiltered && filteredCount < n) {
    u8g2.printf("TARGETS: %d/%d", filteredCount, n);
  } else {
    u8g2.printf("TARGETS: %d", displayCount);
  }
  u8g2.drawLine(0, 8, 128, 8);

  int yStart = 16;
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

    // Optimized: use c_str() and static buffer instead of String
    const char *ssid = WiFi.SSID(actualIndex).c_str();
    char ssidBuf[12]; // Max 10 chars + "." + null terminator
    if (!ssid || strlen(ssid) == 0) {
      strncpy(ssidBuf, "[HIDDEN]", sizeof(ssidBuf) - 1);
    } else {
      size_t len = strlen(ssid);
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
  u8g2.drawLine(0, 55, 128, 55);
  u8g2.setCursor(2, 63);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.print("LINK: ONLINE");
  } else {
    u8g2.print("LINK: DISCONNECTED");
  }
}

// ---------------------------------------------------------------------------
// SIGINT: LoRa Meshtastic sniffer — RSSI waterfall, packet console, REPLAY
// Top: RSSI bar, Middle: [MESH] From: 0x... | SNR, Bottom: Short=Next
// Long=REPLAY
// ---------------------------------------------------------------------------
#define LORA_HEADER_H 10
#define LORA_RSSI_BAR_Y 12
#define LORA_RSSI_BAR_H 6
#define LORA_CONSOLE_Y 22
#define LORA_CONSOLE_H 32
#define LORA_FOOTER_Y 56

void SceneManager::drawLoraSniffer(LoraManager &lora) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  // --- Top: Header + real-time RSSI "waterfall" bar ---
  u8g2.drawBox(0, 0, NOCT_DISP_W, LORA_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("SIGINT 868MHz");
  // Отображение частотного слота справа в заголовке
  int slot = lora.getCurrentFreqSlot();
  u8g2.setCursor(90, 7);
  u8g2.printf("S%d", slot);
  u8g2.setDrawColor(1);

  int histLen = 0;
  const int8_t *hist = lora.getRssiHistory(histLen);
  int barW = NOCT_DISP_W;
  int barY = LORA_RSSI_BAR_Y;
  u8g2.drawFrame(0, barY, barW, LORA_RSSI_BAR_H);
  if (histLen > 0) {
    int L = LoraManager::rssiHistoryLen();
    int head = lora.getRssiHistoryHead();
    for (int i = 0; i < barW && i < histLen; i++) {
      int idx = (head - histLen + i + L) % L;
      int r = hist[idx];
      int h = map(constrain(r, -120, -50), -120, -50, LORA_RSSI_BAR_H - 2, 0);
      if (h > 0 && h < LORA_RSSI_BAR_H - 1)
        u8g2.drawVLine(i, barY + LORA_RSSI_BAR_H - 1 - h, h);
    }
  } else {
    float rssi = lora.getCurrentRSSI();
    int w = map(constrain((int)rssi, -130, -50), -130, -50, 0, barW - 2);
    if (w > 0)
      u8g2.drawBox(1, barY + 1, w, LORA_RSSI_BAR_H - 2);
  }

  // --- Middle: Packet console [MESH] From: 0xDEADBEEF | SNR: 10 ---
  u8g2.drawLine(0, LORA_CONSOLE_Y - 1, NOCT_DISP_W, LORA_CONSOLE_Y - 1);
  int nPackets = lora.getPacketBufferCount();
  static char lineBuf[32];

  if (nPackets == 0) {
    u8g2.setCursor(2, LORA_CONSOLE_Y + 6);
    u8g2.print("[MESH] Listening...");
    u8g2.setCursor(2, LORA_CONSOLE_Y + 14);
    u8g2.print("No packets yet.");
  } else {
    for (int i = 0; i < nPackets && i < 3; i++) {
      const LoraRawPacket *p = lora.getPacketInBuffer(i);
      if (!p || p->len < 4)
        continue;
      int y = LORA_CONSOLE_Y + 2 + i * 10;
      uint32_t fromId = 0;
      if (p->len >= 4)
        fromId = (uint32_t)p->data[0] | ((uint32_t)p->data[1] << 8) |
                 ((uint32_t)p->data[2] << 16) | ((uint32_t)p->data[3] << 24);
      snprintf(lineBuf, sizeof(lineBuf), "[MESH] 0x%lX | SNR %.0f RSSI %.0f",
               (unsigned long)fromId, (double)p->snr, (double)p->rssi);
      u8g2.setCursor(2, y);
      u8g2.print(lineBuf);
    }
  }

  u8g2.drawLine(0, LORA_FOOTER_Y - 1, NOCT_DISP_W, LORA_FOOTER_Y - 1);
  u8g2.setCursor(2, LORA_FOOTER_Y + 6);
  u8g2.print("Short: Clear | Long: REPLAY | 2x: Slot | 3x Out");
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

void SceneManager::drawBadWolf() {
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

  u8g2.drawLine(0, BADWOLF_FOOTER_Y - 1, NOCT_DISP_W, BADWOLF_FOOTER_Y - 1);
  u8g2.setCursor(2, BADWOLF_FOOTER_Y + 6);
  u8g2.print("Short: MATRIX | Long: SNIFFER | 3x Out");

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
#define KICK_HEADER_H 10
#define KICK_ICON_X 48
#define KICK_ICON_Y 14
#define KICK_FOOTER_Y 56

void SceneManager::drawKickMode(KickManager &kick) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  bool attacking = kick.isAttacking();
  int sx = attacking ? (int)((millis() / 25) % 5) - 2 : 0;
  int sy = attacking ? (int)((millis() / 33) % 5) - 2 : 0;

  u8g2.drawBox(0 + sx, 0 + sy, NOCT_DISP_W, KICK_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2 + sx, 7 + sy);
  u8g2.print("DEAUTH");
  u8g2.setDrawColor(1);

  u8g2.drawXBM(KICK_ICON_X + sx, KICK_ICON_Y + sy, 32, 32, wolf_aggressive);

  static char targetBuf[44];
  static char bssidBuf[20];
  kick.getTargetSSID(targetBuf, sizeof(targetBuf));
  kick.getTargetBSSIDStr(bssidBuf, sizeof(bssidBuf));
  if (targetBuf[0] == '\0')
    snprintf(targetBuf, sizeof(targetBuf), "%s", bssidBuf);
  u8g2.setCursor(2 + sx, 48 + sy);
  u8g2.print("TARGET: ");
  u8g2.print(targetBuf[0] ? targetBuf : "(none)");
  u8g2.setCursor(2 + sx, 56 + sy);
  u8g2.print(attacking ? "STATUS: INJECTING" : "STATUS: IDLE");
  u8g2.setCursor(2 + sx, 62 + sy);
  u8g2.print("PKTS: ");
  u8g2.print(kick.getPacketCount());

  if (kick.isTargetProtected()) {
    u8g2.setCursor(2, 58);
    u8g2.print("WARNING: WPA3/PMF");
  }
  if (kick.isTargetOwnAP()) {
    u8g2.setCursor(70 + sx, 48 + sy);
    u8g2.print("[OWN AP]");
  }

  disp_.drawGreebles();
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

// --- GHOSTS (868 MHz sensor sniffer) ---
#define GHOSTS_HEADER_H 10
#define GHOSTS_LIST_Y 14
#define GHOSTS_ROW_H 10

void SceneManager::drawGhostsMode(LoraManager &lora) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFontMode(1);
  u8g2.setFont(TINY_FONT);
  u8g2.setDrawColor(1);

  u8g2.drawBox(0, 0, NOCT_DISP_W, GHOSTS_HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setCursor(2, 7);
  u8g2.print("SENSE 868");
  u8g2.setDrawColor(1);

  unsigned long t = millis() / 80;
  int sweepX = (int)(64 + 40 * cos((t % 360) * 3.14159265359 / 180.0));
  int sweepY = (int)(32 + 24 * sin((t % 360) * 3.14159265359 / 180.0));
  u8g2.drawLine(64, 32, sweepX, sweepY);

  int n = lora.getSenseCount();
  for (int i = 0; i < n && i < 4; i++) {
    const LoraManager::SenseEntry *e = lora.getSenseEntry(i);
    if (!e)
      continue;
    int y = GHOSTS_LIST_Y + i * GHOSTS_ROW_H;
    u8g2.setCursor(2, y);
    u8g2.print(e->hex[0] ? e->hex : "...");
    u8g2.setCursor(100, y);
    u8g2.print((int)e->rssi);
  }

  u8g2.drawLine(0, 55, NOCT_DISP_W, 55);
  u8g2.setCursor(2, 62);
  u8g2.print("3x Out");

  disp_.drawGreebles();
}
