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
#include "BootAnim.h"        // nocturne-core (lib src root)
#include "DisplayEngine.h"   // nocturne-core
#if NOCT_FEATURE_BMW
#include "modules/display/DisplayManager.h"
#endif
#include "modules/display/SceneManager.h"
#if NOCT_FEATURE_BMW
#include "modules/car/BmwManager.h"
#include "modules/car/ObdClient.h"
#endif
#include "BatteryManager.h"  // nocturne-core
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
#include "modules/network/WifiSniffManager.h"
#include "modules/ble/BleManager.h"
#endif
#if NOCT_FEATURE_WOLFPET
#include "modules/game/WolfPet.h"
#endif

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
#if NOCT_FEATURE_BMW
BmwManager bmwManager;
ObdClient obdClient;
#endif
AppState state;
SceneManager sceneManager(display, state);
#if NOCT_FEATURE_BMW
DisplayManager displayManager(display, bmwManager);
#endif
BatteryManager batteryManager;

#if NOCT_FEATURE_MONITORING
NetManager netManager;
#endif
#if NOCT_FEATURE_FORZA
ForzaManager forzaManager;
#endif
#if NOCT_FEATURE_HACKER
BleManager bleManager;
WifiSniffManager wifiSniffManager;
#endif
#if NOCT_FEATURE_WOLFPET
WolfPet wolfPet;
static int wolfActionSel = 0; // 0=feed 1=play 2=rest
#endif

AppModeManager appModeManager(
#if NOCT_FEATURE_BMW
    bmwManager,
#endif
#if NOCT_FEATURE_MONITORING
    netManager,
#endif
#if NOCT_FEATURE_FORZA
    forzaManager,
#endif
#if NOCT_FEATURE_HACKER
    wifiSniffManager,
    bleManager,
#endif
    0);

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
static bool screensaverManual = false; // System-menu standby; any press wakes it

unsigned long lastMenuEventTime = 0;
#define MENU_EVENT_DEBOUNCE_MS 150

static char toastMsg[20] = "";
static unsigned long toastUntil = 0;

#if NOCT_FEATURE_BMW
#define BMW_ACTION_COUNT 12
static int bmwActionIndex = 0;
#endif

static bool needRedraw = true;

AppMode currentMode = NOCT_DEFAULT_MODE;

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
unsigned long alertSnoozeUntil = 0; /* one click snoozes a live alert til here */
#define ALERT_SNOOZE_MS 60000UL     /* re-arm the alert after 60s */
#endif

#if NOCT_FEATURE_FORZA
static unsigned long forzaSplashUntil = 0;
#define FORZA_SPLASH_MS 3000
static bool forzaUdpStarted = false; /* UDP 5300 bound once WiFi is up */
static bool forzaAutoArmed = true;   /* auto-enter Forza once per boot on data */
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
  // Load display prefs BEFORE the boot animation so it respects rotation, colour
  // inversion and the special-effects toggle (#3 + clean/quick boot when FX off).
  {
    Preferences pe; pe.begin("nocturne", true);
    settings.displayInverted = pe.getBool("inverted", true);
    settings.glitchEnabled = pe.getBool("glitch", false);
    settings.colorInverted = pe.getBool("invert", false);
    pe.end();
  }
  display.setScreenFlipped(settings.displayInverted);
  display.setColorInverted(settings.colorInverted);
  display.setEffectsEnabled(settings.glitchEnabled);
  drawBootSequence(display, settings.glitchEnabled);
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
  settings.colorInverted = prefs.getBool("invert", false);
  settings.lowBrightnessDefault = prefs.getBool("lowBright", false);
  settings.displayTimeoutSec = prefs.getInt("dispTimeout", 0);
  if (settings.displayTimeoutSec != 0 && settings.displayTimeoutSec != 30 &&
      settings.displayTimeoutSec != 60)
    settings.displayTimeoutSec = 0;
  settings.pinnedScene = prefs.getInt("pinScene", -1);
  if (settings.pinnedScene < -1 || settings.pinnedScene >= NOCT_TOTAL_SCENES)
    settings.pinnedScene = -1;
  prefs.end();

  {
    uint8_t contrast = settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
    if (!settings.lowBrightnessDefault && settings.displayContrast >= 0)
      contrast = (uint8_t)settings.displayContrast;
    display.u8g2().setContrast(contrast);
  }
  display.setScreenFlipped(settings.displayInverted);
  display.setColorInverted(settings.colorInverted);
  display.setEffectsEnabled(settings.glitchEnabled);
  randomSeed(esp_random());

#if NOCT_FEATURE_MONITORING
#ifdef WIFI_NETWORKS
  // Multi-network auto-failover (#2): connect to the strongest reachable of the
  // list, reconnect across them on drop. Defined in secrets.h; falls back to the
  // single WIFI_SSID/WIFI_PASS when absent (e.g. CI stub).
  static const NetManager::WifiCred kWifiNets[] = WIFI_NETWORKS;
  netManager.begin(kWifiNets, sizeof(kWifiNets) / sizeof(kWifiNets[0]));
#else
  netManager.begin(WIFI_SSID, WIFI_PASS);
#endif
  netManager.setServer(PC_IP, TCP_PORT);
  currentMode = MODE_NORMAL;
  currentScene = (settings.pinnedScene >= 0) ? settings.pinnedScene : 0;
#else
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif

#if NOCT_OBD_ENABLED && NOCT_FEATURE_BMW
  obdClient.begin(NOCT_OBD_TX_PIN, NOCT_OBD_RX_PIN);
  obdClient.setDataCallback(
      [](bool c, int r, int co, int o) { bmwManager.setObdData(c, r, co, o); });
#endif

  batteryManager.update(state);

#if NOCT_FEATURE_WOLFPET
  wolfPet.begin();
#endif

#if !NOCT_FEATURE_MONITORING && NOCT_FEATURE_BMW
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
#if NOCT_FEATURE_MONITORING
  // Boot straight into PC monitoring so a fresh board auto-connects to the
  // server. Booting into the open menu (old behaviour) left the board idle with
  // no TCP link until someone pressed the button. The menu is one double-tap away.
  quickMenuOpen = false;
#else
  quickMenuOpen = true;
#endif
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
  bool ok = appModeManager.switchToMode(currentMode, mode);
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
    int k = 0;
    if (item == k++)
    {
      if (!appModeManager.switchToMode(currentMode, MODE_NORMAL))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
#if NOCT_FEATURE_FORZA
    else if (item == k++)
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
#if NOCT_FEATURE_WOLFPET
    else if (item == k++)
    {
      if (!appModeManager.switchToMode(currentMode, MODE_WOLFPET))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
      wolfActionSel = 0;
    }
#endif
    return true;
  }
#endif

#if NOCT_FEATURE_BMW
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
#endif

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
      display.setEffectsEnabled(settings.glitchEnabled);
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
    else if (item == 7)
    {
      settings.colorInverted = !settings.colorInverted;
      display.setColorInverted(settings.colorInverted);
      Preferences p; p.begin("nocturne", false);
      p.putBool("invert", settings.colorInverted); p.end();
      snprintf(toastMsg, sizeof(toastMsg),
               settings.colorInverted ? "INVERT ON" : "INVERT OFF");
      toastUntil = now + 800;
    }
#if NOCT_FEATURE_MONITORING
    else if (item == 8)
    {
      // Cycle pinned home scene: OFF -> 0 -> ... -> last -> OFF (#10)
      if (settings.pinnedScene < 0) settings.pinnedScene = 0;
      else if (settings.pinnedScene >= NOCT_TOTAL_SCENES - 1) settings.pinnedScene = -1;
      else settings.pinnedScene++;
      Preferences p; p.begin("nocturne", false);
      p.putInt("pinScene", settings.pinnedScene); p.end();
      if (settings.pinnedScene >= 0)
      {
        currentScene = settings.pinnedScene;
        snprintf(toastMsg, sizeof(toastMsg), "PIN:%s",
                 sceneManager.getSceneName(settings.pinnedScene));
      }
      else
        snprintf(toastMsg, sizeof(toastMsg), "PIN OFF");
      toastUntil = now + 1000;
    }
#endif
    return true;
  }

  if (cat == MCAT_SYSTEM)
  {
    int k = 0;
#if NOCT_FEATURE_BMW
    if (item == k++)
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
#endif
    if (item == k++)
    {
      if (!rebootConfirmed)
      { rebootConfirmed = true; rebootConfirmTime = now; }
      else
      { rebootConfirmed = false; esp_restart(); }
      return true;
    }
    if (item == k++)
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
    if (item == k++)
    {
      // Screensaver: full-screen standby animation until any button press.
      quickMenuOpen = false;
      rebootConfirmed = false;
      screensaverManual = true;
      needRedraw = true;
      return true;
    }
    if (item == k++)
    {
      quickMenuOpen = false;
      rebootConfirmed = false;
      esp_sleep_enable_ext0_wakeup((gpio_num_t)NOCT_BUTTON_PIN, 0);
      esp_deep_sleep_start();
      return true;
    }
    if (item == k++)
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
// Apply OLED contrast only when it changes, to avoid a redundant I2C control
// write on every frame. All contrast changes go through here so the cached
// value stays coherent.
static void applyContrast(uint8_t value)
{
  static int appliedContrast = -1;
  if ((int)value != appliedContrast)
  {
    display.u8g2().setContrast(value);
    appliedContrast = (int)value;
  }
}

void loop()
{
  unsigned long now = millis();

  // ── Background tasks ────────────────────────────────────────────────
#if NOCT_FEATURE_BMW
  if (currentMode == MODE_BMW_ASSISTANT)
    bmwManager.tick();
#endif
#if NOCT_FEATURE_WOLFPET
  wolfPet.tick(now); // pet lives in the background regardless of the active scene
#endif

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
          // Single parse: parsePayload() does the deserialize and returns false
          // on malformed JSON, so a separate validity-check parse is wasted work
          // (this runs ~twice/second). markDataReceived only on a good frame.
          if (netManager.parsePayload(buf, bufLen, &state))
          {
            netManager.markDataReceived(now);
            needRedraw = true;
            HardwareData &hw = state.hw;
            display.cpuGraph.push((float)hw.cl);
            display.gpuGraph.push((float)hw.gl);
            display.netDownGraph.setMax(2048);
            display.netDownGraph.push((float)hw.nd);
            display.netUpGraph.setMax(2048);
            display.netUpGraph.push((float)hw.nu);
            // External event (Alertmanager): toast when a new top alert fires.
            static char lastEventTop[21] = {0};
            if (state.events.count > 0 && state.events.top[0])
            {
              if (strncmp(lastEventTop, state.events.top, sizeof(lastEventTop)) != 0)
              {
                snprintf(toastMsg, sizeof(toastMsg), "! %s", state.events.top);
                toastUntil = now + 4000;
                strncpy(lastEventTop, state.events.top, sizeof(lastEventTop) - 1);
                lastEventTop[sizeof(lastEventTop) - 1] = '\0';
              }
            }
            else
              lastEventTop[0] = '\0';
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
  // Lazy-bind the Forza UDP listener once WiFi STA is up, so a Forza stream can
  // be auto-detected while in PC-monitoring mode.
  if (!forzaUdpStarted && WiFi.getMode() == WIFI_STA &&
      WiFi.status() == WL_CONNECTED)
  {
    forzaManager.begin();
    forzaUdpStarted = true;
  }
  if (currentMode == MODE_GAME_FORZA || forzaUdpStarted)
    forzaManager.tick();

  // Auto-enter Forza ONCE per boot when telemetry starts arriving. After a
  // manual exit it will NOT pull back in (forzaAutoArmed latches false).
  if (forzaAutoArmed && currentMode == MODE_NORMAL && splashDone &&
      !quickMenuOpen && forzaManager.isConnected())
  {
    if (appModeManager.switchToMode(currentMode, MODE_GAME_FORZA))
    {
      forzaSplashUntil = now + FORZA_SPLASH_MS;
      forzaAutoArmed = false;
      needRedraw = true;
      snprintf(toastMsg, sizeof(toastMsg), "FORZA LINK");
      toastUntil = now + 1500;
    }
  }

  // Auto-exit: telemetry stopped (signal lost) -> fall back to PC monitoring and
  // re-arm so Forza can auto-enter again when the stream returns (#11).
  if (currentMode == MODE_GAME_FORZA && splashDone && now > forzaSplashUntil &&
      !forzaManager.isConnected())
  {
    if (appModeManager.switchToMode(currentMode, MODE_NORMAL))
    {
      forzaAutoArmed = true;
      needRedraw = true;
      snprintf(toastMsg, sizeof(toastMsg), "FORZA LOST");
      toastUntil = now + 1500;
    }
  }
#endif

  // ── Input ───────────────────────────────────────────────────────────
  ButtonEvent event = input.update();
  if (event != EV_NONE)
    lastInputTime = now;

  // Manual screensaver: any button press wakes it; the press is consumed so it
  // doesn't also trigger navigation/menu.
  if (screensaverManual)
  {
    if (event != EV_NONE) { screensaverManual = false; needRedraw = true; }
    event = EV_NONE;
  }

  if (event == EV_DOUBLE && !quickMenuOpen)
  {
#if NOCT_FEATURE_MONITORING
    if (currentMode != MODE_NORMAL && currentMode != MODE_CHARGE_ONLY
#if NOCT_FEATURE_BMW
        && currentMode != MODE_BMW_ASSISTANT
#endif
       )
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
  // (pinMode is configured once in setup(); no need to repeat every loop.)
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
#if NOCT_FEATURE_BMW
  else if (currentMode == MODE_BMW_ASSISTANT)
  {
    if (bmwManager.isObdConnected() && bmwManager.getObdRpm() >= 5500)
    {
      bool flash = (now / 80) % 2 == 0;
      if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, flash ? HIGH : LOW);
    }
    else if (settings.ledEnabled) digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }
#endif
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
  // A NEW alert (rising edge) re-arms; one button click snoozes a live alert
  // for ALERT_SNOOZE_MS so the carousel/navigation become usable again.
  if (state.alertActive && !lastAlertActive)
    alertSnoozeUntil = 0;
  lastAlertActive = state.alertActive;
  bool alertLive = state.alertActive && now >= alertSnoozeUntil;

  if (alertLive)
  {
    int total = sceneManager.totalScenes();
    int target = state.alertTargetScene;
    if (total <= 0 || target < 0 || target >= total) target = 0;
    // #3: alternate between the alerting hardware scene and the MAIN temps
    // overview every 2.5s so both are visible while the alert is live.
    bool showOverview = ((now / 2500) % 2) == 1;
    currentScene = showOverview ? NOCT_SCENE_MAIN : target;
    needRedraw = true;
  }
  if (settings.carouselEnabled && !predatorMode && !alertLive)
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
  // Pinned "home" scene (#10): carousel off + input idle -> drift back to it.
  if (settings.pinnedScene >= 0 && !settings.carouselEnabled && !alertLive &&
      currentMode == MODE_NORMAL && !quickMenuOpen &&
      currentScene != settings.pinnedScene && lastInputTime != 0 &&
      (now - lastInputTime > NOCT_PIN_RETURN_MS))
  {
    previousScene = currentScene;
    currentScene = settings.pinnedScene;
    if (previousScene != currentScene)
    { inTransition = true; transitionStart = now; }
    needRedraw = true;
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
#if NOCT_FEATURE_BMW
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
#endif

    case MODE_CHARGE_ONLY:
      break;

#if NOCT_FEATURE_MONITORING
    case MODE_NORMAL:
      if (event == EV_SHORT && alertLive)
      {
        // One click snoozes the live alert and frees up navigation/carousel.
        alertSnoozeUntil = now + ALERT_SNOOZE_MS;
        snprintf(toastMsg, sizeof(toastMsg), "ALERT SNOOZED");
        toastUntil = now + 1500;
        needRedraw = true;
      }
      else if (event == EV_SHORT)
      {
        previousScene = currentScene;
        currentScene = (currentScene + 1) % sceneManager.totalScenes();
        if (previousScene != currentScene)
        { inTransition = true; transitionStart = now; }
        sceneManager.setWeatherExpanded(false); // collapse forecast on scene change (#14)
        lastCarousel = now;
        needRedraw = true;
      }
      else if (event == EV_LONG)
      {
        // Per-scene long-press: Weather -> multi-day forecast (#14); Claude ->
        // force-refresh Claude usage (cmd:claude); Services/Forest -> force a
        // status refresh (cmd:status — the same checks the Telegram bot runs,
        // #4); elsewhere -> toggle dim/low-brightness.
        if (currentScene == NOCT_SCENE_WEATHER)
        {
          sceneManager.setWeatherExpanded(!sceneManager.weatherExpanded());
          needRedraw = true;
        }
        else if (currentScene == NOCT_SCENE_CLAUDE)
        {
          netManager.print("cmd:claude\n");
          snprintf(toastMsg, sizeof(toastMsg), "Claude refresh");
          toastUntil = now + 1200; needRedraw = true;
        }
        else if (currentScene == NOCT_SCENE_SERVICES ||
                 currentScene == NOCT_SCENE_FOREST)
        {
          netManager.print("cmd:status\n");
          snprintf(toastMsg, sizeof(toastMsg), "Status refresh");
          toastUntil = now + 1200; needRedraw = true;
        }
        else
        {
          settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
          applyContrast(settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX);
          Preferences p; p.begin("nocturne", false);
          p.putBool("lowBright", settings.lowBrightnessDefault); p.end();
          needRedraw = true;
        }
      }
      break;
#endif
#if NOCT_FEATURE_WOLFPET
    case MODE_WOLFPET:
      if (event == EV_SHORT)
      { wolfActionSel = (wolfActionSel + 1) % 3; needRedraw = true; }
      else if (event == EV_LONG)
      {
        wolfPet.doAction(wolfActionSel);
        snprintf(toastMsg, sizeof(toastMsg), "%s!",
                 wolfActionSel == 0 ? "Fed" : wolfActionSel == 1 ? "Played" : "Rested");
        toastUntil = now + 1000;
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

  // Adaptive frame rate: run the full ~60 FPS only when something is actively
  // animating (scene transition, open menu, glitch effect, or a live alert);
  // otherwise fall back to ~30 FPS. Ambient decoration stays smooth (no freeze
  // risk — we slow, never skip), but steady screens cut I2C/CPU/display power.
  // Revert: set both intervals equal, or pin guiTimer to NOCT_REDRAW_INTERVAL_MS.
  bool animating = quickMenuOpen || state.alertActive || settings.glitchEnabled || screensaverManual;
#if NOCT_FEATURE_MONITORING
  animating = animating || inTransition;  // scene-carousel slide (monitoring only)
#endif
  guiTimer.intervalMs =
      animating ? NOCT_REDRAW_INTERVAL_MS : NOCT_REDRAW_IDLE_INTERVAL_MS;

  if (!needRedraw && !guiTimer.check(now) && !quickMenuOpen)
  {
    if (now - lastYield > 10) { yield(); lastYield = now; }
    return;
  }
  needRedraw = false;

  // Manual screensaver (System menu): full-screen standby until any button press.
  if (screensaverManual)
  {
    display.clearBuffer();
    sceneManager.drawIdleScreensaver(now);
    if (settings.glitchEnabled) display.applyGlitch();
    display.sendBuffer();
    return;
  }

  if (lastInputTime == 0) lastInputTime = now;
  // applyContrast() skips redundant I2C writes when the value is unchanged.
  bool dimByInput = (!quickMenuOpen && settings.displayTimeoutSec > 0 &&
      (now - lastInputTime > (unsigned long)settings.displayTimeoutSec * 1000));
  bool dimByPc = false;
#if NOCT_FEATURE_MONITORING
  // PC-idle dim: PC reports >10 min idle while we're showing its data.
  dimByPc = (currentMode == MODE_NORMAL && state.pcIdleSec >= 600);
#endif
  if (dimByInput || dimByPc)
    applyContrast(NOCT_CONTRAST_MIN);
  else
    applyContrast(settings.displayContrast);

  bool displayManagerSent = false;
  bool bmwHoldsBuffer = false;
#if NOCT_FEATURE_BMW
  bmwHoldsBuffer = (currentMode == MODE_BMW_ASSISTANT && !quickMenuOpen);
#endif
  if (!bmwHoldsBuffer)
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
        settings.displayContrast, settings.displayTimeoutSec,
        settings.pinnedScene, settings.colorInverted);
  }
  else
  {
    switch (currentMode)
    {
    case MODE_CHARGE_ONLY:
      sceneManager.drawChargeOnlyScreen(state.batteryPct, state.isCharging, state.batteryVoltage);
      break;

#if NOCT_FEATURE_BMW
    case MODE_BMW_ASSISTANT:
#if NOCT_OBD_ENABLED
      if (obdClient.isEnabled()) obdClient.tick();
#endif
      displayManagerSent = displayManager.update(now);
      break;
#endif

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
                                 state.pcClock[0] ? state.pcClock : nullptr,
                                 netManager.rssi(), netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging, state.batteryVoltage);
        if (inTransition)
        {
          unsigned long elapsed = now - transitionStart;
          int progress = (int)((elapsed * NOCT_TRANSITION_STEP) / NOCT_TRANSITION_MS);
          if (progress > NOCT_DISP_W) progress = NOCT_DISP_W;
          sceneManager.drawWithOffset(previousScene, -progress, bootTime, blinkState, fanAnimFrame);
          sceneManager.drawWithOffset(currentScene, NOCT_DISP_W - progress, bootTime, blinkState, fanAnimFrame);
          display.applyGlitch();  // cyberpunk scene-change glitch (transient)
          if (progress >= NOCT_DISP_W) inTransition = false;
        }
        else
          sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
        // #3: blinking hazard border while a critical alert is live.
        if (alertLive && (now / 350) % 2 == 0)
          display.drawHazardBorder();
      }
      break;
    }
#endif // NOCT_FEATURE_MONITORING

#if NOCT_FEATURE_WOLFPET
    case MODE_WOLFPET:
      sceneManager.drawWolfPet(wolfPet, wolfActionSel);
      break;
#endif

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
    case MODE_BLE_SCAN:
      if (bleManager.isScanning()) bleManager.tick();
      sceneManager.drawBleScan(bleManager);
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
