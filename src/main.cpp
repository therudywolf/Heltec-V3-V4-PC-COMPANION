/*
 * NOCTURNE_OS — Unified firmware entry (ESP32-S3 Heltec V4).
 * Build profiles: bmw_only, pc_companion, full (see platformio.ini).
 */
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <NimBLEDevice.h>

#if NOCT_FEATURE_MONITORING
#include <ArduinoJson.h>
#endif

#include "AppModeManager.h"
#include "InputHandler.h"
#include "MenuHandler.h"
#include "modules/display/BootAnim.h"
#include "modules/display/DisplayEngine.h"
#include "modules/display/DisplayManager.h"
#include "modules/display/SceneManager.h"
#include "modules/car/BmwManager.h"
#include "modules/car/ObdClient.h"
#include "modules/system/BatteryManager.h"
#include "nocturne/Types.h"
#include "nocturne/config.h"

#if NOCT_FEATURE_MONITORING
#include "modules/network/NetManager.h"
#include "secrets.h"
#endif
#if NOCT_FEATURE_FORZA
#include "modules/car/ForzaManager.h"
#endif
#if NOCT_FEATURE_HACKER
#include "modules/network/TrapManager.h"
#include "modules/network/WifiSniffManager.h"
#include "modules/ble/BleManager.h"
#endif

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
BmwManager bmwManager;
ObdClient obdClient;
AppState state;
SceneManager sceneManager(display, state);
DisplayManager displayManager(display, bmwManager);
BatteryManager batteryManager;

#if NOCT_FEATURE_MONITORING
NetManager netManager;
#endif
#if NOCT_FEATURE_FORZA
ForzaManager forzaManager;
#endif
#if NOCT_FEATURE_HACKER
BleManager bleManager;
TrapManager trapManager;
WifiSniffManager wifiSniffManager;
#endif

AppModeManager appModeManager(
    bmwManager
#if NOCT_FEATURE_MONITORING
    , netManager
#endif
#if NOCT_FEATURE_FORZA
    , forzaManager
#endif
#if NOCT_FEATURE_HACKER
    , trapManager
    , wifiSniffManager
    , bleManager
#endif
);

InputSystem input(NOCT_BUTTON_PIN);
IntervalTimer guiTimer(NOCT_REDRAW_INTERVAL_MS);
IntervalTimer batTimer(NOCT_BAT_READ_INTERVAL_STABLE_MS);

Settings &settings = state.settings;
unsigned long bootTime = 0;
unsigned long splashStart = 0;
bool splashDone = false;
static int menuHackerGroup = 0;

bool predatorMode = false;
unsigned long predatorEnterTime = 0;

bool quickMenuOpen = false;
int quickMenuItem = 0;
int menuLevel = 0;
int menuCategory = 0;

enum MenuState { MENU_MAIN };
MenuState menuState = MENU_MAIN;
bool rebootConfirmed = false;
unsigned long rebootConfirmTime = 0;
static unsigned long lastInputTime = 0;

unsigned long lastMenuEventTime = 0;
#define MENU_EVENT_DEBOUNCE_MS 150

static char toastMsg[20] = "";
static unsigned long toastUntil = 0;

#define BMW_ACTION_COUNT 12
static int bmwActionIndex = 0;

static bool needRedraw = true;

AppMode currentMode = MODE_BMW_ASSISTANT;

#if NOCT_FEATURE_MONITORING
int currentScene = 0;
int previousScene = 0;
unsigned long transitionStart = 0;
bool inTransition = false;
unsigned long lastCarousel = 0;
unsigned long lastFanAnim = 0;
static unsigned long idleStateEnteredMs = 0;
int fanAnimFrame = 0;
bool blinkState = false;
int alertBlinkCounter = 0;
bool lastAlertActive = false;
unsigned long lastBlink = 0;
#endif

#if NOCT_FEATURE_FORZA
static unsigned long forzaSplashUntil = 0;
#define FORZA_SPLASH_MS 3000
#endif

#if NOCT_FEATURE_HACKER
int wifiScanSelected = 0;
int wifiListPage = 0;
int wifiSortMode = 0;
int wifiRssiFilter = -100;
int wifiSortedIndices[32];
int wifiFilteredCount = 0;
static int wifiSniffSelected = 0;
static int bleScanSelected = 0;

static void sortAndFilterWiFiNetworks()
{
  static int lastScanCount = -1;
  static int lastSortMode = -1;
  static int lastRssiFilter = -101;

  int n = WiFi.scanComplete();
  if (n <= 0 || n > 32)
  {
    wifiFilteredCount = 0;
    lastScanCount = -1;
    return;
  }

  static unsigned long lastScanTime = 0;
  unsigned long now = millis();
  const unsigned long SCAN_CACHE_TIMEOUT_MS = 30000;

  if (n == lastScanCount && wifiSortMode == lastSortMode &&
      wifiRssiFilter == lastRssiFilter && wifiFilteredCount > 0 &&
      (now - lastScanTime < SCAN_CACHE_TIMEOUT_MS))
    return;

  lastScanTime = now;
  lastScanCount = n;
  lastSortMode = wifiSortMode;
  lastRssiFilter = wifiRssiFilter;

  for (int i = 0; i < n; i++)
    wifiSortedIndices[i] = i;

  int filtered = 0;
  for (int i = 0; i < n; i++)
  {
    if (WiFi.RSSI(i) >= wifiRssiFilter)
      wifiSortedIndices[filtered++] = i;
  }
  wifiFilteredCount = filtered;

  if (wifiSortMode == 0)
  {
    for (int i = 1; i < filtered; i++)
    {
      int key = wifiSortedIndices[i];
      int keyRssi = WiFi.RSSI(key);
      int j = i - 1;
      while (j >= 0 && WiFi.RSSI(wifiSortedIndices[j]) < keyRssi)
      {
        wifiSortedIndices[j + 1] = wifiSortedIndices[j];
        j--;
      }
      wifiSortedIndices[j + 1] = key;
    }
  }
  else if (wifiSortMode == 1)
  {
    static char ssidBuf1[33], ssidBuf2[33];
    for (int i = 1; i < filtered; i++)
    {
      int key = wifiSortedIndices[i];
      strncpy(ssidBuf1, WiFi.SSID(key).c_str(), sizeof(ssidBuf1) - 1);
      ssidBuf1[sizeof(ssidBuf1) - 1] = '\0';
      int j = i - 1;
      while (j >= 0)
      {
        strncpy(ssidBuf2, WiFi.SSID(wifiSortedIndices[j]).c_str(), sizeof(ssidBuf2) - 1);
        ssidBuf2[sizeof(ssidBuf2) - 1] = '\0';
        if (strcmp(ssidBuf2, ssidBuf1) > 0)
        {
          wifiSortedIndices[j + 1] = wifiSortedIndices[j];
          j--;
        }
        else
          break;
      }
      wifiSortedIndices[j + 1] = key;
    }
  }
  else if (wifiSortMode == 2)
  {
    for (int i = 1; i < filtered; i++)
    {
      int key = wifiSortedIndices[i];
      int keyRssi = WiFi.RSSI(key);
      int j = i - 1;
      while (j >= 0 && WiFi.RSSI(wifiSortedIndices[j]) > keyRssi)
      {
        wifiSortedIndices[j + 1] = wifiSortedIndices[j];
        j--;
      }
      wifiSortedIndices[j + 1] = key;
    }
  }
}
#endif // NOCT_FEATURE_HACKER

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup()
{
  bootTime = millis();
  Serial.begin(115200);
  Serial.println("[NOCT] Nocturne OS " NOCTURNE_VERSION);
  setCpuFrequencyMhz(240);

  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(100));

  pinMode(NOCT_RST_PIN, OUTPUT);
  digitalWrite(NOCT_RST_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));
  digitalWrite(NOCT_RST_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(50));

  display.begin();
  vTaskDelay(pdMS_TO_TICKS(100));

  if (digitalRead(NOCT_VEXT_PIN) != LOW)
    digitalWrite(NOCT_VEXT_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));
  drawBootSequence(display);
  splashDone = true;

  pinMode(NOCT_BUTTON_PIN, INPUT_PULLUP);

  /* Hold PRG at boot -> enable demo mode for this session */
  bool bootDemoRequested = false;
  {
    const unsigned long holdMs = NOCT_DEMO_BOOT_HOLD_MS;
    const unsigned long stepMs = 50;
    unsigned long t = 0;
    while (t < holdMs)
    {
      if (digitalRead(NOCT_BUTTON_PIN) != LOW)
        break;
      vTaskDelay(pdMS_TO_TICKS(stepMs));
      t += stepMs;
    }
    if (t >= holdMs && digitalRead(NOCT_BUTTON_PIN) == LOW)
    {
      bootDemoRequested = true;
      Preferences prefsDemo;
      prefsDemo.begin("nocturne", false);
      prefsDemo.putBool("bmw_demo", true);
      prefsDemo.end();
      display.u8g2().setDrawColor(1);
      display.u8g2().setFont(u8g2_font_6x10_tf);
      display.u8g2().drawStr(2, 32, "Demo mode");
      display.sendBuffer();
      vTaskDelay(pdMS_TO_TICKS(1200));
    }
  }

  quickMenuOpen = false;
  menuLevel = 0;
  menuCategory = 0;
  quickMenuItem = 0;
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(200));
  digitalWrite(NOCT_LED_ALERT_PIN, LOW);

  analogReadResolution(12);
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  analogSetPinAttenuation(NOCT_BAT_PIN, ADC_11db);
#else
  analogSetAttenuation(ADC_11db);
#endif

  vTaskDelay(pdMS_TO_TICKS(100));
  analogRead(NOCT_BAT_PIN);
  vTaskDelay(pdMS_TO_TICKS(50));

  Preferences prefs;
  prefs.begin("nocturne", true);
  settings.ledEnabled = prefs.getBool("led", true);
  settings.carouselEnabled = prefs.getBool("carousel", false);
  settings.carouselIntervalSec = prefs.getInt("carouselSec", 10);
  if (settings.carouselIntervalSec != 5 && settings.carouselIntervalSec != 10 &&
      settings.carouselIntervalSec != 15)
    settings.carouselIntervalSec = 10;
  settings.displayContrast = prefs.getInt("contrast", 128);
  if (settings.displayContrast > 255) settings.displayContrast = 255;
  if (settings.displayContrast < 0) settings.displayContrast = 0;
  settings.displayInverted = prefs.getBool("inverted", true);
  settings.glitchEnabled = prefs.getBool("glitch", false);
  settings.lowBrightnessDefault = prefs.getBool("lowBright", false);
  settings.displayTimeoutSec = prefs.getInt("dispTimeout", 0);
  if (settings.displayTimeoutSec != 0 && settings.displayTimeoutSec != 30 &&
      settings.displayTimeoutSec != 60)
    settings.displayTimeoutSec = 0;
  prefs.end();

  {
    uint8_t contrast = settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
    if (!settings.lowBrightnessDefault && settings.displayContrast >= 0)
      contrast = (uint8_t)settings.displayContrast;
    display.u8g2().setContrast(contrast);
  }
  display.setScreenFlipped(settings.displayInverted);
  randomSeed(esp_random());

#if NOCT_FEATURE_MONITORING
  netManager.begin(WIFI_SSID, WIFI_PASS);
  netManager.setServer(PC_IP, TCP_PORT);
  currentMode = MODE_NORMAL;
#else
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif

#if NOCT_OBD_ENABLED
  obdClient.begin(NOCT_OBD_TX_PIN, NOCT_OBD_RX_PIN);
  obdClient.setDataCallback(
      [](bool c, int r, int co, int o) { bmwManager.setObdData(c, r, co, o); });
#endif

  batteryManager.update(state);

#if !NOCT_FEATURE_MONITORING
  appModeManager.switchToMode(currentMode, MODE_BMW_ASSISTANT);
  {
    Preferences p;
    p.begin("nocturne", true);
    bool demo = p.getBool("bmw_demo", false);
    p.end();
    if (bootDemoRequested) demo = true;
    bmwManager.setDemoMode(demo);
  }
#else
  (void)bootDemoRequested;
  quickMenuOpen = true;
  menuLevel = 0;
#endif
}

// ---------------------------------------------------------------------------
// Menu action handlers
// ---------------------------------------------------------------------------
#if NOCT_FEATURE_HACKER
static bool handleHackerItem(int group, int item, unsigned long now)
{
  quickMenuOpen = false;
  rebootConfirmed = false;
  AppMode mode = getModeForHackerItem(group, item);
  if (mode == MODE_RADAR)
  {
    wifiScanSelected = 0;
    wifiListPage = 0;
    wifiFilteredCount = 0;
  }
  bool ok = (mode == MODE_WIFI_TRAP)
      ? (sortAndFilterWiFiNetworks(),
         appModeManager.switchToMode(currentMode, mode,
                                     wifiScanSelected, wifiFilteredCount,
                                     wifiSortedIndices))
      : appModeManager.switchToMode(currentMode, mode);
  if (!ok)
  {
    snprintf(toastMsg, sizeof(toastMsg), "FAIL");
    toastUntil = now + 1500;
    return false;
  }
#if NOCT_FEATURE_FORZA
  if (mode == MODE_GAME_FORZA)
    forzaSplashUntil = now + FORZA_SPLASH_MS;
#endif
  return true;
}
#endif

static bool handleMenuActionByCategory(int cat, int item, unsigned long now)
{
#if NOCT_FEATURE_MONITORING
  if (cat == MCAT_MONITORING)
  {
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (item == 0)
    {
      if (!appModeManager.switchToMode(currentMode, MODE_NORMAL))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
#if NOCT_FEATURE_FORZA
    else if (item == 1)
    {
      if (!appModeManager.switchToMode(currentMode, MODE_GAME_FORZA))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
      forzaSplashUntil = now + FORZA_SPLASH_MS;
    }
#endif
    return true;
  }
#endif

  if (cat == MCAT_BMW)
  {
    quickMenuOpen = false;
    rebootConfirmed = false;
    Preferences prefs;
    prefs.begin("nocturne", false);
    if (item == 0)
    {
      prefs.putBool("bmw_demo", false);
      prefs.end();
      if (!appModeManager.switchToMode(currentMode, MODE_BMW_ASSISTANT))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    else if (item == 1)
    {
      prefs.putBool("bmw_demo", true);
      prefs.end();
      if (!appModeManager.switchToMode(currentMode, MODE_BMW_ASSISTANT))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    else
      prefs.end();
    return true;
  }

  if (cat == MCAT_CONFIG)
  {
    if (item == 0)
    {
      if (!settings.carouselEnabled)
      { settings.carouselEnabled = true; settings.carouselIntervalSec = 5; }
      else if (settings.carouselIntervalSec == 5) settings.carouselIntervalSec = 10;
      else if (settings.carouselIntervalSec == 10) settings.carouselIntervalSec = 15;
      else settings.carouselEnabled = false;
      Preferences p; p.begin("nocturne", false);
      p.putBool("carousel", settings.carouselEnabled);
      p.putInt("carouselSec", settings.carouselIntervalSec);
      p.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved"); toastUntil = now + 800;
    }
    else if (item == 1)
    {
      settings.displayInverted = !settings.displayInverted;
      display.setScreenFlipped(settings.displayInverted);
      Preferences p; p.begin("nocturne", false);
      p.putBool("inverted", settings.displayInverted); p.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved"); toastUntil = now + 800;
    }
    else if (item == 2)
    {
      settings.glitchEnabled = !settings.glitchEnabled;
      Preferences p; p.begin("nocturne", false);
      p.putBool("glitch", settings.glitchEnabled); p.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved"); toastUntil = now + 800;
    }
    else if (item == 3)
    {
      settings.ledEnabled = !settings.ledEnabled;
      Preferences p; p.begin("nocturne", false);
      p.putBool("led", settings.ledEnabled); p.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved"); toastUntil = now + 800;
    }
    else if (item == 4)
    {
      settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
      settings.displayContrast = settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
      display.u8g2().setContrast(settings.displayContrast);
      Preferences p; p.begin("nocturne", false);
      p.putBool("lowBright", settings.lowBrightnessDefault);
      p.putInt("contrast", settings.displayContrast); p.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved"); toastUntil = now + 800;
    }
    else if (item == 5)
    {
      const int levels[] = {NOCT_CONTRAST_MIN, 64, 128, 192, NOCT_CONTRAST_MAX};
      const int nLevels = sizeof(levels) / sizeof(levels[0]);
      int i = 0;
      while (i < nLevels && levels[i] != settings.displayContrast) i++;
      settings.displayContrast = levels[(i + 1) % nLevels];
      display.u8g2().setContrast(settings.displayContrast);
      Preferences p; p.begin("nocturne", false);
      p.putInt("contrast", settings.displayContrast); p.end();
      snprintf(toastMsg, sizeof(toastMsg), "C:%d", settings.displayContrast); toastUntil = now + 800;
    }
    else if (item == 6)
    {
      if (settings.displayTimeoutSec == 0) settings.displayTimeoutSec = 30;
      else if (settings.displayTimeoutSec == 30) settings.displayTimeoutSec = 60;
      else settings.displayTimeoutSec = 0;
      Preferences p; p.begin("nocturne", false);
      p.putInt("dispTimeout", settings.displayTimeoutSec); p.end();
      snprintf(toastMsg, sizeof(toastMsg),
              settings.displayTimeoutSec ? "T:%ds" : "T:OFF",
              settings.displayTimeoutSec);
      toastUntil = now + 800;
    }
    return true;
  }

  if (cat == MCAT_SYSTEM)
  {
    if (item == 0)
    {
      Preferences p; p.begin("nocturne", false);
      bool demo = p.getBool("bmw_demo", false);
      demo = !demo;
      p.putBool("bmw_demo", demo); p.end();
      bmwManager.setDemoMode(demo);
      snprintf(toastMsg, sizeof(toastMsg), demo ? "Demo ON" : "Demo OFF");
      toastUntil = now + 1200;
      return true;
    }
    if (item == 1)
    {
      if (!rebootConfirmed)
      { rebootConfirmed = true; rebootConfirmTime = now; }
      else
      { rebootConfirmed = false; esp_restart(); }
      return true;
    }
    if (item == 2)
    {
      quickMenuOpen = false;
      rebootConfirmed = false;
      if (!appModeManager.switchToMode(currentMode, MODE_CHARGE_ONLY))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL"); toastUntil = now + 1500;
        return false;
      }
      return true;
    }
    if (item == 3)
    {
      quickMenuOpen = false;
      rebootConfirmed = false;
      esp_sleep_enable_ext0_wakeup((gpio_num_t)NOCT_BUTTON_PIN, 0);
      esp_deep_sleep_start();
      return true;
    }
    if (item == 4)
    {
      snprintf(toastMsg, sizeof(toastMsg), "v" NOCTURNE_VERSION);
      toastUntil = now + 2000;
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop()
{
  unsigned long now = millis();

  // ── Background tasks ────────────────────────────────────────────────
  if (currentMode == MODE_BMW_ASSISTANT)
    bmwManager.tick();

#if NOCT_FEATURE_MONITORING
  bool pcMonitoringActive = (currentMode == MODE_NORMAL && splashDone && !quickMenuOpen);
  if (pcMonitoringActive)
    netManager.tick(now);
  else if (netManager.isTcpConnected())
    netManager.disconnectTcp();

  if (netManager.isTcpConnected())
  {
    while (netManager.available())
    {
      char c = (char)netManager.read();
      if (c == '\n')
      {
        char *buf = netManager.getLineBuffer();
        size_t bufLen = netManager.getLineBufferLen();
        if (bufLen > 0 && bufLen <= NOCT_TCP_LINE_MAX)
        {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, buf, bufLen);
          if (!err)
          {
            netManager.markDataReceived(now);
            if (netManager.parsePayload(buf, bufLen, &state))
            {
              needRedraw = true;
              HardwareData &hw = state.hw;
              display.cpuGraph.push((float)hw.cl);
              display.gpuGraph.push((float)hw.gl);
              display.netDownGraph.setMax(2048);
              display.netDownGraph.push((float)hw.nd);
              display.netUpGraph.setMax(2048);
              display.netUpGraph.push((float)hw.nu);
            }
          }
        }
        netManager.clearLineBuffer();
      }
      else
        netManager.appendLineBuffer(c);
    }
  }
#endif

#if NOCT_FEATURE_FORZA
  if (currentMode == MODE_GAME_FORZA)
    forzaManager.tick();
#endif

  // ── Input ───────────────────────────────────────────────────────────
  ButtonEvent event = input.update();
  if (event != EV_NONE)
    lastInputTime = now;

  if (event == EV_DOUBLE && !quickMenuOpen)
  {
#if NOCT_FEATURE_MONITORING
    if (currentMode != MODE_NORMAL && currentMode != MODE_BMW_ASSISTANT && currentMode != MODE_CHARGE_ONLY)
      appModeManager.exitToNormal(currentMode);
#endif
    quickMenuOpen = true;
    menuState = MENU_MAIN;
    menuLevel = 0;
    menuCategory = 0;
    quickMenuItem = 0;
    lastMenuEventTime = now;
    needRedraw = true;
    event = EV_NONE;
  }

  // ── Battery ─────────────────────────────────────────────────────────
  if (batTimer.check(now))
  {
    unsigned long nextInterval = batteryManager.update(state);
    batTimer.intervalMs = nextInterval;
    batTimer.lastMs = now;
  }

  // ── LED ─────────────────────────────────────────────────────────────
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  if (predatorMode)
  {
    unsigned long t = (now - predatorEnterTime) / 20;
    int breath = (int)(128 + 127 * sin(t * 0.1f));
    if (breath < 0) breath = 0;
    if (settings.ledEnabled) analogWrite(NOCT_LED_ALERT_PIN, breath);
    else digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }
#if NOCT_FEATURE_FORZA
  else if (currentMode == MODE_GAME_FORZA)
  {
    const ForzaState &fs = forzaManager.getState();
    if (fs.connected && fs.maxRpm > 0)
    {
      float pct = fs.currentRpm / fs.maxRpm;
      if (pct >= FORZA_SHIFT_THRESHOLD)
      {
        bool flash = (now / 80) % 2 == 0;
        if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, flash ? HIGH : LOW);
      }
      else if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, LOW);
    }
    else if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }
#endif
  else if (currentMode == MODE_BMW_ASSISTANT)
  {
    if (bmwManager.isObdConnected() && bmwManager.getObdRpm() >= 5500)
    {
      bool flash = (now / 80) % 2 == 0;
      if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, flash ? HIGH : LOW);
    }
    else if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }
  else
  {
    if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }

  // ── Splash ──────────────────────────────────────────────────────────
  if (!splashDone)
  {
    if (splashStart == 0) splashStart = now;
    if (now - splashStart >= (unsigned long)NOCT_SPLASH_MS)
    { splashDone = true; needRedraw = true; }
  }
  if (quickMenuOpen) splashDone = true;

#if NOCT_FEATURE_MONITORING
  // ── Alert & carousel (monitoring) ───────────────────────────────────
  if (state.alertActive)
  {
    int total = sceneManager.totalScenes();
    int target = state.alertTargetScene;
    currentScene = (total > 0 && target >= 0 && target < total) ? target : 0;
    needRedraw = true;
  }
  if (settings.carouselEnabled && !predatorMode && !state.alertActive)
  {
    unsigned long intervalMs = (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs)
    {
      needRedraw = true;
      previousScene = currentScene;
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
      if (previousScene != currentScene)
      { inTransition = true; transitionStart = now; }
      lastCarousel = now;
    }
  }
  if (pcMonitoringActive && netManager.isTcpConnected() &&
      netManager.getLastSentScreen() != currentScene)
  {
    char screenMsg[16];
    snprintf(screenMsg, sizeof(screenMsg), "screen:%d\n", currentScene);
    netManager.print(screenMsg);
    netManager.setLastSentScreen(currentScene);
  }
  if (now - lastFanAnim >= 50)
  { fanAnimFrame = (fanAnimFrame + 1) % 12; lastFanAnim = now; }
#endif

  // ── Menu logic ──────────────────────────────────────────────────────
  if (quickMenuOpen)
  {
    if (rebootConfirmed && (now - rebootConfirmTime > 5000))
    { rebootConfirmed = false; needRedraw = true; }

    if (event != EV_NONE && (now - lastMenuEventTime < MENU_EVENT_DEBOUNCE_MS))
      event = EV_NONE;

    if (event == EV_DOUBLE)
    {
      lastMenuEventTime = now;
      if (menuLevel == 2) { menuLevel = 1; quickMenuItem = 0; rebootConfirmed = false; }
      else if (menuLevel == 1) { menuLevel = 0; quickMenuItem = menuCategory; rebootConfirmed = false; }
      else { quickMenuOpen = false; rebootConfirmed = false; }
      needRedraw = true;
    }
    else if (event == EV_SHORT)
    {
      lastMenuEventTime = now;
      if (menuLevel == 0)
        quickMenuItem = (quickMenuItem + 1) % MENU_CATEGORIES;
      else if (menuLevel == 1)
      {
        int count = submenuCount(menuCategory);
        quickMenuItem = (quickMenuItem + 1) % count;
        if (menuCategory == MCAT_SYSTEM && quickMenuItem != 1)
          rebootConfirmed = false;
      }
      else
      {
        int count = submenuCountForHackerGroup(menuHackerGroup);
        quickMenuItem = (quickMenuItem + 1) % (count > 0 ? count : 1);
      }
      needRedraw = true;
    }
    else if (event == EV_LONG)
    {
      lastMenuEventTime = now;
      if (menuLevel == 0)
      { menuLevel = 1; menuCategory = quickMenuItem; quickMenuItem = 0; }
#if NOCT_FEATURE_HACKER
      else if (menuLevel == 1 && menuCategory == MCAT_HACKER)
      { menuLevel = 2; menuHackerGroup = quickMenuItem; quickMenuItem = 0; }
      else if (menuLevel == 2)
      {
        bool ok = handleHackerItem(menuHackerGroup, quickMenuItem, now);
        if (ok) needRedraw = true;
      }
#endif
      else if (menuLevel == 1)
      {
        bool ok = handleMenuActionByCategory(menuCategory, quickMenuItem, now);
        if (ok) needRedraw = true;
      }
    }
  }
  else
  {
    // ── Mode-specific input ───────────────────────────────────────────
    switch (currentMode)
    {
    case MODE_BMW_ASSISTANT:
      if (event == EV_SHORT)
      { bmwActionIndex = (bmwActionIndex + 1) % BMW_ACTION_COUNT; needRedraw = true; }
      else if (event == EV_LONG)
      {
        if (!bmwManager.isIbusSynced())
        {
#if NOCT_BMW_DEBUG
          Serial.println("[BMW] Run ignored: No IBus");
#endif
          snprintf(toastMsg, sizeof(toastMsg), "No IBus"); toastUntil = now + 1500;
        }
        else
        {
          switch (bmwActionIndex)
          {
          case 0: bmwManager.sendGoodbyeLights(); bmwManager.setLastActionFeedback("Goodbye"); break;
          case 1: bmwManager.sendFollowMeHome(); bmwManager.setLastActionFeedback("FollowMe"); break;
          case 2: bmwManager.sendParkLights(); bmwManager.setLastActionFeedback("Park"); break;
          case 3: bmwManager.sendHazardLights(); bmwManager.setLastActionFeedback("Hazard"); break;
          case 4: bmwManager.sendLowBeams(); bmwManager.setLastActionFeedback("LowBeam"); break;
          case 5: bmwManager.sendLightsOff(); bmwManager.setLastActionFeedback("Lights off"); break;
          case 6: bmwManager.sendUnlock(); bmwManager.setLastActionFeedback("Unlock sent"); break;
          case 7: bmwManager.sendLock(); bmwManager.setLastActionFeedback("Lock sent"); break;
          case 8: bmwManager.sendTrunkOpen(); bmwManager.setLastActionFeedback("Trunk open"); break;
          case 9: bmwManager.sendClusterText("NOCT"); bmwManager.setLastActionFeedback("Cluster"); break;
          case 10: bmwManager.sendDoorsUnlockInterior(); bmwManager.setLastActionFeedback("Door unlock"); break;
          case 11: bmwManager.sendDoorsLockKey(); bmwManager.setLastActionFeedback("Door lock"); break;
          default: break;
          }
        }
        needRedraw = true;
      }
      break;

    case MODE_CHARGE_ONLY:
      break;

#if NOCT_FEATURE_MONITORING
    case MODE_NORMAL:
      if (event == EV_SHORT && !state.alertActive)
      {
        previousScene = currentScene;
        currentScene = (currentScene + 1) % sceneManager.totalScenes();
        if (previousScene != currentScene)
        { inTransition = true; transitionStart = now; }
        lastCarousel = now;
        needRedraw = true;
      }
      else if (event == EV_LONG)
      {
        settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
        uint8_t contrast = settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
        display.u8g2().setContrast(contrast);
        Preferences p; p.begin("nocturne", false);
        p.putBool("lowBright", settings.lowBrightnessDefault); p.end();
        needRedraw = true;
      }
      break;
#endif

#if NOCT_FEATURE_HACKER
    case MODE_RADAR:
      if (event == EV_SHORT)
      {
        int n = WiFi.scanComplete();
        if (n > 0)
        {
          sortAndFilterWiFiNetworks();
          int count = wifiFilteredCount > 0 ? wifiFilteredCount : n;
          if (count > 0)
          {
            wifiScanSelected = (wifiScanSelected + 1) % count;
            if (wifiScanSelected >= wifiListPage + 5) wifiListPage = wifiScanSelected - 4;
            else if (wifiScanSelected < wifiListPage) wifiListPage = wifiScanSelected;
          }
        }
        needRedraw = true;
      }
      else if (event == EV_LONG)
      {
        int n = WiFi.scanComplete();
        if (n > 0)
        {
          wifiSortMode = (wifiSortMode + 1) % 3;
          sortAndFilterWiFiNetworks();
          wifiScanSelected = 0;
          wifiListPage = 0;
        }
        else
        {
          WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
          vTaskDelay(pdMS_TO_TICKS(100));
          WiFi.mode(WIFI_STA);
          WiFi.scanNetworks(true, true);
          wifiFilteredCount = 0;
        }
        needRedraw = true;
      }
      break;
    case MODE_WIFI_SNIFF:
      if (event == EV_SHORT)
      {
        int n = wifiSniffManager.getApCount();
        if (n > 0) { wifiSniffSelected = (wifiSniffSelected + 1) % n; needRedraw = true; }
      }
      break;
#endif

    default:
      break;
    }
  }

  // ── Render ──────────────────────────────────────────────────────────
  static unsigned long lastYield = 0;

  if (predatorMode)
  {
    display.clearBuffer();
    display.sendBuffer();
    if (now - lastYield > 10) { yield(); lastYield = now; }
    return;
  }

  if (!needRedraw && !guiTimer.check(now) && !quickMenuOpen)
  {
    if (now - lastYield > 10) { yield(); lastYield = now; }
    return;
  }
  needRedraw = false;

  if (lastInputTime == 0) lastInputTime = now;
  if (!quickMenuOpen && settings.displayTimeoutSec > 0 &&
      (now - lastInputTime > (unsigned long)settings.displayTimeoutSec * 1000))
    display.u8g2().setContrast(NOCT_CONTRAST_MIN);
  else
    display.u8g2().setContrast(settings.displayContrast);

  bool displayManagerSent = false;
  if (!(currentMode == MODE_BMW_ASSISTANT && !quickMenuOpen))
    display.clearBuffer();

  if (!splashDone)
    display.drawSplash();
  else if (quickMenuOpen)
  {
    sceneManager.drawMenu(
        menuLevel, menuCategory, quickMenuItem, menuHackerGroup,
        settings.carouselEnabled, settings.carouselIntervalSec,
        settings.displayInverted, settings.glitchEnabled, settings.ledEnabled,
        settings.lowBrightnessDefault, rebootConfirmed,
        settings.displayContrast, settings.displayTimeoutSec);
  }
  else
  {
    switch (currentMode)
    {
    case MODE_CHARGE_ONLY:
      sceneManager.drawChargeOnlyScreen(state.batteryPct, state.isCharging, state.batteryVoltage);
      break;

    case MODE_BMW_ASSISTANT:
#if NOCT_OBD_ENABLED
      if (obdClient.isEnabled()) obdClient.tick();
#endif
      displayManagerSent = displayManager.update(now);
      break;

#if NOCT_FEATURE_MONITORING
    case MODE_NORMAL:
    {
      bool signalLost = netManager.isSignalLost(now);
      if (signalLost && netManager.isTcpConnected() && netManager.hasReceivedData())
        netManager.disconnectTcp();

      bool idleState = !netManager.isWifiConnected() || !netManager.isTcpConnected();
      if (!idleState) idleStateEnteredMs = 0;
      if (idleState && idleStateEnteredMs == 0) idleStateEnteredMs = now;
      bool showScreensaver = idleState && idleStateEnteredMs != 0 &&
          (now - idleStateEnteredMs >= (unsigned long)NOCT_IDLE_SCREENSAVER_MS);

      if (!netManager.isWifiConnected())
      {
        if (showScreensaver) { sceneManager.drawIdleScreensaver(now); display.applyGlitch(); }
        else sceneManager.drawNoSignal(false, false, 0, blinkState);
      }
      else if (!netManager.isTcpConnected())
      {
        if (showScreensaver) { sceneManager.drawIdleScreensaver(now); display.applyGlitch(); }
        else sceneManager.drawConnecting(netManager.rssi(), blinkState);
      }
      else if (netManager.isSearchMode() || signalLost)
      {
        sceneManager.drawSearchMode((int)(now / 100) % 12);
      }
      else
      {
        display.drawGlobalHeader(sceneManager.getSceneName(currentScene),
                                 nullptr, netManager.rssi(), netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging, state.batteryVoltage);
        if (inTransition)
        {
          unsigned long elapsed = now - transitionStart;
          int progress = (int)((elapsed * NOCT_TRANSITION_STEP) / NOCT_TRANSITION_MS);
          if (progress > NOCT_DISP_W) progress = NOCT_DISP_W;
          sceneManager.drawWithOffset(previousScene, -progress, bootTime, blinkState, fanAnimFrame);
          sceneManager.drawWithOffset(currentScene, NOCT_DISP_W - progress, bootTime, blinkState, fanAnimFrame);
          if (progress >= NOCT_DISP_W) inTransition = false;
        }
        else
          sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
      }
      break;
    }
#endif // NOCT_FEATURE_MONITORING

#if NOCT_FEATURE_HACKER
    case MODE_RADAR:
    {
      int n = WiFi.scanComplete();
      if (n > 0 && wifiFilteredCount == 0) sortAndFilterWiFiNetworks();
      sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage,
                                   wifiFilteredCount > 0 ? wifiSortedIndices : nullptr,
                                   wifiFilteredCount);
      break;
    }
    case MODE_WIFI_PROBE_SCAN:
    case MODE_WIFI_EAPOL_SCAN:
    case MODE_WIFI_STATION_SCAN:
    case MODE_WIFI_PACKET_MONITOR:
    case MODE_WIFI_CHANNEL_ANALYZER:
    case MODE_WIFI_CHANNEL_ACTIVITY:
    case MODE_WIFI_PACKET_RATE:
    case MODE_WIFI_PINESCAN:
    case MODE_WIFI_MULTISSID:
    case MODE_WIFI_SIGNAL_STRENGTH:
    case MODE_WIFI_RAW_CAPTURE:
    case MODE_WIFI_AP_STA:
    case MODE_WIFI_SNIFF:
      wifiSniffManager.tick();
      sceneManager.drawWifiSniffMode(wifiSniffSelected, wifiSniffManager);
      break;
    case MODE_BLE_SPAM:
    case MODE_BLE_SOUR_APPLE:
    case MODE_BLE_SWIFTPAIR_MICROSOFT:
    case MODE_BLE_SWIFTPAIR_GOOGLE:
    case MODE_BLE_SWIFTPAIR_SAMSUNG:
    case MODE_BLE_FLIPPER_SPAM:
    {
      static int lastPhantomPayloadIndex = -1;
      if (bleManager.isActive()) bleManager.tick();
      sceneManager.drawBleSpammer(bleManager.getPacketCount());
      if (lastPhantomPayloadIndex >= 0 &&
          bleManager.getCurrentPayloadIndex() != lastPhantomPayloadIndex)
        display.applyGlitch();
      lastPhantomPayloadIndex = bleManager.getCurrentPayloadIndex();
      break;
    }
    case MODE_WIFI_TRAP:
      if (trapManager.isActive()) trapManager.tick();
      sceneManager.drawTrapMode(
          trapManager.getClientCount(), trapManager.getLogsCaptured(),
          trapManager.getLastPassword(), trapManager.getLastPasswordShowUntil(),
          trapManager.getClonedSSID());
      break;
#endif // NOCT_FEATURE_HACKER

#if NOCT_FEATURE_FORZA
    case MODE_GAME_FORZA:
    {
      bool showSplash = (now < forzaSplashUntil);
      sceneManager.drawForzaDash(forzaManager, showSplash, (uint32_t)WiFi.localIP());
      break;
    }
#endif

    default:
      break;
    }
  }

  if (settings.glitchEnabled) display.applyGlitch();
  if (toastUntil && now >= toastUntil) { toastUntil = 0; toastMsg[0] = '\0'; }
  if (toastUntil && now < toastUntil && toastMsg[0])
    sceneManager.drawToast(toastMsg);
  if (!displayManagerSent || (toastUntil && now < toastUntil && toastMsg[0]))
    display.sendBuffer();

  static unsigned long lastMainYield = 0;
  if (now - lastMainYield > 10) { yield(); lastMainYield = now; }
}
