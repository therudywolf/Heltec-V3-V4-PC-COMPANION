/*
 * Heltec PC Monitor v2.0
 * Display firmware for Heltec WiFi LoRa 32 V3 (128x64 OLED).
 * Data source: TCP JSON from monitor.py (host PC). One line per update.
 * Screens (0-10): Main, Cores, CPU Details, GPU Details, Memory, Storage,
 *   Media, Cooling, Weather (3 subpages), Top CPU, Equalizer.
 * Controls: short press = next screen; long press = open/close menu;
 *   carousel (if enabled) auto-cycles screens.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>

// ============================================================================
// CONFIGURATION — WiFi, PC, alerts (override alerts via platformio build_flags)
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
#ifndef CPU_LOAD_ALERT
#define CPU_LOAD_ALERT 95
#endif
#ifndef GPU_LOAD_ALERT
#define GPU_LOAD_ALERT 95
#endif

// ============================================================================
// HARDWARE PINOUT (Heltec WiFi LoRa 32 V3)
// ============================================================================
#define SDA_PIN 17
#define SCL_PIN 18
#define RST_PIN 21
#define VEXT_PIN 36
#define LED_PIN 35
#define BUTTON_PIN 0

// ============================================================================
// DISPLAY — dimensions, margins, TCP/cover buffer limits
// ============================================================================
#define DISP_W 128
#define DISP_H 64
#define MARGIN 8
#define CARD_PADDING 3
#define TCP_LINE_MAX 2048
#define COVER_BITMAP_BYTES 288
#define COVER_B64_MAX_LEN 600

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_PIN);

// ============================================================================
// UNIFIED FONT SYSTEM — readable, safe U8g2 fonts for 128x64 (digits, %, °,
// Latin/Cyrillic)
// ============================================================================
#define FONT_TITLE u8g2_font_7x13B_tf // Bold headers
#define FONT_MAIN u8g2_font_7x13_tf   // Main data
#define FONT_SMALL u8g2_font_6x10_tf  // Small text (readable, predictable)
#define FONT_TINY u8g2_font_4x6_tf    // Tiny (C1-C6, menu; 6px line fit)
#define FONT_LARGE u8g2_font_9x15B_tf // Large numbers
#define FONT_MEDIA                                                             \
  u8g2_font_7x13_t_cyrillic // Latin + Cyrillic (media, weather)

// ============================================================================
// GLOBAL DATA STRUCTURES
// ============================================================================

// Hardware Monitoring Data
struct HardwareData {
  // Temperatures (C)
  int cpuTemp = 0;
  int gpuTemp = 0;
  int gpuHotSpot = 0;
  int nvme2Temp = 0;
  int chipsetTemp = 0;
  int diskTemp[4] = {0};

  // Loads & Power (%)
  int cpuLoad = 0;
  int cpuPwr = 0;
  int gpuLoad = 0;
  int gpuPwr = 0;

  // Fans (RPM)
  int fanPump = 0;
  int fanRad = 0;
  int fanCase = 0;
  int fanGpu = 0;

  // Clock & Cores
  int gpuClk = 0;
  int cores[6] = {0};
  int cpuMhzFallback = 0;
  int coreLoad[6] = {0};

  // Memory
  int ramPct = 0;
  float ramUsed = 0.0;
  float ramTotal = 0.0;
  float vramUsed = 0.0;
  float vramTotal = 0.0;

  // Disks
  float diskUsed[4] = {0};
  float diskTotal[4] = {0};

  // Network & Disk I/O
  int netUp = 0;
  int netDown = 0;
  int diskRead = 0;
  int diskWrite = 0;
} hw;

// Weather Data
struct WeatherData {
  int temp = 0;
  String desc = "";
  int icon = 0;
  int dayHigh = 0;
  int dayLow = 0;
  int dayCode = 0;
  int weekHigh[7] = {0};
  int weekLow[7] = {0};
  int weekCode[7] = {0};
  int page = 0; // 0=Now, 1=Today, 2=Week
} weather;

// Process Data
struct ProcessData {
  String cpuNames[3] = {"", "", ""};
  int cpuPercent[3] = {0};
  String ramNames[2] = {"", ""};
  int ramMb[2] = {0};
} procs;

// Media Player Data
struct MediaData {
  String artist = "No Data";
  String track = "Waiting...";
  bool isPlaying = false;
  uint8_t coverBitmap[COVER_BITMAP_BYTES];
  bool hasCover = false;
} media;

// ============================================================================
// SETTINGS & STATE
// ============================================================================
struct Settings {
  bool ledEnabled = true;
  bool carouselEnabled = false;
  int carouselIntervalSec = 10;
  int displayContrast = 255;
  int eqStyle = 0; // 0=Bars, 1=Wave, 2=Circle
  bool displayInverted = false;
} settings;

// UI State — screen order must match JSON usage: 0=Main, 1=Cores, 2=CPU, 3=GPU,
// 4=Memory, 5=Storage, 6=Media, 7=Cooling, 8=Weather, 9=Top CPU, 10=Equalizer
int currentScreen = 0;
const int TOTAL_SCREENS = 11;
bool inMenu = false;
int menuItem = 0;
const int MENU_ITEMS = 7;

// Button State
unsigned long btnPressTime = 0;
bool btnHeld = false;
bool menuHoldHandled = false;
bool wasInMenuOnPress = false;
unsigned long lastMenuActivity = 0;
const unsigned long MENU_TIMEOUT_MS = 5 * 60 * 1000;

// Timers
unsigned long lastUpdate = 0;
unsigned long lastCarousel = 0;
unsigned long lastBlink = 0;
unsigned long lastEqUpdate = 0;
unsigned long lastWifiRetry = 0;
bool blinkState = false;
const unsigned long SIGNAL_TIMEOUT_MS = 10000;
const unsigned long EQ_UPDATE_MS = 28;
const unsigned long WIFI_RETRY_INTERVAL = 30000;

// Equalizer
const int EQ_BARS = 16;
uint8_t eqHeights[EQ_BARS] = {0};
uint8_t eqTargets[EQ_BARS] = {0};

// Splash Screen
bool splashDone = false;
unsigned long splashStart = 0;
const unsigned long SPLASH_MS = 1500;

// WiFi & TCP
WiFiClient tcpClient;
unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TRY_MS = 8000;
String tcpLineBuffer;
int lastSentScreen = -1;
bool wifiConnected = false;
int wifiRssi = 0;

// Animation
int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;
const unsigned long SCROLL_INTERVAL = 50;

// ============================================================================
// WEATHER ICONS (16x16 bitmaps)
// ============================================================================
const uint8_t iconSunny[32] PROGMEM = {
    0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x04, 0x20, 0x0C, 0x30, 0x3F,
    0xFC, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC, 0x0C, 0x30,
    0x04, 0x20, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00};

const uint8_t iconCloudy[32] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x09, 0x80, 0x11,
    0xC0, 0x21, 0xC0, 0x7F, 0xE0, 0x7F, 0xF0, 0xFF, 0xF8, 0xFF, 0xF8,
    0xFF, 0xF8, 0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t iconRain[32] PROGMEM = {
    0x00, 0x00, 0x07, 0x00, 0x09, 0x80, 0x11, 0xC0, 0x7F, 0xE0, 0xFF,
    0xF0, 0xFF, 0xF8, 0x7F, 0xF0, 0x00, 0x00, 0x22, 0x44, 0x22, 0x44,
    0x22, 0x44, 0x44, 0x22, 0x44, 0x22, 0x44, 0x22, 0x00, 0x00};

const uint8_t iconSnow[32] PROGMEM = {
    0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x11, 0x88, 0x0A, 0x50, 0x04,
    0x20, 0xE7, 0xCE, 0x04, 0x20, 0x0A, 0x50, 0x11, 0x88, 0x01, 0x80,
    0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
String _weatherDescFromCode(int code);
void parseHardwareBasic(JsonDocument &doc);
void parseFullPayload(JsonDocument &doc);
void parseMediaFromDoc(JsonDocument &doc, bool useDefaults);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void VextON() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
}

// Base64 Decoder for album covers
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
    val = (val << 6) | (p - tbl);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      *out++ = (val >> bits) & 0xFF;
      n++;
      outMax--;
    }
  }
  return n;
}

// Check if string is ASCII only
static bool isOnlyAscii(const String &s) {
  for (unsigned int i = 0; i < s.length(); i++) {
    if ((unsigned char)s.charAt(i) > 127)
      return false;
  }
  return true;
}

// Parse basic hardware and play state (heartbeat / any payload).
// Keys match monitor.py build_payload; values are numbers (LHM parsed by
// clean_val on server).
void parseHardwareBasic(JsonDocument &doc) {
  hw.cpuTemp = doc["ct"] | 0;
  hw.gpuTemp = doc["gt"] | 0;
  hw.gpuHotSpot = doc["gth"] | 0;
  hw.cpuLoad = doc["cpu_load"] | 0;
  hw.cpuPwr = doc["cpu_pwr"] | 0;
  hw.gpuLoad = doc["gpu_load"] | 0;
  hw.gpuPwr = doc["gpu_pwr"] | 0;
  hw.fanPump = doc["p"] | 0;
  hw.fanRad = doc["r"] | 0;
  hw.fanCase = doc["c"] | 0;
  hw.fanGpu = doc["gf"] | 0;
  hw.gpuClk = doc["gck"] | 0;
  hw.cpuMhzFallback = doc["cpu_mhz"] | 0;
  hw.nvme2Temp = doc["nvme2_t"] | 0;
  hw.chipsetTemp = doc["chipset_t"] | 0;
  media.isPlaying = doc["play"] | false;
}

// Parse full payload: cores, memory, disks, network, weather, processes, media.
// Disk indices: 0=nvme/2, 1=nvme/3, 2=hdd/0, 3=ssd/1 (match TARGETS d1..d4 in
// monitor.py).
void parseFullPayload(JsonDocument &doc) {
  JsonArray c_arr = doc["c_arr"];
  for (int i = 0; i < 6; i++) {
    hw.cores[i] = (i < c_arr.size()) ? (c_arr[i] | 0) : 0;
  }
  JsonArray cl_arr = doc["cl_arr"];
  for (int i = 0; i < 6; i++) {
    hw.coreLoad[i] = (i < cl_arr.size()) ? (cl_arr[i] | 0) : 0;
  }

  float ramAvail = doc["ram_a"] | 0.0;
  hw.ramUsed = doc["ram_u"] | 0.0;
  hw.ramTotal = hw.ramUsed + ramAvail;
  hw.ramPct = doc["ram_pct"] | 0;
  hw.vramUsed = doc["vram_u"] | 0.0;
  hw.vramTotal = doc["vram_t"] | 0.0;

  for (int i = 0; i < 4; i++) {
    char tk[10], uk[10], fk[10];
    snprintf(tk, sizeof(tk), "d%d_t", i + 1);
    snprintf(uk, sizeof(uk), "d%d_u", i + 1);
    snprintf(fk, sizeof(fk), "d%d_f", i + 1);
    hw.diskTemp[i] = doc[tk] | 0;
    hw.diskUsed[i] = doc[uk] | 0.0;
    float free = doc[fk] | 0.0;
    hw.diskTotal[i] = hw.diskUsed[i] + free;
  }

  hw.netUp = doc["net_up"] | 0;
  hw.netDown = doc["net_down"] | 0;
  hw.diskRead = doc["disk_r"] | 0;
  hw.diskWrite = doc["disk_w"] | 0;

  weather.temp = doc["wt"] | 0;
  const char *wd = doc["wd"];
  weather.desc = String(wd ? wd : "");
  weather.icon = doc["wi"] | 0;
  weather.dayHigh = doc["w_dh"] | 0;
  weather.dayLow = doc["w_dl"] | 0;
  weather.dayCode = doc["w_dc"] | 0;
  JsonArray wh = doc["w_wh"];
  JsonArray wl = doc["w_wl"];
  JsonArray wc = doc["w_wc"];
  for (int i = 0; i < 7; i++) {
    weather.weekHigh[i] = (i < wh.size()) ? (wh[i] | 0) : 0;
    weather.weekLow[i] = (i < wl.size()) ? (wl[i] | 0) : 0;
    weather.weekCode[i] = (i < wc.size()) ? (wc[i] | 0) : 0;
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

  parseMediaFromDoc(doc, true);
}

// Parse artist, track, cover from doc; useDefaults = true for full payload
void parseMediaFromDoc(JsonDocument &doc, bool useDefaults) {
  const char *art = doc["art"];
  const char *trk = doc["trk"];
  if (useDefaults) {
    media.artist = String(art ? art : "No Data");
    media.track = String(trk ? trk : "Waiting...");
  } else {
    if (art)
      media.artist = String(art);
    if (trk)
      media.track = String(trk);
  }
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
// DRAWING HELPERS
// ============================================================================

// Modern rounded progress bar
void drawProgressBar(int x, int y, int w, int h, float pct) {
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;

  u8g2.drawRFrame(x, y, w, h, 2);
  int fillW = (int)((w - 4) * (pct / 100.0f));
  if (fillW > 0) {
    u8g2.drawRBox(x + 2, y + 2, fillW, h - 4, 1);
  }
}

// Legacy bar for compatibility
void drawBar(int x, int y, int w, int h, float val, float max) {
  float pct = (max > 0.0f) ? (val / max * 100.0f) : 0.0f;
  drawProgressBar(x, y, w, h, pct);
}

// Draw card with title and content area (FONT_SMALL 6x10: 10px title height)
void drawCard(int x, int y, int w, int h, const char *title) {
  u8g2.drawRFrame(x, y, w, h, 3);
  if (title && strlen(title) > 0) {
    u8g2.setFont(FONT_SMALL);
    int tw = u8g2.getStrWidth(title);
    u8g2.setDrawColor(0);
    u8g2.drawBox(x + 4, y - 4, tw + 4, 10);
    u8g2.setDrawColor(1);
    u8g2.drawStr(x + 6, y + 5, title);
  }
}

// Draw WiFi signal strength indicator
void drawWiFiIcon(int x, int y, int rssi) {
  int bars = 0;
  if (rssi >= -50)
    bars = 4;
  else if (rssi >= -60)
    bars = 3;
  else if (rssi >= -70)
    bars = 2;
  else if (rssi >= -80)
    bars = 1;

  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 2;
    if (i < bars) {
      u8g2.drawBox(x + i * 3, y + 8 - h, 2, h);
    } else {
      u8g2.drawFrame(x + i * 3, y + 8 - h, 2, h);
    }
  }
}

// Scrolling text for long strings
void drawScrollingText(int x, int y, int maxW, const String &text,
                       const uint8_t *font) {
  u8g2.setFont(font);
  int textW = u8g2.getUTF8Width(text.c_str());

  if (textW <= maxW) {
    u8g2.drawUTF8(x, y, text.c_str());
  } else {
    int offset = scrollOffset % (textW + 20);
    u8g2.setClipWindow(x, 0, x + maxW, DISP_H);
    u8g2.drawUTF8(x - offset, y, text.c_str());
    u8g2.setMaxClipWindow();
  }
}

// ============================================================================
// SCREEN DRAWING FUNCTIONS
// ============================================================================

void drawSplash() {
  u8g2.setFont(u8g2_font_9x15B_tf);
  u8g2.drawStr(25, 25, "HELTEC");
  u8g2.setFont(FONT_MAIN);
  u8g2.drawStr(20, 40, "PC Monitor");
  u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(35, 55, "v2.0");

  // Animation
  int barW = (int)((millis() - splashStart) * 128.0 / SPLASH_MS);
  if (barW > 128)
    barW = 128;
  u8g2.drawBox(0, DISP_H - 2, barW, 2);
}

void drawMainScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "SYSTEM");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 11;

  // CPU — temp + load right-aligned before bar so they never overflow
  u8g2.drawStr(4, y, "CPU");
  char buf[20];
  snprintf(buf, sizeof(buf), "%d%cC ", hw.cpuTemp, (char)0xB0);
  int twTemp = u8g2.getStrWidth(buf);
  snprintf(buf, sizeof(buf), "%d%%", hw.cpuLoad);
  u8g2.setFont(FONT_TINY);
  int twLoad = u8g2.getStrWidth(buf);
  u8g2.setFont(FONT_SMALL);
  int startX = 80 - twTemp - twLoad - 2;
  if (startX < 30)
    startX = 30;
  snprintf(buf, sizeof(buf), "%d%cC ", hw.cpuTemp, (char)0xB0);
  u8g2.drawStr(startX, y, buf);
  snprintf(buf, sizeof(buf), "%d%%", hw.cpuLoad);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(startX + twTemp, y, buf);
  u8g2.setFont(FONT_SMALL);
  drawProgressBar(80, y - 6, 44, 8, hw.cpuLoad);

  // GPU — same
  y += 12;
  u8g2.drawStr(4, y, "GPU");
  snprintf(buf, sizeof(buf), "%d%cC ", hw.gpuTemp, (char)0xB0);
  twTemp = u8g2.getStrWidth(buf);
  snprintf(buf, sizeof(buf), "%d%%", hw.gpuLoad);
  u8g2.setFont(FONT_TINY);
  twLoad = u8g2.getStrWidth(buf);
  u8g2.setFont(FONT_SMALL);
  startX = 80 - twTemp - twLoad - 2;
  if (startX < 30)
    startX = 30;
  snprintf(buf, sizeof(buf), "%d%cC ", hw.gpuTemp, (char)0xB0);
  u8g2.drawStr(startX, y, buf);
  snprintf(buf, sizeof(buf), "%d%%", hw.gpuLoad);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(startX + twTemp, y, buf);
  u8g2.setFont(FONT_SMALL);
  drawProgressBar(80, y - 6, 44, 8, hw.gpuLoad);

  // RAM — value right-aligned before bar so it never overflows
  y += 12;
  u8g2.drawStr(4, y, "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fGB", hw.ramUsed, hw.ramTotal);
  {
    int tw = u8g2.getStrWidth(buf);
    u8g2.drawStr(80 - tw - 2, y, buf);
  }
  drawProgressBar(80, y - 6, 44, 8, hw.ramPct);

  // Network — value right-aligned before bar
  y += 12;
  u8g2.drawStr(4, y, "NET");
  if (hw.netDown < 1000) {
    snprintf(buf, sizeof(buf), "↓%dK ↑%dK", hw.netDown, hw.netUp);
  } else {
    snprintf(buf, sizeof(buf), "↓%.1fM ↑%.1fM", hw.netDown / 1024.0,
             hw.netUp / 1024.0);
  }
  {
    int tw = u8g2.getStrWidth(buf);
    u8g2.drawStr(80 - tw - 2, y, buf);
  }
}

void drawCoresScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "CPU CORES");

  int y = MARGIN + 11;

  for (int i = 0; i < 6; i++) {
    int x = (i % 2 == 0) ? 4 : 66;
    if (i % 2 == 0 && i > 0)
      y += 12;

    char buf[20];
    snprintf(buf, sizeof(buf), "C%d", i + 1);
    u8g2.setFont(FONT_TINY);
    u8g2.drawStr(x, y, buf);

    int mhz = (hw.cores[i] > 0) ? hw.cores[i] : hw.cpuMhzFallback;
    snprintf(buf, sizeof(buf), "%dMHz", mhz);
    u8g2.setFont(FONT_SMALL);
    {
      int tw = u8g2.getStrWidth(buf);
      u8g2.drawStr(x + 56 - tw, y, buf);
    }

    // Load bar
    drawProgressBar(x, y + 2, 56, 6, hw.coreLoad[i]);
  }
}

void drawCPUScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "CPU DETAILS");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 11;

  char buf[30];
  int valX = DISP_W - 4;

  // Temperature
  u8g2.drawStr(4, y, "Temp:");
  snprintf(buf, sizeof(buf), "%d%cC", hw.cpuTemp, (char)0xB0);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Load
  y += 10;
  u8g2.drawStr(4, y, "Load:");
  snprintf(buf, sizeof(buf), "%d%%", hw.cpuLoad);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);
  u8g2.setFont(FONT_SMALL);
  drawProgressBar(65, y - 6, 58, 8, hw.cpuLoad);

  // Power
  y += 10;
  u8g2.drawStr(4, y, "Power:");
  snprintf(buf, sizeof(buf), "%dW", hw.cpuPwr);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // CPU frequency
  y += 10;
  u8g2.drawStr(4, y, "Freq:");
  snprintf(buf, sizeof(buf), "%d MHz", hw.cpuMhzFallback);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);
}

void drawGPUScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "GPU DETAILS");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 11;

  char buf[30];
  int valX = DISP_W - 4;

  // Temperature
  u8g2.drawStr(4, y, "Core:");
  snprintf(buf, sizeof(buf), "%d%cC", hw.gpuTemp, (char)0xB0);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Hot Spot
  y += 10;
  u8g2.drawStr(4, y, "Hot:");
  snprintf(buf, sizeof(buf), "%d%cC", hw.gpuHotSpot, (char)0xB0);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Load
  y += 10;
  u8g2.drawStr(4, y, "Load:");
  snprintf(buf, sizeof(buf), "%d%%", hw.gpuLoad);
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);
  u8g2.setFont(FONT_SMALL);
  drawProgressBar(65, y - 6, 58, 8, hw.gpuLoad);

  // Clock
  y += 10;
  u8g2.drawStr(4, y, "Clock:");
  snprintf(buf, sizeof(buf), "%dMHz", hw.gpuClk);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Power
  y += 10;
  u8g2.drawStr(4, y, "Power:");
  snprintf(buf, sizeof(buf), "%dW", hw.gpuPwr);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);
}

void drawMemoryScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "MEMORY");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 11;

  char buf[30];

  // RAM — value right-aligned so long numbers stay on screen
  u8g2.drawStr(4, y, "RAM:");
  snprintf(buf, sizeof(buf), "%.1f/%.1fGB", hw.ramUsed, hw.ramTotal);
  u8g2.drawStr(124 - u8g2.getStrWidth(buf), y, buf);
  y += 2;
  drawProgressBar(4, y, 120, 8, hw.ramPct);

  // Top RAM processes — name truncated, MB right-aligned
  y += 14;
  u8g2.drawStr(4, y, "Top Processes:");

  for (int i = 0; i < 2; i++) {
    y += 10;
    if (procs.ramNames[i].length() > 0) {
      String name = procs.ramNames[i];
      if (name.length() > 12)
        name = name.substring(0, 10) + "..";
      u8g2.drawStr(6, y, name.c_str());

      snprintf(buf, sizeof(buf), "%dMB", procs.ramMb[i]);
      int tw = u8g2.getStrWidth(buf);
      u8g2.drawStr(DISP_W - tw - 4, y, buf);
    }
  }

  // VRAM — value right-aligned
  y += 12;
  u8g2.drawStr(4, y, "VRAM:");
  snprintf(buf, sizeof(buf), "%.1f/%.1fGB", hw.vramUsed, hw.vramTotal);
  u8g2.drawStr(124 - u8g2.getStrWidth(buf), y, buf);
}

void drawDisksScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "STORAGE");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 11;

  char buf[30];
  const char *diskNames[] = {"NVMe1", "NVMe2", "HDD", "SSD"};

  for (int i = 0; i < 4; i++) {
    if (hw.diskTotal[i] > 0) {
      u8g2.drawStr(4, y, diskNames[i]);

      float pct = (hw.diskUsed[i] / hw.diskTotal[i]) * 100.0;
      snprintf(buf, sizeof(buf), "%.0fG %d%cC",
               hw.diskTotal[i] - hw.diskUsed[i], hw.diskTemp[i], (char)0xB0);
      int tw = u8g2.getStrWidth(buf);
      u8g2.drawStr(DISP_W - tw - 4, y, buf);

      y += 2;
      drawProgressBar(4, y, 120, 6, pct);
      y += 10;
    }
  }
}

void drawPlayerScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "MEDIA");

  // Cover frame; draw bitmap when available
  u8g2.drawRFrame(4, MARGIN + 8, 48, 48, 3);
  if (media.hasCover) {
    u8g2.drawXBM(4, MARGIN + 8, 48, 48, media.coverBitmap);
  } else {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(12, MARGIN + 32, "No");
    u8g2.drawStr(8, MARGIN + 44, "Cover");
  }

  // Artist and track — always use readable 7x13 Cyrillic font
  int textX = 56;
  int textW = DISP_W - textX - 4;
  int y = MARGIN + 16;
  u8g2.setFont(FONT_MEDIA);
  drawScrollingText(textX, y, textW, media.artist, FONT_MEDIA);
  y += 12;
  drawScrollingText(textX, y, textW, media.track, FONT_MEDIA);
  y += 14;
  u8g2.setFont(FONT_SMALL);
  if (media.isPlaying) {
    u8g2.drawStr(textX, y, "Playing");
  } else {
    u8g2.drawStr(textX, y, "Paused");
  }
}

void drawFansScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "COOLING");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 11;

  char buf[20];

  int valX = DISP_W - 4;

  // Pump
  u8g2.drawStr(4, y, "Pump:");
  snprintf(buf, sizeof(buf), "%d RPM", hw.fanPump);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Radiator
  y += 12;
  u8g2.drawStr(4, y, "Rad:");
  snprintf(buf, sizeof(buf), "%d RPM", hw.fanRad);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Case
  y += 12;
  u8g2.drawStr(4, y, "Case:");
  snprintf(buf, sizeof(buf), "%d RPM", hw.fanCase);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // GPU
  y += 12;
  u8g2.drawStr(4, y, "GPU:");
  snprintf(buf, sizeof(buf), "%d RPM", hw.fanGpu);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  // Additional temps
  y += 14;
  u8g2.drawStr(4, y, "Chipset:");
  snprintf(buf, sizeof(buf), "%d%cC", hw.chipsetTemp, (char)0xB0);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);

  y += 10;
  u8g2.drawStr(4, y, "NVMe:");
  snprintf(buf, sizeof(buf), "%d%cC", hw.nvme2Temp, (char)0xB0);
  u8g2.drawStr(valX - u8g2.getStrWidth(buf), y, buf);
}

void drawWeatherScreen() {
  const uint8_t *icon = iconCloudy;
  int code = weather.icon;

  if (code == 0)
    icon = iconSunny;
  else if (code >= 71 && code <= 86)
    icon = iconSnow;
  else if (code >= 51)
    icon = iconRain;

  if (weather.page == 0) {
    // Current weather
    drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "WEATHER NOW");

    u8g2.drawXBM(6, MARGIN + 10, 16, 16, icon);

    u8g2.setFont(FONT_LARGE);
    char buf[20];
    snprintf(buf, sizeof(buf), "%d%cC", weather.temp, (char)0xB0);
    u8g2.drawStr(28, MARGIN + 22, buf);

    u8g2.setFont(isOnlyAscii(weather.desc) ? FONT_SMALL : FONT_MEDIA);
    u8g2.setClipWindow(6, MARGIN + 30, DISP_W - 6, MARGIN + 50);
    u8g2.drawUTF8(6, MARGIN + 38, weather.desc.c_str());
    u8g2.setMaxClipWindow();

  } else if (weather.page == 1) {
    // Today forecast
    drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "TODAY");

    u8g2.setFont(FONT_MAIN);
    char buf[30];
    snprintf(buf, sizeof(buf), "High: %d%cC", weather.dayHigh, (char)0xB0);
    u8g2.drawStr(6, MARGIN + 18, buf);

    snprintf(buf, sizeof(buf), "Low:  %d%cC", weather.dayLow, (char)0xB0);
    u8g2.drawStr(6, MARGIN + 32, buf);

    u8g2.setFont(FONT_SMALL);
    String desc = "Today: " + String(_weatherDescFromCode(weather.dayCode));
    u8g2.setClipWindow(6, MARGIN + 42, DISP_W - 6, DISP_H);
    u8g2.drawStr(6, MARGIN + 48, desc.c_str());
    u8g2.setMaxClipWindow();

  } else {
    // Week forecast
    drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "WEEK");

    u8g2.setFont(FONT_SMALL);
    int y = MARGIN + 14;

    for (int i = 0; i < min(5, 7); i++) {
      if (i < (int)(sizeof(weather.weekHigh) / sizeof(weather.weekHigh[0]))) {
        char buf[25];
        snprintf(buf, sizeof(buf), "D%d: %d/%d%cC", i + 1, weather.weekHigh[i],
                 weather.weekLow[i], (char)0xB0);
        u8g2.drawStr(6, y, buf);
        y += 10;
      }
    }
  }
}

void drawTopProcsScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "TOP CPU");

  u8g2.setFont(FONT_SMALL);
  int y = MARGIN + 13;

  for (int i = 0; i < 3; i++) {
    if (procs.cpuNames[i].length() > 0) {
      char buf[25];
      String name = procs.cpuNames[i];
      if (name.length() > 14)
        name = name.substring(0, 11) + "...";

      snprintf(buf, sizeof(buf), "%d%%", procs.cpuPercent[i]);
      u8g2.setFont(FONT_TINY);
      int tw = u8g2.getStrWidth(buf);
      u8g2.setFont(FONT_SMALL);
      int nameMaxX = DISP_W - tw - 10;
      if (nameMaxX > 10) {
        u8g2.setClipWindow(6, y - 6, nameMaxX, y + 8);
        u8g2.drawStr(6, y, name.c_str());
        u8g2.setMaxClipWindow();
      } else {
        u8g2.drawStr(6, y, name.c_str());
      }

      u8g2.setFont(FONT_TINY);
      u8g2.drawStr(DISP_W - tw - 6, y, buf);
      u8g2.setFont(FONT_SMALL);

      y += 12;
      if (i < 2) {
        u8g2.drawLine(6, y - 5, DISP_W - 6, y - 5);
      }
    }
  }
}

void drawEqualizerScreen() {
  drawCard(0, MARGIN, DISP_W, DISP_H - MARGIN - 6, "EQUALIZER");

  int barW = (DISP_W - 8) / EQ_BARS;
  int maxH = DISP_H - MARGIN - 12;

  if (settings.eqStyle == 0) {
    // Bars style
    for (int i = 0; i < EQ_BARS; i++) {
      int x = 4 + i * barW;
      int h = map(eqHeights[i], 0, 48, 0, maxH);
      if (h > 0) {
        u8g2.drawBox(x, DISP_H - 4 - h, barW - 1, h);
      }
    }
  } else if (settings.eqStyle == 1) {
    // Wave style
    for (int i = 0; i < EQ_BARS - 1; i++) {
      int x1 = 4 + i * barW;
      int x2 = 4 + (i + 1) * barW;
      int y1 = DISP_H - 4 - map(eqHeights[i], 0, 48, 0, maxH);
      int y2 = DISP_H - 4 - map(eqHeights[i + 1], 0, 48, 0, maxH);
      u8g2.drawLine(x1, y1, x2, y2);
    }
  } else {
    // Circle style
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;
    int baseR = 8;

    for (int i = 0; i < EQ_BARS; i++) {
      float angle = i * 2.0 * PI / EQ_BARS;
      int r = baseR + map(eqHeights[i], 0, 48, 0, 20);
      int x = cx + r * cos(angle);
      int y = cy + r * sin(angle);
      u8g2.drawLine(cx, cy, x, y);
    }
  }
}

void drawScreen(int screen) {
  switch (screen) {
  case 0:
    drawMainScreen();
    break;
  case 1:
    drawCoresScreen();
    break;
  case 2:
    drawCPUScreen();
    break;
  case 3:
    drawGPUScreen();
    break;
  case 4:
    drawMemoryScreen();
    break;
  case 5:
    drawDisksScreen();
    break;
  case 6:
    drawPlayerScreen();
    break;
  case 7:
    drawFansScreen();
    break;
  case 8:
    drawWeatherScreen();
    break;
  case 9:
    drawTopProcsScreen();
    break;
  case 10:
    drawEqualizerScreen();
    break;
  }
}

// Helper function for weather description
String _weatherDescFromCode(int code) {
  if (code == 0)
    return "Clear";
  if (code >= 1 && code <= 3)
    return "Cloudy";
  if (code >= 45 && code <= 48)
    return "Fog";
  if (code >= 51 && code <= 67)
    return "Rain";
  if (code >= 71 && code <= 77)
    return "Snow";
  if (code >= 80 && code <= 82)
    return "Showers";
  if (code >= 85 && code <= 86)
    return "Snow";
  if (code >= 95)
    return "Storm";
  return "Cloudy";
}

void drawMenu() {
  // Overlay: all items must fit in DISP_H (64px)
  u8g2.setDrawColor(0);
  u8g2.drawBox(8, 6, DISP_W - 16, DISP_H - 12);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(8, 6, DISP_W - 16, DISP_H - 12, 2);

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(12, 12, "SETTINGS");

  // Menu values: Contrast 128|192|255; Interval 5|10|15 s; EQ
  // 0=Bars|1=Wave|2=Circle; Display 0|180
  const char *labels[] = {"LED", "Carousel", "Contrast", "Interval",
                          "EQ",  "Display",  "Exit"};
  const int lineH = 6;
  const int startY = 14; // 7 items * 6px = 42, 14+42=56, fits in 64

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = startY + i * lineH;
    if (y + 6 > DISP_H - 5)
      break;

    if (i == menuItem) {
      u8g2.drawBox(10, y - 5, DISP_W - 20, lineH);
      u8g2.setDrawColor(0);
    }

    char buf[28];
    if (i == 6) {
      strcpy(buf, "> Exit");
    } else {
      snprintf(buf, sizeof(buf), "%s:", labels[i]);
      if (i == 0)
        strcat(buf, settings.ledEnabled ? " ON" : " OFF");
      else if (i == 1)
        strcat(buf, settings.carouselEnabled ? " ON" : " OFF");
      else if (i == 2) {
        char t[8];
        snprintf(t, sizeof(t), " %d", settings.displayContrast);
        strcat(buf, t);
      } else if (i == 3) {
        char t[8];
        snprintf(t, sizeof(t), " %ds", settings.carouselIntervalSec);
        strcat(buf, t);
      } else if (i == 4) {
        const char *s[] = {" Bars", " Wave", " Circle"};
        strcat(buf, s[settings.eqStyle % 3]);
      } else if (i == 5)
        strcat(buf, settings.displayInverted ? " 180" : " 0");
    }

    u8g2.setClipWindow(10, 8, DISP_W - 10, DISP_H - 6);
    u8g2.drawStr(12, y, buf);
    u8g2.setMaxClipWindow();

    if (i == menuItem)
      u8g2.setDrawColor(1);
  }

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(12, DISP_H - 4, "Hold=Select");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Hardware init
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

  // Load settings from flash
  Preferences prefs;
  prefs.begin("heltec", true);
  settings.ledEnabled = prefs.getBool("led", true);
  settings.carouselEnabled = prefs.getBool("carousel", false);
  // Valid: carousel 5|10|15 s; contrast 0–255
  settings.carouselIntervalSec = prefs.getInt("carouselSec", 10);
  if (settings.carouselIntervalSec != 5 && settings.carouselIntervalSec != 10 &&
      settings.carouselIntervalSec != 15) {
    settings.carouselIntervalSec = 10;
  }
  settings.displayContrast = prefs.getInt("contrast", 255);
  if (settings.displayContrast > 255)
    settings.displayContrast = 255;
  if (settings.displayContrast < 0)
    settings.displayContrast = 0;
  settings.eqStyle = prefs.getInt("eqStyle", 0);
  settings.displayInverted = prefs.getBool("inverted", false);
  prefs.end();

  u8g2.setContrast((uint8_t)settings.displayContrast);
  u8g2.setFlipMode(settings.displayInverted ? 1 : 0);

  // WiFi setup
  if (strlen(WIFI_PASS) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    wifiConnectStart = millis();
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();

  // ============================================================================
  // 1. WiFi CONNECTION MANAGEMENT
  // ============================================================================
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      wifiRssi = WiFi.RSSI();
    }

    // Update RSSI periodically
    if (now % 5000 < 100) {
      wifiRssi = WiFi.RSSI();
    }

    // TCP connection
    if (!tcpClient.connected()) {
      tcpClient.connect(PC_IP, TCP_PORT);
      if (tcpClient.connected()) {
        tcpLineBuffer = "";
        lastSentScreen = -1;
      }
    }
  } else {
    wifiConnected = false;

    // Retry connection periodically
    if (now - lastWifiRetry > WIFI_RETRY_INTERVAL) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      lastWifiRetry = now;
    }
  }

  // ============================================================================
  // 2. DATA RECEPTION
  // ============================================================================
  if (tcpClient.connected()) {
    while (tcpClient.available()) {
      char c = tcpClient.read();
      if (c == '\n') {
        if (tcpLineBuffer.length() > 0) {
          // Parse JSON
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, tcpLineBuffer);

          if (!err) {
            lastUpdate = now;
            // Full payload: server sends weather + media/cover; detect by key
            // presence
            bool fullPayload = (tcpLineBuffer.length() >= 350 &&
                                (tcpLineBuffer.indexOf("cover_b64") >= 0 ||
                                 tcpLineBuffer.indexOf("\"wt\"") >= 0));

            parseHardwareBasic(doc);
            if (fullPayload)
              parseFullPayload(doc);
            else
              parseMediaFromDoc(doc, false);
          }

          tcpLineBuffer = "";
        }
      } else {
        tcpLineBuffer += c;
        if (tcpLineBuffer.length() > TCP_LINE_MAX) {
          tcpLineBuffer = "";
        }
      }
    }
  }

  // ============================================================================
  // 3. BUTTON HANDLING
  // ============================================================================
  int btnState = digitalRead(BUTTON_PIN); // Active LOW

  if (btnState == LOW && !btnHeld) {
    btnHeld = true;
    btnPressTime = now;
    menuHoldHandled = false;
    wasInMenuOnPress = inMenu;
  }

  if (btnState == HIGH && btnHeld) {
    unsigned long duration = now - btnPressTime;
    btnHeld = false;

    if (duration > 800) {
      // Long press - open/close menu
      if (!wasInMenuOnPress) {
        inMenu = true;
        menuItem = 0;
        lastMenuActivity = now;
      }
    } else {
      // Short press
      if (inMenu) {
        lastMenuActivity = now;
        menuItem++;
        if (menuItem >= MENU_ITEMS)
          menuItem = 0;
      } else {
        // Navigate screens
        if (currentScreen == 8) {
          weather.page++;
          if (weather.page >= 3) {
            weather.page = 0;
            currentScreen++;
            if (currentScreen >= TOTAL_SCREENS)
              currentScreen = 0;
            lastCarousel = now;
          }
        } else {
          currentScreen++;
          if (currentScreen >= TOTAL_SCREENS)
            currentScreen = 0;
          lastCarousel = now;
        }
      }
    }
  }

  // Menu selection (long press while in menu)
  if (inMenu && btnState == LOW && (now - btnPressTime > 800) &&
      !menuHoldHandled) {
    menuHoldHandled = true;
    lastMenuActivity = now;

    Preferences p;
    p.begin("heltec", false);

    switch (menuItem) {
    case 0: // LED
      settings.ledEnabled = !settings.ledEnabled;
      p.putBool("led", settings.ledEnabled);
      break;

    case 1: // Carousel
      settings.carouselEnabled = !settings.carouselEnabled;
      p.putBool("carousel", settings.carouselEnabled);
      break;

    case 2: // Contrast
      if (settings.displayContrast <= 128)
        settings.displayContrast = 192;
      else if (settings.displayContrast <= 192)
        settings.displayContrast = 255;
      else
        settings.displayContrast = 128;
      u8g2.setContrast((uint8_t)settings.displayContrast);
      p.putInt("contrast", settings.displayContrast);
      break;

    case 3: // Interval
      if (settings.carouselIntervalSec == 5)
        settings.carouselIntervalSec = 10;
      else if (settings.carouselIntervalSec == 10)
        settings.carouselIntervalSec = 15;
      else
        settings.carouselIntervalSec = 5;
      p.putInt("carouselSec", settings.carouselIntervalSec);
      break;

    case 4: // EQ Style
      settings.eqStyle = (settings.eqStyle + 1) % 3;
      p.putInt("eqStyle", settings.eqStyle);
      break;

    case 5: // Display rotation
      settings.displayInverted = !settings.displayInverted;
      u8g2.setFlipMode(settings.displayInverted ? 1 : 0);
      p.putBool("inverted", settings.displayInverted);
      break;

    case 6: // Exit
      inMenu = false;
      break;
    }

    p.end();
  }

  // ============================================================================
  // 4. MENU AUTO-CLOSE
  // ============================================================================
  if (inMenu && (now - lastMenuActivity > MENU_TIMEOUT_MS)) {
    inMenu = false;
  }

  // ============================================================================
  // 5. CAROUSEL
  // ============================================================================
  if (settings.carouselEnabled && !inMenu) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
      lastCarousel = now;
    }
  }

  // ============================================================================
  // 6. SEND CURRENT SCREEN TO PC
  // ============================================================================
  if (tcpClient.connected() && lastSentScreen != currentScreen) {
    tcpClient.print("screen:");
    tcpClient.print(currentScreen);
    tcpClient.print("\n");
    lastSentScreen = currentScreen;
  }

  // ============================================================================
  // 7. EQUALIZER ANIMATION
  // ============================================================================
  if (now - lastEqUpdate >= EQ_UPDATE_MS) {
    lastEqUpdate = now;

    if (media.isPlaying) {
      for (int i = 0; i < EQ_BARS; i++) {
        if (random(0, 4) == 0) {
          eqTargets[i] = (uint8_t)random(4, 48);
        }

        int step = 2;
        if (eqHeights[i] < eqTargets[i]) {
          eqHeights[i] += step;
          if (eqHeights[i] > DISP_H - 4)
            eqHeights[i] = DISP_H - 4;
          if (eqHeights[i] > (int)eqTargets[i])
            eqHeights[i] = eqTargets[i];
        } else if (eqHeights[i] > eqTargets[i]) {
          if (eqHeights[i] > step)
            eqHeights[i] -= step;
          else
            eqHeights[i] = 0;
          if (eqHeights[i] < (int)eqTargets[i])
            eqHeights[i] = eqTargets[i];
        }
      }
    } else {
      for (int i = 0; i < EQ_BARS; i++) {
        eqTargets[i] = 0;
        if (eqHeights[i] > 2)
          eqHeights[i] -= 3;
        else if (eqHeights[i] > 0)
          eqHeights[i] = 0;
      }
    }
  }

  // ============================================================================
  // 8. SCROLLING TEXT ANIMATION
  // ============================================================================
  if (now - lastScrollUpdate >= SCROLL_INTERVAL) {
    scrollOffset++;
    lastScrollUpdate = now;
  }

  // ============================================================================
  // 9. ALARM & LED
  // ============================================================================
  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  bool anyAlarm =
      (hw.cpuTemp >= CPU_TEMP_ALERT) || (hw.gpuTemp >= GPU_TEMP_ALERT) ||
      (hw.cpuLoad >= CPU_LOAD_ALERT) || (hw.gpuLoad >= GPU_LOAD_ALERT);

  if (settings.ledEnabled && anyAlarm && blinkState) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  // ============================================================================
  // 10. SPLASH SCREEN
  // ============================================================================
  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= SPLASH_MS)
      splashDone = true;
  }

  // ============================================================================
  // 11. RENDERING
  // ============================================================================
  u8g2.clearBuffer();

  bool signalLost = (now - lastUpdate > SIGNAL_TIMEOUT_MS);

  if (!splashDone) {
    drawSplash();
  } else if (signalLost) {
    drawScreen(currentScreen);
    u8g2.setFont(FONT_MAIN);
    u8g2.drawStr((DISP_W - 70) / 2, DISP_H / 2, "Reconnect");

    // WiFi status
    if (wifiConnected) {
      drawWiFiIcon(DISP_W - 16, 2, wifiRssi);
    } else {
      u8g2.setFont(FONT_SMALL);
      u8g2.drawStr(DISP_W - 20, 8, "WiFi");
    }
  } else {
    drawScreen(currentScreen);

    // WiFi indicator
    if (currentScreen != 1) {
      if (wifiConnected) {
        drawWiFiIcon(DISP_W - 16, 2, wifiRssi);
      } else {
        u8g2.setFont(FONT_SMALL);
        u8g2.drawStr(DISP_W - 10, MARGIN + 6, "X");
      }
    }

    // Alert indicator
    if (anyAlarm) {
      if (blinkState) {
        u8g2.drawFrame(0, 0, DISP_W, DISP_H);
      }
      u8g2.setFont(FONT_SMALL);
      u8g2.drawStr(DISP_W - 38, MARGIN + 6, "ALERT");
    }

    // Menu overlay
    if (inMenu) {
      drawMenu();
    }

    // Screen indicators (dots)
    if (!inMenu) {
      for (int i = 0; i < TOTAL_SCREENS; i++) {
        int x = DISP_W / 2 - (TOTAL_SCREENS * 3) + (i * 6);
        if (i == currentScreen) {
          u8g2.drawBox(x, DISP_H - 2, 2, 2);
        } else {
          u8g2.drawPixel(x + 1, DISP_H - 1);
        }
      }
    }
  }

  u8g2.sendBuffer();
}