/*
 * NOCTURNE_OS — SceneManager: Cortex, Neural, Weather (WMO icons), Media,
 * Phantom Limb (radar)
 */
#include "nocturne/SceneManager.h"
#include "nocturne/config.h"
#include <WiFi.h>

#define LINE_H_DATA NOCT_LINE_H_DATA
#define LINE_H_LABEL NOCT_LINE_H_LABEL
#define LINE_H_HEAD NOCT_LINE_H_HEAD
#define LINE_H_BIG NOCT_LINE_H_BIG

SceneManager::SceneManager(DisplayEngine &disp, DataManager &data)
    : disp_(disp), data_(data) {}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame) {
  switch (sceneIndex) {
  case NOCT_SCENE_CORTEX:
    drawCortex(bootTime);
    break;
  case NOCT_SCENE_NEURAL:
    drawNeural();
    break;
  case NOCT_SCENE_THERMAL:
    drawThermal(fanFrame);
    break;
  case NOCT_SCENE_MEMBANK:
    drawMemBank();
    break;
  case NOCT_SCENE_TASKKILL:
    drawTaskKill(blinkState);
    break;
  case NOCT_SCENE_DECK:
    drawDeck(blinkState);
    break;
  case NOCT_SCENE_WEATHER:
    drawWeather();
    break;
  case NOCT_SCENE_RADAR:
    drawRadar();
    break;
  default:
    drawCortex(bootTime);
    break;
  }
}

void SceneManager::drawCortex(unsigned long bootTime) {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[CORTEX]");

  int gx = NOCT_MARGIN + 2, gy = NOCT_MARGIN + LINE_H_HEAD + 4;
  int halfW = (NOCT_DISP_W - 2 * NOCT_MARGIN - 4) / 2;
  int halfH = (NOCT_DISP_H - gy - NOCT_MARGIN - 2) / 2;
  char buf[24];

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(gx, gy + LINE_H_LABEL, "CPU");
  snprintf(buf, sizeof(buf), "%dC", hw.ct);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(gx, gy + LINE_H_LABEL + LINE_H_BIG, buf);
  disp_.drawProgressBar(gx, gy + LINE_H_LABEL + LINE_H_BIG + 2, halfW - 4, 6,
                        (float)hw.cl);

  int rx = gx + halfW + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(rx, gy + LINE_H_LABEL, "GPU");
  snprintf(buf, sizeof(buf), "%dC", hw.gt);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(rx, gy + LINE_H_LABEL + LINE_H_BIG, buf);
  disp_.drawProgressBar(rx, gy + LINE_H_LABEL + LINE_H_BIG + 2, halfW - 4, 6,
                        (float)hw.gl);

  int by = gy + halfH + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(gx, by + LINE_H_LABEL, "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.ru, hw.ra);
  disp_.drawMetricStr(gx, by + LINE_H_LABEL + LINE_H_DATA, "RAM", String(buf));

  u8g2.setFont(FONT_LABEL);
  if (hw.vt > 0.0f) {
    u8g2.drawStr(rx, by + LINE_H_LABEL, "VRAM");
    snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.vu, hw.vt);
  } else {
    u8g2.drawStr(rx, by + LINE_H_LABEL, "UPTM");
    unsigned long s = (millis() - bootTime) / 1000;
    int m = (int)(s / 60), h = m / 60;
    snprintf(buf, sizeof(buf), "%d:%02d", h, m % 60);
  }
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(rx, by + LINE_H_LABEL + LINE_H_DATA, buf);
}

void SceneManager::drawNeural() {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[NEURAL]");

  int y = NOCT_MARGIN + LINE_H_HEAD + 6;
  char buf[24];

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "DOWN");
  if (hw.nd >= 1024)
    snprintf(buf, sizeof(buf), "%.1fM", hw.nd / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "%dK", hw.nd);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_BIG, buf);
  y += LINE_H_BIG + 4;

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "UP");
  if (hw.nu >= 1024)
    snprintf(buf, sizeof(buf), "%.1fM", hw.nu / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "%dK", hw.nu);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_BIG, buf);
  y += LINE_H_BIG + 4;

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "PING");
  snprintf(buf, sizeof(buf), "%dms", hw.pg);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_DATA, buf);

  u8g2.setFont(FONT_TINY);
  String ip = WiFi.localIP().toString();
  if (ip.length() > 20)
    ip = ip.substring(0, 20);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H - NOCT_MARGIN - 2, ip.c_str());
}

void SceneManager::drawThermal(int fanFrame) {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[THERMAL]");
  disp_.drawFanIcon(NOCT_DISP_W - 12, NOCT_MARGIN + LINE_H_HEAD - 2, fanFrame);

  int y = NOCT_MARGIN + LINE_H_HEAD + 4;
  char buf[24];
  const char *labels[] = {"PUMP", "RAD", "SYS2", "GPU"};
  int vals[] = {hw.cf, hw.s1, hw.s2, hw.gf};
  for (int i = 0; i < 4 && y + LINE_H_DATA <= NOCT_DISP_H - NOCT_MARGIN - 8;
       i++) {
    snprintf(buf, sizeof(buf), "%d RPM", vals[i]);
    disp_.drawMetricStr(NOCT_MARGIN + 32, y + LINE_H_DATA, labels[i],
                        String(buf));
    y += LINE_H_DATA + 2;
  }
  if (hw.ch > 0) {
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H - NOCT_MARGIN - 8, "CHIP");
    snprintf(buf, sizeof(buf), "%dC", hw.ch);
    u8g2.setFont(FONT_VAL);
    u8g2.drawUTF8(NOCT_MARGIN + 32, NOCT_DISP_H - NOCT_MARGIN, buf);
  }
}

void SceneManager::drawMemBank() {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[MEM.BANK]");

  int y = NOCT_MARGIN + LINE_H_HEAD + 6;
  char buf[24];
  float ramPct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.ru, hw.ra);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - u8g2.getUTF8Width(buf), y, buf);
  disp_.drawProgressBar(NOCT_MARGIN + 2, y + 2,
                        NOCT_DISP_W - 2 * NOCT_MARGIN - 4, 6, ramPct);
  y += LINE_H_DATA + 4;

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "SYS(C:)");
  snprintf(buf, sizeof(buf), "%d%%", hw.su);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - u8g2.getUTF8Width(buf), y, buf);
  disp_.drawProgressBar(NOCT_MARGIN + 2, y + 2,
                        NOCT_DISP_W - 2 * NOCT_MARGIN - 4, 6, (float)hw.su);
  y += LINE_H_DATA + 4;

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "DATA(D:)");
  snprintf(buf, sizeof(buf), "%d%%", hw.du);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - u8g2.getUTF8Width(buf), y, buf);
  disp_.drawProgressBar(NOCT_MARGIN + 2, y + 2,
                        NOCT_DISP_W - 2 * NOCT_MARGIN - 4, 6, (float)hw.du);

  u8g2.setFont(FONT_TINY);
  snprintf(buf, sizeof(buf), "R:%dK W:%dK", hw.dr, hw.dw);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H - NOCT_MARGIN - 2, buf);
}

void SceneManager::drawTaskKill(bool blinkState) {
  ProcessData &procs = data_.procs();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[TASK.KILL]");

  int y = NOCT_MARGIN + LINE_H_HEAD + 6;
  char buf[24];

  if (procs.cpuNames[0].length() > 0) {
    if (procs.cpuPercent[0] > 80 && blinkState)
      u8g2.drawStr(NOCT_DISP_W - 24, NOCT_MARGIN + LINE_H_HEAD, "WARN");
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(NOCT_MARGIN + 2, y, "TOP PROC");
    String procName = procs.cpuNames[0];
    if (procName.length() > 18)
      procName = procName.substring(0, 18);
    u8g2.setFont(FONT_VAL);
    u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_DATA, procName.c_str());
    y += LINE_H_DATA + 4;
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(NOCT_MARGIN + 2, y, "CPU");
    snprintf(buf, sizeof(buf), "%d%%", procs.cpuPercent[0]);
    u8g2.setFont(FONT_BIG);
    u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_BIG, buf);
    y += LINE_H_BIG + 4;
    u8g2.setFont(FONT_TINY);
    for (int i = 1; i < 3; i++) {
      if (procs.cpuNames[i].length() > 0) {
        String pn = procs.cpuNames[i];
        if (pn.length() > 16)
          pn = pn.substring(0, 16);
        snprintf(buf, sizeof(buf), "%s %d%%", pn.c_str(), procs.cpuPercent[i]);
        u8g2.drawStr(NOCT_MARGIN + 2, y, buf);
        y += 6;
      }
    }
  } else {
    u8g2.setFont(FONT_VAL);
    u8g2.drawStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "NO DATA");
  }
}

void SceneManager::drawDeck(bool blinkState) {
  MediaData &media = data_.media();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[DECK]");

  int y = NOCT_MARGIN + LINE_H_HEAD + 6;

  // IDLE: paused / no session — show IDLE + sleep icon
  if (media.isIdle || (!media.isPlaying && (media.artist.length() > 0 ||
                                            media.track.length() > 0))) {
    u8g2.setFont(FONT_BIG);
    u8g2.drawStr(NOCT_MARGIN + 2, y + LINE_H_BIG, "IDLE");
    int cx = NOCT_DISP_W / 2, cy = y + LINE_H_BIG + 12;
    u8g2.drawRFrame(cx - 16, cy - 8, 32, 16, 2);
    u8g2.drawCircle(cx - 8, cy, 4);
    u8g2.drawCircle(cx + 8, cy, 4);
    u8g2.drawLine(cx - 8, cy - 4, cx + 8, cy - 4);
    u8g2.drawLine(cx - 8, cy + 4, cx + 8, cy + 4);
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(cx - 12, cy + 6, "zzz");
    return;
  }

  if (!media.isPlaying) {
    u8g2.setFont(FONT_BIG);
    u8g2.drawStr(NOCT_MARGIN + 2, y + LINE_H_BIG, "STANDBY");
    int cx = NOCT_DISP_W / 2, cy = y + LINE_H_BIG + 12;
    u8g2.drawRFrame(cx - 16, cy - 8, 32, 16, 2);
    u8g2.drawCircle(cx - 8, cy, 4);
    u8g2.drawCircle(cx + 8, cy, 4);
    u8g2.drawLine(cx - 8, cy - 4, cx + 8, cy - 4);
    u8g2.drawLine(cx - 8, cy + 4, cx + 8, cy + 4);
    return;
  }

  String art = media.artist.length() > 0 ? media.artist : "Unknown";
  if (art.length() > 20)
    art = art.substring(0, 20);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "ARTIST");
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_DATA, art.c_str());
  y += LINE_H_DATA + 4;

  String trk = media.track.length() > 0 ? media.track : "Unknown";
  if (trk.length() > 20)
    trk = trk.substring(0, 20);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "TRACK");
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_MARGIN + 2, y + LINE_H_DATA, trk.c_str());
  y += LINE_H_DATA + 4;

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 2, y, "STATUS");
  u8g2.setFont(FONT_VAL);
  const char *status = blinkState ? "> PLAYING" : "  PLAYING";
  u8g2.drawStr(NOCT_MARGIN + 2, y + LINE_H_DATA, status);
}

void SceneManager::drawWmoIcon(int x, int y, int wmoCode) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  int cx = x + 8, cy = y + 8;
  if (wmoCode == 0) {
    u8g2.drawCircle(cx, cy, 5);
    for (int i = 0; i < 4; i++) {
      float a = i * 90 * 3.14159f / 180.0f;
      u8g2.drawLine(cx, cy, cx + (int)(6 * cos(a)), cy + (int)(6 * sin(a)));
    }
    return;
  }
  if (wmoCode >= 95 && wmoCode <= 99) {
    u8g2.drawLine(cx - 4, cy - 6, cx + 2, cy);
    u8g2.drawLine(cx + 2, cy, cx - 2, cy + 6);
    return;
  }
  if (wmoCode >= 71 && wmoCode <= 86) {
    u8g2.drawLine(cx - 5, cy, cx, cy - 4);
    u8g2.drawLine(cx, cy - 4, cx + 5, cy);
    u8g2.drawLine(cx + 5, cy, cx - 3, cy + 4);
    u8g2.drawLine(cx - 3, cy + 4, cx - 5, cy);
    return;
  }
  if (wmoCode >= 51 && wmoCode <= 82) {
    for (int i = 0; i < 3; i++)
      u8g2.drawLine(cx - 4 + i * 4, cy - 2, cx - 4 + i * 4, cy + 6);
    return;
  }
  u8g2.drawDisc(cx, cy - 2, 4);
  u8g2.drawDisc(cx + 6, cy + 2, 3);
  u8g2.drawDisc(cx - 4, cy + 2, 3);
}

void SceneManager::drawWeather() {
  WeatherData &w = data_.weather();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[WEATHER]");

  int y = NOCT_MARGIN + LINE_H_HEAD + 6;
  drawWmoIcon(NOCT_MARGIN, y, w.wmoCode);

  char buf[24];
  snprintf(buf, sizeof(buf), "%dC", w.temp);
  u8g2.setFont(FONT_BIG);
  u8g2.drawUTF8(NOCT_MARGIN + 24, y + 10, buf);
  u8g2.setFont(FONT_TINY);
  String desc = w.desc;
  if (desc.length() > 16)
    desc = desc.substring(0, 16);
  u8g2.drawStr(NOCT_MARGIN + 24, y + 20, desc.c_str());
}

void SceneManager::drawRadar() {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[PHANTOM]");

  int rssi = WiFi.RSSI();
  int y = NOCT_MARGIN + LINE_H_HEAD + 8;
  int barW = NOCT_DISP_W - 2 * NOCT_MARGIN - 4;
  int barH = 8;
  float pct = (rssi >= -50)    ? 100.0f
              : (rssi >= -100) ? (float)(rssi + 100) * 2.0f
                               : 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;
  disp_.drawProgressBar(NOCT_MARGIN + 2, y, barW, barH, pct);

  u8g2.setFont(FONT_TINY);
  char buf[16];
  snprintf(buf, sizeof(buf), "RSSI %d dBm", rssi);
  u8g2.drawStr(NOCT_MARGIN + 2, y + barH + 8, buf);

  int cx = NOCT_DISP_W / 2, cy = NOCT_DISP_H - 20;
  int r = 18;
  u8g2.drawCircle(cx, cy, r);
  u8g2.drawCircle(cx, cy, r / 2);
  int sweep = (millis() / 80) % 360;
  float rad = sweep * 3.14159f / 180.0f;
  u8g2.drawLine(cx, cy, cx + (int)(r * cos(rad)), cy + (int)(r * sin(rad)));
}

void SceneManager::drawSearchMode(int scanPhase) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr((NOCT_DISP_W - 90) / 2, NOCT_DISP_H / 2 - 8, "SEARCH_MODE");

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 4, "Scanning for host...");

  int cx = NOCT_DISP_W / 2, cy = NOCT_DISP_H - 16;
  int r = 14;
  u8g2.drawCircle(cx, cy, r);
  int angle = (scanPhase * 30) % 360;
  float rad = angle * 3.14159f / 180.0f;
  u8g2.drawLine(cx, cy, cx + (int)(r * cos(rad)), cy + (int)(r * sin(rad)));
}

void SceneManager::drawMenu(int menuItem) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setDrawColor(0);
  u8g2.drawBox(NOCT_MARGIN + 4, NOCT_MARGIN + 2,
               NOCT_DISP_W - 2 * NOCT_MARGIN - 8,
               NOCT_DISP_H - 2 * NOCT_MARGIN - 4);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(NOCT_MARGIN + 4, NOCT_MARGIN + 2,
                  NOCT_DISP_W - 2 * NOCT_MARGIN - 8,
                  NOCT_DISP_H - 2 * NOCT_MARGIN - 4, 2);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN + 6, NOCT_MARGIN + 8, "SETTINGS");
  const char *labels[] = {"LED", "Carousel", "Contrast", "Display", "Exit"};
  int startY = NOCT_MARGIN + 14;
  for (int i = 0; i < 5; i++) {
    int y = startY + i * 8;
    if (y + 8 > NOCT_DISP_H - NOCT_MARGIN - 4)
      break;
    if (i == menuItem) {
      u8g2.drawBox(NOCT_MARGIN + 6, y - 6, NOCT_DISP_W - 2 * NOCT_MARGIN - 12,
                   8);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(NOCT_MARGIN + 8, y, labels[i]);
    if (i == menuItem)
      u8g2.setDrawColor(1);
  }
}

void SceneManager::drawNoSignal(bool wifiOk, bool tcpOk, int rssi,
                                bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_BIG);
  u8g2.drawStr((NOCT_DISP_W - 72) / 2, NOCT_DISP_H / 2 - 4, "NO SIGNAL");
  u8g2.setFont(FONT_TINY);
  if (!wifiOk)
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "WiFi: DISCONNECTED");
  else if (!tcpOk)
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "TCP: CONNECTING...");
  else
    u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "WAITING FOR DATA...");
  for (int i = 0; i < 30; i++)
    u8g2.drawPixel(random(0, NOCT_DISP_W), random(0, NOCT_DISP_H));
  if (wifiOk)
    disp_.drawWiFiIcon(NOCT_DISP_W - NOCT_MARGIN - 14, NOCT_MARGIN, rssi);
  if (blinkState)
    u8g2.drawBox(NOCT_DISP_W / 2 + 40, NOCT_DISP_H / 2 - 8, 6, 12);
}

void SceneManager::drawConnecting(int rssi, bool blinkState) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)blinkState;
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr((NOCT_DISP_W - 72) / 2, NOCT_DISP_H / 2 - 4, "LINKING...");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_DISP_H / 2 + 10, "Establishing data link");
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++)
    u8g2.drawBox(NOCT_DISP_W / 2 + 36 + i * 6, NOCT_DISP_H / 2 + 8, 3, 3);
  disp_.drawWiFiIcon(NOCT_DISP_W - NOCT_MARGIN - 14, NOCT_MARGIN, rssi);
}
