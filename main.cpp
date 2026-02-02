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

// Wolf face 24x24 (game/small)
const uint8_t wolfFace[72] PROGMEM = {
    0x00, 0x18, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x18, 0x00,
    0x01, 0xFF, 0x80, 0x03, 0xFF, 0xC0, 0x07, 0xFF, 0xE0, 0x07, 0xC3, 0xE0,
    0x07, 0x81, 0xE0, 0x07, 0x81, 0xE0, 0x07, 0xC3, 0xE0, 0x03, 0xFF, 0xC0,
    0x01, 0xFF, 0x80, 0x00, 0x7E, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Wolf head 48x48 for splash (big icon on boot)
const uint8_t wolfIcon48[288] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xFF, 0xE0, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0,
    0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xF8, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFC,
    0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFE,
    0x00, 0x00, 0x7F, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E,
    0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E,
    0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7F, 0x00, 0x00, 0xFE,
    0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xF8,
    0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0,
    0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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

// --- SPLASH ---
void drawSplash() {
  if (splashPhase == 0) {
    u8g2.setFont(FONT_TITLE);
    const char *line1 = "Forest";
    const char *line2 = "OS";
    int w1 = u8g2.getStrWidth(line1);
    int w2 = u8g2.getStrWidth(line2);
    u8g2.drawStr((DISP_W - w1) / 2, 26, line1);
    u8g2.drawStr((DISP_W - w2) / 2, 48, line2);
  } else if (splashPhase == 1) {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(DISP_W - 32, 8, "Online");
    u8g2.drawXBMP((DISP_W - 48) / 2, 4, 48, 48, wolfIcon48);
    const char *credits = "Rudy Wolf";
    int cw = u8g2.getStrWidth(credits);
    u8g2.drawStr((DISP_W - cw) / 2, DISP_H - 4, credits);
  } else {
    for (int i = 0; i < 80; i++)
      u8g2.drawPixel(random(0, DISP_W), random(0, DISP_H));
    u8g2.setDrawColor(0);
    for (int y = 0; y < DISP_H; y += 4)
      u8g2.drawBox(0, y, DISP_W, 1);
    u8g2.setDrawColor(1);
  }
}

// 1. Main (Dashboard): три ряда на весь экран, бары на всю ширину
void drawMain() {
  const int rowH = DISP_H / 3;
  const int barY0 = 10, barH = 8;
  const int barX = MARGIN, barW = DISP_W - 2 * MARGIN;
  char buf[24];

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 9, "CPU");
  snprintf(buf, sizeof(buf), "%d%cC", cpuTemp, (char)0xB0);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 9, buf);
  drawProgressBar(barX, barY0, barW, barH, (float)cpuLoad);

  u8g2.drawStr(MARGIN, 9 + rowH, "GPU");
  snprintf(buf, sizeof(buf), "%d%cC", gpuTemp, (char)0xB0);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 9 + rowH, buf);
  drawProgressBar(barX, barY0 + rowH, barW, barH, (float)gpuLoad);

  u8g2.drawStr(MARGIN, 9 + 2 * rowH, "RAM");
  snprintf(buf, sizeof(buf), "%.1fG", ramUsed);
  u8g2.drawStr(DISP_W - MARGIN - u8g2.getStrWidth(buf), 9 + 2 * rowH, buf);
  drawProgressBar(barX, barY0 + 2 * rowH, barW, barH, (float)ramPct);

  bool alert = (cpuTemp >= CPU_TEMP_ALERT) || (gpuTemp >= GPU_TEMP_ALERT) ||
               (cpuLoad >= CPU_LOAD_ALERT) || (gpuLoad >= GPU_LOAD_ALERT);
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(MARGIN, DISP_H - 4);
  u8g2.print(alert && blinkState ? "Warn" : "OK");
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

// GPU: данные слева, круг нагрузки справа, на весь экран
void drawGpu() {
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "Clock");
  u8g2.setFont(FONT_MAIN);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d MHz", gpuClk);
  u8g2.drawStr(MARGIN, 26, buf);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 42, "Power");
  snprintf(buf, sizeof(buf), "%d W", gpuPwr);
  u8g2.setFont(FONT_MAIN);
  u8g2.drawStr(MARGIN, 56, buf);
  if (gpuTemp >= GPU_TEMP_ALERT) {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(MARGIN, DISP_H - 4, "ALERT");
  }

  int cx = DISP_W - 32;
  int cy = DISP_H / 2 - 2;
  int r = 22;
  u8g2.drawCircle(cx, cy, r);
  float angle = (gpuLoad / 100.0f) * 6.28318f - 1.5708f;
  int lx = cx + (int)(cos(angle) * (r - 2));
  int ly = cy + (int)(sin(angle) * (r - 2));
  u8g2.drawLine(cx, cy, lx, ly);
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(cx - 5, cy + 3);
  u8g2.print(gpuLoad);
  u8g2.print("%");
}

// Memory: RAM и VRAM на весь экран
void drawMemory() {
  const int barW = DISP_W - 2 * MARGIN;
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "RAM");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN, 26);
  u8g2.print(ramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)ramTotal);
  u8g2.print("G ");
  u8g2.print(ramPct);
  u8g2.print("%");
  drawBar(MARGIN, 18, barW, 8, ramUsed, ramTotal > 0 ? ramTotal : 1);

  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 40, "VRAM");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(MARGIN, 54);
  u8g2.print(vramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)vramTotal);
  u8g2.print("G");
  drawBar(MARGIN, 46, barW, 8, vramUsed, vramTotal > 0 ? vramTotal : 1);
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

// Fans: 4 строки на весь экран
void drawFans() {
  const int rowH = (DISP_H - 2 * MARGIN) / 4;
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12, "Pump");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(DISP_W - MARGIN - 24, 12);
  u8g2.print(fanPump);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12 + rowH, "Rad");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(DISP_W - MARGIN - 24, 12 + rowH);
  u8g2.print(fanRad);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12 + 2 * rowH, "Case");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(DISP_W - MARGIN - 24, 12 + 2 * rowH);
  u8g2.print(fanCase);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr(MARGIN, 12 + 3 * rowH, "GPU");
  u8g2.setFont(FONT_MAIN);
  u8g2.setCursor(DISP_W - MARGIN - 24, 12 + 3 * rowH);
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

// Weather: NOW / DAY / WEEK на весь экран
void drawWeather() {
  u8g2.setFont(FONT_SMALL);
  const char *titles[] = {"NOW", "DAY", "WEEK"};
  u8g2.setCursor(DISP_W - 32, MARGIN + 6);
  u8g2.print(titles[weatherPage]);

  if (weatherPage == 0) {
    bool noData = (weatherTemp == 0 && weatherDesc.length() == 0);
    const uint8_t *icon =
        noData ? iconCloudy : weatherIconFromCode(weatherIcon);
    int iconY = 12;
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
    u8g2.setFont(FONT_MAIN);
    u8g2.setCursor(42, iconY + 10);
    u8g2.print(noData ? "--" : String(weatherTemp));
    u8g2.print((char)0xB0);
    u8g2.setFont(isOnlyAscii(weatherDesc) ? FONT_SMALL : FONT_MEDIA);
    u8g2.setCursor(42, iconY + 26);
    u8g2.print(noData ? "..." : weatherDesc.substring(0, 16));
    return;
  }

  if (weatherPage == 1) {
    const uint8_t *icon = weatherIconFromCode(weatherDayCode);
    u8g2.drawXBMP(MARGIN, 12, 16, 16, icon);
    u8g2.setFont(FONT_MAIN);
    u8g2.setCursor(24, 22);
    u8g2.print("Lo ");
    u8g2.print(weatherDayLow);
    u8g2.print((char)0xB0);
    u8g2.setCursor(24, 38);
    u8g2.print("Hi ");
    u8g2.print(weatherDayHigh);
    u8g2.print((char)0xB0);
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(24, DISP_H - 6);
    u8g2.print("Today");
    return;
  }

  u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(MARGIN, 10, "Hi");
  u8g2.drawStr(MARGIN, 20, "Lo");
  for (int i = 0; i < 7; i++) {
    int x = 16 + i * 16;
    u8g2.setCursor(x, 10);
    u8g2.print(weatherWeekHigh[i]);
    u8g2.setCursor(x, 20);
    u8g2.print(weatherWeekLow[i]);
  }
  u8g2.drawLine(MARGIN, 28, DISP_W - MARGIN, 28);
  for (int i = 0; i < 7; i++) {
    u8g2.setCursor(18 + i * 16, 38);
    u8g2.print(i + 1);
  }
}

// Top 3 processes на весь экран
void drawTopProcs() {
  const int rowH = (DISP_H - 2 * MARGIN) / 3;
  u8g2.setFont(FONT_TITLE);
  bool any = false;
  for (int i = 0; i < 3; i++) {
    int y = MARGIN + 12 + i * rowH;
    u8g2.setFont(isOnlyAscii(procNames[i]) ? FONT_MAIN : FONT_MEDIA);
    u8g2.setCursor(MARGIN, y);
    if (procNames[i].length() > 0) {
      u8g2.print(procNames[i].substring(0, 18));
      u8g2.setCursor(DISP_W - MARGIN - 20, y);
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

// Disks: D1..D4 на весь экран
void drawDisks() {
  u8g2.drawRFrame(MARGIN, MARGIN, DISP_W - 2 * MARGIN, DISP_H - 2 * MARGIN, 3);
  const int rowH = (DISP_H - 2 * MARGIN - 8) / 4;
  u8g2.setFont(FONT_MAIN);
  for (int i = 0; i < 4; i++) {
    int y = MARGIN + 12 + i * rowH;
    u8g2.setCursor(MARGIN + 4, y);
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

// Wolf Runner: на весь экран
void drawWolfGame() {
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(DISP_W - 24, MARGIN + 6);
  u8g2.print(gameScore);
  if (gameOver) {
    if (gameOverGlitchPhase > 0) {
      u8g2.setDrawColor(0);
      for (int y = 0; y < DISP_H; y += 2)
        u8g2.drawBox(0, y, DISP_W, 1);
      u8g2.setDrawColor(1);
      for (int i = 0; i < 40; i++)
        u8g2.drawPixel(random(0, DISP_W), random(0, DISP_H));
      gameOverGlitchPhase--;
      return;
    }
    u8g2.setFont(FONT_TITLE);
    u8g2.setCursor(28, DISP_H / 2 - 8);
    u8g2.print("GAME OVER");
    u8g2.setCursor(38, DISP_H / 2 + 8);
    u8g2.print(gameScore);
    u8g2.print(" *w*");
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(MARGIN, DISP_H - 4);
    u8g2.print("Short=Again Long=Exit");
    return;
  }
  unsigned long left =
      (gameStartTime > 0 && (millis() - gameStartTime) < GAME_DURATION_MS)
          ? (GAME_DURATION_MS - (millis() - gameStartTime)) / 1000
          : 0;
  u8g2.setCursor(MARGIN, DISP_H - 4);
  u8g2.print(left);
  u8g2.print("s Long=Exit");
  u8g2.drawLine(0, GAME_GROUND_Y + GAME_WOLF_H, DISP_W,
                GAME_GROUND_Y + GAME_WOLF_H);
  u8g2.drawFrame(GAME_WOLF_X, gameWolfY, GAME_WOLF_W, GAME_WOLF_H);
  u8g2.drawPixel(GAME_WOLF_X + 2, gameWolfY);
  u8g2.drawPixel(GAME_WOLF_X + GAME_WOLF_W - 2, gameWolfY);
  for (int i = 0; i < GAME_OBSTACLES; i++) {
    if (gameObstacleX[i] < DISP_W)
      u8g2.drawBox(gameObstacleX[i],
                   GAME_GROUND_Y + GAME_WOLF_H - GAME_OBSTACLE_H,
                   GAME_OBSTACLE_W, GAME_OBSTACLE_H);
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
  bool artistAscii = isOnlyAscii(artist);
  bool trackAscii = isOnlyAscii(track);
  u8g2.setFont(artistAscii ? FONT_MAIN : FONT_MEDIA);
  const int maxArtistChars = 12;
  const int maxTrackChars = 14;
  int y1 = 10, y2 = 26;
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
  u8g2.setFont(trackAscii ? FONT_MAIN : FONT_MEDIA);
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
  u8g2.setFont(FONT_SMALL);
  int iconY = 36;
  if (isPlaying) {
    u8g2.drawTriangle(xText, iconY, xText, iconY + 8, xText + 6, iconY + 4);
    if ((millis() % 200) > 100) {
      u8g2.drawBox(xText + 12, iconY + 2, 3, 5);
      u8g2.drawBox(xText + 17, iconY, 3, 8);
      u8g2.drawBox(xText + 22, iconY + 3, 3, 5);
    }
  } else {
    u8g2.drawBox(xText, iconY, 2, 8);
    u8g2.drawBox(xText + 4, iconY, 2, 8);
  }
  int barW = DISP_W - xText - 2 * MARGIN;
  u8g2.drawRFrame(xText, DISP_H - 10, barW, 6, 2);
  if (isPlaying) {
    int prog = (millis() / 100) % (barW - 4 + 1);
    if (prog > 0)
      u8g2.drawBox(xText + 2, DISP_H - 8, prog, 2);
  }
}

// Меню: на весь экран 128x64, все 7 пунктов
void drawMenu() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, DISP_W, DISP_H);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(MARGIN, MARGIN, DISP_W - 2 * MARGIN, DISP_H - 2 * MARGIN, 3);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawStr((DISP_W - 48) / 2, 12, "Settings");
  u8g2.drawLine(MARGIN + 4, 16, DISP_W - MARGIN - 4, 16);

  const int rowH = 7;
  const int x0 = MARGIN + 4;
  u8g2.setFont(FONT_MAIN);
  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 20 + i * rowH;
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

// Киберпанк-глитч при смене экрана: полосы/шум поверх текущего экрана
void drawGlitchTransition() {
  drawScreen(currentScreen);
  if (transitionGlitchPhase <= 0)
    return;
  // Чётные/нечётные строки — чёрные полосы (артефакт)
  if (transitionGlitchPhase >= 4) {
    u8g2.setDrawColor(0);
    for (int y = 0; y < DISP_H; y += 2)
      u8g2.drawBox(0, y, DISP_W, 1);
  } else if (transitionGlitchPhase >= 2) {
    u8g2.setDrawColor(0);
    for (int y = 1; y < DISP_H; y += 2)
      u8g2.drawBox(0, y, DISP_W, 1);
  }
  // Фаза 1 — без полос (мягкий выход)
}

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
    u8g2.setFont(FONT_MAIN);
    u8g2.drawStr((DISP_W - 56) / 2, MARGIN + 10, "Reconnect");
  } else if (splashDone) {
    if (transitionGlitchPhase > 0) {
      drawGlitchTransition();
      transitionGlitchPhase--;
    } else {
      drawScreen(currentScreen);
    }
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(DISP_W - 10, MARGIN + 6);
    u8g2.print("W");

    if (anyAlarm) {
      if (blinkState)
        u8g2.drawFrame(0, 0, DISP_W, DISP_H);
      u8g2.setCursor(DISP_W - 38, MARGIN + 10);
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
        int x = DISP_W / 2 - (TOTAL_SCREENS * 3) + (i * 6);
        if (i == currentScreen) {
          if (dotBlinkOn)
            u8g2.drawBox(x, DISP_H - 2, 2, 2);
          else
            u8g2.drawFrame(x, DISP_H - 2, 2, 2);
        } else
          u8g2.drawPixel(x + 1, DISP_H - 1);
      }
    }
  }

  u8g2.sendBuffer();
}