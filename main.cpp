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

int currentScreen = 0;
// 0=Main, 1=Cores, 2=GPU, 3=Mem, 4=Player, 5=Equalizer, 6=Power
const int TOTAL_SCREENS = 7;

bool inMenu = false; // Мы в меню?
int menuItem = 0;    // Пункт меню (0=LED, 1=Carousel, 2=Exit)

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
const unsigned long EQ_UPDATE_MS = 80;

// Equalizer: 16 bars, smooth interpolation
const int EQ_BARS = 16;
uint8_t eqHeights[EQ_BARS] = {0};
uint8_t eqTargets[EQ_BARS] = {0};

// WiFi / Serial source
WiFiClient tcpClient;
bool useWifi = false;
unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TRY_MS = 8000;
String tcpLineBuffer; // накопление строки до \n

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
  Serial.begin(115200);

  Preferences prefs;
  prefs.begin("heltec", true);
  ledEnabled = prefs.getBool("led", true);
  carouselEnabled = prefs.getBool("carousel", false);
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

// --- SCREEN ZONES: header y=0..11, content below ---
// 1. Main (сводка)
void drawMain() {
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("CPU:");
  u8g2.print(cpuTemp);
  u8g2.print((char)0xB0);
  u8g2.print(" GPU:");
  u8g2.print(gpuTemp);
  u8g2.print((char)0xB0);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 22);
  u8g2.print("RAM:");
  u8g2.print(ramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)ramTotal);
  u8g2.print("G ");
  u8g2.print(ramPct);
  u8g2.print("%");
  u8g2.setCursor(0, 33);
  u8g2.print("VRAM:");
  u8g2.print(vramUsed, 1);
  u8g2.print("/");
  u8g2.print((int)vramTotal);
  u8g2.print("G");
  u8g2.setCursor(0, 44);
  u8g2.print("Load CPU:");
  u8g2.print(cpuLoad);
  u8g2.print("% GPU:");
  u8g2.print(gpuLoad);
  u8g2.print("%");
  if (cpuTemp > 80 && blinkState)
    u8g2.drawBox(0, 0, 128, 64);
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

// Equalizer: заголовок y=0..11, столбики y=12..63 (без пиков поверх)
void drawEqualizer() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("VISUALIZER ");
  u8g2.print(isPlaying ? "[PLAY]" : "[PAUSE]");

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
  u8g2.drawBox(10, 10, 108, 44);
  u8g2.setDrawColor(1); // Clear area
  u8g2.drawFrame(10, 10, 108, 44);

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(45, 22, "- MENU -");

  // Item 0: LED
  u8g2.setCursor(20, 32);
  u8g2.print(menuItem == 0 ? "> LED: " : "  LED: ");
  u8g2.print(ledEnabled ? "ON" : "OFF");

  // Item 1: Carousel
  u8g2.setCursor(20, 42);
  u8g2.print(menuItem == 1 ? "> AUTO: " : "  AUTO: ");
  u8g2.print(carouselEnabled ? "ON" : "OFF");

  // Item 2: Exit
  u8g2.setCursor(20, 52);
  u8g2.print(menuItem == 2 ? "> EXIT" : "  EXIT");
}

void loop() {
  unsigned long now = millis();

  // --- 0. WiFi / TCP connect (при старте или при обрыве) ---
  if (strlen(WIFI_PASS) > 0) {
    if (WiFi.status() == WL_CONNECTED && !tcpClient.connected() && !useWifi) {
      if (tcpClient.connect(PC_IP, TCP_PORT, 2000)) {
        useWifi = true;
      }
    }
    if (WiFi.status() != WL_CONNECTED &&
        (now - wifiConnectStart < WIFI_TRY_MS)) {
      delay(10); // даём WiFi время при старте
    }
    if (useWifi && !tcpClient.connected()) {
      useWifi = false;
      tcpClient.stop();
    }
  }

  // --- 1. JSON READ (WiFi или Serial) ---
  String input;
  if (useWifi && tcpClient.connected()) {
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
  } else {
    tcpLineBuffer = "";
    if (Serial.available() > 0)
      input = Serial.readStringUntil('\n');
  }

  if (input.length() > 0 && input.length() < 4096) {
    StaticJsonDocument<3600> doc;
    DeserializationError error = deserializeJson(doc, input);
    if (!error) {
      int hwOk = doc["hw_ok"] | 1;
      cpuTemp = doc["ct"];
      gpuTemp = doc["gt"];
      gpuHotSpot = doc["gth"];
      vrmTemp = doc["vt"];
      cpuLoad = doc["cpu_load"];
      cpuPwr = (int)(float(doc["cpu_pwr"]) + 0.5f);
      gpuLoad = doc["gpu_load"];
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

      const char *art = doc["art"];
      const char *trk = doc["trk"];
      artist = String(art ? art : "");
      track = String(trk ? trk : "");
      int p = doc["play"];
      isPlaying = (p == 1);

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

      lastUpdate = now;
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
        if (menuItem > 2)
          menuItem = 0;
      } else {
        // Screen Navigation
        currentScreen++;
        if (currentScreen >= TOTAL_SCREENS)
          currentScreen = 0;
        // Reset carousel timer to give user time to look
        lastCarousel = now;
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
    if (menuItem == 2)
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

  // --- 5. EQUALIZER ANIMATION (smooth interpolation) ---
  if (now - lastEqUpdate >= EQ_UPDATE_MS) {
    lastEqUpdate = now;
    if (isPlaying) {
      for (int i = 0; i < EQ_BARS; i++) {
        if (random(0, 4) == 0) // occasionally pick new target
          eqTargets[i] = (uint8_t)random(4, 48);
        if (eqHeights[i] < eqTargets[i] && eqHeights[i] < 52)
          eqHeights[i]++;
        else if (eqHeights[i] > eqTargets[i] && eqHeights[i] > 0)
          eqHeights[i]--;
      }
    } else {
      for (int i = 0; i < EQ_BARS; i++) {
        eqTargets[i] = 0;
        if (eqHeights[i] > 0)
          eqHeights[i]--;
      }
    }
  }

  // --- 6. ALARM LOGIC ---
  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  bool alarm = (cpuTemp > 80) || (ramUsed > 25.0) || (vramUsed > 10.0);

  if (ledEnabled && alarm && blinkState)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);

  // --- 7. DRAWING ---
  u8g2.clearBuffer();
  bool signalLost = (now - lastUpdate > SIGNAL_TIMEOUT_MS);

  if (signalLost) {
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
    }
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(70, 8, "Reconnect");
  } else {
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
    }
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(118, 8);
    u8g2.print(useWifi ? "W" : "S");

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