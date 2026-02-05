/*
 * NOCTURNE_OS — Forest OS v3.0 boot: BIOS post, Digital Tree logo, hex loading
 * bar.
 */
#include "BootAnim.h"
#include "../../include/nocturne/config.h"
#include "DisplayEngine.h"
#include <Arduino.h>

static const char *biosLines[] = {
    "INIT MEMORY...",
    "MOUNTING /DEV/WOLF...",
    "LOADING KERNEL...",
    "CHECKING SENSORS... OK",
};
static const int biosLineCount = 4;

// Phase 1: show BIOS lines with scroll/typewriter
static void phaseBiosPost(DisplayEngine &display, unsigned long &phaseStart) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  const int lineH = 10;
  const int topY = 8;
  unsigned long elapsed = millis() - phaseStart;
  int lineIndex = (int)(elapsed / 400) % (biosLineCount + 1);
  if (lineIndex >= biosLineCount)
    lineIndex = biosLineCount - 1;

  display.clearBuffer();
  u8g2.setFont(LABEL_FONT);
  for (int i = 0; i <= lineIndex; i++) {
    int y = topY + i * lineH;
    if (y + lineH > NOCT_DISP_H - 4)
      break;
    u8g2.drawUTF8(2, y, biosLines[i]);
  }
  // Cursor blink
  if ((elapsed / 200) % 2 == 0) {
    int cy = topY + lineIndex * lineH;
    int cw = u8g2.getUTF8Width(biosLines[lineIndex]);
    u8g2.drawPixel(2 + cw + 2, cy - 6);
    u8g2.drawPixel(2 + cw + 2, cy - 5);
  }
  display.sendBuffer();
}

// 32x32 Wolf Funny (boot logo)
static const unsigned char wolf_funny_logo[] = {
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

static void phaseLogo(DisplayEngine &display, unsigned long &phaseStart) {
  (void)phaseStart;
  display.clearBuffer();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  const int wolfW = 32;
  const int wolfH = 32;
  u8g2.drawXBMP((NOCT_DISP_W - wolfW) / 2, 10, wolfW, wolfH, wolf_funny_logo);
  u8g2.setFont(HEADER_FONT);
  const char *title = "FOREST OS v3.0";
  int tw = u8g2.getUTF8Width(title);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, 58, title);
  display.sendBuffer();
}

// Phase 3: loading bar — segments show random hex then turn solid
static const char hexChars[] = "0123456789ABCDEF";
static void phaseLoadingBar(DisplayEngine &display, unsigned long &phaseStart) {
  unsigned long elapsed = millis() - phaseStart;
  const int barX = 8;
  const int barY = 52;
  const int barW = NOCT_DISP_W - 16;
  const int barH = 6;
  const int segCount = 16;
  const unsigned long totalMs = 1200;
  int progress = (int)((elapsed * 100) / totalMs);
  if (progress > 100)
    progress = 100;

  display.clearBuffer();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  display.drawTechFrame(barX, barY, barW, barH);
  int segW = (barW - 2) / segCount;
  int filledSegs = (progress * segCount) / 100;

  u8g2.setFont(LABEL_FONT);
  for (int i = 0; i < segCount; i++) {
    int sx = barX + 1 + i * segW;
    if (i < filledSegs) {
      u8g2.drawBox(sx, barY + 1, segW - 1, barH - 2);
    } else if (i == filledSegs && progress < 100) {
      char h = hexChars[random(16)];
      u8g2.drawUTF8(sx + 1, barY + 4, &h);
    }
  }
  u8g2.drawUTF8(barX, barY - 2, "LOADING");
  display.sendBuffer();
}

void drawBootSequence(DisplayEngine &display) {
  unsigned long phaseStart = millis();
  const unsigned long phase1Ms = 2200;
  const unsigned long phase2Ms = 1800;
  const unsigned long phase3Ms = 1200;

  while (millis() - phaseStart < phase1Ms) {
    phaseBiosPost(display, phaseStart);
    delay(80);
  }

  phaseStart = millis();
  while (millis() - phaseStart < phase2Ms) {
    phaseLogo(display, phaseStart);
    delay(80);
  }

  phaseStart = millis();
  while (millis() - phaseStart < phase3Ms) {
    phaseLoadingBar(display, phaseStart);
    delay(60);
  }
}
