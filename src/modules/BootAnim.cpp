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

// Phase 2: procedural "Digital Tree" (lines + triangles) + "FOREST OS v3.0"
static void drawDigitalTree(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2) {
  const int cx = NOCT_DISP_W / 2;
  const int baseY = 50;
  const int topY = 14;
  // Trunk
  u8g2.drawLine(cx, baseY, cx, baseY - 18);
  // Branches (symmetrical)
  u8g2.drawLine(cx, baseY - 12, cx - 14, baseY - 24);
  u8g2.drawLine(cx, baseY - 12, cx + 14, baseY - 24);
  u8g2.drawLine(cx - 14, baseY - 24, cx - 8, topY + 4);
  u8g2.drawLine(cx + 14, baseY - 24, cx + 8, topY + 4);
  // Small triangles as "leaves" / nodes
  u8g2.drawTriangle(cx, topY, cx - 6, topY + 8, cx + 6, topY + 8);
  u8g2.drawTriangle(cx - 14, baseY - 26, cx - 20, baseY - 18, cx - 8,
                    baseY - 20);
  u8g2.drawTriangle(cx + 14, baseY - 26, cx + 8, baseY - 20, cx + 20,
                    baseY - 18);
}

static void phaseLogo(DisplayEngine &display, unsigned long &phaseStart) {
  unsigned long elapsed = millis() - phaseStart;
  display.clearBuffer();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  drawDigitalTree(u8g2);
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
  u8g2.drawFrame(barX, barY, barW, barH);
  int segW = (barW - 2) / segCount;
  int filledSegs = (progress * segCount) / 100;

  for (int i = 0; i < segCount; i++) {
    int sx = barX + 1 + i * segW;
    if (i < filledSegs) {
      u8g2.drawBox(sx, barY + 1, segW - 1, barH - 2);
    } else if (i == filledSegs && progress < 100) {
      // Leading segment: show hex char that "solidifies"
      char h = hexChars[random(16)];
      u8g2.setFont(LABEL_FONT);
      u8g2.drawUTF8(sx + 1, barY + 4, &h);
    }
  }
  u8g2.setFont(LABEL_FONT);
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
