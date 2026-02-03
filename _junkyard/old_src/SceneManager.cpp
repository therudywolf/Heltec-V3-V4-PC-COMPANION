/*
 * NOCTURNE_OS — SceneManager: HUB, CPU, GPU, NET, ATMOS, MEDIA (6 scenes)
 */
#include "nocturne/SceneManager.h"
#include "nocturne/config.h"
#include <WiFi.h>
#include <mbedtls/base64.h>

#define LINE_H_DATA NOCT_LINE_H_DATA
#define LINE_H_LABEL NOCT_LINE_H_LABEL
#define LINE_H_HEAD NOCT_LINE_H_HEAD
#define LINE_H_BIG NOCT_LINE_H_BIG

SceneManager::SceneManager(DisplayEngine &disp, DataManager &data)
    : disp_(disp), data_(data) {}

void SceneManager::draw(int sceneIndex, unsigned long bootTime, bool blinkState,
                        int fanFrame) {
  switch (sceneIndex) {
  case NOCT_SCENE_HUB:
    drawHub(bootTime);
    break;
  case NOCT_SCENE_CPU:
    drawCpu(bootTime, fanFrame);
    break;
  case NOCT_SCENE_GPU:
    drawGpu(fanFrame);
    break;
  case NOCT_SCENE_NET:
    drawNet();
    break;
  case NOCT_SCENE_ATMOS:
    drawAtmos();
    break;
  case NOCT_SCENE_MEDIA:
    drawMedia(blinkState);
    break;
  default:
    drawHub(bootTime);
    break;
  }
}

// SCENE_HUB: Top CPU 55° | GPU 64°; Mid 12:45 (big); Bot RAM [|||||| ] bar
void SceneManager::drawHub(unsigned long bootTime) {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  char buf[24];

  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, NOCT_MARGIN + LINE_H_LABEL, "CPU");
  snprintf(buf, sizeof(buf), "%dC", hw.ct);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_MARGIN, NOCT_MARGIN + LINE_H_LABEL + LINE_H_DATA, buf);
  u8g2.drawStr(NOCT_DISP_W / 2 - 20, NOCT_MARGIN + LINE_H_LABEL, "GPU");
  snprintf(buf, sizeof(buf), "%dC", hw.gt);
  u8g2.drawUTF8(NOCT_DISP_W / 2 - 20, NOCT_MARGIN + LINE_H_LABEL + LINE_H_DATA,
                buf);

  unsigned long s = (millis() - bootTime) / 1000;
  int m = (int)(s / 60), h = m / 60;
  snprintf(buf, sizeof(buf), "%d:%02d", h, m % 60);
  u8g2.setFont(FONT_BIG);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.drawUTF8((NOCT_DISP_W - tw) / 2, NOCT_DISP_H / 2 + 4, buf);

  float ramPct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(NOCT_MARGIN, NOCT_DISP_H - NOCT_MARGIN - LINE_H_LABEL - 6,
               "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.ru, hw.ra);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(NOCT_DISP_W - NOCT_MARGIN - u8g2.getUTF8Width(buf),
                NOCT_DISP_H - NOCT_MARGIN - LINE_H_LABEL - 6, buf);
  disp_.drawProgressBar(NOCT_MARGIN, NOCT_DISP_H - NOCT_MARGIN - 6,
                        NOCT_DISP_W - 2 * NOCT_MARGIN, 5, ramPct);
}

// SCENE_CPU: Rolling Load sparkline; Stats 4.6GHz, 65W, FAN 1200
void SceneManager::drawCpu(unsigned long bootTime, int fanFrame) {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  (void)bootTime;
  char buf[24];

  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[CPU]");
  disp_.drawFanIcon(NOCT_DISP_W - 14, NOCT_MARGIN + LINE_H_HEAD - 2, fanFrame);

  int graphY = NOCT_FOOTER_Y;
  disp_.drawRollingGraph(NOCT_MARGIN, graphY, NOCT_DISP_W - 2 * NOCT_MARGIN,
                         NOCT_GRAPH_HEIGHT, disp_.cpuGraph, 100);

  int y = NOCT_MARGIN + LINE_H_HEAD + 4;
  float ghz = hw.cc / 1000.0f;
  snprintf(buf, sizeof(buf), "%.1fGHz", ghz > 0 ? ghz : 0.0f);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
  y += LINE_H_DATA + 2;
  snprintf(buf, sizeof(buf), "%dW", hw.pw);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "PWR", String(buf));
  y += LINE_H_DATA + 2;
  snprintf(buf, sizeof(buf), "FAN %d", hw.cf);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
}

// SCENE_GPU: Rolling Load; HOTSPOT 85°, VRAM 80%, FAN 0
void SceneManager::drawGpu(int fanFrame) {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  char buf[24];

  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[GPU]");
  disp_.drawFanIcon(NOCT_DISP_W - 14, NOCT_MARGIN + LINE_H_HEAD - 2, fanFrame);

  int graphY = NOCT_FOOTER_Y;
  disp_.drawRollingGraph(NOCT_MARGIN, graphY, NOCT_DISP_W - 2 * NOCT_MARGIN,
                         NOCT_GRAPH_HEIGHT, disp_.gpuGraph, 100);

  int y = NOCT_MARGIN + LINE_H_HEAD + 4;
  snprintf(buf, sizeof(buf), "HOTSPOT %dC", hw.gh);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
  y += LINE_H_DATA + 2;
  snprintf(buf, sizeof(buf), "VRAM %d%%", hw.gv);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
  y += LINE_H_DATA + 2;
  snprintf(buf, sizeof(buf), "FAN %d", hw.gf);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
}

// SCENE_NET: Network graph; DL 15 MB/s, UP 2 MB/s
void SceneManager::drawNet() {
  HardwareData &hw = data_.hw();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  char buf[24];

  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[NET]");

  int graphY = NOCT_FOOTER_Y;
  disp_.netDownGraph.setMax(2048);
  disp_.drawRollingGraph(NOCT_MARGIN, graphY, NOCT_DISP_W - 2 * NOCT_MARGIN,
                         NOCT_GRAPH_HEIGHT, disp_.netDownGraph, 2048);

  int y = NOCT_MARGIN + LINE_H_HEAD + 4;
  if (hw.nd >= 1024)
    snprintf(buf, sizeof(buf), "DL %.1f MB/s", hw.nd / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "DL %d KB/s", hw.nd);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
  y += LINE_H_DATA + 2;
  if (hw.nu >= 1024)
    snprintf(buf, sizeof(buf), "UP %.1f MB/s", hw.nu / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "UP %d KB/s", hw.nu);
  disp_.drawMetricStr(NOCT_MARGIN + 2, y + LINE_H_DATA, "", String(buf));
}

// SCENE_ATMOS: Open-Meteo; pixel icon + temp (-10°C)
void SceneManager::drawAtmos() {
  WeatherData &w = data_.weather();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  u8g2.setFont(FONT_HEAD);
  u8g2.drawStr(NOCT_MARGIN + 2, NOCT_MARGIN + LINE_H_HEAD, "[ATMOS]");

  int y = NOCT_MARGIN + LINE_H_HEAD + 6;
  if (!data_.weatherReceived() || (w.temp == 0 && w.desc.length() == 0)) {
    u8g2.setFont(FONT_BIG);
    u8g2.drawStr(NOCT_MARGIN + 2, y + LINE_H_BIG, "NO DATA");
    return;
  }
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

// SCENE_MEDIA: Left 0–64 dithered art (Base64); Right 65–128 track (scroll),
// artist
void SceneManager::drawMedia(bool blinkState) {
  MediaData &media = data_.media();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = disp_.u8g2();
  const int ART_W = 64;
  const int ART_H = 64;
  const int RAW_BYTES = (ART_W * ART_H) / 8;

  if (media.coverB64.length() > 0) {
    unsigned char raw[512];
    size_t olen = 0;
    int ret = mbedtls_base64_decode(
        raw, sizeof(raw), &olen, (const unsigned char *)media.coverB64.c_str(),
        media.coverB64.length());
    if (ret == 0 && olen == (size_t)RAW_BYTES) {
      for (int row = 0; row < ART_H; row++) {
        for (int col = 0; col < ART_W; col++) {
          int byteIdx = row * (ART_W / 8) + col / 8;
          int bit = col % 8;
          if (raw[byteIdx] & (1 << bit))
            u8g2.drawPixel(col, row);
        }
      }
    }
  }

  int rx = 65;
  int ry = NOCT_MARGIN;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(rx, ry + LINE_H_LABEL, "TRACK");
  String trk = media.track.length() > 0 ? media.track : "—";
  if (trk.length() > 14)
    trk = trk.substring(0, 14);
  u8g2.setFont(FONT_VAL);
  int scrollLen = (int)trk.length() * 6 + 24;
  if (scrollLen < 1)
    scrollLen = 1;
  int scroll = (millis() / 50) % scrollLen;
  if (trk.length() > 10) {
    int offset = 64 - scroll;
    u8g2.drawUTF8(rx + offset, ry + LINE_H_LABEL + LINE_H_DATA, trk.c_str());
  } else {
    u8g2.drawUTF8(rx, ry + LINE_H_LABEL + LINE_H_DATA, trk.c_str());
  }
  ry += LINE_H_DATA + 8;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(rx, ry + LINE_H_LABEL, "ARTIST");
  String art = media.artist.length() > 0 ? media.artist : "—";
  if (art.length() > 14)
    art = art.substring(0, 14);
  u8g2.setFont(FONT_VAL);
  u8g2.drawUTF8(rx, ry + LINE_H_LABEL + LINE_H_DATA, art.c_str());

  if (!media.isPlaying && !media.isIdle) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(rx, NOCT_DISP_H - NOCT_MARGIN, "STANDBY");
  } else if (media.isIdle) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(rx, NOCT_DISP_H - NOCT_MARGIN, blinkState ? "IDLE" : "zzz");
  }
  (void)blinkState;
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
