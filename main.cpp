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

// --- PINOUT ---
#define SDA_PIN 17
#define SCL_PIN 18
#define RST_PIN 21
#define VEXT_PIN 36
#define LED_PIN 35
#define BUTTON_PIN 0

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_PIN);

// --- DISPLAY: Heltec v4 0.96" OLED 128x64 ---
#define DISP_W 128
#define DISP_H 64

// --- FONTS: единый набор под 128x64, читаемые ---
#define FONT_TITLE u8g2_font_7x13B_tf // заголовки, метки (жирный 7x13)
#define FONT_MAIN u8g2_font_7x13_tf   // данные (7x13)
#define FONT_SMALL u8g2_font_5x8_tf   // мелкий текст (5x8)
#define FONT_HEADER FONT_TITLE
#define FONT_DATA FONT_MAIN
#define FONT_MEDIA u8g2_font_cu12_t_cyrillic // плеер: артист/трек (кириллица)

// --- GLOBAL DATA ---
// Hardware
int cpuTemp = 0, gpuTemp = 0, gpuHotSpot = 0, vrmTemp = 0;
int cpuLoad = 0, cpuPwr = 0, gpuLoad = 0, gpuPwr = 0;
int fanPump = 0, fanRad = 0, fanCase = 0, fanGpu = 0;
int gpuClk = 0;
int cores[6] = {0};    // legacy clock (c1-c6)
int coreLoad[6] = {0}; // per-core load % (cl1-cl6)
int ramPct = 0;
float ramUsed = 0.0, ramTotal = 0.0;
float vramUsed = 0.0, vramTotal = 0.0;
float vcore = 0.0f;
int nvme2Temp = 0;
int chipsetTemp = 0;
int diskTemp[4] = {0, 0, 0, 0}; // D1..D4 temps °C
float diskUsed[4] = {0, 0, 0, 0}, diskTotal[4] = {0, 0, 0, 0}; // GB
// Network
int netUp = 0, netDown = 0;
int diskRead = 0, diskWrite = 0;
// Weather
int weatherTemp = 0;
String weatherDesc = "";
int weatherIcon = 0;
int weatherDayHigh = 0, weatherDayLow = 0, weatherDayCode = 0;
int weatherWeekHigh[7] = {0}, weatherWeekLow[7] = {0}, weatherWeekCode[7] = {0};
int weatherPage = 0; // 0=Now, 1=Day, 2=+2 days
// Top processes (CPU)
String procNames[3] = {"", "", ""};
int procCpu[3] = {0, 0, 0};
// Top processes (RAM) for Memory screen
String procRamNames[2] = {"", ""};
int procRamMb[2] = {0, 0};
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
int carouselIntervalSec = 10; // Интервал карусели: 5, 10 или 15 сек
int displayContrast = 255;    // Контраст дисплея 0–255
int eqStyle = 0;              // Стиль эквалайзера: 0=Bars, 1=Wave, 2=Circle
bool displayInverted = false; // Ориентация: 0=обычный, 1=инвертированный (180°)

int currentScreen = 0;
// 0=Main 1=Cores 2=CPU 3=GPU 4=Memory 5=Disks 6=Player 7=Fans 8=Weather
// 9=TopProcs
const int TOTAL_SCREENS = 10;

bool inMenu = false; // Мы в меню?
int menuItem =
    0; // 0=LED 1=Carousel 2=Contrast 3=Interval 4=EQ 5=Display 6=Exit
const int MENU_ITEMS = 7;

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

// Пороги алертов заданы в config_private.h или дефолтами выше

// Equalizer: 16 bars, smooth interpolation
const int EQ_BARS = 16;
uint8_t eqHeights[EQ_BARS] = {0};
uint8_t eqTargets[EQ_BARS] = {0};

// Splash (single phase)
bool splashDone = false;
unsigned long splashStart = 0;
const unsigned long SPLASH_MS = 1500;

// WiFi source
WiFiClient tcpClient;
unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TRY_MS = 8000;
String tcpLineBuffer;    // накопление строки до \n
int lastSentScreen = -1; // для отправки screen:N при смене экрана

// --- BITMAPS (weather icons 16x16) ---
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
  u8g2.enableUTF8Print();
  u8g2.setContrast(255);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Preferences prefs;
  prefs.begin("heltec", true);
  ledEnabled = prefs.getBool("led", true);
  carouselEnabled = prefs.getBool("carousel", false);
  carouselIntervalSec = prefs.getInt("carouselSec", 10);
  if (carouselIntervalSec != 5 && carouselIntervalSec != 10 &&
      carouselIntervalSec != 15)
    carouselIntervalSec = 10;
  displayContrast = prefs.getInt("contrast", 255);
  if (displayContrast > 255)
    displayContrast = 255;
  eqStyle = prefs.getInt("eqStyle", 0);
  displayInverted = prefs.getBool("inverted", false);
  prefs.end();
  u8g2.setContrast((uint8_t)displayContrast);
  u8g2.setFlipMode(displayInverted ? 1 : 0);

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
// Rounded progress bar (Apple/Noir style)
void drawProgressBar(int x, int y, int w, int h, float pct) {
  if (pct > 100.0f)
    pct = 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  u8g2.drawRFrame(x, y, w, h, 2);
  int fillW = (int)((w - 4) * (pct / 100.0f));
  if (fillW > 0)
    u8g2.drawRBox(x + 2, y + 2, fillW, h - 4, 1);
}

// Legacy bar (val/max) for backward compat
void drawBar(int x, int y, int w, int h, float val, float max) {
  float pct = (max > 0.0f) ? (val / max * 100.0f) : 0.0f;
  drawProgressBar(x, y, w, h, pct);
}

// Проверка: строка только ASCII (для выбора латинского шрифта в плейере)
static bool isOnlyAscii(const String &s) {
  for (unsigned int i = 0; i < s.length(); i++)
    if ((unsigned char)s.charAt(i) > 127)
      return false;
  return true;
}

// Контент с верха экрана, отступы по краям
const int CONTENT_TOP = 0;
const int MARGIN = 2;

// --- SCREENS ---

// --- SPLASH (single phase, short) ---
void drawSplash() {
  u8g2.setFont(FONT_TITLE);
  const char *line1 = "Forest";
  const char *line2 = "OS";
  int w1 = u8g2.getStrWidth(line1);
  int w2 = u8g2.getStrWidth(line2);
  u8g2.drawStr((DISP_W - w1) / 2, 26, line1);
  u8g2.drawStr((DISP_W - w2) / 2, 48, line2);
}

// 1. Main (Dashboard): три ряда на весь экран, бары на всю ширину
void drawMain() {
  const int rowH = DISP_H / 3;
  const int barY0 = 10, barH = 8;
  const int barX = MARGIN, barW = DISP_W - 2 * MARGIN;
  char buf[24];

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 9, "CPU");
  snprintf(buf, sizeof(buf), "%d%c/%d", cpuTemp, (char)0xB0, fanPump);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 9, buf);
  drawProgressBar(barX, barY0, barW, barH, (float)cpuLoad);

  u8g2.drawStr(MARGIN, 9 + rowH, "GPU");
  snprintf(buf, sizeof(buf), "%d%c/%d", gpuTemp, (char)0xB0, fanGpu);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 9 + rowH, buf);
  drawProgressBar(barX, barY0 + rowH, barW, barH, (float)gpuLoad);

  u8g2.drawStr(MARGIN, 9 + 2 * rowH, "RAM");
  snprintf(buf, sizeof(buf), "%.1fG", ramUsed);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 9 + 2 * rowH, buf);
  drawProgressBar(barX, barY0 + 2 * rowH, barW, barH, (float)ramPct);

  bool alert = (cpuTemp >= CPU_TEMP_ALERT) || (gpuTemp >= GPU_TEMP_ALERT) ||
               (cpuLoad >= CPU_LOAD_ALERT) || (gpuLoad >= GPU_LOAD_ALERT);
  if (alert && blinkState) {
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(MARGIN, DISP_H - 4);
    u8g2.print("Warn");
  }
}

// Cores: 6 баров на всю высоту экрана
void drawCores() {
  const int barTop = MARGIN;
  const int barH = DISP_H - 14;
  const int barW = 16;
  const int gap = 3;
  const int totalW = 6 * barW + 5 * gap;
  int startX = (DISP_W - totalW) / 2;
  for (int i = 0; i < 6; i++) {
    int h = map(coreLoad[i], 0, 100, 0, barH);
    if (h < 0)
      h = 0;
    if (h > barH)
      h = barH;
    int x = startX + i * (barW + gap);
    u8g2.drawRFrame(x, barTop, barW, barH, 2);
    if (h > 0)
      u8g2.drawBox(x + 1, barTop + barH - h, barW - 2, h);
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(x + 5, DISP_H - 4);
    u8g2.print(i + 1);
  }
}

// CPU only: watts, temperature, frequency, load
void drawCpuOnly() {
  int cpuMhz = cores[0];
  for (int i = 1; i < 6; i++)
    if (cores[i] > cpuMhz)
      cpuMhz = cores[i];
  char buf[32];
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "CPU Temp");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%d%cC", cpuTemp, (char)0xB0);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 12, buf);

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 26, "Freq");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%d MHz", cpuMhz);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 26, buf);

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 40, "Load");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%d%%", cpuLoad);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 40, buf);

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 54, "Power");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%d W", cpuPwr);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 54, buf);
}

// GPU: Temp\W, Clock, VRAM (no graph)
void drawGpu() {
  char buf[32];
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "Temp\\W");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%d%cC  %d W", gpuTemp, (char)0xB0, gpuPwr);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 12, buf);

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 26, "Clock");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%d MHz", gpuClk);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 26, buf);

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 40, "VRAM");
  u8g2.setFont(FONT_MAIN);
  snprintf(buf, sizeof(buf), "%.1f/%.0f G", vramUsed, vramTotal);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 40, buf);
  if (gpuTemp >= GPU_TEMP_ALERT) {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(MARGIN, DISP_H - 4, "ALERT");
  }
}

// Memory: RAM used/total + top 2 processes by RAM
void drawMemory() {
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "RAM");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN, 26);
  u8g2.print(ramUsed, 1);
  u8g2.print(" / ");
  u8g2.print((int)ramTotal);
  u8g2.print(" G  ");
  u8g2.print(ramPct);
  u8g2.print("%");

  u8g2.setFont(FONT_SMALL);
  for (int i = 0; i < 2; i++) {
    int y = 38 + i * 12;
    if (procRamNames[i].length() > 0) {
      u8g2.setCursor(MARGIN, y);
      u8g2.print(procRamNames[i].substring(0, 16));
      u8g2.setCursor(DISP_W - MARGIN - 24, y);
      u8g2.print(procRamMb[i]);
      u8g2.print("M");
    }
  }
}

// Power: Vcore, CPU/GPU на весь экран
void drawPower() {
  u8g2.drawRFrame(MARGIN, MARGIN, DISP_W - 2 * MARGIN, DISP_H - 2 * MARGIN, 3);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN + 4, 18, "Vcore");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN + 4, 34);
  u8g2.print(vcore, 2);
  u8g2.print(" V");
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN + 4, 50, "CPU / GPU");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN + 4, DISP_H - 8);
  u8g2.print(cpuPwr);
  u8g2.print(" W  /  ");
  u8g2.print(gpuPwr);
  u8g2.print(" W");
}

// Fans: one horizontal row (Pump Rad Case GPU)
void drawFans() {
  u8g2.setFont(FONT_SMALL);
  const int slotW = DISP_W / 4;
  const int y = DISP_H / 2 - 4;
  char buf[8];
  snprintf(buf, sizeof(buf), "P%d", fanPump);
  u8g2.drawStr(MARGIN, y, buf);
  snprintf(buf, sizeof(buf), "R%d", fanRad);
  u8g2.drawStr(MARGIN + slotW, y, buf);
  snprintf(buf, sizeof(buf), "C%d", fanCase);
  u8g2.drawStr(MARGIN + 2 * slotW, y, buf);
  snprintf(buf, sizeof(buf), "G%d", fanGpu);
  u8g2.drawStr(MARGIN + 3 * slotW, y, buf);
}

// Выбор иконки по коду погоды
static const uint8_t *weatherIconFromCode(int code) {
  if (code == 0)
    return iconSunny;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82) ||
      (code >= 95 && code <= 99))
    return iconRain;
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86))
    return iconSnow;
  return iconCloudy;
}

// Weather: NOW / DAY / +2 days (larger temp and desc on NOW/DAY)
void drawWeather() {
  u8g2.setFont(FONT_SMALL);
  const char *titles[] = {"NOW", "DAY", "+2d"};
  u8g2.setCursor(DISP_W - 24, MARGIN + 6);
  u8g2.print(titles[weatherPage]);

  if (weatherPage == 0) {
    bool noData = (weatherTemp == 0 && weatherDesc.length() == 0);
    const uint8_t *icon =
        noData ? iconCloudy : weatherIconFromCode(weatherIcon);
    int iconY = 8;
    for (int dy = 0; dy < 16; dy++)
      for (int dx = 0; dx < 16; dx++) {
        int byteIdx = dy * 2 + (dx / 8);
        uint8_t b = icon[byteIdx];
        if (b & (1 << (7 - (dx % 8)))) {
          u8g2.drawPixel(MARGIN + dx * 2, iconY + dy * 2);
          u8g2.drawPixel(MARGIN + dx * 2 + 1, iconY + dy * 2);
          u8g2.drawPixel(MARGIN + dx * 2, iconY + dy * 2 + 1);
          u8g2.drawPixel(MARGIN + dx * 2 + 1, iconY + dy * 2 + 1);
        }
      }
    u8g2.setFont(FONT_TITLE);
    u8g2.setCursor(40, iconY + 14);
    u8g2.print(noData ? "--" : String(weatherTemp));
    u8g2.print((char)0xB0);
    u8g2.setFont(isOnlyAscii(weatherDesc) ? FONT_MAIN : FONT_MEDIA);
    u8g2.setCursor(40, iconY + 30);
    u8g2.print(noData ? "..." : weatherDesc.substring(0, 14));
    return;
  }

  if (weatherPage == 1) {
    const uint8_t *icon = weatherIconFromCode(weatherDayCode);
    u8g2.drawXBMP(MARGIN, 10, 16, 16, icon);
    u8g2.setFont(FONT_TITLE);
    u8g2.setCursor(24, 22);
    u8g2.print("Lo ");
    u8g2.print(weatherDayLow);
    u8g2.print((char)0xB0);
    u8g2.setCursor(24, 40);
    u8g2.print("Hi ");
    u8g2.print(weatherDayHigh);
    u8g2.print((char)0xB0);
    u8g2.setFont(FONT_MAIN);
    u8g2.setCursor(24, DISP_H - 6);
    u8g2.print("Today");
    return;
  }

  // +2 days: tomorrow and day after (indices 1, 2)
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "Tmrw");
  u8g2.drawStr(DISP_W / 2 + MARGIN, 12, "+2");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN, 28);
  u8g2.print(weatherWeekHigh[1]);
  u8g2.print("/");
  u8g2.print(weatherWeekLow[1]);
  u8g2.print((char)0xB0);
  u8g2.setCursor(DISP_W / 2 + MARGIN, 28);
  u8g2.print(weatherWeekHigh[2]);
  u8g2.print("/");
  u8g2.print(weatherWeekLow[2]);
  u8g2.print((char)0xB0);
}

// Top 3 CPU processes: title + rows, aligned %
void drawTopProcs() {
  const int rowH = 16;
  bool any = false;
  for (int i = 0; i < 3; i++) {
    int y = 10 + i * rowH;
    if (procNames[i].length() > 0) {
      u8g2.setFont(FONT_MAIN);
      u8g2.setCursor(MARGIN, y);
      u8g2.print(procNames[i].substring(0, 14));
      u8g2.setCursor(DISP_W - MARGIN - 24, y);
      u8g2.print(procCpu[i]);
      u8g2.print("%");
      any = true;
    }
  }
  if (!any) {
    u8g2.setFont(FONT_MAIN);
    u8g2.setCursor(MARGIN, DISP_H / 2 - 6);
    u8g2.print("No data");
  }
}

// Network: Up/Dn на весь экран
void drawNetwork() {
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 22, "Up");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN, 38);
  u8g2.print(netUp);
  u8g2.print(" KB/s");
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 54, "Dn");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN, DISP_H - 6);
  u8g2.print(netDown);
  u8g2.print(" KB/s");
}

// Disks: % use and temp per row, no header
void drawDisks() {
  const int rowH = (DISP_H - 2 * MARGIN) / 4;
  u8g2.setFont(FONT_MAIN);
  char buf[24];
  for (int i = 0; i < 4; i++) {
    int y = 10 + i * rowH;
    int pct = (diskTotal[i] > 0)
                  ? (int)(100.0f * diskUsed[i] / diskTotal[i] + 0.5f)
                  : 0;
    if (pct > 100)
      pct = 100;
    snprintf(buf, sizeof(buf), "%d%%  %d%cC", pct, diskTemp[i], (char)0xB0);
    u8g2.setCursor(MARGIN, y);
    u8g2.print(buf);
  }
}

// Equalizer Bars — на весь экран
void drawEqBars() {
  const int barTopY = 0;
  const int barZoneH = DISP_H;
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

// Equalizer Wave — на весь экран
void drawEqWave() {
  int prevY = DISP_H / 2;
  int prevX = 0;
  for (int x = 0; x < DISP_W; x += 2) {
    int idx = (x * EQ_BARS) / DISP_W;
    if (idx >= EQ_BARS)
      idx = EQ_BARS - 1;
    float amp = eqHeights[idx] / 4.0;
    int y = DISP_H / 2 + (int)(sin(x * 0.1) * amp);
    if (y < 0)
      y = 0;
    if (y >= DISP_H)
      y = DISP_H - 1;
    if (x > 0)
      u8g2.drawLine(prevX, prevY, x, y);
    prevY = y;
    prevX = x;
  }
}

// Equalizer Circle — по центру экрана
void drawEqCircle() {
  int cx = DISP_W / 2;
  int cy = DISP_H / 2;
  for (int i = 0; i < EQ_BARS; i++) {
    float angle = (i * 22.5) * PI / 180.0;
    int len = 8 + eqHeights[i] / 2;
    int x = cx + (int)(cos(angle) * len);
    int y = cy + (int)(sin(angle) * len);
    if (x < 0)
      x = 0;
    if (x >= DISP_W)
      x = DISP_W - 1;
    if (y < 0)
      y = 0;
    if (y >= DISP_H)
      y = DISP_H - 1;
    u8g2.drawLine(cx, cy, x, y);
  }
}

// Equalizer: на весь экран
void drawEqualizer() {
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
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(DISP_W - 28, MARGIN + 6);
  u8g2.print(isPlaying ? "PLAY" : "---");
}

// Player: обложка слева по центру по вертикали, текст справа, на весь экран
void drawPlayer() {
  const int xText = COVER_SIZE + MARGIN + 2;
  const int coverY = (DISP_H - COVER_SIZE) / 2;
  if (hasCover) {
    u8g2.drawXBM(MARGIN, coverY, COVER_SIZE, COVER_SIZE, coverBitmap);
  } else {
    u8g2.drawRFrame(MARGIN, coverY, COVER_SIZE, COVER_SIZE, 4);
    u8g2.drawLine(MARGIN, coverY, MARGIN + COVER_SIZE, coverY + COVER_SIZE);
    u8g2.drawLine(MARGIN + COVER_SIZE, coverY, MARGIN, coverY + COVER_SIZE);
  }
  u8g2.setFont(isOnlyAscii(artist) ? FONT_SMALL : FONT_MEDIA);
  const int maxArtistChars = 16;
  const int maxTrackChars = 18;
  int y1 = 8, y2 = 20;
  if (artist.length() <= (unsigned)maxArtistChars) {
    u8g2.setCursor(xText, y1);
    u8g2.print(artist.substring(0, maxArtistChars));
  } else {
    String ext = artist + "   ";
    unsigned int len = ext.length();
    unsigned int off = (millis() / 400) % len;
    u8g2.setCursor(xText, y1);
    for (int i = 0; i < maxArtistChars; i++)
      u8g2.print(ext.charAt((off + i) % len));
  }
  u8g2.setFont(isOnlyAscii(track) ? FONT_SMALL : FONT_MEDIA);
  if (track.length() <= (unsigned)maxTrackChars) {
    u8g2.setCursor(xText, y2);
    u8g2.print(track.substring(0, maxTrackChars));
  } else {
    String ext = track + "   ";
    unsigned int len = ext.length();
    unsigned int off = (millis() / 300) % len;
    u8g2.setCursor(xText, y2);
    for (int i = 0; i < maxTrackChars; i++)
      u8g2.print(ext.charAt((off + i) % len));
  }
}

// Menu: scroll, 4 visible rows, rowH 9
const int MENU_VISIBLE = 4;
const int MENU_ROW_H = 9;
void drawMenu() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, DISP_W, DISP_H);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(MARGIN, MARGIN, DISP_W - 2 * MARGIN, DISP_H - 2 * MARGIN, 3);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr((DISP_W - 48) / 2, 11, "Settings");
  u8g2.drawLine(MARGIN + 4, 15, DISP_W - MARGIN - 4, 15);

  int offset = 0;
  if (menuItem >= MENU_VISIBLE)
    offset = menuItem - MENU_VISIBLE + 1;
  if (menuItem < MENU_VISIBLE)
    offset = 0;
  const int x0 = MARGIN + 4;
  u8g2.setFont(FONT_MAIN);
  for (int v = 0; v < MENU_VISIBLE; v++) {
    int i = offset + v;
    if (i >= MENU_ITEMS)
      break;
    int y = 18 + v * MENU_ROW_H;
    u8g2.setCursor(x0, y);
    u8g2.print(i == menuItem ? ">" : " ");
    if (i == 0) {
      u8g2.print("LED ");
      u8g2.print(ledEnabled ? "ON" : "OFF");
    } else if (i == 1) {
      u8g2.print("AUTO ");
      u8g2.print(carouselEnabled ? "ON" : "OFF");
    } else if (i == 2) {
      u8g2.print("Bright ");
      u8g2.print(displayContrast);
    } else if (i == 3) {
      u8g2.print("Intvl ");
      u8g2.print(carouselIntervalSec);
      u8g2.print("s");
    } else if (i == 4) {
      u8g2.print("EQ ");
      u8g2.print(eqStyle == 0 ? "Bars" : (eqStyle == 1 ? "Wave" : "Circ"));
    } else if (i == 5) {
      u8g2.print("Disp ");
      u8g2.print(displayInverted ? "Inv" : "Norm");
    } else {
      u8g2.print("EXIT");
    }
  }
}

// Forward: drawScreen определена ниже
void drawScreen(int screen);

void drawScreen(int screen) {
  u8g2.setFont(FONT_MAIN);
  switch (screen) {
  case 0:
    drawMain();
    break;
  case 1:
    drawCores();
    break;
  case 2:
    drawCpuOnly();
    break;
  case 3:
    drawGpu();
    break;
  case 4:
    drawMemory();
    break;
  case 5:
    drawDisks();
    break;
  case 6:
    drawPlayer();
    break;
  case 7:
    drawFans();
    break;
  case 8:
    drawWeather();
    break;
  case 9:
    drawTopProcs();
    break;
  default:
    drawMain();
    break;
  }
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
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, input);
    if (!error && !doc.overflowed()) {
      bool isHeartbeat = input.length() < 350 && input.indexOf("cover_b64") < 0;

      cpuTemp = doc["ct"];
      gpuTemp = doc["gt"];
      cpuLoad = doc["cpu_load"];
      gpuLoad = doc["gpu_load"];
      isPlaying = (doc["play"] | 0) == 1;
      lastUpdate = now;

      if (doc["wt"].is<int>()) {
        weatherTemp = doc["wt"];
        const char *wd = doc["wd"];
        if (wd)
          weatherDesc = String(wd);
        if (doc["wi"].is<int>())
          weatherIcon = doc["wi"];
      }
      if (doc["art"].is<const char *>()) {
        const char *a = doc["art"];
        artist = String(a ? a : "");
      }
      if (doc["trk"].is<const char *>()) {
        const char *t = doc["trk"];
        track = String(t ? t : "");
      }

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
        coreLoad[0] = doc["cl1"] | 0;
        coreLoad[1] = doc["cl2"] | 0;
        coreLoad[2] = doc["cl3"] | 0;
        coreLoad[3] = doc["cl4"] | 0;
        coreLoad[4] = doc["cl5"] | 0;
        coreLoad[5] = doc["cl6"] | 0;

        ramUsed = doc["ru"];
        ramTotal = doc["rt"];
        ramPct = doc["rp"];
        vramUsed = doc["vu"];
        vramTotal = doc["vt_tot"];
        vcore = doc["vcore"];
        nvme2Temp = doc["nvme2_t"];
        chipsetTemp = doc["chipset_t"];
        JsonArray dt = doc["disk_t"];
        for (int i = 0; i < 4; i++) {
          diskTemp[i] = (i < dt.size()) ? (int)dt[i] : 0;
        }
        JsonArray du = doc["disk_used"];
        JsonArray dtotal = doc["disk_total"];
        for (int i = 0; i < 4; i++) {
          diskUsed[i] = (i < du.size()) ? (float)du[i].as<double>() : 0.0f;
          diskTotal[i] =
              (i < dtotal.size()) ? (float)dtotal[i].as<double>() : 0.0f;
        }

        netUp = doc["nu"];
        netDown = doc["nd"];
        diskRead = doc["dr"];
        diskWrite = doc["dw"];

        weatherTemp = doc["wt"];
        const char *wd = doc["wd"];
        weatherDesc = String(wd ? wd : "");
        weatherIcon = doc["wi"];
        weatherDayHigh = doc["wday_high"] | 0;
        weatherDayLow = doc["wday_low"] | 0;
        weatherDayCode = doc["wday_code"] | 0;
        JsonArray wh = doc["week_high"];
        JsonArray wl = doc["week_low"];
        JsonArray wc = doc["week_code"];
        for (int i = 0; i < 7; i++) {
          weatherWeekHigh[i] = (i < wh.size()) ? (int)wh[i] : 0;
          weatherWeekLow[i] = (i < wl.size()) ? (int)wl[i] : 0;
          weatherWeekCode[i] = (i < wc.size()) ? (int)wc[i] : 0;
        }

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
        JsonArray tpRam = doc["tp_ram"];
        for (int i = 0; i < 2; i++) {
          if (i < tpRam.size()) {
            const char *n = tpRam[i]["n"];
            procRamNames[i] = String(n ? n : "");
            procRamMb[i] = tpRam[i]["r"] | 0;
          } else {
            procRamNames[i] = "";
            procRamMb[i] = 0;
          }
        }

        const char *art = doc["art"];
        const char *trk = doc["trk"];
        artist = String(art ? art : "");
        track = String(trk ? trk : "");

        const char *cov = doc["cover_b64"];
        if (cov) {
          size_t covLen = strlen(cov);
          if (covLen > 0 && covLen <= 600) {
            int n = b64Decode(cov, covLen, coverBitmap, COVER_BYTES);
            if (n == (int)COVER_BYTES)
              hasCover = true;
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
    unsigned long duration = now - btnPressTime;
    btnHeld = false;

    if (duration > 800) {
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
        if (menuItem >= MENU_ITEMS)
          menuItem = 0;
      } else {
        if (currentScreen == 8) {
          weatherPage++;
          if (weatherPage >= 3) {
            weatherPage = 0;
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

  // Logic inside menu for "Selection" (Wait... one button?)
  // One button dilemma: Short=Next, Long=Select?
  // Let's change logic:
  // IN MENU: Short = Scroll, Long = Toggle/Select

  if (inMenu && btnState == LOW && (now - btnPressTime > 800) &&
      !menuHoldHandled) {
    menuHoldHandled = true;
    lastMenuActivity = now;
    Preferences p;
    if (menuItem == 0) {
      ledEnabled = !ledEnabled;
      p.begin("heltec", false);
      p.putBool("led", ledEnabled);
      p.end();
    }
    if (menuItem == 1) {
      carouselEnabled = !carouselEnabled;
      p.begin("heltec", false);
      p.putBool("carousel", carouselEnabled);
      p.end();
    }
    if (menuItem == 2) {
      if (displayContrast <= 128)
        displayContrast = 192;
      else if (displayContrast <= 192)
        displayContrast = 255;
      else
        displayContrast = 128;
      u8g2.setContrast((uint8_t)displayContrast);
      p.begin("heltec", false);
      p.putInt("contrast", displayContrast);
      p.end();
    }
    if (menuItem == 3) {
      if (carouselIntervalSec == 5)
        carouselIntervalSec = 10;
      else if (carouselIntervalSec == 10)
        carouselIntervalSec = 15;
      else
        carouselIntervalSec = 5;
      p.begin("heltec", false);
      p.putInt("carouselSec", carouselIntervalSec);
      p.end();
    }
    if (menuItem == 4) {
      eqStyle = (eqStyle + 1) % 3;
      p.begin("heltec", false);
      p.putInt("eqStyle", eqStyle);
      p.end();
    }
    if (menuItem == 5) {
      displayInverted = !displayInverted;
      u8g2.setFlipMode(displayInverted ? 1 : 0);
      p.begin("heltec", false);
      p.putBool("inverted", displayInverted);
      p.end();
    }
    if (menuItem == 6)
      inMenu = false;
  }

  // --- 3. MENU AUTO-CLOSE (5 min) ---
  if (inMenu && (now - lastMenuActivity > MENU_TIMEOUT_MS)) {
    inMenu = false;
  }

  // --- 4. CAROUSEL LOGIC ---
  if (carouselEnabled && !inMenu) {
    unsigned long intervalMs = (unsigned long)carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
      lastCarousel = now;
    }
  }

  // --- 4b. Отправить текущий экран на ПК (для полного пакета по экрану) ---
  if (tcpClient.connected() && lastSentScreen != currentScreen) {
    tcpClient.print("screen:");
    tcpClient.print(currentScreen);
    tcpClient.print("\n");
    lastSentScreen = currentScreen;
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
    if (now - splashStart >= SPLASH_MS)
      splashDone = true;
  }

  // --- 7. DRAWING ---
  u8g2.clearBuffer();
  bool signalLost = (now - lastUpdate > SIGNAL_TIMEOUT_MS);

  if (!splashDone) {
    drawSplash();
  } else if (signalLost) {
    drawScreen(currentScreen);
    u8g2.setFont(FONT_MAIN);
    u8g2.drawStr((DISP_W - 56) / 2, MARGIN + 10, "Reconnect");
  } else if (splashDone) {
    drawScreen(currentScreen);
    if (currentScreen != 1) {
      u8g2.setFont(FONT_SMALL);
      u8g2.setCursor(DISP_W - 10, MARGIN + 6);
      u8g2.print("W");
    }

    if (anyAlarm) {
      if (blinkState)
        u8g2.drawFrame(0, 0, DISP_W, DISP_H);
      u8g2.setCursor(DISP_W - 38, MARGIN + 10);
      u8g2.print("ALERT");
    }

    if (inMenu)
      drawMenu();

    if (!inMenu) {
      for (int i = 0; i < TOTAL_SCREENS; i++) {
        int x = DISP_W / 2 - (TOTAL_SCREENS * 3) + (i * 6);
        if (i == currentScreen)
          u8g2.drawBox(x, DISP_H - 2, 2, 2);
        else
          u8g2.drawPixel(x + 1, DISP_H - 1);
      }
    }
  }

  u8g2.sendBuffer();
}