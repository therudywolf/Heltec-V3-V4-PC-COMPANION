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

// --- FONTS (Noir-style typography) ---
#define FONT_HEADER u8g2_font_helvB08_tr
#define FONT_DATA u8g2_font_6x10_tr
#define FONT_SMALL u8g2_font_5x7_tr
#define FONT_MEDIA u8g2_font_cu12_t_cyrillic

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
int weatherPage = 0; // 0=Now, 1=Day, 2=Week
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
int carouselIntervalSec = 10; // Интервал карусели: 5, 10 или 15 сек
int displayContrast = 255;    // Контраст дисплея 0–255
int eqStyle = 0;              // Стиль эквалайзера: 0=Bars, 1=Wave, 2=Circle
bool displayInverted = false; // Ориентация: 0=обычный, 1=инвертированный (180°)

int currentScreen = 0;
// 0=Main 1=Cores 2=GPU 3=Memory 4=Player 5=EQ 6=Power 7=Fans 8=Weather
// 9=TopProcs 10=Network 11=Disks 12=Game
const int TOTAL_SCREENS = 13;
const int GAME_SCREEN_INDEX = 12;

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
bool wasInGameOnPress = false;      // были на экране игры (long press = выход)
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

// Splash
bool splashDone = false;
unsigned long splashStart = 0;
int splashPhase = 0; // 0 = Forest OS, 1 = By RudyWolf, 2 = глитч перед выходом
const unsigned long SPLASH_PHASE0_MS = 3200;
const unsigned long SPLASH_PHASE1_MS = 3200;
const unsigned long SPLASH_GLITCH_MS = 120; // 1–2 кадра шума
const unsigned long SPLASH_FRAME_MS = 180;
unsigned long lastSplashFrame = 0;
int splashFrame = 0;

// WiFi source
WiFiClient tcpClient;
unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TRY_MS = 8000;
String tcpLineBuffer;    // накопление строки до \n
int lastSentScreen = -1; // для отправки screen:N при смене экрана

// Киберпанк-глитч при смене экрана (3–6 кадров)
int transitionGlitchPhase = 0; // 0 = нет, 1..5 = кадры глитча
// Game Over: 2–3 кадра глитча перед "GAME OVER"
int gameOverGlitchPhase = 0; // 0 = показывать GAME OVER, 1..3 = глитч
// Индикатор экранов: мерцание текущей точки каждые 1–2 с
unsigned long lastDotBlink = 0;
bool dotBlinkOn = true;

// Wolf Runner (screen GAME_SCREEN_INDEX): волк прыгает через препятствия
const int GAME_WOLF_X = 20;
const int GAME_WOLF_W = 14, GAME_WOLF_H = 10;
const int GAME_GROUND_Y = 52;
const int GAME_JUMP_VY = -10;
const int GAME_GRAVITY = 1;
const int GAME_OBSTACLE_W = 8, GAME_OBSTACLE_H = 14;
const int GAME_OBSTACLES = 3;
const int GAME_SPEED = 2;
const unsigned long GAME_TICK_MS = 40;
const int GAME_DURATION_MS = 60000;

int gameWolfY = GAME_GROUND_Y;
int gameWolfVy = 0;
int gameObstacleX[3] = {128, 180, 230};
int gameScore = 0;
bool gameOver = false;
unsigned long gameStartTime = 0;
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

// Apple-style header: centered title, thin separator line
const int HEADER_H = 10;
void drawHeader(const char *title) {
  u8g2.setDrawColor(1);
  u8g2.setFont(FONT_HEADER);
  int w = u8g2.getStrWidth(title);
  u8g2.drawStr((128 - w) / 2, 9, title);
  u8g2.drawLine(4, 11, 124, 11);
}

// --- SCREENS ---

// --- SPLASH: Forest OS, затем By RudyWolf + иконка волка, затем 1–2 кадра шума
// ---
void drawSplash() {
  if (splashPhase == 0) {
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.drawStr(28, 30, "Forest");
    u8g2.drawStr(52, 50, "OS");
  } else if (splashPhase == 1) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(34, 12, "By RudyWolf");
    u8g2.drawXBMP(52, 22, 24, 24, wolfFace);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(98, 8, "ONLINE");
  } else {
    for (int i = 0; i < 80; i++)
      u8g2.drawPixel(random(0, 128), random(0, 64));
    u8g2.setDrawColor(0);
    for (int y = 0; y < 64; y += 4)
      u8g2.drawBox(0, y, 128, 1);
    u8g2.setDrawColor(1);
  }
}

// --- SCREEN ZONES: header y=0..HEADER_H, content y>=HEADER_H+2 ---
// 1. Main: Noir layout — label | bar | value per row + footer alert
void drawMain() {
  drawHeader("Dashboard");
  u8g2.setFont(FONT_DATA);
  char buf[24];

  // CPU row: label | bar | temp
  u8g2.drawStr(4, 24, "CPU");
  snprintf(buf, sizeof(buf), "%d%cC", cpuTemp, (char)0xB0);
  u8g2.drawStr(124 - u8g2.getStrWidth(buf), 24, buf);
  drawProgressBar(30, 16, 60, 9, (float)cpuLoad);

  // GPU row
  u8g2.drawStr(4, 38, "GPU");
  snprintf(buf, sizeof(buf), "%d%cC", gpuTemp, (char)0xB0);
  u8g2.drawStr(124 - u8g2.getStrWidth(buf), 38, buf);
  drawProgressBar(30, 30, 60, 9, (float)gpuLoad);

  // RAM row
  u8g2.drawStr(4, 52, "RAM");
  snprintf(buf, sizeof(buf), "%.1fG", ramUsed);
  u8g2.drawStr(124 - u8g2.getStrWidth(buf), 52, buf);
  drawProgressBar(30, 44, 60, 9, (float)ramPct);

  // Footer alert
  bool alert = (cpuTemp >= CPU_TEMP_ALERT) || (gpuTemp >= GPU_TEMP_ALERT) ||
               (cpuLoad >= CPU_LOAD_ALERT) || (gpuLoad >= GPU_LOAD_ALERT);
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(4, 62);
  if (alert && blinkState)
    u8g2.print("Warning: High Temp");
  else
    u8g2.print("System Normal");
}

// Cores: per-core load 0–100% (rounded bars, labels 1–6)
void drawCores() {
  drawHeader("CPU Cores");
  const int barTop = HEADER_H + 4;
  const int barH = 40;
  const int barW = 16;
  const int gap = 4;
  const int startX = 6;
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
    u8g2.setCursor(x + 5, 62);
    u8g2.print(i + 1);
  }
}

// GPU: left column data + right circular load gauge
void drawGpu() {
  drawHeader("GPU Statistics");
  if (gpuTemp >= GPU_TEMP_ALERT) {
    u8g2.setDrawColor(0);
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(96, 7, "ALERT");
    u8g2.setDrawColor(1);
  }
  u8g2.setFont(FONT_DATA);
  char buf[32];
  u8g2.setCursor(4, 25);
  u8g2.print("Clock:");
  snprintf(buf, sizeof(buf), "%d MHz", gpuClk);
  u8g2.setCursor(4, 35);
  u8g2.print(buf);
  u8g2.setCursor(4, 50);
  u8g2.print("Power:");
  snprintf(buf, sizeof(buf), "%d W", gpuPwr);
  u8g2.setCursor(4, 60);
  u8g2.print(buf);

  // Circular load gauge (right side)
  int cx = 90, cy = 40, r = 18;
  u8g2.drawCircle(cx, cy, r);
  float angle = (gpuLoad / 100.0f) * 6.28318f - 1.5708f; // 2*PI - PI/2
  int lx = cx + (int)(cos(angle) * (r - 2));
  int ly = cy + (int)(sin(angle) * (r - 2));
  u8g2.drawLine(cx, cy, lx, ly);
  u8g2.setFont(FONT_HEADER);
  u8g2.setCursor(cx - 6, cy + 4);
  u8g2.print(gpuLoad);
}

// Memory: RAM и VRAM с барами
void drawMemory() {
  drawHeader("MEMORY");
  u8g2.setFont(FONT_DATA);
  u8g2.setCursor(0, HEADER_H + 4);
  u8g2.print("RAM ");
  u8g2.print(ramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)ramTotal);
  u8g2.print("G ");
  u8g2.print(ramPct);
  u8g2.print("%");
  drawBar(0, HEADER_H + 10, 128, 6, ramUsed, ramTotal > 0 ? ramTotal : 1);
  u8g2.setCursor(0, HEADER_H + 24);
  u8g2.print("VRAM ");
  u8g2.print(vramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)vramTotal);
  u8g2.print("G");
  drawBar(0, HEADER_H + 30, 128, 6, vramUsed, vramTotal > 0 ? vramTotal : 1);
}

// Power: только питание (Vcore, CPU W, GPU W)
void drawPower() {
  drawHeader("POWER");
  u8g2.drawRFrame(0, HEADER_H + 2, 128, 38, 3);
  u8g2.setFont(FONT_DATA);
  u8g2.setCursor(0, HEADER_H + 6);
  u8g2.print("Vcore ");
  u8g2.print(vcore, 2);
  u8g2.print(" V");
  u8g2.setCursor(0, HEADER_H + 18);
  u8g2.print("CPU ");
  u8g2.print(cpuPwr);
  u8g2.print(" W   GPU ");
  u8g2.print(gpuPwr);
  u8g2.print(" W");
}

// Fans: Pump, Rad, Case, GPU
void drawFans() {
  drawHeader("FANS");
  u8g2.setFont(FONT_DATA);
  u8g2.setCursor(0, HEADER_H + 6);
  u8g2.print("Pump: ");
  u8g2.print(fanPump);
  u8g2.setCursor(0, HEADER_H + 18);
  u8g2.print("Rad:  ");
  u8g2.print(fanRad);
  u8g2.setCursor(0, HEADER_H + 30);
  u8g2.print("Case: ");
  u8g2.print(fanCase);
  u8g2.setCursor(0, HEADER_H + 42);
  u8g2.print("GPU:  ");
  u8g2.print(fanGpu);
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

// Weather: 3 зоны — NOW / DAY / WEEK, чистая разметка, без наезда на шапку
void drawWeather() {
  drawHeader("WEATHER");
  u8g2.setFont(FONT_SMALL);
  const char *titles[] = {"NOW", "DAY", "WEEK"};
  u8g2.setCursor(92, 7);
  u8g2.print(titles[weatherPage]);

  if (weatherPage == 0) {
    bool noData = (weatherTemp == 0 && weatherDesc.length() == 0);
    const uint8_t *icon =
        noData ? iconCloudy : weatherIconFromCode(weatherIcon);
    int iconY = HEADER_H + 3;
    for (int dy = 0; dy < 16; dy++)
      for (int dx = 0; dx < 16; dx++) {
        int byteIdx = dy * 2 + (dx / 8);
        uint8_t b = icon[byteIdx];
        if (b & (1 << (7 - (dx % 8)))) {
          u8g2.drawPixel(4 + dx * 2, iconY + dy * 2);
          u8g2.drawPixel(5 + dx * 2, iconY + dy * 2);
          u8g2.drawPixel(4 + dx * 2, iconY + dy * 2 + 1);
          u8g2.drawPixel(5 + dx * 2, iconY + dy * 2 + 1);
        }
      }
    u8g2.setFont(FONT_HEADER);
    u8g2.setCursor(40, iconY + 12);
    u8g2.print(noData ? "--" : String(weatherTemp));
    u8g2.print((char)0xB0);
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(40, iconY + 26);
    u8g2.print(noData ? "..." : weatherDesc.substring(0, 14));
    return;
  }

  if (weatherPage == 1) {
    const uint8_t *icon = weatherIconFromCode(weatherDayCode);
    u8g2.drawXBMP(2, HEADER_H + 3, 16, 16, icon);
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(22, HEADER_H + 10);
    u8g2.print("Lo ");
    u8g2.print(weatherDayLow);
    u8g2.print((char)0xB0);
    u8g2.setCursor(22, HEADER_H + 20);
    u8g2.print("Hi ");
    u8g2.print(weatherDayHigh);
    u8g2.print((char)0xB0);
    u8g2.setCursor(22, HEADER_H + 32);
    u8g2.print("Today");
    return;
  }

  u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(0, HEADER_H + 5, "Hi");
  u8g2.drawStr(0, HEADER_H + 14, "Lo");
  for (int i = 0; i < 7; i++) {
    int x = 14 + i * 16;
    u8g2.setCursor(x, HEADER_H + 5);
    u8g2.print(weatherWeekHigh[i]);
    u8g2.setCursor(x, HEADER_H + 14);
    u8g2.print(weatherWeekLow[i]);
  }
  u8g2.drawLine(0, HEADER_H + 22, 128, HEADER_H + 22);
  for (int i = 0; i < 7; i++) {
    u8g2.setCursor(16 + i * 16, HEADER_H + 30);
    u8g2.print(i + 1);
  }
}

// Top 3 processes by CPU
void drawTopProcs() {
  drawHeader("TOP 3 CPU");
  u8g2.setFont(FONT_DATA);
  bool any = false;
  for (int i = 0; i < 3; i++) {
    int y = HEADER_H + 5 + i * 14;
    u8g2.setCursor(0, y);
    if (procNames[i].length() > 0) {
      u8g2.print(procNames[i].substring(0, 14));
      u8g2.setCursor(88, y);
      u8g2.print(procCpu[i]);
      u8g2.print("%");
      any = true;
    }
  }
  if (!any) {
    u8g2.setCursor(0, HEADER_H + 24);
    u8g2.print("No data");
  }
}

// Network only (Up/Dn KB/s)
void drawNetwork() {
  drawHeader("NET");
  u8g2.setFont(FONT_DATA);
  u8g2.setCursor(0, HEADER_H + 8);
  u8g2.print("Up ");
  u8g2.print(netUp);
  u8g2.print(" KB/s");
  u8g2.setCursor(0, HEADER_H + 22);
  u8g2.print("Dn ");
  u8g2.print(netDown);
  u8g2.print(" KB/s");
}

// Disks: D1..D4 — темп °C, занято/всего GB
void drawDisks() {
  drawHeader("DISKS");
  u8g2.drawRFrame(0, HEADER_H + 2, 128, 54, 3);
  u8g2.setFont(FONT_SMALL);
  for (int i = 0; i < 4; i++) {
    int y = HEADER_H + 4 + i * 14;
    u8g2.setCursor(0, y);
    u8g2.print("D");
    u8g2.print(i + 1);
    u8g2.print(" ");
    u8g2.print(diskTemp[i]);
    u8g2.print((char)0xB0);
    u8g2.print(" ");
    int usedG = (int)(diskUsed[i] + 0.5f);
    int totalG = (int)(diskTotal[i] + 0.5f);
    u8g2.print(usedG);
    u8g2.print("/");
    u8g2.print(totalG);
    u8g2.print("G");
  }
}

// Wolf Runner: волк, препятствия, счёт
void drawWolfGame() {
  drawHeader("Wolf Run");
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(70, 7);
  u8g2.print(gameScore);
  if (gameOver) {
    if (gameOverGlitchPhase > 0) {
      u8g2.setDrawColor(0);
      for (int y = 0; y < 64; y += 2)
        u8g2.drawBox(0, y, 128, 1);
      u8g2.setDrawColor(1);
      for (int i = 0; i < 40; i++)
        u8g2.drawPixel(random(0, 128), random(0, 64));
      gameOverGlitchPhase--;
      return;
    }
    u8g2.setFont(FONT_HEADER);
    u8g2.setCursor(28, 36);
    u8g2.print("GAME OVER");
    u8g2.setCursor(36, 48);
    u8g2.print(gameScore);
    u8g2.print(" *w*");
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(0, 62);
    u8g2.print("Short=Again Long=Exit");
    return;
  }
  unsigned long left =
      (gameStartTime > 0 && (millis() - gameStartTime) < GAME_DURATION_MS)
          ? (GAME_DURATION_MS - (millis() - gameStartTime)) / 1000
          : 0;
  u8g2.setCursor(0, 62);
  u8g2.print(left);
  u8g2.print("s  Long=Exit");
  // Земля
  u8g2.drawLine(0, GAME_GROUND_Y + GAME_WOLF_H, 128,
                GAME_GROUND_Y + GAME_WOLF_H);
  // Волк (прямоугольник + уши)
  u8g2.drawFrame(GAME_WOLF_X, gameWolfY, GAME_WOLF_W, GAME_WOLF_H);
  u8g2.drawPixel(GAME_WOLF_X + 2, gameWolfY);
  u8g2.drawPixel(GAME_WOLF_X + GAME_WOLF_W - 2, gameWolfY);
  // Препятствия (камни)
  for (int i = 0; i < GAME_OBSTACLES; i++) {
    if (gameObstacleX[i] < 128)
      u8g2.drawBox(gameObstacleX[i],
                   GAME_GROUND_Y + GAME_WOLF_H - GAME_OBSTACLE_H,
                   GAME_OBSTACLE_W, GAME_OBSTACLE_H);
  }
}

// Equalizer Bars (столбики)
void drawEqBars() {
  const int barTopY = HEADER_H + 2;
  const int barZoneH = 64 - barTopY;
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
    if (y < HEADER_H + 2)
      y = HEADER_H + 2;
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
    if (y < HEADER_H + 2)
      y = HEADER_H + 2;
    if (y > 63)
      y = 63;
    u8g2.drawLine(cx, cy, x, y);
  }
}

// Equalizer: шапка + визуализация
void drawEqualizer() {
  drawHeader("Visualizer");
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(80, 7);
  u8g2.print(isPlaying ? "PLAY" : "---");
  u8g2.setDrawColor(1);

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

// Player: no header, cover left, artist/track right, play/pause icons + mini
// visualizer
void drawPlayer() {
  const int xText = COVER_SIZE + 6;
  if (hasCover) {
    u8g2.drawXBM(0, 8, COVER_SIZE, COVER_SIZE, coverBitmap);
  } else {
    u8g2.drawRFrame(0, 8, COVER_SIZE, COVER_SIZE, 4);
    u8g2.drawLine(0, 8, COVER_SIZE, 8 + COVER_SIZE);
    u8g2.drawLine(COVER_SIZE, 8, 0, 8 + COVER_SIZE);
  }
  u8g2.setFont(FONT_MEDIA);
  const int maxArtistChars = 12;
  const int maxTrackChars = 14;
  if (artist.length() <= (unsigned)maxArtistChars) {
    u8g2.setCursor(xText, 20);
    u8g2.print(artist);
  } else {
    String ext = artist + "   ";
    unsigned int len = ext.length();
    unsigned int off = (millis() / 400) % len;
    u8g2.setCursor(xText, 20);
    for (int i = 0; i < maxArtistChars; i++)
      u8g2.print(ext.charAt((off + i) % len));
  }
  u8g2.setFont(FONT_SMALL);
  if (track.length() <= (unsigned)maxTrackChars) {
    u8g2.setCursor(xText, 35);
    u8g2.print(track.substring(0, maxTrackChars));
  } else {
    String ext = track + "   ";
    unsigned int len = ext.length();
    unsigned int off = (millis() / 300) % len;
    u8g2.setCursor(xText, 35);
    for (int i = 0; i < maxTrackChars; i++)
      u8g2.print(ext.charAt((off + i) % len));
  }
  // Play/Pause icons + mini visualizer
  int iconY = 45;
  if (isPlaying) {
    u8g2.drawTriangle(xText, iconY, xText, iconY + 10, xText + 8, iconY + 5);
    if ((millis() % 200) > 100) {
      u8g2.drawBox(xText + 15, iconY + 3, 4, 6);
      u8g2.drawBox(xText + 21, iconY + 1, 4, 8);
      u8g2.drawBox(xText + 27, iconY + 4, 4, 5);
    }
  } else {
    u8g2.drawBox(xText, iconY, 3, 10);
    u8g2.drawBox(xText + 5, iconY, 3, 10);
  }
  // Progress bar
  u8g2.drawRFrame(xText, 56, 128 - xText - 2, 6, 2);
  if (isPlaying) {
    int barW = 128 - xText - 6;
    int prog = (millis() / 100) % (barW + 1);
    if (prog > 0)
      u8g2.drawBox(xText + 2, 58, prog, 2);
  }
}

void drawMenu() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(10, 10, 108, 44);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(10, 10, 108, 44, 4);
  u8g2.setFont(FONT_HEADER);
  u8g2.drawStr(45, 22, "Settings");
  u8g2.drawLine(15, 24, 113, 24);

  u8g2.setFont(FONT_DATA);
  const int rowH = 10;
  const int x0 = 12;
  for (int i = 0; i < 4; i++) {
    int idx = (menuItem + i) % MENU_ITEMS;
    int y = 22 + i * rowH;
    u8g2.setCursor(x0, y);
    u8g2.print(i == 0 ? ">" : " ");
    if (idx == 0) {
      u8g2.print("LED ");
      u8g2.print(ledEnabled ? "ON" : "OFF");
    } else if (idx == 1) {
      u8g2.print("AUTO ");
      u8g2.print(carouselEnabled ? "ON" : "OFF");
    } else if (idx == 2) {
      u8g2.print("Bright ");
      u8g2.print(displayContrast);
    } else if (idx == 3) {
      u8g2.print("Intvl ");
      u8g2.print(carouselIntervalSec);
      u8g2.print("s");
    } else if (idx == 4) {
      u8g2.print("EQ ");
      u8g2.print(eqStyle == 0 ? "Bars" : (eqStyle == 1 ? "Wave" : "Circ"));
    } else if (idx == 5) {
      u8g2.print("Disp ");
      u8g2.print(displayInverted ? "Inv" : "Norm");
    } else {
      u8g2.print("EXIT");
    }
  }
}

// Forward: drawScreen определена ниже
void drawScreen(int screen);

// Киберпанк-глитч при смене экрана: полосы/шум поверх текущего экрана
void drawGlitchTransition() {
  drawScreen(currentScreen);
  if (transitionGlitchPhase <= 0)
    return;
  // Чётные/нечётные строки — чёрные полосы (артефакт)
  if (transitionGlitchPhase >= 4) {
    u8g2.setDrawColor(0);
    for (int y = 0; y < 64; y += 2)
      u8g2.drawBox(0, y, 128, 1);
  } else if (transitionGlitchPhase >= 2) {
    u8g2.setDrawColor(0);
    for (int y = 1; y < 64; y += 2)
      u8g2.drawBox(0, y, 128, 1);
  }
  // Фаза 1 — без полос (мягкий выход)
}

void drawScreen(int screen) {
  switch (screen) {
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
    drawFans();
    break;
  case 8:
    drawWeather();
    break;
  case 9:
    drawTopProcs();
    break;
  case 10:
    drawNetwork();
    break;
  case 11:
    drawDisks();
    break;
  case 12:
    drawWolfGame();
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
    wasInGameOnPress = (currentScreen == GAME_SCREEN_INDEX);
  }

  if (btnState == HIGH && btnHeld) {
    unsigned long duration = now - btnPressTime;
    btnHeld = false;

    if (wasInGameOnPress && duration > 800) {
      currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
      lastCarousel = now;
      transitionGlitchPhase = 5;
    } else if (duration > 800) {
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
            transitionGlitchPhase = 5;
          }
        } else if (currentScreen == GAME_SCREEN_INDEX) {
          if (gameOver) {
            gameOver = false;
            gameScore = 0;
            gameWolfY = GAME_GROUND_Y;
            gameWolfVy = 0;
            gameObstacleX[0] = 128;
            gameObstacleX[1] = 170;
            gameObstacleX[2] = 220;
            gameStartTime = now;
          } else {
            gameWolfVy = GAME_JUMP_VY;
          }
        } else {
          currentScreen++;
          if (currentScreen >= TOTAL_SCREENS)
            currentScreen = 0;
          lastCarousel = now;
          transitionGlitchPhase = 5;
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
      int next = (currentScreen + 1) % TOTAL_SCREENS;
      if (next == GAME_SCREEN_INDEX)
        next = (next + 1) % TOTAL_SCREENS;
      currentScreen = next;
      lastCarousel = now;
      transitionGlitchPhase = 5;
    }
  }

  // --- 4b. Отправить текущий экран на ПК (для полного пакета по экрану) ---
  if (tcpClient.connected() && lastSentScreen != currentScreen) {
    tcpClient.print("screen:");
    tcpClient.print(currentScreen);
    tcpClient.print("\n");
    lastSentScreen = currentScreen;
    if (currentScreen == GAME_SCREEN_INDEX) {
      gameStartTime = now;
      gameScore = 0;
      gameOver = false;
      gameWolfY = GAME_GROUND_Y;
      gameWolfVy = 0;
      gameObstacleX[0] = 128;
      gameObstacleX[1] = 170;
      gameObstacleX[2] = 220;
    }
  }

  // --- 4c. Wolf Runner physics: волк прыгает, препятствия движутся влево
  if (currentScreen == GAME_SCREEN_INDEX && splashDone && !gameOver &&
      (now - lastGameTick >= GAME_TICK_MS)) {
    lastGameTick = now;
    if (gameStartTime == 0)
      gameStartTime = now;
    unsigned long gameElapsed = now - gameStartTime;
    if (gameElapsed >= GAME_DURATION_MS) {
      gameOver = true;
      gameOverGlitchPhase = 3;
    } else {
      gameWolfY += gameWolfVy;
      gameWolfVy += GAME_GRAVITY;
      if (gameWolfY >= GAME_GROUND_Y) {
        gameWolfY = GAME_GROUND_Y;
        gameWolfVy = 0;
      }
      if (gameWolfY < 18)
        gameWolfY = 18;

      for (int i = 0; i < GAME_OBSTACLES; i++) {
        gameObstacleX[i] -= GAME_SPEED;
        if (gameObstacleX[i] + GAME_OBSTACLE_W < 0) {
          gameObstacleX[i] = 128 + (i * 45) + (now % 30);
          gameScore++;
        }
        int oy = GAME_GROUND_Y + GAME_WOLF_H - GAME_OBSTACLE_H;
        if (gameObstacleX[i] < GAME_WOLF_X + GAME_WOLF_W &&
            gameObstacleX[i] + GAME_OBSTACLE_W > GAME_WOLF_X &&
            gameWolfY + GAME_WOLF_H > oy && gameWolfY < oy + GAME_OBSTACLE_H) {
          gameOver = true;
          gameOverGlitchPhase = 3;
        }
      }
    }
  }
  if (currentScreen == GAME_SCREEN_INDEX && gameOver &&
      (now - lastGameTick >= GAME_TICK_MS)) {
    lastGameTick = now;
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
      splashPhase = 2;
      splashStart = now;
    } else if (splashPhase == 2 && elapsed >= SPLASH_GLITCH_MS) {
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
    drawScreen(currentScreen);
    u8g2.setFont(FONT_DATA);
    u8g2.drawStr(70, 8, "Reconnect");
  } else if (splashDone) {
    if (transitionGlitchPhase > 0) {
      drawGlitchTransition();
      transitionGlitchPhase--;
    } else {
      drawScreen(currentScreen);
    }
    u8g2.setFont(FONT_DATA);
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
      if (now - lastDotBlink > 1500) {
        lastDotBlink = now;
        dotBlinkOn = !dotBlinkOn;
      }
      for (int i = 0; i < TOTAL_SCREENS; i++) {
        int x = 64 - (TOTAL_SCREENS * 3) + (i * 6);
        if (i == currentScreen) {
          if (dotBlinkOn)
            u8g2.drawBox(x, 62, 2, 2);
          else
            u8g2.drawFrame(x, 62, 2, 2);
        } else
          u8g2.drawPixel(x + 1, 63);
      }
    }
  }

  u8g2.sendBuffer();
}