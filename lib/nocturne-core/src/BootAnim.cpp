/*
 * NOCTURNE_OS — Forest OS v3.0 boot: BIOS post, wolf logo, hex loading bar.
 *
 * Visual-polish pass (display only — same 3 phases, same ~5.2s total):
 *   - Ordered 4x4 Bayer dither wipe between phases (flicker-free fade-out).
 *   - Phase 2: logo reveals top-to-bottom via an eased clip window; title
 *     fades up; an underline sweeps out from the title centre; small byline.
 *   - Phase 3: rounded segmented bar with a moving scan highlight, a single
 *     random hex glyph at the fill edge, tick marks and a centred percentage.
 *
 * Every drawn element is provably inside 0..127 x 0..63; the per-block pixel
 * arithmetic is annotated in comments next to each draw.
 */
#include "BootAnim.h"
#include "nocturne/config.h"
#include "DisplayEngine.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
//  File-local helpers
// ============================================================================
namespace {

static inline int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Ease-out cubic over a 0..1000 fixed-point range (no float / no <math.h>).
static int easeOutCubic1000(int t) {
  if (t <= 0) return 0;
  if (t >= 1000) return 1000;
  long u = 1000 - t;
  long cubed = (u * u * u) / (1000L * 1000L); // 0..1000
  return (int)(1000 - cubed);
}

// 4x4 ordered Bayer matrix (thresholds 0..15) for flicker-free dither fades.
static const uint8_t kBayer4[16] = {
     0, 8, 2, 10,
    12, 4, 14, 6,
     3, 11, 1, 9,
    15, 7, 13, 5
};

// Dissolve a rectangle toward black using the Bayer mask. `keep` 0..16 is how
// many of the 16 thresholds stay lit (16 = untouched, 0 = fully cleared);
// pixels whose threshold >= keep are erased. Fully clipped to 128x64.
static void ditherWipe(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2, int x, int y,
                       int w, int h, int keep) {
  if (keep >= 16) return;
  u8g2.setDrawColor(0);
  if (keep <= 0) {
    int cx = clampi(x, 0, 127), cy = clampi(y, 0, 63);
    int cw = clampi(w, 0, NOCT_DISP_W - cx), ch = clampi(h, 0, NOCT_DISP_H - cy);
    if (cw > 0 && ch > 0) u8g2.drawBox(cx, cy, cw, ch);
    u8g2.setDrawColor(1);
    return;
  }
  for (int yy = 0; yy < h; yy++) {
    int py = y + yy;
    if (py < 0 || py > 63) continue;
    for (int xx = 0; xx < w; xx++) {
      int px = x + xx;
      if (px < 0 || px > 127) continue;
      if (kBayer4[(yy & 3) * 4 + (xx & 3)] >= keep) u8g2.drawPixel(px, py);
    }
  }
  u8g2.setDrawColor(1);
}

// Boot frame pacing (~33 fps). Phase durations below set total length; this
// only affects smoothness.
static const unsigned long kBootFrameMs = 30;

}  // namespace

// ============================================================================
//  Phase 1 — BIOS POST (typewriter lines + blinking block cursor + fade tail)
// ============================================================================
static const char *biosLines[] = {
    "INIT MEMORY...",
    "MOUNTING /DEV/WOLF...",
    "LOADING KERNEL...",
    "CHECKING SENSORS... OK",
};
static const int biosLineCount = 4;

static void phaseBiosPost(DisplayEngine &display, unsigned long phaseStart,
                          unsigned long phaseMs) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  const int lineH = 10;
  const int topY = 8;                          // first baseline; last = 8+3*10 = 38
  const unsigned long FADE = 240;              // fade-out tail length
  unsigned long elapsed = millis() - phaseStart;
  unsigned long revealSpan = (phaseMs > FADE) ? (phaseMs - FADE) : phaseMs;

  // Reveal one line per (revealSpan / lineCount) ms, capped at the last line.
  int lineIndex =
      (int)((elapsed * biosLineCount) / (revealSpan ? revealSpan : 1));
  if (lineIndex >= biosLineCount) lineIndex = biosLineCount - 1;
  if (lineIndex < 0) lineIndex = 0;

  display.clearBuffer();
  u8g2.setFont(LABEL_FONT);
  for (int i = 0; i <= lineIndex; i++) {
    int y = topY + i * lineH;                  // <= 38
    if (y + 2 > NOCT_DISP_H - 2) break;        // never draw below y=62
    u8g2.drawUTF8(2, y, biosLines[i]);
  }
  // Block cursor after the current line (blinks ~2.5 Hz). 4x6 box.
  if ((elapsed / 200) % 2 == 0) {
    int cw = u8g2.getUTF8Width(biosLines[lineIndex]);
    int cx = 2 + cw + 2;
    int cy = topY + lineIndex * lineH;         // baseline of current line (<=38)
    if (cx >= 0 && cx <= NOCT_DISP_W - 4 && cy >= 6)
      u8g2.drawBox(cx, cy - 6, 4, 6);          // top = cy-6 >= 0, bottom <= 38
  }
  // Fade the whole text band (y 0..40) away over the tail.
  if (elapsed > phaseMs - FADE) {
    int prog = (int)(((elapsed - (phaseMs - FADE)) * 16) / FADE); // 0..16
    ditherWipe(u8g2, 0, 0, NOCT_DISP_W, 41, 16 - clampi(prog, 0, 16));
  }
  display.sendBuffer();
}

// ============================================================================
//  Phase 2 — Wolf logo + title (top-down reveal, fade-up title, underline sweep)
// ============================================================================
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

static void phaseLogo(DisplayEngine &display, unsigned long phaseStart,
                      unsigned long phaseMs) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  const int wolfW = 32, wolfH = 32;
  const int lx = (NOCT_DISP_W - wolfW) / 2;    // 48 -> x 48..79
  const int ly = 6;                            // y 6..37
  const unsigned long FADE = 240;
  const unsigned long REVEAL = 520;            // logo wipe-in duration
  unsigned long elapsed = millis() - phaseStart;

  display.clearBuffer();

  // Logo entrance: reveal top-to-bottom via an eased clip window. Clip rect is
  // bounds-safe: x in [48,80], y in [6, 6+revH<=38].
  int revH = wolfH;
  if (elapsed < REVEAL) {
    int e = easeOutCubic1000((int)((elapsed * 1000) / REVEAL)); // 0..1000
    revH = clampi((wolfH * e) / 1000, 0, wolfH);
  }
  if (revH > 0) {
    u8g2.setClipWindow(lx, ly, lx + wolfW, ly + revH);
    u8g2.drawXBMP(lx, ly, wolfW, wolfH, wolf_funny_logo);
    u8g2.setMaxClipWindow();
  }

  // Title, centred, fading up via dither just after the logo starts.
  u8g2.setFont(HEADER_FONT);
  const char *title = "FOREST OS v3.0";
  int tw = u8g2.getUTF8Width(title);
  int tx = (NOCT_DISP_W - tw) / 2;             // centred (tw ~ 78 -> tx ~ 25)
  const int tBase = 50;                        // baseline 50 (cap top ~41)
  u8g2.drawUTF8(tx, tBase, title);
  if (elapsed < REVEAL + 260) {
    int fp = (int)((elapsed * 16) / (REVEAL + 260));            // 0..16 ramp
    ditherWipe(u8g2, clampi(tx - 1, 0, 127), tBase - 10, tw + 2, 12,
               clampi(fp, 0, 16));            // band y 40..51
  }

  // Underline sweep: grows from the title centre outward to full title width.
  // y = 53. Half-width clamped so the line never leaves the panel.
  int half = clampi((int)((elapsed * (tw / 2)) / (phaseMs ? phaseMs : 1)),
                    0, tw / 2);
  int ux = clampi(tx + tw / 2 - half, 0, 127);
  int uw = clampi(half * 2, 0, NOCT_DISP_W - ux);
  if (uw > 0) u8g2.drawHLine(ux, 53, uw);

  // Byline, centred, tiny font. baseline 62 (cap top ~57; bottom <= 62).
  u8g2.setFont(u8g2_font_4x6_tr);
  const char *sub = "by therudywolf";
  int sw = u8g2.getUTF8Width(sub);
  u8g2.drawUTF8(clampi((NOCT_DISP_W - sw) / 2, 0, 127), 62, sub);

  // Fade the whole frame away before phase 3.
  if (elapsed > phaseMs - FADE) {
    int prog = (int)(((elapsed - (phaseMs - FADE)) * 16) / FADE);
    ditherWipe(u8g2, 0, 0, NOCT_DISP_W, NOCT_DISP_H, 16 - clampi(prog, 0, 16));
  }
  display.sendBuffer();
}

// ============================================================================
//  Phase 3 — Loading bar (rounded segmented fill + scan + hex + ticks + %)
// ============================================================================
static const char hexChars[] = "0123456789ABCDEF";

static void phaseLoadingBar(DisplayEngine &display, unsigned long phaseStart,
                            unsigned long phaseMs) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display.u8g2();
  unsigned long elapsed = millis() - phaseStart;
  int progress = (int)((elapsed * 100) / (phaseMs ? phaseMs : 1));
  progress = clampi(progress, 0, 100);

  display.clearBuffer();

  // Heading, centred. baseline 16 (cap top ~9).
  u8g2.setFont(LABEL_FONT);
  const char *msg = "LOADING";
  int mw = u8g2.getUTF8Width(msg);
  u8g2.drawUTF8((NOCT_DISP_W - mw) / 2, 16, msg);

  // Bar: x 8..119 (w=112), y 28..41 (h=14). Rounded outer frame.
  const int bx = 8, by = 28, bw = NOCT_DISP_W - 16, bh = 14;
  u8g2.drawRFrame(bx, by, bw, bh, 2);
  const int ix = bx + 2, iy = by + 2, iw = bw - 4, ih = bh - 4; // inner 10..117 / 30..39
  int fillW = (iw * progress) / 100;           // 0..iw
  if (fillW > 0) u8g2.drawBox(ix, iy, fillW, ih);

  if (fillW < iw) {
    // Moving 1px scan highlight travelling the unfilled region.
    int travel = iw - fillW;                   // >=1 here
    int sx = ix + fillW + (int)((elapsed / 24) % (travel > 0 ? travel : 1));
    if (sx >= ix && sx <= ix + iw - 1) u8g2.drawVLine(sx, iy, ih);
    // Single random hex glyph just past the fill edge (Fenrir flavour).
    int hgx = ix + fillW + 1;
    if (progress < 100 && hgx >= ix && hgx + 5 <= ix + iw) {
      char h[2] = { hexChars[random(16)], '\0' };
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawUTF8(hgx, by + 10, h);          // 5x7, baseline 38, inside bar
    }
  }

  // Tick marks under the bar at 0/25/50/75/100%, y 44..45.
  for (int t = 0; t <= 4; t++) {
    int tickX = clampi(ix + ((iw - 1) * t) / 4, 0, 127);
    u8g2.drawVLine(tickX, 44, 2);
  }

  // Percentage, centred. baseline 58 (cap top ~50).
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", progress);
  u8g2.setFont(LABEL_FONT);
  int pw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((NOCT_DISP_W - pw) / 2, 58, buf);

  display.sendBuffer();
}

// ============================================================================
//  Driver — runs the 3 phases (blocking). Total = 2200+1800+1200 = 5200 ms.
// ============================================================================
void drawBootSequence(DisplayEngine &display, bool fancy) {
  // Respect the user's screen rotation during boot (#3) — was hardcoded to 1.
  display.u8g2().setFlipMode(display.isScreenFlipped() ? 1 : 0);
  display.u8g2().setFontPosBaseline();

  if (!fancy) {
    // Special effects off: clean quick boot — static splash, no dither/scan/hex.
    display.clearBuffer();
    display.drawSplash();
    display.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(900));
    display.u8g2().setMaxClipWindow();
    display.clearBuffer();
    display.sendBuffer();
    return;
  }

  const unsigned long phase1Ms = 2200;
  const unsigned long phase2Ms = 1800;
  const unsigned long phase3Ms = 1200;

  unsigned long phaseStart = millis();
  while (millis() - phaseStart < phase1Ms) {
    phaseBiosPost(display, phaseStart, phase1Ms);
    vTaskDelay(pdMS_TO_TICKS(kBootFrameMs));
  }

  phaseStart = millis();
  while (millis() - phaseStart < phase2Ms) {
    phaseLogo(display, phaseStart, phase2Ms);
    vTaskDelay(pdMS_TO_TICKS(kBootFrameMs));
  }

  phaseStart = millis();
  while (millis() - phaseStart < phase3Ms) {
    phaseLoadingBar(display, phaseStart, phase3Ms);
    vTaskDelay(pdMS_TO_TICKS(kBootFrameMs));
  }

  // Leave a clean blank buffer; restore clip + flip state for the UI.
  display.u8g2().setMaxClipWindow();
  display.clearBuffer();
  display.sendBuffer();
  display.u8g2().setFlipMode(display.isScreenFlipped() ? 1 : 0);
}
