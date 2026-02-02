#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>

#if __has_include("config_private.h")
#include "config_private.h"
#else
#define WIFI_SSID "Forest"
#define WIFI_PASS ""
#define PC_IP "192.168.1.2"
#define TCP_PORT 8888
#endif

// --- PINOUT ---
#define SDA_PIN 17
#define SCL_PIN 18
#define RST_PIN 21
#define VEXT_PIN 36
#define LED_PIN 35
#define BUTTON_PIN 0

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_PIN);

// --- GLOBAL DATA ---
// Hardware
int cpuTemp = 0, gpuTemp = 0, gpuHotSpot = 0, vrmTemp = 0;
int cpuLoad = 0, cpuPwr = 0, gpuLoad = 0, gpuPwr = 0;
int fanPump = 0, fanRad = 0, fanCase = 0, fanGpu = 0;
int gpuClk = 0;
int cores[6] = {0};
int ramPct = 0;
float ramUsed = 0.0, ramTotal = 0.0;
float vramUsed = 0.0, vramTotal = 0.0;
float vcore = 0.0f;
int nvme2Temp = 0;
int chipsetTemp = 0;
// Network & Disk
int netUp = 0, netDown = 0;
int diskRead = 0, diskWrite = 0;
// Weather
int weatherTemp = 0;
String weatherDesc = "";
int weatherIcon = 0;
// Top processes
String procNames[3] = {"", "", ""};
int procCpu[3] = {0, 0, 0};
// Media
String artist = "No Data";
String track = "Waiting...";
bool isPlaying = false;
#define COVER_SIZE 48
#define COVER_BYTES ((COVER_SIZE * COVER_SIZE) / 8)
uint8_t coverBitmap[COVER_BYTES];
bool hasCover = false;

// --- SETTINGS & STATE ---
bool ledEnabled = true;       // Настройка: Диод вкл/выкл
bool carouselEnabled = false; // Настройка: Авто-смена экранов
int eqStyle = 0;              // Стиль эквалайзера: 0=Bars, 1=Wave, 2=Circle

int currentScreen = 0;
// 0=Main .. 9=Network, 10=Wolf Game
const int TOTAL_SCREENS = 11;

bool inMenu = false; // Мы в меню?
int menuItem = 0;    // Пункт меню (0=LED, 1=Carousel, 2=EQ Style, 3=Exit)

// Button Logic
unsigned long btnPressTime = 0;
bool btnHeld = false;
bool menuHoldHandled = false; // один раз за удержание в меню
bool wasInMenuOnPress =
    false; // были в меню в момент нажатия (чтобы EXIT не открывал снова)
unsigned long lastMenuActivity = 0; // для авто-закрытия меню через 5 мин
const unsigned long MENU_TIMEOUT_MS = 5 * 60 * 1000;

// Timers
unsigned long lastUpdate = 0;
unsigned long lastCarousel = 0;
unsigned long lastBlink = 0;
unsigned long lastEqUpdate = 0;
bool blinkState = false;
const unsigned long SIGNAL_TIMEOUT_MS = 10000; // 10 sec
const unsigned long EQ_UPDATE_MS = 28;

// Пороги алертов (температура °C, нагрузка %)
const int CPU_TEMP_ALERT = 80;
const int GPU_TEMP_ALERT = 85;
const int CPU_LOAD_ALERT = 95;
const int GPU_LOAD_ALERT = 95;

// Equalizer: 16 bars, smooth interpolation
const int EQ_BARS = 16;
uint8_t eqHeights[EQ_BARS] = {0};
uint8_t eqTargets[EQ_BARS] = {0};

// Splash
bool splashDone = false;
unsigned long splashStart = 0;
int splashPhase = 0; // 0 = Forest OS, 1 = By RudyWolf
const unsigned long SPLASH_PHASE0_MS = 2500;
const unsigned long SPLASH_PHASE1_MS = 2500;
const unsigned long SPLASH_FRAME_MS = 250;
unsigned long lastSplashFrame = 0;
int splashFrame = 0;

// WiFi source
WiFiClient tcpClient;
unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TRY_MS = 8000;
String tcpLineBuffer;    // накопление строки до \n
int lastSentScreen = -1; // для отправки screen:N при смене экрана

// Wolf & Moon game (screen 10)
int gameMoonX = 64, gameMoonY = 10;
int gameWolfX = 56, gameWolfY = 52;
int gameWolfVy = 0;
int gameScore = 0;
unsigned long gameStartTime = 0;
const int GAME_DURATION_MS = 30000;
const int GAME_TARGET_SCORE = 5;
const unsigned long GAME_TICK_MS = 50;
const int MOON_R = 4;
const int WOLF_W = 16, WOLF_H = 10;
const int JUMP_VY = -8;
const int GRAVITY = 1;
unsigned long lastGameTick = 0;

// Weather icons (16x16, 32 bytes)
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

// Wolf face 24x24 for splash
const uint8_t wolfFace[72] PROGMEM = {
    0x00, 0x18, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x18, 0x00,
    0x01, 0xFF, 0x80, 0x03, 0xFF, 0xC0, 0x07, 0xFF, 0xE0, 0x07, 0xC3, 0xE0,
    0x07, 0x81, 0xE0, 0x07, 0x81, 0xE0, 0x07, 0xC3, 0xE0, 0x03, 0xFF, 0xC0,
    0x01, 0xFF, 0x80, 0x00, 0x7E, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void VextON() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
}

void setup() {
  VextON();
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(50);
  digitalWrite(RST_PIN, HIGH);
  delay(50);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(255);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Preferences prefs;
  prefs.begin("heltec", true);
  ledEnabled = prefs.getBool("led", true);
  carouselEnabled = prefs.getBool("carousel", false);
  eqStyle = prefs.getInt("eqStyle", 0);
  prefs.end();

  // WiFi: пробуем подключиться (в loop будем проверять и подключаться к TCP)
  if (strlen(WIFI_PASS) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    wifiConnectStart = millis();
  }
}

// --- BASE64 DECODE (для обложки) ---
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

// --- DRAWING HELPERS ---
void drawBar(int x, int y, int w, int h, float val, float max) {
  u8g2.drawFrame(x, y, w, h);
  int f = (int)((val / max) * (w - 2));
  if (f > w - 2)
    f = w - 2;
  u8g2.drawBox(x + 1, y + 1, f, h - 2);
}

// --- SCREENS ---

// --- SPLASH ---
void drawSplash() {
  if (splashPhase == 0) {
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.drawStr(20, 28, "Forest");
    u8g2.drawStr(28, 48, "OS");
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(28, 18, "By RudyWolf");
    u8g2.drawXBMP(52, 28, 24, 24, wolfFace);
    // Лапки — точки по углам, сдвиг по кадру
    int o = (splashFrame % 4) * 4;
    u8g2.drawPixel(8 + o, 56);
    u8g2.drawPixel(120 - o, 56);
    u8g2.drawPixel(8 + o, 8);
    u8g2.drawPixel(120 - o, 8);
  }
}

// --- SCREEN ZONES: header y=0..13, content y>=14 ---
// 1. Main (сводка): отступ от верха, блоки CPU/GPU, RAM, VRAM, Load
void drawMain() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "MAIN");

  u8g2.setCursor(0, 20);
  u8g2.print("CPU ");
  u8g2.print(cpuTemp);
  u8g2.print((char)0xB0);
  u8g2.print("C  GPU ");
  u8g2.print(gpuTemp);
  u8g2.print((char)0xB0);
  u8g2.print("C");

  u8g2.setCursor(0, 30);
  u8g2.print("RAM ");
  u8g2.print(ramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)ramTotal);
  u8g2.print("G ");
  u8g2.print(ramPct);
  u8g2.print("%");

  u8g2.setCursor(0, 40);
  u8g2.print("VRAM ");
  u8g2.print(vramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)vramTotal);
  u8g2.print("G");

  u8g2.setCursor(0, 50);
  u8g2.print("Load CPU ");
  u8g2.print(cpuLoad);
  u8g2.print("% GPU ");
  u8g2.print(gpuLoad);
  u8g2.print("%");
}

// Cores: зона заголовка y=0..11, столбики y=12..63
void drawCores() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "CORES");
  const int barTop = 14;
  const int barH = 48;
  for (int i = 0; i < 6; i++) {
    int h = map(cores[i], 3000, 4850, 2, barH);
    if (h < 0)
      h = 0;
    if (h > barH)
      h = barH;
    int x = i * 20 + 4;
    u8g2.drawBox(x, barTop + barH - h, 15, h);
  }
}

// GPU: заголовок, частота, VRAM bar, вентилятор, нагрузка, мощность
void drawGpu() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "GPU");
  u8g2.setCursor(0, 22);
  u8g2.print(gpuClk);
  u8g2.print(" MHz ");
  u8g2.print(gpuLoad);
  u8g2.print("% ");
  u8g2.print(gpuPwr);
  u8g2.print("W");
  drawBar(0, 26, 128, 8, vramUsed, vramTotal > 0 ? vramTotal : 1);
  u8g2.setCursor(0, 42);
  u8g2.print("Fan:");
  u8g2.print(fanGpu);
  u8g2.print(" Hot:");
  u8g2.print(gpuHotSpot);
  u8g2.print((char)0xB0);
}

// Memory: RAM и VRAM (used/total, %)
void drawMemory() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "MEMORY");
  u8g2.setCursor(0, 24);
  u8g2.print("RAM ");
  u8g2.print(ramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)ramTotal);
  u8g2.print("G ");
  u8g2.print(ramPct);
  u8g2.print("%");
  drawBar(0, 28, 128, 6, ramUsed, ramTotal > 0 ? ramTotal : 1);
  u8g2.setCursor(0, 42);
  u8g2.print("VRAM ");
  u8g2.print(vramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)vramTotal);
  u8g2.print("G");
  drawBar(0, 46, 128, 6, vramUsed, vramTotal > 0 ? vramTotal : 1);
}

// Power: Vcore, CPU W, GPU W, NVMe temp
void drawPower() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "POWER");
  u8g2.setCursor(0, 24);
  u8g2.print("Vcore ");
  u8g2.print(vcore, 2);
  u8g2.print("V");
  u8g2.setCursor(0, 36);
  u8g2.print("CPU ");
  u8g2.print(cpuPwr);
  u8g2.print("W  GPU ");
  u8g2.print(gpuPwr);
  u8g2.print("W");
  u8g2.setCursor(0, 50);
  u8g2.print("NVMe ");
  u8g2.print(nvme2Temp);
  u8g2.print((char)0xB0);
  u8g2.print("C");
}

// Weather: Moscow weather display
void drawWeather() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "WEATHER MSK");

  // Icon (16x16) — Open-Meteo WMO weather_code 0–99
  const uint8_t *icon = iconCloudy;
  if (weatherIcon == 0)
    icon = iconSunny;
  else if (weatherIcon >= 1 && weatherIcon <= 3)
    icon = iconCloudy;
  else if (weatherIcon >= 45 && weatherIcon <= 48)
    icon = iconCloudy;
  else if (weatherIcon >= 51 && weatherIcon <= 67)
    icon = iconRain;
  else if (weatherIcon >= 71 && weatherIcon <= 77)
    icon = iconSnow;
  else if (weatherIcon >= 80 && weatherIcon <= 82)
    icon = iconRain;
  else if (weatherIcon >= 85 && weatherIcon <= 86)
    icon = iconSnow;
  else if (weatherIcon >= 95 && weatherIcon <= 99)
    icon = iconRain;

  u8g2.drawXBMP(4, 20, 16, 16, icon);

  // Температура и описание справа
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.setCursor(26, 28);
  u8g2.print(weatherTemp);
  u8g2.print((char)0xB0);
  u8g2.print("C");

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(26, 40);
  u8g2.print(weatherDesc.substring(0, 14));
}

// Top 3 processes by CPU
void drawTopProcs() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "TOP 3 CPU");

  for (int i = 0; i < 3; i++) {
    int y = 24 + i * 12;
    u8g2.setCursor(0, y);
    if (procNames[i].length() > 0) {
      u8g2.print(procNames[i].substring(0, 12));
      u8g2.setCursor(80, y);
      u8g2.print(procCpu[i]);
      u8g2.print("%");
    }
  }
}

// Network & Disk screen
void drawNetwork() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "NETWORK & DISK");

  u8g2.setCursor(0, 24);
  u8g2.print("Net Up: ");
  u8g2.print(netUp);
  u8g2.print(" KB/s");

  u8g2.setCursor(0, 36);
  u8g2.print("Net Dn: ");
  u8g2.print(netDown);
  u8g2.print(" KB/s");

  u8g2.setCursor(0, 48);
  u8g2.print("Disk R: ");
  u8g2.print(diskRead);
  u8g2.print(" MB/s");

  u8g2.setCursor(0, 60);
  u8g2.print("Disk W: ");
  u8g2.print(diskWrite);
  u8g2.print(" MB/s");
}

// Wolf & Moon game
void drawWolfGame() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 8);
  u8g2.print("Wolf & Moon ");
  u8g2.print(gameScore);
  u8g2.print("/");
  u8g2.print(GAME_TARGET_SCORE);

  unsigned long gameElapsed =
      (gameStartTime > 0) ? (millis() - gameStartTime) : 0;
  int left = (gameElapsed < GAME_DURATION_MS)
                 ? (GAME_DURATION_MS - gameElapsed) / 1000
                 : 0;

  if (gameScore >= GAME_TARGET_SCORE) {
    u8g2.setCursor(20, 32);
    u8g2.print("YOU WIN!");
  } else if (left <= 0) {
    u8g2.setCursor(16, 32);
    u8g2.print("TIME UP!");
  } else {
    u8g2.drawDisc(gameMoonX, gameMoonY, MOON_R);
    u8g2.drawFrame(gameWolfX, gameWolfY, WOLF_W, WOLF_H);
  }
  u8g2.setCursor(90, 8);
  u8g2.print(left);
  u8g2.print("s");
}

// Equalizer Bars (столбики)
void drawEqBars() {
  const int barTopY = 12;
  const int barZoneH = 52;
  const int barW = 6;
  const int barGap = 2;

  for (int i = 0; i < EQ_BARS; i++) {
    int h = eqHeights[i];
    if (h > barZoneH)
      h = barZoneH;
    int x = i * (barW + barGap);
    int y = barTopY + barZoneH - h;
    u8g2.drawBox(x, y, barW, h);
  }
}

// Equalizer Wave (синусоида iTunes-style)
void drawEqWave() {
  int prevY = 32;
  int prevX = 0;
  for (int x = 0; x < 128; x += 2) {
    int idx = (x * EQ_BARS) / 128;
    if (idx >= EQ_BARS)
      idx = EQ_BARS - 1;
    float amp = eqHeights[idx] / 4.0;
    int y = 32 + (int)(sin(x * 0.1) * amp);
    if (y < 12)
      y = 12;
    if (y > 63)
      y = 63;
    if (x > 0)
      u8g2.drawLine(prevX, prevY, x, y);
    prevY = y;
    prevX = x;
  }
}

// Equalizer Circle (круговая визуализация)
void drawEqCircle() {
  int cx = 64, cy = 38;
  for (int i = 0; i < EQ_BARS; i++) {
    float angle = (i * 22.5) * PI / 180.0;
    int len = 8 + eqHeights[i] / 2;
    int x = cx + (int)(cos(angle) * len);
    int y = cy + (int)(sin(angle) * len);
    if (x < 0)
      x = 0;
    if (x > 127)
      x = 127;
    if (y < 12)
      y = 12;
    if (y > 63)
      y = 63;
    u8g2.drawLine(cx, cy, x, y);
  }
}

// Equalizer: заголовок y=0..11, визуализация y=12..63
void drawEqualizer() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("VISUALIZER ");
  u8g2.print(isPlaying ? "[PLAY]" : "[PAUSE]");

  switch (eqStyle) {
  case 0:
    drawEqBars();
    break;
  case 1:
    drawEqWave();
    break;
  case 2:
    drawEqCircle();
    break;
  }
}

void drawPlayer() {
  int textX = 42;
  if (hasCover) {
    u8g2.drawBitmap(0, (64 - COVER_SIZE) / 2, COVER_SIZE / 8, COVER_SIZE,
                    coverBitmap);
    textX = COVER_SIZE + 4;
  } else {
    u8g2.drawDisc(20, 32, 14, U8G2_DRAW_ALL);
    if (isPlaying) {
      if (blinkState)
        u8g2.setDrawColor(0);
      u8g2.drawDisc(20, 32, 4, U8G2_DRAW_ALL);
      u8g2.setDrawColor(1);
    } else {
      u8g2.setDrawColor(0);
      u8g2.drawBox(18, 30, 4, 4);
      u8g2.setDrawColor(1);
    }
  }
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.setCursor(textX, 20);
  u8g2.print(artist);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(textX, 35);
  u8g2.print(track.substring(0, 14));
  u8g2.drawFrame(textX, 45, 128 - textX - 2, 6);
  if (isPlaying) {
    int barW = 128 - textX - 4;
    int prog = (millis() / 100) % barW;
    if (prog < 0)
      prog = 0;
    u8g2.drawBox(textX + 2, 47, prog, 2);
  }
}

void drawMenu() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(12, 12, 104, 48);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(12, 12, 104, 48);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(42, 20, "- MENU -");

  // Item 0: LED
  u8g2.setCursor(16, 30);
  u8g2.print(menuItem == 0 ? "> LED: " : "  LED: ");
  u8g2.print(ledEnabled ? "ON" : "OFF");

  // Item 1: Carousel
  u8g2.setCursor(16, 40);
  u8g2.print(menuItem == 1 ? "> AUTO: " : "  AUTO: ");
  u8g2.print(carouselEnabled ? "ON" : "OFF");

  // Item 2: EQ Style
  u8g2.setCursor(16, 50);
  u8g2.print(menuItem == 2 ? "> EQ: " : "  EQ: ");
  if (eqStyle == 0)
    u8g2.print("Bars");
  else if (eqStyle == 1)
    u8g2.print("Wave");
  else
    u8g2.print("Circle");

  // Item 3: Exit
  u8g2.setCursor(16, 58);
  u8g2.print(menuItem == 3 ? "> EXIT" : "  EXIT");
}

void loop() {
  unsigned long now = millis();

  // --- 0. WiFi / TCP connect (при старте или при обрыве) ---
  if (strlen(WIFI_PASS) > 0) {
    if (WiFi.status() == WL_CONNECTED && !tcpClient.connected()) {
      tcpClient.connect(PC_IP, TCP_PORT, 2000);
    }
    if (WiFi.status() != WL_CONNECTED &&
        (now - wifiConnectStart < WIFI_TRY_MS)) {
      delay(10); // даём WiFi время при старте
    }
    if (!tcpClient.connected()) {
      tcpClient.stop();
    }
  }

  // --- 1. JSON READ (WiFi) ---
  String input;
  if (tcpClient.connected()) {
    while (tcpClient.available()) {
      char c = (char)tcpClient.read();
      if (c == '\n') {
        input = tcpLineBuffer;
        tcpLineBuffer = "";
        break;
      }
      if (tcpLineBuffer.length() < 4096)
        tcpLineBuffer += c;
      else
        tcpLineBuffer = "";
    }
  }

  if (input.length() > 0 && input.length() < 4096) {
    StaticJsonDocument<3600> doc;
    DeserializationError error = deserializeJson(doc, input);
    if (!error) {
      bool isHeartbeat = input.indexOf("art") < 0 || input.length() < 200;

      cpuTemp = doc["ct"];
      gpuTemp = doc["gt"];
      cpuLoad = doc["cpu_load"];
      gpuLoad = doc["gpu_load"];
      isPlaying = (doc["play"] | 0) == 1;
      lastUpdate = now;

      if (isHeartbeat) {
        // Только алерты — остальные поля не трогаем
      } else {
        gpuHotSpot = doc["gth"];
        vrmTemp = doc["vt"];
        cpuPwr = (int)(float(doc["cpu_pwr"]) + 0.5f);
        gpuPwr = (int)(float(doc["gpu_pwr"]) + 0.5f);
        fanPump = doc["p"];
        fanRad = doc["r"];
        fanCase = doc["c"];
        fanGpu = doc["gf"];
        gpuClk = doc["gck"];

        cores[0] = doc["c1"];
        cores[1] = doc["c2"];
        cores[2] = doc["c3"];
        cores[3] = doc["c4"];
        cores[4] = doc["c5"];
        cores[5] = doc["c6"];

        ramUsed = doc["ru"];
        ramTotal = doc["rt"];
        ramPct = doc["rp"];
        vramUsed = doc["vu"];
        vramTotal = doc["vt_tot"];
        vcore = doc["vcore"];
        nvme2Temp = doc["nvme2_t"];
        chipsetTemp = doc["chipset_t"];

        netUp = doc["nu"];
        netDown = doc["nd"];
        diskRead = doc["dr"];
        diskWrite = doc["dw"];

        weatherTemp = doc["wt"];
        const char *wd = doc["wd"];
        weatherDesc = String(wd ? wd : "");
        weatherIcon = doc["wi"];

        JsonArray tp = doc["tp"];
        for (int i = 0; i < 3; i++) {
          if (i < tp.size()) {
            const char *n = tp[i]["n"];
            procNames[i] = String(n ? n : "");
            procCpu[i] = tp[i]["c"];
          } else {
            procNames[i] = "";
            procCpu[i] = 0;
          }
        }

        const char *art = doc["art"];
        const char *trk = doc["trk"];
        artist = String(art ? art : "");
        track = String(trk ? trk : "");

        const char *cov = doc["cover_b64"];
        if (cov) {
          size_t covLen = strlen(cov);
          if (covLen > 0 && covLen <= 512) {
            int n = b64Decode(cov, covLen, coverBitmap, COVER_BYTES);
            hasCover = (n == (int)COVER_BYTES);
          } else {
            hasCover = false;
          }
        } else {
          hasCover = false;
        }
      }
    }
  }

  // --- 2. BUTTON LOGIC (Long Press) ---
  int btnState = digitalRead(BUTTON_PIN); // Active LOW

  if (btnState == LOW && !btnHeld) {
    btnHeld = true;
    btnPressTime = now;
    menuHoldHandled = false;
    wasInMenuOnPress = inMenu;
  }

  if (btnState == HIGH && btnHeld) {
    // Button Released
    unsigned long duration = now - btnPressTime;
    btnHeld = false;

    if (duration > 800) {
      // --- LONG PRESS: открыть меню только если при нажатии мы НЕ были в меню
      // ---
      if (!wasInMenuOnPress) {
        inMenu = true;
        menuItem = 0;
        lastMenuActivity = now;
      }
    } else {
      // --- SHORT PRESS ---
      if (inMenu) {
        lastMenuActivity = now;
        menuItem++;
        if (menuItem > 3)
          menuItem = 0;
      } else {
        if (currentScreen == 10) {
          gameWolfVy = JUMP_VY;
        } else {
          currentScreen++;
          if (currentScreen >= TOTAL_SCREENS)
            currentScreen = 0;
          lastCarousel = now;
        }
      }
    }
  }

  // Logic inside menu for "Selection" (Wait... one button?)
  // One button dilemma: Short=Next, Long=Select?
  // Let's change logic:
  // IN MENU: Short = Scroll, Long = Toggle/Select

  if (inMenu && btnState == LOW && (now - btnPressTime > 800) &&
      !menuHoldHandled) {
    menuHoldHandled = true;
    lastMenuActivity = now;
    if (menuItem == 0) {
      ledEnabled = !ledEnabled;
      Preferences p;
      p.begin("heltec", false);
      p.putBool("led", ledEnabled);
      p.end();
    }
    if (menuItem == 1) {
      carouselEnabled = !carouselEnabled;
      Preferences p;
      p.begin("heltec", false);
      p.putBool("carousel", carouselEnabled);
      p.end();
    }
    if (menuItem == 2) {
      eqStyle = (eqStyle + 1) % 3;
      Preferences p;
      p.begin("heltec", false);
      p.putInt("eqStyle", eqStyle);
      p.end();
    }
    if (menuItem == 3)
      inMenu = false;
  }

  // --- 3. MENU AUTO-CLOSE (5 min) ---
  if (inMenu && (now - lastMenuActivity > MENU_TIMEOUT_MS)) {
    inMenu = false;
  }

  // --- 4. CAROUSEL LOGIC ---
  if (carouselEnabled && !inMenu) {
    if (now - lastCarousel > 4000) { // 4 sec per screen
      currentScreen++;
      if (currentScreen >= TOTAL_SCREENS)
        currentScreen = 0;
      lastCarousel = now;
    }
  }

  // --- 4b. Отправить текущий экран на ПК (для полного пакета по экрану) ---
  if (tcpClient.connected() && lastSentScreen != currentScreen) {
    tcpClient.print("screen:");
    tcpClient.print(currentScreen);
    tcpClient.print("\n");
    lastSentScreen = currentScreen;
    if (currentScreen == 10) {
      gameStartTime = now;
      gameScore = 0;
      gameMoonX = 64;
      gameMoonY = 10;
      gameWolfX = 56;
      gameWolfY = 52;
      gameWolfVy = 0;
    }
  }

  // --- 4c. Wolf & Moon game physics (screen 10), tick every GAME_TICK_MS ---
  if (currentScreen == 10 && splashDone &&
      (now - lastGameTick >= GAME_TICK_MS)) {
    lastGameTick = now;
    if (gameStartTime == 0) {
      gameStartTime = now;
    }
    unsigned long gameElapsed = now - gameStartTime;
    if (gameElapsed < GAME_DURATION_MS && gameScore < GAME_TARGET_SCORE) {
      gameMoonY += 2;
      if (gameMoonY > 64 + MOON_R) {
        gameMoonY = 10;
        gameMoonX = 20 + (now % 88);
      }
      gameWolfY += gameWolfVy;
      gameWolfVy += GRAVITY;
      if (gameWolfY > 54) {
        gameWolfY = 54;
        gameWolfVy = 0;
      }
      if (gameWolfY < 20)
        gameWolfY = 20;

      int moonCx = gameMoonX, moonCy = gameMoonY;
      int wolfLeft = gameWolfX, wolfRight = gameWolfX + WOLF_W;
      int wolfTop = gameWolfY, wolfBot = gameWolfY + WOLF_H;
      if (moonCy + MOON_R >= wolfTop && moonCy - MOON_R <= wolfBot &&
          moonCx + MOON_R >= wolfLeft && moonCx - MOON_R <= wolfRight) {
        gameScore++;
        gameMoonY = 10;
        gameMoonX = 20 + (now % 88);
      }
    }
  }

  // --- 5. EQUALIZER ANIMATION (быстрая интерполяция, шаг 2–3) ---
  if (now - lastEqUpdate >= EQ_UPDATE_MS) {
    lastEqUpdate = now;
    if (isPlaying) {
      for (int i = 0; i < EQ_BARS; i++) {
        if (random(0, 4) == 0)
          eqTargets[i] = (uint8_t)random(4, 48);
        int step = 2;
        if (eqHeights[i] < eqTargets[i]) {
          eqHeights[i] += step;
          if (eqHeights[i] > 52)
            eqHeights[i] = 52;
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

  // --- 6. ALARM LOGIC ---
  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  bool anyAlarm = (cpuTemp >= CPU_TEMP_ALERT) || (gpuTemp >= GPU_TEMP_ALERT) ||
                  (cpuLoad >= CPU_LOAD_ALERT) || (gpuLoad >= GPU_LOAD_ALERT);

  if (ledEnabled && anyAlarm && blinkState)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);

  // --- 6b. SPLASH STATE ---
  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    unsigned long elapsed = now - splashStart;
    if (splashPhase == 0 && elapsed >= SPLASH_PHASE0_MS) {
      splashPhase = 1;
      splashStart = now;
    } else if (splashPhase == 1 && elapsed >= SPLASH_PHASE1_MS) {
      splashDone = true;
    }
    if (now - lastSplashFrame >= SPLASH_FRAME_MS) {
      lastSplashFrame = now;
      splashFrame++;
    }
  }

  // --- 7. DRAWING ---
  u8g2.clearBuffer();
  bool signalLost = (now - lastUpdate > SIGNAL_TIMEOUT_MS);

  if (!splashDone) {
    drawSplash();
  } else if (signalLost) {
    switch (currentScreen) {
    case 0:
      drawMain();
      break;
    case 1:
      drawCores();
      break;
    case 2:
      drawGpu();
      break;
    case 3:
      drawMemory();
      break;
    case 4:
      drawPlayer();
      break;
    case 5:
      drawEqualizer();
      break;
    case 6:
      drawPower();
      break;
    case 7:
      drawWeather();
      break;
    case 8:
      drawTopProcs();
      break;
    case 9:
      drawNetwork();
      break;
    case 10:
      drawWolfGame();
      break;
    }
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(70, 8, "Reconnect");
  } else if (splashDone) {
    switch (currentScreen) {
    case 0:
      drawMain();
      break;
    case 1:
      drawCores();
      break;
    case 2:
      drawGpu();
      break;
    case 3:
      drawMemory();
      break;
    case 4:
      drawPlayer();
      break;
    case 5:
      drawEqualizer();
      break;
    case 6:
      drawPower();
      break;
    case 7:
      drawWeather();
      break;
    case 8:
      drawTopProcs();
      break;
    case 9:
      drawNetwork();
      break;
    case 10:
      drawWolfGame();
      break;
    }
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(118, 8);
    u8g2.print("W");

    if (anyAlarm) {
      if (blinkState)
        u8g2.drawFrame(0, 0, 128, 64);
      u8g2.setCursor(92, 10);
      u8g2.print("ALERT");
    }

    if (inMenu)
      drawMenu();

    if (!inMenu) {
      for (int i = 0; i < TOTAL_SCREENS; i++) {
        int x = 64 - (TOTAL_SCREENS * 3) + (i * 6);
        if (i == currentScreen)
          u8g2.drawBox(x, 62, 2, 2);
        else
          u8g2.drawPixel(x + 1, 63);
      }
    }
  }

  u8g2.sendBuffer();
}