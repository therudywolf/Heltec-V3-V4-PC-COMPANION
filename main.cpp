/*
 * Heltec PC Monitor v3.0 — Cyber-Terminal
 * Heltec WiFi LoRa 32 V3 (ESP32-S3) + SSD1306 128x64.
 * Data: TCP JSON from monitor.py (2-char keys). Screens: SYS.OP, NET.IO,
 * THERMAL, STORAGE, ATMOS, MEDIA. Anti-ghosting: drawMetric() clears
 * value area before drawing. Fonts: 6x12 (data), micro (labels), haxrcorp
 * (headers).
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>

// ============================================================================
// CONFIG
// ============================================================================
#define WIFI_SSID "Forest"
#define WIFI_PASS ""
#define PC_IP "192.168.1.2"
#define TCP_PORT 8888

#ifndef CPU_TEMP_ALERT
#define CPU_TEMP_ALERT 80
#endif
#ifndef GPU_TEMP_ALERT
#define GPU_TEMP_ALERT 85
#endif

#define SDA_PIN 17
#define SCL_PIN 18
#define RST_PIN 21
#define VEXT_PIN 36
#define LED_PIN 35
#define BUTTON_PIN 0

#define DISP_W 128
#define DISP_H 64
#define MARGIN 2
#define TCP_LINE_MAX 4096
#define COVER_BITMAP_BYTES 288
#define COVER_B64_MAX_LEN 800

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_PIN);

// ============================================================================
// FONTS — Cyber-terminal
// ============================================================================
#define FONT_MAIN u8g2_font_6x12_tf
#define FONT_TINY u8g2_font_micro_tr
#define FONT_HEADER u8g2_font_haxrcorp4089_tr
#define FONT_WEATHER_ICON u8g2_font_open_iconic_weather_4x_t

#define LINE_H_MAIN 12
#define LINE_H_TINY 5
#define LINE_H_HEADER 10

// ============================================================================
// GLOBAL DATA (2-char keys from monitor.py)
// ============================================================================
struct HardwareData {
  int ct = 0, gt = 0, cl = 0, gl = 0;
  float ru = 0.0f, ra = 0.0f;
  int nd = 0, nu = 0;
  int cf = 0, s1 = 0, s2 = 0, gf = 0;
  int su = 0, du = 0;
  float vu = 0.0f, vt = 0.0f;
  int ch = 0;
  int dr = 0, dw = 0;
} hw;

struct WeatherData {
  int temp = 0;
  String desc = "";
  int icon = 0;
  int weekHigh[7] = {0};
  int weekLow[7] = {0};
  int weekCode[7] = {0};
} weather;

struct ProcessData {
  String cpuNames[3] = {"", "", ""};
  int cpuPercent[3] = {0};
  String ramNames[2] = {"", ""};
  int ramMb[2] = {0};
} procs;

struct MediaData {
  String artist = "";
  String track = "";
  bool isPlaying = false;
  uint8_t coverBitmap[COVER_BITMAP_BYTES];
  bool hasCover = false;
} media;

struct Settings {
  bool ledEnabled = true;
  bool carouselEnabled = false;
  int carouselIntervalSec = 10;
  int displayContrast = 128;
  bool displayInverted = false;
} settings;

// ============================================================================
// STATE
// ============================================================================
const int TOTAL_SCREENS = 6;
int currentScreen = 0;
bool inMenu = false;
int menuItem = 0;
const int MENU_ITEMS = 5;

unsigned long btnPressTime = 0;
bool btnHeld = false;
bool menuHoldHandled = false;
unsigned long lastMenuActivity = 0;
const unsigned long MENU_TIMEOUT_MS = 5 * 60 * 1000;

unsigned long lastUpdate = 0;
unsigned long lastCarousel = 0;
unsigned long lastBlink = 0;
unsigned long lastWifiRetry = 0;
unsigned long bootTime = 0;
bool blinkState = false;
const unsigned long SIGNAL_TIMEOUT_MS = 10000;
const unsigned long WIFI_RETRY_INTERVAL = 30000;

int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;
const unsigned long SCROLL_INTERVAL = 50;

bool splashDone = false;
unsigned long splashStart = 0;
const unsigned long SPLASH_MS = 1500;

WiFiClient tcpClient;
String tcpLineBuffer;
int lastSentScreen = -1;
bool wifiConnected = false;
int wifiRssi = 0;

// ============================================================================
// ANTI-GHOSTING — clear value area then draw (MANDATORY for dynamic text)
// ============================================================================
void drawMetric(int x, int y, const char *label, const char *value) {
  u8g2.setFont(FONT_MAIN);
  int vw = value ? u8g2.getUTF8Width(value) : 0;
  if (vw < 1)
    vw = 24;
  int maxW = DISP_W - x - MARGIN;
  if (vw > maxW)
    vw = maxW;

  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y - LINE_H_MAIN + 1, vw + 2, LINE_H_MAIN);
  u8g2.setDrawColor(1);
  if (label && strlen(label) > 0) {
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(x, y - LINE_H_MAIN - 1, label);
    u8g2.setFont(FONT_MAIN);
  }
  if (value)
    u8g2.drawUTF8(x, y, value);
}

void drawMetricStr(int x, int y, const char *label, const String &value) {
  drawMetric(x, y, label, value.c_str());
}

// ============================================================================
// HELPERS
// ============================================================================
void VextON() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
}

static int b64Decode(const char *in, size_t inLen, uint8_t *out,
                     size_t outMax) {
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int n = 0, val = 0, bits = 0;
  for (size_t i = 0; i < inLen && outMax > 0; i++) {
    if (in[i] == '=')
      break;
    const char *p = strchr(tbl, in[i]);
    if (!p)
      continue;
    val = (val << 6) | (int)(p - tbl);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      *out++ = (uint8_t)((val >> bits) & 0xFF);
      n++;
      outMax--;
    }
  }
  return n;
}

void drawProgressBar(int x, int y, int w, int h, float pct) {
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  u8g2.drawRFrame(x, y, w, h, 1);
  int fillW = (int)((w - 2) * (pct / 100.0f));
  if (fillW > 0)
    u8g2.drawRBox(x + 1, y + 1, fillW, h - 2, 1);
}

void drawWiFiIcon(int x, int y, int rssi) {
  int bars = (rssi >= -50)   ? 4
             : (rssi >= -60) ? 3
             : (rssi >= -70) ? 2
             : (rssi >= -80) ? 1
                             : 0;
  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 2;
    if (i < bars)
      u8g2.drawBox(x + i * 3, y + 8 - h, 2, h);
    else
      u8g2.drawFrame(x + i * 3, y + 8 - h, 2, h);
  }
}

// ============================================================================
// PARSING (2-char keys)
// ============================================================================
void parsePayload(JsonDocument &doc) {
  hw.ct = doc["ct"] | 0;
  hw.gt = doc["gt"] | 0;
  hw.cl = doc["cl"] | 0;
  hw.gl = doc["gl"] | 0;
  hw.ru = doc["ru"] | 0.0f;
  hw.ra = doc["ra"] | 0.0f;
  hw.nd = doc["nd"] | 0;
  hw.nu = doc["nu"] | 0;
  hw.cf = doc["cf"] | 0;
  hw.s1 = doc["s1"] | 0;
  hw.s2 = doc["s2"] | 0;
  hw.gf = doc["gf"] | 0;
  hw.su = doc["su"] | 0;
  hw.du = doc["du"] | 0;
  hw.vu = doc["vu"] | 0.0f;
  hw.vt = doc["vt"] | 0.0f;
  hw.ch = doc["ch"] | 0;
  hw.dr = doc["dr"] | 0;
  hw.dw = doc["dw"] | 0;

  if (doc.containsKey("wt")) {
    weather.temp = doc["wt"] | 0;
    const char *wd = doc["wd"];
    weather.desc = String(wd ? wd : "");
    weather.icon = doc["wi"] | 0;
    JsonArray wh = doc["w_wh"];
    JsonArray wl = doc["w_wl"];
    JsonArray wc = doc["w_wc"];
    for (int i = 0; i < 7; i++) {
      weather.weekHigh[i] = (i < wh.size()) ? (wh[i] | 0) : 0;
      weather.weekLow[i] = (i < wl.size()) ? (wl[i] | 0) : 0;
      weather.weekCode[i] = (i < wc.size()) ? (wc[i] | 0) : 0;
    }
  }

  JsonArray tp = doc["tp"];
  for (int i = 0; i < 3; i++) {
    if (i < tp.size()) {
      const char *n = tp[i]["n"];
      procs.cpuNames[i] = String(n ? n : "");
      procs.cpuPercent[i] = tp[i]["c"] | 0;
    } else {
      procs.cpuNames[i] = "";
      procs.cpuPercent[i] = 0;
    }
  }
  JsonArray tpRam = doc["tp_ram"];
  for (int i = 0; i < 2; i++) {
    if (i < tpRam.size()) {
      const char *n = tpRam[i]["n"];
      procs.ramNames[i] = String(n ? n : "");
      procs.ramMb[i] = tpRam[i]["r"] | 0;
    } else {
      procs.ramNames[i] = "";
      procs.ramMb[i] = 0;
    }
  }

  const char *art = doc["art"];
  const char *trk = doc["trk"];
  media.artist = String(art ? art : "");
  media.track = String(trk ? trk : "");
  media.isPlaying = doc["play"] | false;

  const char *cov = doc["cover_b64"];
  if (!cov || strlen(cov) == 0) {
    media.hasCover = false;
  } else {
    size_t covLen = strlen(cov);
    if (covLen <= (size_t)COVER_B64_MAX_LEN) {
      int n = b64Decode(cov, covLen, media.coverBitmap, COVER_BITMAP_BYTES);
      media.hasCover = (n == COVER_BITMAP_BYTES);
    }
  }
}

// ============================================================================
// SCREENS
// ============================================================================

// [SYS.OP] — 2x2 grid: CPU temp+load, GPU temp+load, RAM, VRAM or Uptime
void drawScreenSysOp() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN, MARGIN + LINE_H_HEADER, "[SYS.OP]");

  int gx = MARGIN;
  int gy = MARGIN + LINE_H_HEADER + 2;
  int halfW = (DISP_W - 2 * MARGIN - 2) / 2;
  int halfH = (DISP_H - gy - MARGIN - 2) / 2;
  char buf[24];

  // Top-Left: CPU Temp + Load bar
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(gx, gy + LINE_H_TINY, "CPU");
  snprintf(buf, sizeof(buf), "%dC %d%%", hw.ct, hw.cl);
  drawMetric(gx + halfW - 36, gy + LINE_H_MAIN + 2, nullptr, buf);
  drawProgressBar(gx, gy + LINE_H_MAIN + 4, halfW - 2, 5, (float)hw.cl);

  // Top-Right: GPU Temp + Load bar
  int rx = gx + halfW + 2;
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(rx, gy + LINE_H_TINY, "GPU");
  snprintf(buf, sizeof(buf), "%dC %d%%", hw.gt, hw.gl);
  drawMetric(rx + halfW - 36, gy + LINE_H_MAIN + 2, nullptr, buf);
  drawProgressBar(rx, gy + LINE_H_MAIN + 4, halfW - 2, 5, (float)hw.gl);

  int by = gy + halfH + 2;
  // Bot-Left: RAM Used/Total
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(gx, by + LINE_H_TINY, "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.ru, hw.ra);
  drawMetric(gx, by + LINE_H_MAIN + 2, nullptr, buf);

  // Bot-Right: VRAM or Uptime
  u8g2.setFont(FONT_TINY);
  if (hw.vt > 0.0f) {
    u8g2.drawStr(rx, by + LINE_H_TINY, "VRAM");
    snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.vu, hw.vt);
    drawMetric(rx, by + LINE_H_MAIN + 2, nullptr, buf);
  } else {
    u8g2.drawStr(rx, by + LINE_H_TINY, "UPTM");
    unsigned long s = (millis() - bootTime) / 1000;
    int m = s / 60, h = m / 60;
    snprintf(buf, sizeof(buf), "%d:%02d", h, m % 60);
    drawMetric(rx, by + LINE_H_MAIN + 2, nullptr, buf);
  }
}

// [NET.IO] — DOWN/UP big, graph-like bars, IP at bottom
void drawScreenNetIo() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN, MARGIN + LINE_H_HEADER, "[NET.IO]");

  int y = MARGIN + LINE_H_HEADER + 4;
  char buf[20];
  int downK = hw.nd;
  int upK = hw.nu;
  if (downK >= 1024)
    snprintf(buf, sizeof(buf), "DN %.1fM", downK / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "DN %dK", downK);
  drawMetricStr(MARGIN, y + LINE_H_MAIN, "DOWN", String(buf));
  y += LINE_H_MAIN + 4;
  if (upK >= 1024)
    snprintf(buf, sizeof(buf), "UP %.1fM", upK / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "UP %dK", upK);
  drawMetricStr(MARGIN, y + LINE_H_MAIN, "UP", String(buf));

  int barW = DISP_W - 2 * MARGIN - 4;
  int barY = y + LINE_H_MAIN + 6;
  int maxVal = (downK > upK) ? downK : (upK > 0 ? upK : 1);
  if (maxVal > 0) {
    float pctD = (downK * 100.0f) / (maxVal > 0 ? maxVal : 1);
    float pctU = (upK * 100.0f) / (maxVal > 0 ? maxVal : 1);
    drawProgressBar(MARGIN, barY, barW, 4, pctD > 100.0f ? 100.0f : pctD);
    drawProgressBar(MARGIN, barY + 6, barW, 4, pctU > 100.0f ? 100.0f : pctU);
  }

  u8g2.setFont(FONT_TINY);
  String ip = WiFi.localIP().toString();
  if (ip.length() > 18)
    ip = ip.substring(0, 18);
  u8g2.drawStr(MARGIN, DISP_H - MARGIN - 2, ip.c_str());
}

// [THERMAL] — Fans: CPU, SYS1, SYS2, GPU + Chipset temp
void drawScreenThermal() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN, MARGIN + LINE_H_HEADER, "[THERMAL]");

  int y = MARGIN + LINE_H_HEADER + 4;
  char buf[20];
  const char *labels[] = {"PUMP", "RAD ", "SYS2", "GPU "};
  int vals[] = {hw.cf, hw.s1, hw.s2, hw.gf};
  for (int i = 0; i < 4 && y + LINE_H_MAIN <= DISP_H - MARGIN; i++) {
    snprintf(buf, sizeof(buf), "%d RPM", vals[i]);
    drawMetricStr(MARGIN + 32, y + LINE_H_MAIN, labels[i], String(buf));
    y += LINE_H_MAIN + 2;
  }
  if (y + LINE_H_MAIN <= DISP_H - MARGIN && hw.ch > 0) {
    snprintf(buf, sizeof(buf), "%dC", hw.ch);
    drawMetricStr(MARGIN + 32, y + LINE_H_MAIN, "CHIP", String(buf));
  }
}

// [STORAGE] — NVMe / HDD bars, SYS (C:), DATA (D:), % usage
void drawScreenStorage() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN, MARGIN + LINE_H_HEADER, "[STORAGE]");

  int y = MARGIN + LINE_H_HEADER + 4;
  char buf[16];
  int pctSys = (hw.su > 100) ? 100 : (hw.su < 0 ? 0 : hw.su);
  int pctData = (hw.du > 100) ? 100 : (hw.du < 0 ? 0 : hw.du);

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(MARGIN, y + LINE_H_TINY, "SYS (C:)");
  snprintf(buf, sizeof(buf), "%d%%", pctSys);
  drawMetric(DISP_W - MARGIN - 28, y + LINE_H_MAIN, nullptr, buf);
  drawProgressBar(MARGIN, y + LINE_H_MAIN + 2, DISP_W - 2 * MARGIN - 4, 5,
                  (float)pctSys);
  y += LINE_H_MAIN + 10;

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(MARGIN, y + LINE_H_TINY, "DATA (D:)");
  snprintf(buf, sizeof(buf), "%d%%", pctData);
  drawMetric(DISP_W - MARGIN - 28, y + LINE_H_MAIN, nullptr, buf);
  drawProgressBar(MARGIN, y + LINE_H_MAIN + 2, DISP_W - 2 * MARGIN - 4, 5,
                  (float)pctData);
}

// [ATMOS] — Weather icon (open_iconic_weather: 69=sun, 65=sun_cloud, 64=cloud,
// 67=rain)
void drawScreenAtmos() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN, MARGIN + LINE_H_HEADER, "[ATMOS]");

  int iconX = MARGIN + 2;
  int iconY = MARGIN + LINE_H_HEADER + 4;
  u8g2.setFont(FONT_WEATHER_ICON);
  int code = weather.icon;
  uint8_t glyph = 64; // cloud
  if (code == 0)
    glyph = 69;
  else if (code >= 1 && code <= 3)
    glyph = 65;
  else if (code >= 51)
    glyph = 67;
  else if (code >= 71 && code <= 86)
    glyph = 67;
  u8g2.drawGlyph(iconX, iconY + 16, glyph);

  char buf[16];
  snprintf(buf, sizeof(buf), "%dC", weather.temp);
  u8g2.setFont(FONT_MAIN);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(38, iconY, tw + 2, LINE_H_MAIN);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(40, iconY + LINE_H_MAIN - 1, buf);

  int descW = DISP_W - 42 - MARGIN;
  u8g2.setFont(FONT_TINY);
  String d = weather.desc.length() > 0 ? weather.desc : "—";
  if (d.length() > 12)
    d = d.substring(0, 12);
  int textW = u8g2.getUTF8Width(d.c_str());
  if (textW <= descW) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(38, iconY + LINE_H_MAIN + 2, descW, LINE_H_TINY);
    u8g2.setDrawColor(1);
    u8g2.drawUTF8(40, iconY + LINE_H_MAIN + LINE_H_TINY + 1, d.c_str());
  } else {
    int offset = (scrollOffset / 4) % (textW + 24);
    u8g2.setDrawColor(0);
    u8g2.drawBox(38, iconY + LINE_H_MAIN + 2, descW, LINE_H_TINY);
    u8g2.setDrawColor(1);
    u8g2.setClipWindow(38, iconY + LINE_H_MAIN + 2, 38 + descW,
                       iconY + LINE_H_MAIN + LINE_H_TINY + 4);
    u8g2.drawUTF8(40 - offset, iconY + LINE_H_MAIN + LINE_H_TINY + 1,
                  d.c_str());
    u8g2.setMaxClipWindow();
  }

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(MARGIN, DISP_H - MARGIN - 2, "LOCATION");
}

// [MEDIA] — STANDBY or Artist / Track + progress
void drawScreenMedia() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN, MARGIN + LINE_H_HEADER, "[MEDIA]");

  int y = MARGIN + LINE_H_HEADER + 4;
  if (!media.isPlaying) {
    u8g2.setFont(FONT_MAIN);
    u8g2.setDrawColor(0);
    u8g2.drawBox(MARGIN, y, DISP_W - 2 * MARGIN, LINE_H_MAIN);
    u8g2.setDrawColor(1);
    u8g2.drawStr(MARGIN + 2, y + LINE_H_MAIN - 2, "STANDBY");
    return;
  }

  String art = media.artist.length() > 0 ? media.artist : "—";
  if (art.length() > 18)
    art = art.substring(0, 18);
  String trk = media.track.length() > 0 ? media.track : "—";
  if (trk.length() > 18)
    trk = trk.substring(0, 18);

  u8g2.setFont(FONT_MAIN);
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN, y, DISP_W - 2 * MARGIN, LINE_H_MAIN);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_MAIN - 2, art.c_str());
  y += LINE_H_MAIN + 2;

  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN, y, DISP_W - 2 * MARGIN, LINE_H_MAIN);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_MAIN - 2, trk.c_str());
  y += LINE_H_MAIN + 4;

  drawProgressBar(MARGIN, y, DISP_W - 2 * MARGIN, 4, 50.0f);
}

// ============================================================================
// DISPATCH
// ============================================================================
void drawScreen(int screen) {
  switch (screen) {
  case 0:
    drawScreenSysOp();
    break;
  case 1:
    drawScreenNetIo();
    break;
  case 2:
    drawScreenThermal();
    break;
  case 3:
    drawScreenStorage();
    break;
  case 4:
    drawScreenAtmos();
    break;
  case 5:
    drawScreenMedia();
    break;
  default:
    drawScreenSysOp();
    break;
  }
}

void drawSplash() {
  u8g2.setFont(FONT_HEADER);
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN, 8, DISP_W - 2 * MARGIN, LINE_H_HEADER);
  u8g2.setDrawColor(1);
  u8g2.drawStr(24, 18, "[HELTEC]");
  u8g2.setFont(FONT_MAIN);
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN, 24, 70, LINE_H_MAIN);
  u8g2.setDrawColor(1);
  u8g2.drawStr(24, 34, "PC MON v3");
  int barW =
      (int)((millis() - splashStart) * (DISP_W - 2 * MARGIN) / SPLASH_MS);
  if (barW > DISP_W - 2 * MARGIN)
    barW = DISP_W - 2 * MARGIN;
  u8g2.drawBox(MARGIN, DISP_H - 2 - MARGIN, barW, 2);
}

void drawMenu() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 4, MARGIN + 2, DISP_W - 2 * MARGIN - 8,
               DISP_H - 2 * MARGIN - 4);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(MARGIN + 4, MARGIN + 2, DISP_W - 2 * MARGIN - 8,
                  DISP_H - 2 * MARGIN - 4, 2);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(MARGIN + 6, MARGIN + 8, "SET");
  const char *labels[] = {"LED", "Carousel", "Contrast", "Display", "Exit"};
  int startY = MARGIN + 12;
  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = startY + i * 6;
    if (y + 6 > DISP_H - MARGIN - 4)
      break;
    if (i == menuItem) {
      u8g2.drawBox(MARGIN + 6, y - 5, DISP_W - 2 * MARGIN - 12, 6);
      u8g2.setDrawColor(0);
    }
    if (i == 4) {
      u8g2.drawStr(MARGIN + 8, y, "> Exit");
    } else {
      u8g2.drawStr(MARGIN + 8, y, labels[i]);
    }
    if (i == menuItem)
      u8g2.setDrawColor(1);
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  bootTime = millis();
  VextON();
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(50);
  digitalWrite(RST_PIN, HIGH);
  delay(50);

  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.enableUTF8Print();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Preferences prefs;
  prefs.begin("heltec", true);
  settings.ledEnabled = prefs.getBool("led", true);
  settings.carouselEnabled = prefs.getBool("carousel", false);
  settings.carouselIntervalSec = prefs.getInt("carouselSec", 10);
  if (settings.carouselIntervalSec != 5 && settings.carouselIntervalSec != 10 &&
      settings.carouselIntervalSec != 15)
    settings.carouselIntervalSec = 10;
  settings.displayContrast = prefs.getInt("contrast", 128);
  if (settings.displayContrast > 255)
    settings.displayContrast = 255;
  if (settings.displayContrast < 0)
    settings.displayContrast = 0;
  settings.displayInverted = prefs.getBool("inverted", false);
  prefs.end();

  u8g2.setContrast((uint8_t)settings.displayContrast);
  u8g2.setFlipMode(settings.displayInverted ? 1 : 0);

  if (strlen(WIFI_PASS) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      wifiRssi = WiFi.RSSI();
    }
    if (now % 5000 < 100)
      wifiRssi = WiFi.RSSI();

    if (!tcpClient.connected()) {
      tcpClient.connect(PC_IP, TCP_PORT);
      if (tcpClient.connected()) {
        tcpLineBuffer = "";
        lastSentScreen = -1;
      }
    }
  } else {
    wifiConnected = false;
    if (now - lastWifiRetry > WIFI_RETRY_INTERVAL) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      lastWifiRetry = now;
    }
  }

  if (tcpClient.connected()) {
    while (tcpClient.available()) {
      char c = tcpClient.read();
      if (c == '\n') {
        if (tcpLineBuffer.length() > 0) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, tcpLineBuffer);
          if (!err) {
            lastUpdate = now;
            parsePayload(doc);
          }
          tcpLineBuffer = "";
        }
      } else {
        tcpLineBuffer += c;
        if (tcpLineBuffer.length() >= TCP_LINE_MAX)
          tcpLineBuffer = "";
      }
    }
  }

  int btnState = digitalRead(BUTTON_PIN);
  if (btnState == LOW && !btnHeld) {
    btnHeld = true;
    btnPressTime = now;
    menuHoldHandled = false;
  }
  if (btnState == HIGH && btnHeld) {
    unsigned long duration = now - btnPressTime;
    btnHeld = false;
    if (duration > 800) {
      if (!inMenu) {
        inMenu = true;
        menuItem = 0;
        lastMenuActivity = now;
      }
    } else {
      if (inMenu) {
        lastMenuActivity = now;
        menuItem++;
        if (menuItem >= MENU_ITEMS)
          menuItem = 0;
      } else {
        currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
        lastCarousel = now;
      }
    }
  }

  if (inMenu && btnState == LOW && (now - btnPressTime > 800) &&
      !menuHoldHandled) {
    menuHoldHandled = true;
    lastMenuActivity = now;
    Preferences prefs;
    prefs.begin("heltec", false);
    switch (menuItem) {
    case 0:
      settings.ledEnabled = !settings.ledEnabled;
      prefs.putBool("led", settings.ledEnabled);
      break;
    case 1:
      settings.carouselEnabled = !settings.carouselEnabled;
      prefs.putBool("carousel", settings.carouselEnabled);
      break;
    case 2:
      settings.displayContrast = (settings.displayContrast <= 128)   ? 192
                                 : (settings.displayContrast <= 192) ? 255
                                                                     : 128;
      u8g2.setContrast((uint8_t)settings.displayContrast);
      prefs.putInt("contrast", settings.displayContrast);
      break;
    case 3:
      settings.displayInverted = !settings.displayInverted;
      u8g2.setFlipMode(settings.displayInverted ? 1 : 0);
      prefs.putBool("inverted", settings.displayInverted);
      break;
    case 4:
      inMenu = false;
      break;
    }
    prefs.end();
  }

  if (inMenu && (now - lastMenuActivity > MENU_TIMEOUT_MS))
    inMenu = false;

  if (settings.carouselEnabled && !inMenu) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
      lastCarousel = now;
    }
  }

  if (tcpClient.connected() && lastSentScreen != currentScreen) {
    tcpClient.print("screen:");
    tcpClient.print(currentScreen);
    tcpClient.print("\n");
    lastSentScreen = currentScreen;
  }

  if (now - lastScrollUpdate >= SCROLL_INTERVAL) {
    scrollOffset++;
    lastScrollUpdate = now;
  }

  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }
  bool anyAlarm = (hw.ct >= CPU_TEMP_ALERT) || (hw.gt >= GPU_TEMP_ALERT);
  if (settings.ledEnabled && anyAlarm && blinkState)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);

  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= SPLASH_MS)
      splashDone = true;
  }

  u8g2.clearBuffer();
  bool signalLost = (now - lastUpdate > SIGNAL_TIMEOUT_MS);

  if (!splashDone) {
    drawSplash();
  } else if (signalLost) {
    if (tcpClient.connected()) {
      tcpClient.stop();
      tcpLineBuffer = "";
      lastSentScreen = -1;
    }
    u8g2.setFont(FONT_MAIN);
    u8g2.setDrawColor(0);
    u8g2.drawBox(MARGIN, (DISP_H - LINE_H_MAIN) / 2 - 2, DISP_W - 2 * MARGIN,
                 LINE_H_MAIN + 4);
    u8g2.setDrawColor(1);
    u8g2.drawStr((DISP_W - 56) / 2, DISP_H / 2 + 4, "RECONNECT");
    if (wifiConnected)
      drawWiFiIcon(DISP_W - MARGIN - 14, MARGIN, wifiRssi);
  } else {
    drawScreen(currentScreen);
    if (wifiConnected)
      drawWiFiIcon(DISP_W - 16, 2, wifiRssi);
    if (anyAlarm && blinkState)
      u8g2.drawFrame(0, 0, DISP_W, DISP_H);
    if (inMenu)
      drawMenu();
    for (int i = 0; i < TOTAL_SCREENS; i++) {
      int x = DISP_W / 2 - (TOTAL_SCREENS * 3) + (i * 6);
      if (i == currentScreen)
        u8g2.drawBox(x, DISP_H - 2, 2, 2);
      else
        u8g2.drawPixel(x + 1, DISP_H - 1);
    }
  }

  u8g2.sendBuffer();
}
