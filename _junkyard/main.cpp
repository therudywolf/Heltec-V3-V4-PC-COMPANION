/*
 * Heltec PC Monitor v7.0 [NEURAL LINK] — Cyberpunk Cyberdeck Edition
 *
 * Hardware: Heltec WiFi LoRa 32 V3 (ESP32-S3) + SSD1306 OLED 128x64
 *
 * VISUAL ENGINE:
 * - Anti-ghosting: drawMetric() clears exact text area (black box) before
 * redraw
 * - Cyberpunk HUD: Corner crosshairs, status bar, high-contrast monospace fonts
 * - Screen cycling via button press
 *
 * SCREENS:
 * 1. [CORTEX]    - CPU/GPU Temps & Loads (2x2 grid)
 * 2. [NEURAL]    - Network Up/Down + Ping latency
 * 3. [THERMAL]   - Fan RPMs (CPU, SYS1, SYS2, GPU) + animated icon
 * 4. [MEM.BANK]  - RAM + Storage bars (NVMe & HDD)
 * 5. [TASK.KILL] - Top CPU process + usage %
 * 6. [DECK]      - Media player (Artist, Track, Status)
 *
 * DATA PROTOCOL:
 * - TCP JSON stream from monitor.py (2-char keys)
 * - Robust parsing: handles incomplete packets
 * - WiFi reconnection logic
 * - "NO SIGNAL" screen if TCP disconnects > 3 sec
 *
 * v7.0 CHANGES:
 * - FIXED: Rapid connect/disconnect loop (lastUpdate initialization)
 * - FIXED: TCP connection stability with proper handshake
 * - ADDED: Connection grace period after TCP connect
 * - IMPROVED: Anti-ghosting with full buffer clear strategy
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>

// ============================================================================
// CONFIG — Update these for your network
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

// Hardware pins (Heltec WiFi LoRa 32 V3)
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
#define TCP_CONNECT_TIMEOUT 5000    // 5 sec connection timeout
#define TCP_RECONNECT_INTERVAL 2000 // 2 sec between reconnect attempts

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_PIN);

// ============================================================================
// FONTS — Cyberpunk monospace aesthetic
// ============================================================================
#define FONT_DATA u8g2_font_6x12_tf          // Monospaced data values
#define FONT_LABEL u8g2_font_micro_tr        // Tiny labels
#define FONT_HEADER u8g2_font_helvB08_tf     // Bold headers
#define FONT_BIG u8g2_font_helvB12_tf        // Large numbers
#define FONT_TINY u8g2_font_tom_thumb_4x6_tr // Ultra-compact

#define LINE_H_DATA 12
#define LINE_H_LABEL 5
#define LINE_H_HEADER 10
#define LINE_H_BIG 14

// ============================================================================
// GLOBAL DATA (2-char keys from monitor.py)
// ============================================================================
struct HardwareData {
  int ct = 0, gt = 0, cl = 0, gl = 0; // CPU/GPU temp & load
  float ru = 0.0f, ra = 0.0f;         // RAM used/total (GB)
  int nd = 0, nu = 0;                 // Network down/up (KB/s)
  int pg = 0;                         // Ping latency (ms)
  int cf = 0, s1 = 0, s2 = 0, gf = 0; // Fans (RPM)
  int su = 0, du = 0;                 // Storage usage (%)
  float vu = 0.0f, vt = 0.0f;         // VRAM used/total (GB)
  int ch = 0;                         // Chipset temp
  int dr = 0, dw = 0;                 // Disk read/write (KB/s)
} hw;

struct WeatherData {
  int temp = 0;
  String desc = "";
  int icon = 0;
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
unsigned long lastTcpAttempt = 0; // [v7] Track TCP reconnect attempts
unsigned long tcpConnectTime = 0; // [v7] Track when TCP connected
unsigned long bootTime = 0;
bool blinkState = false;
const unsigned long SIGNAL_TIMEOUT_MS = 5000; // [v7] Increased to 5 sec
const unsigned long SIGNAL_GRACE_PERIOD_MS =
    8000; // [v7] Grace period after connect
const unsigned long WIFI_RETRY_INTERVAL = 30000;

int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;
const unsigned long SCROLL_INTERVAL = 50;

bool splashDone = false;
unsigned long splashStart = 0;
const unsigned long SPLASH_MS = 2000;

WiFiClient tcpClient;
String tcpLineBuffer;
int lastSentScreen = -1;
bool wifiConnected = false;
bool tcpConnectedState = false; // [v7] Track TCP connection state
bool firstDataReceived = false; // [v7] Track if we ever received valid data
int wifiRssi = 0;

// Fan animation state
int fanAnimFrame = 0;
unsigned long lastFanAnim = 0;
const unsigned long FAN_ANIM_INTERVAL = 100;

// Glitch effect state (cyberpunk aesthetic)
unsigned long lastGlitch = 0;
const unsigned long GLITCH_INTERVAL = 5000;
bool doGlitch = false;

// ============================================================================
// ANTI-GHOSTING — MANDATORY for dynamic text (prevents overlapping/ghosting)
// ============================================================================
void drawMetric(int x, int y, const char *label, const char *value) {
  /*
   * CYBERPUNK DESIGN PATTERN: Clear-then-draw
   * 1. Set DrawColor 0 (Black) - erase mode
   * 2. Draw box covering exact previous text area
   * 3. Set DrawColor 1 (White) - draw mode
   * 4. Draw new text
   *
   * This prevents text ghosting on OLED displays.
   */
  u8g2.setFont(FONT_DATA);
  int vw = value ? u8g2.getUTF8Width(value) : 0;
  if (vw < 1)
    vw = 24;
  int maxW = DISP_W - x - MARGIN;
  if (vw > maxW)
    vw = maxW;

  // STEP 1 & 2: Clear area (black box)
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y - LINE_H_DATA + 1, vw + 2, LINE_H_DATA);

  // STEP 3 & 4: Draw new content (white)
  u8g2.setDrawColor(1);
  if (label && strlen(label) > 0) {
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(x, y - LINE_H_DATA - 1, label);
    u8g2.setFont(FONT_DATA);
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

void drawCornerCrosshairs() {
  /*
   * CYBERPUNK HUD ELEMENT: Corner targeting reticles
   * Gives military/tactical aesthetic
   */
  int len = 4;
  // Top-left
  u8g2.drawLine(0, 0, len, 0);
  u8g2.drawLine(0, 0, 0, len);
  // Top-right
  u8g2.drawLine(DISP_W - len - 1, 0, DISP_W - 1, 0);
  u8g2.drawLine(DISP_W - 1, 0, DISP_W - 1, len);
  // Bottom-left
  u8g2.drawLine(0, DISP_H - 1, len, DISP_H - 1);
  u8g2.drawLine(0, DISP_H - len - 1, 0, DISP_H - 1);
  // Bottom-right
  u8g2.drawLine(DISP_W - len - 1, DISP_H - 1, DISP_W - 1, DISP_H - 1);
  u8g2.drawLine(DISP_W - 1, DISP_H - len - 1, DISP_W - 1, DISP_H - 1);
}

void drawFanIcon(int x, int y, int frame) {
  /*
   * ANIMATED FAN ICON: Rotating blades (4 frames)
   * Cyberpunk industrial aesthetic
   */
  int r = 6;
  u8g2.drawCircle(x, y, r);
  u8g2.drawDisc(x, y, 2);

  // Rotating blades based on frame
  for (int i = 0; i < 3; i++) {
    float angle = (frame * 30 + i * 120) * 3.14159 / 180.0;
    int x1 = x + (int)(r * 0.8 * cos(angle));
    int y1 = y + (int)(r * 0.8 * sin(angle));
    u8g2.drawLine(x, y, x1, y1);
  }
}

void drawLinkStatus(int x, int y, bool linked) {
  /*
   * CYBERPUNK HUD: Data link status indicator
   */
  u8g2.setFont(FONT_TINY);
  if (linked) {
    u8g2.drawStr(x, y, "LINK");
  } else {
    if (blinkState) {
      u8g2.drawStr(x, y, "----");
    }
  }
}

void drawGlitchEffect() {
  /*
   * CYBERPUNK GLITCH: Random horizontal line displacement
   * Simulates signal interference / data corruption
   */
  if (random(100) < 15) { // 15% chance
    int y = random(DISP_H);
    int shift = random(-3, 4);
    // Draw a displaced horizontal segment
    for (int x = 0; x < DISP_W; x++) {
      if (random(10) < 3) {
        u8g2.drawPixel((x + shift + DISP_W) % DISP_W, y);
      }
    }
  }
}

// ============================================================================
// PARSING (2-char keys, robust against incomplete packets)
// ============================================================================
void parsePayload(JsonDocument &doc) {
  /*
   * ROBUST JSON PARSING: Handles incomplete/corrupted packets gracefully
   * Uses default values if keys are missing
   */
  hw.ct = doc["ct"] | 0;
  hw.gt = doc["gt"] | 0;
  hw.cl = doc["cl"] | 0;
  hw.gl = doc["gl"] | 0;
  hw.ru = doc["ru"] | 0.0f;
  hw.ra = doc["ra"] | 0.0f;
  hw.nd = doc["nd"] | 0;
  hw.nu = doc["nu"] | 0;
  hw.pg = doc["pg"] | 0;
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

  JsonArray tr = doc["tr"];
  for (int i = 0; i < 2; i++) {
    if (i < tr.size()) {
      const char *n = tr[i]["n"];
      procs.ramNames[i] = String(n ? n : "");
      procs.ramMb[i] = tr[i]["r"] | 0;
    } else {
      procs.ramNames[i] = "";
      procs.ramMb[i] = 0;
    }
  }

  const char *art = doc["art"];
  const char *trk = doc["trk"];
  media.artist = String(art ? art : "");
  media.track = String(trk ? trk : "");
  media.isPlaying = doc["mp"] | false;
}

// ============================================================================
// TCP CONNECTION MANAGEMENT [v7 - CRITICAL FIX]
// ============================================================================
void tcpDisconnect() {
  if (tcpClient.connected()) {
    tcpClient.stop();
  }
  tcpLineBuffer = "";
  lastSentScreen = -1;
  tcpConnectedState = false;
  tcpConnectTime = 0;
}

bool tcpConnect() {
  unsigned long now = millis();

  // Don't attempt too frequently
  if (now - lastTcpAttempt < TCP_RECONNECT_INTERVAL) {
    return false;
  }
  lastTcpAttempt = now;

  // Ensure clean state before connecting
  if (tcpClient.connected()) {
    return true; // Already connected
  }

  // Set connection timeout
  tcpClient.setTimeout(TCP_CONNECT_TIMEOUT / 1000); // setTimeout uses seconds

  if (tcpClient.connect(PC_IP, TCP_PORT)) {
    tcpLineBuffer = "";
    lastSentScreen = -1;
    tcpConnectedState = true;
    tcpConnectTime = now;

    // [v7] CRITICAL: Set lastUpdate to NOW to prevent immediate signal loss
    lastUpdate = now;

    // Send initial handshake
    tcpClient.print("HELO\n");

    return true;
  }

  return false;
}

bool isSignalLost(unsigned long now) {
  // [v7] If we just connected, give grace period for first data
  if (tcpConnectedState && tcpConnectTime > 0) {
    unsigned long sinceConnect = now - tcpConnectTime;
    if (sinceConnect < SIGNAL_GRACE_PERIOD_MS) {
      // Still within grace period - not lost
      return false;
    }
  }

  // [v7] If never received data, use connection time as reference
  if (!firstDataReceived && tcpConnectedState) {
    return (now - tcpConnectTime > SIGNAL_GRACE_PERIOD_MS);
  }

  // Normal signal loss check
  return (now - lastUpdate > SIGNAL_TIMEOUT_MS);
}

// ============================================================================
// SCREENS — Cyberpunk HUD Design
// ============================================================================

// [CORTEX] — CPU/GPU Temps & Loads (2x2 grid)
void drawScreenCortex() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN + 2, MARGIN + LINE_H_HEADER, "[CORTEX]");

  int gx = MARGIN + 2;
  int gy = MARGIN + LINE_H_HEADER + 4;
  int halfW = (DISP_W - 2 * MARGIN - 4) / 2;
  int halfH = (DISP_H - gy - MARGIN - 2) / 2;
  char buf[24];

  // Top-Left: CPU Temp + Load
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(gx, gy + LINE_H_LABEL, "CPU");
  snprintf(buf, sizeof(buf), "%dC", hw.ct);
  u8g2.setFont(FONT_BIG);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(gx, gy + LINE_H_LABEL + 2, tw + 2, LINE_H_BIG);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(gx, gy + LINE_H_LABEL + LINE_H_BIG, buf);
  drawProgressBar(gx, gy + LINE_H_LABEL + LINE_H_BIG + 2, halfW - 4, 6,
                  (float)hw.cl);

  // Top-Right: GPU Temp + Load
  int rx = gx + halfW + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(rx, gy + LINE_H_LABEL, "GPU");
  snprintf(buf, sizeof(buf), "%dC", hw.gt);
  u8g2.setFont(FONT_BIG);
  tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(rx, gy + LINE_H_LABEL + 2, tw + 2, LINE_H_BIG);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(rx, gy + LINE_H_LABEL + LINE_H_BIG, buf);
  drawProgressBar(rx, gy + LINE_H_LABEL + LINE_H_BIG + 2, halfW - 4, 6,
                  (float)hw.gl);

  // Bottom-Left: RAM
  int by = gy + halfH + 2;
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(gx, by + LINE_H_LABEL, "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.ru, hw.ra);
  u8g2.setFont(FONT_DATA);
  u8g2.setDrawColor(0);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawBox(gx, by + LINE_H_LABEL + 2, tw + 2, LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(gx, by + LINE_H_LABEL + LINE_H_DATA, buf);

  // Bottom-Right: VRAM or Uptime
  u8g2.setFont(FONT_LABEL);
  if (hw.vt > 0.0f) {
    u8g2.drawStr(rx, by + LINE_H_LABEL, "VRAM");
    snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.vu, hw.vt);
  } else {
    u8g2.drawStr(rx, by + LINE_H_LABEL, "UPTM");
    unsigned long s = (millis() - bootTime) / 1000;
    int m = s / 60, h = m / 60;
    snprintf(buf, sizeof(buf), "%d:%02d", h, m % 60);
  }
  u8g2.setFont(FONT_DATA);
  u8g2.setDrawColor(0);
  tw = u8g2.getUTF8Width(buf);
  u8g2.drawBox(rx, by + LINE_H_LABEL + 2, tw + 2, LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(rx, by + LINE_H_LABEL + LINE_H_DATA, buf);
}

// [NEURAL] — Network Up/Down + Ping (Big numbers, graph-like)
void drawScreenNeural() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN + 2, MARGIN + LINE_H_HEADER, "[NEURAL]");

  int y = MARGIN + LINE_H_HEADER + 6;
  char buf[24];

  // Network Down (Big)
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "DOWN");
  if (hw.nd >= 1024)
    snprintf(buf, sizeof(buf), "%.1fM", hw.nd / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "%dK", hw.nd);
  u8g2.setFont(FONT_BIG);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_BIG);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_BIG, buf);
  y += LINE_H_BIG + 4;

  // Network Up (Big)
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "UP");
  if (hw.nu >= 1024)
    snprintf(buf, sizeof(buf), "%.1fM", hw.nu / 1024.0f);
  else
    snprintf(buf, sizeof(buf), "%dK", hw.nu);
  u8g2.setFont(FONT_BIG);
  tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_BIG);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_BIG, buf);
  y += LINE_H_BIG + 4;

  // Ping Latency
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "PING");
  snprintf(buf, sizeof(buf), "%dms", hw.pg);
  u8g2.setFont(FONT_DATA);
  tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_DATA, buf);

  // IP Address at bottom
  u8g2.setFont(FONT_TINY);
  String ip = WiFi.localIP().toString();
  if (ip.length() > 20)
    ip = ip.substring(0, 20);
  u8g2.drawStr(MARGIN + 2, DISP_H - MARGIN - 2, ip.c_str());
}

// [THERMAL] — Fan RPMs + Animated fan icon
void drawScreenThermal() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN + 2, MARGIN + LINE_H_HEADER, "[THERMAL]");

  // Animated fan icon
  drawFanIcon(DISP_W - 12, MARGIN + LINE_H_HEADER - 2, fanAnimFrame);

  int y = MARGIN + LINE_H_HEADER + 4;
  char buf[24];
  const char *labels[] = {"PUMP", "RAD", "SYS2", "GPU"};
  int vals[] = {hw.cf, hw.s1, hw.s2, hw.gf};

  for (int i = 0; i < 4 && y + LINE_H_DATA <= DISP_H - MARGIN - 8; i++) {
    snprintf(buf, sizeof(buf), "%d RPM", vals[i]);
    drawMetricStr(MARGIN + 32, y + LINE_H_DATA, labels[i], String(buf));
    y += LINE_H_DATA + 2;
  }

  // Chipset temp at bottom
  if (hw.ch > 0) {
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(MARGIN + 2, DISP_H - MARGIN - 8, "CHIP");
    snprintf(buf, sizeof(buf), "%dC", hw.ch);
    u8g2.setFont(FONT_DATA);
    int tw = u8g2.getUTF8Width(buf);
    u8g2.setDrawColor(0);
    u8g2.drawBox(MARGIN + 32, DISP_H - MARGIN - 8, tw + 2, LINE_H_DATA);
    u8g2.setDrawColor(1);
    u8g2.drawUTF8(MARGIN + 32, DISP_H - MARGIN, buf);
  }
}

// [MEM.BANK] — RAM + Storage bars
void drawScreenMemBank() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN + 2, MARGIN + LINE_H_HEADER, "[MEM.BANK]");

  int y = MARGIN + LINE_H_HEADER + 6;
  char buf[24];

  // RAM Usage
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "RAM");
  snprintf(buf, sizeof(buf), "%.1f/%.1fG", hw.ru, hw.ra);
  u8g2.setFont(FONT_DATA);
  int tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(DISP_W - MARGIN - tw - 2, y - LINE_H_DATA + 2, tw + 2,
               LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(DISP_W - MARGIN - tw, y, buf);
  float ramPct = (hw.ra > 0) ? (hw.ru / hw.ra * 100.0f) : 0.0f;
  drawProgressBar(MARGIN + 2, y + 2, DISP_W - 2 * MARGIN - 4, 6, ramPct);
  y += LINE_H_DATA + 4;

  // NVMe System Drive
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "SYS(C:)");
  snprintf(buf, sizeof(buf), "%d%%", hw.su);
  u8g2.setFont(FONT_DATA);
  tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(DISP_W - MARGIN - tw - 2, y - LINE_H_DATA + 2, tw + 2,
               LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(DISP_W - MARGIN - tw, y, buf);
  drawProgressBar(MARGIN + 2, y + 2, DISP_W - 2 * MARGIN - 4, 6, (float)hw.su);
  y += LINE_H_DATA + 4;

  // HDD Data Drive
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "DATA(D:)");
  snprintf(buf, sizeof(buf), "%d%%", hw.du);
  u8g2.setFont(FONT_DATA);
  tw = u8g2.getUTF8Width(buf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(DISP_W - MARGIN - tw - 2, y - LINE_H_DATA + 2, tw + 2,
               LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(DISP_W - MARGIN - tw, y, buf);
  drawProgressBar(MARGIN + 2, y + 2, DISP_W - 2 * MARGIN - 4, 6, (float)hw.du);
  y += LINE_H_DATA + 4;

  // Disk I/O
  u8g2.setFont(FONT_TINY);
  snprintf(buf, sizeof(buf), "R:%dK W:%dK", hw.dr, hw.dw);
  u8g2.drawStr(MARGIN + 2, DISP_H - MARGIN - 2, buf);
}

// [TASK.KILL] — Top CPU process + usage %
void drawScreenTaskKill() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN + 2, MARGIN + LINE_H_HEADER, "[TASK.KILL]");

  int y = MARGIN + LINE_H_HEADER + 6;
  char buf[24];

  if (procs.cpuNames[0].length() > 0) {
    // Warning icon if high CPU
    if (procs.cpuPercent[0] > 80) {
      u8g2.setFont(FONT_LABEL);
      if (blinkState) {
        u8g2.drawStr(DISP_W - 24, MARGIN + LINE_H_HEADER, "WARN");
      }
    }

    // Top process name
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(MARGIN + 2, y, "TOP PROC");
    String procName = procs.cpuNames[0];
    if (procName.length() > 18)
      procName = procName.substring(0, 18);
    u8g2.setFont(FONT_DATA);
    int tw = u8g2.getUTF8Width(procName.c_str());
    u8g2.setDrawColor(0);
    u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_DATA);
    u8g2.setDrawColor(1);
    u8g2.drawUTF8(MARGIN + 2, y + LINE_H_DATA, procName.c_str());
    y += LINE_H_DATA + 4;

    // CPU usage (Big)
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(MARGIN + 2, y, "CPU");
    snprintf(buf, sizeof(buf), "%d%%", procs.cpuPercent[0]);
    u8g2.setFont(FONT_BIG);
    tw = u8g2.getUTF8Width(buf);
    u8g2.setDrawColor(0);
    u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_BIG);
    u8g2.setDrawColor(1);
    u8g2.drawUTF8(MARGIN + 2, y + LINE_H_BIG, buf);
    y += LINE_H_BIG + 4;

    // Other top processes (compact)
    u8g2.setFont(FONT_TINY);
    for (int i = 1; i < 3 && i < 3; i++) {
      if (procs.cpuNames[i].length() > 0) {
        String pn = procs.cpuNames[i];
        if (pn.length() > 16)
          pn = pn.substring(0, 16);
        snprintf(buf, sizeof(buf), "%s %d%%", pn.c_str(), procs.cpuPercent[i]);
        u8g2.drawStr(MARGIN + 2, y, buf);
        y += 6;
      }
    }
  } else {
    u8g2.setFont(FONT_DATA);
    u8g2.drawStr(MARGIN + 2, y + LINE_H_DATA, "NO DATA");
  }
}

// [DECK] — Media player (Artist, Track, Status)
void drawScreenDeck() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(MARGIN + 2, MARGIN + LINE_H_HEADER, "[DECK]");

  int y = MARGIN + LINE_H_HEADER + 6;

  if (!media.isPlaying) {
    u8g2.setFont(FONT_BIG);
    u8g2.drawStr(MARGIN + 2, y + LINE_H_BIG, "STANDBY");

    // Draw cassette icon (cyberpunk retro aesthetic)
    int cx = DISP_W / 2;
    int cy = y + LINE_H_BIG + 12;
    u8g2.drawRFrame(cx - 16, cy - 8, 32, 16, 2);
    u8g2.drawCircle(cx - 8, cy, 4);
    u8g2.drawCircle(cx + 8, cy, 4);
    u8g2.drawLine(cx - 8, cy - 4, cx + 8, cy - 4);
    u8g2.drawLine(cx - 8, cy + 4, cx + 8, cy + 4);
    return;
  }

  // Artist
  String art = media.artist.length() > 0 ? media.artist : "Unknown";
  if (art.length() > 20)
    art = art.substring(0, 20);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "ARTIST");
  u8g2.setFont(FONT_DATA);
  int tw = u8g2.getUTF8Width(art.c_str());
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_DATA, art.c_str());
  y += LINE_H_DATA + 4;

  // Track
  String trk = media.track.length() > 0 ? media.track : "Unknown";
  if (trk.length() > 20)
    trk = trk.substring(0, 20);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "TRACK");
  u8g2.setFont(FONT_DATA);
  tw = u8g2.getUTF8Width(trk.c_str());
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 2, y + 2, tw + 2, LINE_H_DATA);
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(MARGIN + 2, y + LINE_H_DATA, trk.c_str());
  y += LINE_H_DATA + 4;

  // Playing indicator (animated)
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 2, y, "STATUS");
  u8g2.setFont(FONT_DATA);
  const char *status = blinkState ? "> PLAYING" : "  PLAYING";
  u8g2.drawStr(MARGIN + 2, y + LINE_H_DATA, status);
}

// ============================================================================
// DISPATCH
// ============================================================================
void drawScreen(int screen) {
  switch (screen) {
  case 0:
    drawScreenCortex();
    break;
  case 1:
    drawScreenNeural();
    break;
  case 2:
    drawScreenThermal();
    break;
  case 3:
    drawScreenMemBank();
    break;
  case 4:
    drawScreenTaskKill();
    break;
  case 5:
    drawScreenDeck();
    break;
  default:
    drawScreenCortex();
    break;
  }
}

void drawSplash() {
  u8g2.setFont(FONT_BIG);
  u8g2.drawStr(8, 20, "HELTEC v7.0");
  u8g2.setFont(FONT_DATA);
  u8g2.drawStr(8, 34, "[NEURAL LINK]");
  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(8, 44, "Cyberdeck Edition");

  // WiFi status during splash
  u8g2.setFont(FONT_TINY);
  if (wifiConnected) {
    u8g2.drawStr(8, 54, "WiFi: OK");
    drawWiFiIcon(DISP_W - 16, 48, wifiRssi);
  } else {
    u8g2.drawStr(8, 54, "WiFi: ...");
  }

  int barW =
      (int)((millis() - splashStart) * (DISP_W - 2 * MARGIN) / SPLASH_MS);
  if (barW > DISP_W - 2 * MARGIN)
    barW = DISP_W - 2 * MARGIN;
  u8g2.drawBox(MARGIN, DISP_H - 4 - MARGIN, barW, 3);
}

void drawMenu() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(MARGIN + 4, MARGIN + 2, DISP_W - 2 * MARGIN - 8,
               DISP_H - 2 * MARGIN - 4);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(MARGIN + 4, MARGIN + 2, DISP_W - 2 * MARGIN - 8,
                  DISP_H - 2 * MARGIN - 4, 2);
  u8g2.setFont(FONT_LABEL);
  u8g2.drawStr(MARGIN + 6, MARGIN + 8, "SETTINGS");
  const char *labels[] = {"LED", "Carousel", "Contrast", "Display", "Exit"};
  int startY = MARGIN + 14;
  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = startY + i * 8;
    if (y + 8 > DISP_H - MARGIN - 4)
      break;
    if (i == menuItem) {
      u8g2.drawBox(MARGIN + 6, y - 6, DISP_W - 2 * MARGIN - 12, 8);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(MARGIN + 8, y, labels[i]);
    if (i == menuItem)
      u8g2.setDrawColor(1);
  }
}

void drawNoSignal() {
  u8g2.setFont(FONT_BIG);
  u8g2.drawStr((DISP_W - 72) / 2, DISP_H / 2 - 4, "NO SIGNAL");

  // Connection status text
  u8g2.setFont(FONT_TINY);
  if (!wifiConnected) {
    u8g2.drawStr(MARGIN + 2, DISP_H / 2 + 10, "WiFi: DISCONNECTED");
  } else if (!tcpConnectedState) {
    u8g2.drawStr(MARGIN + 2, DISP_H / 2 + 10, "TCP: CONNECTING...");
  } else {
    u8g2.drawStr(MARGIN + 2, DISP_H / 2 + 10, "WAITING FOR DATA...");
  }

  // Static noise effect (cyberpunk glitch)
  for (int i = 0; i < 30; i++) {
    int x = random(0, DISP_W);
    int y = random(0, DISP_H);
    u8g2.drawPixel(x, y);
  }

  if (wifiConnected)
    drawWiFiIcon(DISP_W - MARGIN - 14, MARGIN, wifiRssi);

  // Blinking cursor effect
  if (blinkState) {
    u8g2.drawBox(DISP_W / 2 + 40, DISP_H / 2 - 8, 6, 12);
  }
}

void drawConnecting() {
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr((DISP_W - 72) / 2, DISP_H / 2 - 4, "LINKING...");

  u8g2.setFont(FONT_TINY);
  u8g2.drawStr(MARGIN + 2, DISP_H / 2 + 10, "Establishing data link");

  // Animated dots
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++) {
    u8g2.drawBox(DISP_W / 2 + 36 + i * 6, DISP_H / 2 + 8, 3, 3);
  }

  if (wifiConnected)
    drawWiFiIcon(DISP_W - MARGIN - 14, MARGIN, wifiRssi);
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

  // Initialize random seed for glitch effects
  randomSeed(analogRead(0));

  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  // [v7] Initialize lastUpdate to boot time to prevent immediate signal loss
  lastUpdate = bootTime;
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  // WiFi connection handling with reconnection logic
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      wifiRssi = WiFi.RSSI();
    }
    if (now % 5000 < 100)
      wifiRssi = WiFi.RSSI();

    // [v7] TCP connection with proper state management
    if (!tcpClient.connected()) {
      if (tcpConnectedState) {
        // Connection was lost - reset state
        tcpDisconnect();
      }
      // Attempt to connect
      tcpConnect();
    }
  } else {
    wifiConnected = false;
    if (tcpConnectedState) {
      tcpDisconnect();
    }
    if (now - lastWifiRetry > WIFI_RETRY_INTERVAL) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      lastWifiRetry = now;
    }
  }

  // TCP data reception (robust parsing)
  if (tcpClient.connected()) {
    while (tcpClient.available()) {
      char c = tcpClient.read();
      if (c == '\n') {
        if (tcpLineBuffer.length() > 0) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, tcpLineBuffer);
          if (!err) {
            lastUpdate = now;
            firstDataReceived = true; // [v7] Mark that we got valid data
            parsePayload(doc);
          }
          tcpLineBuffer = "";
        }
      } else {
        tcpLineBuffer += c;
        // [v7] Buffer overflow protection
        if (tcpLineBuffer.length() >= TCP_LINE_MAX) {
          tcpLineBuffer = "";
        }
      }
    }
  }

  // Button handling (short press = cycle, long press = menu)
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

  // Carousel mode
  if (settings.carouselEnabled && !inMenu) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
      lastCarousel = now;
    }
  }

  // Send screen change to server
  if (tcpClient.connected() && lastSentScreen != currentScreen) {
    tcpClient.print("screen:");
    tcpClient.print(currentScreen);
    tcpClient.print("\n");
    lastSentScreen = currentScreen;
  }

  // Animation updates
  if (now - lastScrollUpdate >= SCROLL_INTERVAL) {
    scrollOffset++;
    lastScrollUpdate = now;
  }

  if (now - lastFanAnim >= FAN_ANIM_INTERVAL) {
    fanAnimFrame = (fanAnimFrame + 1) % 12;
    lastFanAnim = now;
  }

  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  // Glitch effect timing
  if (now - lastGlitch > GLITCH_INTERVAL) {
    doGlitch = (random(100) < 10); // 10% chance
    lastGlitch = now;
  }

  // LED alert (temperature warnings)
  bool anyAlarm = (hw.ct >= CPU_TEMP_ALERT) || (hw.gt >= GPU_TEMP_ALERT);
  if (settings.ledEnabled && anyAlarm && blinkState)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);

  // Splash screen
  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= SPLASH_MS) {
      splashDone = true;
      // [v7] Reset lastUpdate when splash ends to start fresh
      lastUpdate = now;
    }
  }

  // RENDER FRAME
  u8g2.clearBuffer();
  bool signalLost = splashDone && isSignalLost(now);

  if (!splashDone) {
    drawSplash();
  } else if (!wifiConnected) {
    // WiFi not connected - show connecting state
    drawNoSignal();
  } else if (!tcpConnectedState) {
    // WiFi OK but TCP not connected
    drawConnecting();
  } else if (signalLost) {
    // [v7] Only disconnect if truly lost (after grace period)
    if (tcpClient.connected() && firstDataReceived) {
      // Only disconnect if we previously had data
      tcpDisconnect();
    }
    drawNoSignal();
  } else {
    // Normal operation - draw HUD
    drawCornerCrosshairs();
    drawScreen(currentScreen);

    // WiFi icon
    if (wifiConnected)
      drawWiFiIcon(DISP_W - 16, 2, wifiRssi);

    // Link status indicator
    drawLinkStatus(MARGIN + 2, DISP_H - MARGIN - 2,
                   tcpConnectedState && firstDataReceived);

    // Temperature alarm frame
    if (anyAlarm && blinkState)
      u8g2.drawFrame(0, 0, DISP_W, DISP_H);

    // Optional glitch effect
    if (doGlitch) {
      drawGlitchEffect();
      doGlitch = false;
    }

    // Menu overlay
    if (inMenu)
      drawMenu();

    // Screen indicator dots at bottom
    for (int i = 0; i < TOTAL_SCREENS; i++) {
      int x = DISP_W / 2 - (TOTAL_SCREENS * 3) + (i * 6);
      if (i == currentScreen)
        u8g2.drawBox(x, DISP_H - 2, 3, 2);
      else
        u8g2.drawPixel(x + 1, DISP_H - 1);
    }
  }

  u8g2.sendBuffer();

  // Small delay to prevent CPU hogging
  delay(10);
}
