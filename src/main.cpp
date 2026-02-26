/*
 * NOCTURNE_OS — Firmware entry (ESP32 Heltec).
 * Loop: NetManager.tick (TCP/JSON) -> SceneManager.draw (current scene) ->
 * sendBuffer. Button: short = next scene / menu; long = predator (screen off,
 * LED breath). Config: NOCT_* in config.h; WiFi/PC in secrets.h.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <NimBLEDevice.h>

// -----------------------------------

#include "AppModeManager.h"
#include "InputHandler.h"
#include "MenuHandler.h"
#include "modules/ble/BleManager.h"
#include "modules/display/BootAnim.h"
#include "modules/display/DisplayEngine.h"
#include "modules/display/DisplayManager.h"
#include "modules/network/NetManager.h"
#include "modules/display/SceneManager.h"
#include "modules/car/BmwManager.h"
#include "modules/car/ObdClient.h"
#include "modules/system/BatteryManager.h"
#include "nocturne/Types.h"
#include "nocturne/config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Local constants (after includes, before any global object instantiations)
// ---------------------------------------------------------------------------
#define NOCT_CONFIG_MSG_MS 1500
#define NOCT_BAT_READ_INTERVAL_MS 5000
#define NOCT_BAT_SAMPLES 20
#define NOCT_BAT_CALIBRATION 1.1f
static int menuHackerGroup = 0;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
NetManager netManager;
BleManager bleManager;
BmwManager bmwManager;
ObdClient obdClient;
AppState state;
SceneManager sceneManager(display, state);
DisplayManager displayManager(display, bmwManager);
AppModeManager appModeManager(netManager, bleManager, bmwManager);
BatteryManager batteryManager;

InputSystem input(NOCT_BUTTON_PIN);
IntervalTimer guiTimer(NOCT_REDRAW_INTERVAL_MS);
IntervalTimer
    batTimer(NOCT_BAT_READ_INTERVAL_STABLE_MS); // Will be updated adaptively

Settings &settings = state.settings;
unsigned long bootTime = 0;
unsigned long splashStart = 0;
bool splashDone = false;
int currentScene = 0;
int previousScene = 0;
unsigned long transitionStart = 0;
bool inTransition = false;

unsigned long lastCarousel = 0;
unsigned long lastFanAnim = 0;
static unsigned long idleStateEnteredMs = 0; /* For 10s -> wolf screensaver */
int fanAnimFrame = 0;
bool blinkState = false;
int alertBlinkCounter = 0;
bool lastAlertActive = false;
unsigned long lastBlink = 0;

bool predatorMode = false;
unsigned long predatorEnterTime = 0;

bool quickMenuOpen = false;
int quickMenuItem = 0;
int menuLevel = 0;    // 0 = categories (Config/WiFi/Tools/System), 1 = submenu
int menuCategory = 0; // which category when in submenu (0..3)

enum MenuState
{
  MENU_MAIN
};
MenuState menuState = MENU_MAIN;
int submenuIndex =
    0; // Оставлено для совместимости с drawMenu, но не используется
bool rebootConfirmed = false;
unsigned long rebootConfirmTime = 0;
static unsigned long lastInputTime = 0;  // for display timeout

// Защита от множественных срабатываний событий
unsigned long lastMenuEventTime = 0;
#define MENU_EVENT_DEBOUNCE_MS 150

// Toast: brief on-screen message (FAIL on mode error, Saved on Config save)
static char toastMsg[20] = "";
static unsigned long toastUntil = 0;

// BMW Assistant: selected action index (short = next, long = execute)
static int bmwActionIndex = 0;

AppMode currentMode = MODE_NORMAL;

static bool needRedraw = true;

static bool vextPinState = false; // Track Vext pin (never drive HIGH)

static void VextON()
{
  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(100));
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup()
{
  bootTime = millis();
  Serial.begin(115200);
  Serial.println("[NOCT] Nocturne OS " NOCTURNE_VERSION);
  setCpuFrequencyMhz(240); // V4: max speed (also set in platformio.ini)

  // ========================================================================
  // Power-On Kick: Vext (36) LOW = power OLED. RST (21) pulse, then begin().
  // Do NOT use GPIO 18 as RST — 18 is SCL (I2C). Standard V4: Vext=36, RST=21.
  // ========================================================================
  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  vextPinState = true;
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
  /* Check for demo-mode boot: hold PRG for NOCT_DEMO_BOOT_HOLD_MS */
  pinMode(NOCT_BUTTON_PIN, INPUT_PULLUP);
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
  /* Boot directly into BMW Assistant — no menu. */
  quickMenuOpen = false;
  menuLevel = 0;
  menuCategory = 0;
  quickMenuItem = 0;
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(200));
  digitalWrite(NOCT_LED_ALERT_PIN, LOW);

  // Настройка ADC для чтения батареи (GPIO 1 на ESP32-S3)
  // На ESP32-S3 GPIO 1 соответствует ADC1_CH0
  analogReadResolution(
      12); // 12-bit resolution (0-4095)
// Устанавливаем максимальное затухание для измерения до ~3.1V (с учетом
// делителя 1/2 это ~6.2V) Для ESP32-S3 используем analogSetPinAttenuation, для
// других версий - analogSetAttenuation
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
           // ESP32-S3: используем analogSetPinAttenuation с константой ADC_11db
  analogSetPinAttenuation(NOCT_BAT_PIN, ADC_11db);
#else
           // Для других версий ESP32
  analogSetAttenuation(ADC_11db);
#endif

  // Первое чтение для инициализации ADC
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
  if (settings.displayContrast > 255)
    settings.displayContrast = 255;
  if (settings.displayContrast < 0)
    settings.displayContrast = 0;
  settings.displayInverted = prefs.getBool("inverted", true);
  settings.glitchEnabled = prefs.getBool("glitch", false);
  settings.lowBrightnessDefault = prefs.getBool("lowBright", false);
  settings.displayTimeoutSec = prefs.getInt("dispTimeout", 0);
  if (settings.displayTimeoutSec != 0 && settings.displayTimeoutSec != 30 &&
      settings.displayTimeoutSec != 60)
    settings.displayTimeoutSec = 0;
  prefs.end();

  {
    uint8_t contrast =
        settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
    if (!settings.lowBrightnessDefault && settings.displayContrast >= 0)
      contrast = (uint8_t)settings.displayContrast;
    display.u8g2().setContrast(contrast);
  }
  display.setScreenFlipped(settings.displayInverted);
  randomSeed(esp_random());

  netManager.begin(WIFI_SSID, WIFI_PASS);
  netManager.setServer(PC_IP, TCP_PORT);

#if NOCT_OBD_ENABLED
  obdClient.begin(NOCT_OBD_TX_PIN, NOCT_OBD_RX_PIN);
  obdClient.setDataCallback(
      [](bool c, int r, int co, int o) { bmwManager.setObdData(c, r, co, o); });
#endif

  batteryManager.update(state);

  /* Enter BMW Assistant mode (WiFi off, BmwManager begun). */
  appModeManager.switchToMode(currentMode, MODE_BMW_ASSISTANT);
  /* Apply saved demo state (or boot-time hold: already written to NVS). */
  {
    Preferences prefs;
    prefs.begin("nocturne", true);
    bool demo = prefs.getBool("bmw_demo", false);
    prefs.end();
    if (bootDemoRequested)
      demo = true;
    bmwManager.setDemoMode(demo);
  }
}

static bool handleMenuActionByCategory(int cat, int item, unsigned long now)
{
  if (cat == 0)
  {
    // BMW: BMW Assistant (0), BMW Demo (1)
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
  if (cat == 1)
  {
    // Config: AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT
    if (item == 0)
    {
      if (!settings.carouselEnabled)
      {
        settings.carouselEnabled = true;
        settings.carouselIntervalSec = 5;
      }
      else if (settings.carouselIntervalSec == 5)
      {
        settings.carouselIntervalSec = 10;
      }
      else if (settings.carouselIntervalSec == 10)
      {
        settings.carouselIntervalSec = 15;
      }
      else
      {
        settings.carouselEnabled = false;
      }
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("carousel", settings.carouselEnabled);
      prefs.putInt("carouselSec", settings.carouselIntervalSec);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    }
    else if (item == 1)
    {
      settings.displayInverted = !settings.displayInverted;
      display.setScreenFlipped(settings.displayInverted);
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("inverted", settings.displayInverted);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    }
    else if (item == 2)
    {
      settings.glitchEnabled = !settings.glitchEnabled;
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("glitch", settings.glitchEnabled);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    }
    else if (item == 3)
    {
      settings.ledEnabled = !settings.ledEnabled;
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("led", settings.ledEnabled);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    }
    else if (item == 4)
    {
      settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
      settings.displayContrast =
          settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
      display.u8g2().setContrast(settings.displayContrast);
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("lowBright", settings.lowBrightnessDefault);
      prefs.putInt("contrast", settings.displayContrast);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    }
    else if (item == 5)
    {
      // CONTRAST: cycle 12 -> 64 -> 128 -> 192 -> 255 -> 12
      const int levels[] = {NOCT_CONTRAST_MIN, 64, 128, 192, NOCT_CONTRAST_MAX};
      const int nLevels = sizeof(levels) / sizeof(levels[0]);
      int i = 0;
      while (i < nLevels && levels[i] != settings.displayContrast)
        i++;
      settings.displayContrast = levels[(i + 1) % nLevels];
      display.u8g2().setContrast(settings.displayContrast);
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putInt("contrast", settings.displayContrast);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "C:%d", settings.displayContrast);
      toastUntil = now + 800;
    }
    else if (item == 6)
    {
      // TIMEOUT: 0 -> 30 -> 60 -> 0 (display dim after sec of no input)
      if (settings.displayTimeoutSec == 0)
        settings.displayTimeoutSec = 30;
      else if (settings.displayTimeoutSec == 30)
        settings.displayTimeoutSec = 60;
      else
        settings.displayTimeoutSec = 0;
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putInt("dispTimeout", settings.displayTimeoutSec);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg),
              settings.displayTimeoutSec ? "T:%ds" : "T:OFF",
              settings.displayTimeoutSec);
      toastUntil = now + 800;
    }
    return true;
  }
  if (cat == 2)
  {
    // System: Demo, REBOOT, CHARGE ONLY, POWER OFF, VERSION
    if (item == 0)
    { // Demo toggle
      Preferences prefs;
      prefs.begin("nocturne", false);
      bool demo = prefs.getBool("bmw_demo", false);
      demo = !demo;
      prefs.putBool("bmw_demo", demo);
      prefs.end();
      bmwManager.setDemoMode(demo);
      snprintf(toastMsg, sizeof(toastMsg), demo ? "Demo ON" : "Demo OFF");
      toastUntil = now + 1200;
      return true;
    }
    if (item == 1)
    { // REBOOT
      if (!rebootConfirmed)
      {
        rebootConfirmed = true;
        rebootConfirmTime = now;
      }
      else
      {
        rebootConfirmed = false;
        esp_restart();
      }
      return true;
    }
    if (item == 2)
    { // CHARGE ONLY
      quickMenuOpen = false;
      rebootConfirmed = false;
      if (!appModeManager.switchToMode(currentMode, MODE_CHARGE_ONLY))
      {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
      return true;
    }
    if (item == 3)
    { // POWER OFF (deep sleep; GPIO0 wake)
      quickMenuOpen = false;
      rebootConfirmed = false;
      // Show "Off" briefly then deep sleep. Wake on GPIO0 (button).
      esp_sleep_enable_ext0_wakeup((gpio_num_t)NOCT_BUTTON_PIN, 0); // LOW = pressed
      esp_deep_sleep_start();
      return true;
    }
    if (item == 4)
    { // VERSION
      snprintf(toastMsg, sizeof(toastMsg), "v" NOCTURNE_VERSION);
      toastUntil = now + 2000;
      return true;
    }
  }
  return false;
}

void loop()
{
  unsigned long now = millis();

  // 1. Critical background tasks — only when in NORMAL (PC monitoring). BMW-only boots to BMW_ASSISTANT.
  bool pcMonitoringActive =
      (currentMode == MODE_NORMAL && splashDone && !quickMenuOpen);
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
      {
        netManager.appendLineBuffer(c);
      }
    }
  }

  // 2. Non-blocking input
  ButtonEvent event = input.update();
  if (event != EV_NONE)
    lastInputTime = now;

  // 3. Global: DOUBLE = open menu when closed; when menu already open, leave
  // EV_DOUBLE for menu block (back / close). In BMW mode open System-only menu.
  if (event == EV_DOUBLE && !quickMenuOpen)
  {
    if (currentMode == MODE_BMW_ASSISTANT)
    {
      /* BMW-only firmware: open minimal System menu (Charge only, Power off, Version). */
      quickMenuOpen = true;
      menuState = MENU_MAIN;
      menuLevel = 1;
      menuCategory = 2; /* System */
      quickMenuItem = 0;
    }
    else
    {
      if (currentMode != MODE_NORMAL)
        appModeManager.exitToNormal(currentMode);
      quickMenuOpen = true;
      menuState = MENU_MAIN;
      menuLevel = 0;
      menuCategory = 0;
      quickMenuItem = 0;
    }
    lastMenuEventTime = now;
    needRedraw = true;
    event = EV_NONE;
  }
  else if (event == EV_LONG && currentMode == MODE_NORMAL && !quickMenuOpen)
  {
    settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
    uint8_t contrast =
        settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
    display.u8g2().setContrast(contrast);
    Preferences prefs;
    prefs.begin("nocturne", false);
    prefs.putBool("lowBright", settings.lowBrightnessDefault);
    prefs.end();
    needRedraw = true;
    event = EV_NONE;
  }

  // Alert / carousel / screen sync / fan animation
  if (state.alertActive)
  {
    int total = sceneManager.totalScenes();
    int target = state.alertTargetScene;
    currentScene = (total > 0 && target >= 0 && target < total)
                       ? target
                       : 0;
    needRedraw = true;
  }
  if (settings.carouselEnabled && !predatorMode && !state.alertActive)
  {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs)
    {
      needRedraw = true;
      previousScene = currentScene;
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
      if (previousScene != currentScene)
      {
        inTransition = true;
        transitionStart = now;
      }
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
  {
    fanAnimFrame = (fanAnimFrame + 1) % 12;
    lastFanAnim = now;
  }

  // Battery: periodic update with adaptive interval
  if (batTimer.check(now))
  {
    unsigned long nextInterval = batteryManager.update(state);
    batTimer.intervalMs = nextInterval;
    batTimer.lastMs = now;
  }

  // LED: predator breath / alert blink / Forza shift light / cursor blink
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  if (predatorMode)
  {
    unsigned long t = (now - predatorEnterTime) / 20;
    int breath = (int)(128 + 127 * sin(t * 0.1f));
    if (breath < 0)
      breath = 0;
    if (settings.ledEnabled)
      analogWrite(NOCT_LED_ALERT_PIN, breath);
    else
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }
  else if (currentMode == MODE_BMW_ASSISTANT)
  {
    if (bmwManager.isObdConnected() && bmwManager.getObdRpm() >= 5500)
    {
      bool flash = (now / 80) % 2 == 0;
      if (settings.ledEnabled)
        digitalWrite(NOCT_LED_ALERT_PIN, flash ? HIGH : LOW);
    }
    else
    {
      if (settings.ledEnabled)
        digitalWrite(NOCT_LED_ALERT_PIN, LOW);
    }
  }
  else
  {
    if (state.alertActive && !lastAlertActive)
    {
      alertBlinkCounter = 0;
      blinkState = true;
      if (settings.ledEnabled)
        digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
      lastBlink = now;
    }
    lastAlertActive = state.alertActive;
    if (state.alertActive)
    {
      if (now - lastBlink >= NOCT_ALERT_LED_BLINK_MS)
      {
        lastBlink = now;
        if (alertBlinkCounter < NOCT_ALERT_MAX_BLINKS * 2)
        {
          blinkState = !blinkState;
          if (settings.ledEnabled)
            digitalWrite(NOCT_LED_ALERT_PIN, blinkState ? HIGH : LOW);
          alertBlinkCounter++;
        }
        else
        {
          blinkState = false;
          if (settings.ledEnabled)
            digitalWrite(NOCT_LED_ALERT_PIN, LOW);
        }
      }
    }
    else
    {
      if (settings.ledEnabled)
        digitalWrite(NOCT_LED_ALERT_PIN, LOW);
      if (now - lastBlink > 500)
      {
        blinkState = !blinkState;
        lastBlink = now;
      }
    }
    if (!settings.ledEnabled)
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }

  if (!splashDone)
  {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= (unsigned long)NOCT_SPLASH_MS)
    {
      splashDone = true;
      needRedraw = true;
    }
  }
  // Render guarantee: menu has highest priority; never block menu with splash
  if (quickMenuOpen)
    splashDone = true;

  // 4. Mode handling: menu vs app
  if (quickMenuOpen)
  {
    // Сброс подтверждения перезагрузки при таймауте (5 секунд)
    if (rebootConfirmed && (now - rebootConfirmTime > 5000))
    {
      rebootConfirmed = false;
      needRedraw = true;
    }

    // Защита от множественных срабатываний событий
    if (event != EV_NONE &&
        (now - lastMenuEventTime < MENU_EVENT_DEBOUNCE_MS))
    {
      event = EV_NONE; // Игнорируем событие, если прошло слишком мало времени
    }

    // --- MENU LOGIC: level 0 = 3 categories (BMW, Config, System), level 1 = submenu
    if (event == EV_DOUBLE)
    {
      lastMenuEventTime = now;
      if (currentMode == MODE_BMW_ASSISTANT && menuLevel == 1 && menuCategory == 2)
      {
        /* BMW minimal menu: one double-press closes. */
        quickMenuOpen = false;
        rebootConfirmed = false;
      }
      else if (menuLevel == 2)
      {
        menuLevel = 1;
        quickMenuItem = 0;
        rebootConfirmed = false;
      }
      else if (menuLevel == 1)
      {
        menuLevel = 0;
        quickMenuItem = menuCategory; // Categories 0..2 in order
        rebootConfirmed = false;
      }
      else
      {
        quickMenuOpen = false;
        rebootConfirmed = false;
      }
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
        if (menuCategory == 2 && quickMenuItem != 1)
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
      {
        menuLevel = 1;
        menuCategory = quickMenuItem;
        quickMenuItem = 0;
      }
      else if (menuLevel == 1)
      {
        bool ok = handleMenuActionByCategory(menuCategory, quickMenuItem, now);
        if (ok)
          needRedraw = true;
      }
    }
  }
  else
  {
    // App mode: SHORT = navigate, LONG = action (or predator in normal)
    switch (currentMode)
    {
    case MODE_NORMAL:
      if (event == EV_SHORT && !state.alertActive)
      {
        previousScene = currentScene;
        currentScene = (currentScene + 1) % sceneManager.totalScenes();
        if (previousScene != currentScene)
        {
          inTransition = true;
          transitionStart = now;
        }
        lastCarousel = now;
        needRedraw = true;
      }
      break;
    case MODE_BMW_ASSISTANT:
      if (event == EV_SHORT)
      {
        bmwActionIndex = (bmwActionIndex + 1) % BMW_ACTION_COUNT;
        needRedraw = true;
      }
      else if (event == EV_LONG)
      {
        if (!bmwManager.isIbusSynced())
        {
#if NOCT_BMW_DEBUG
          Serial.println("[BMW] Run ignored: No IBus");
#endif
          snprintf(toastMsg, sizeof(toastMsg), "No IBus");
          toastUntil = now + 1500;
        }
        else
        {
#if NOCT_BMW_DEBUG
          Serial.printf("[BMW] Run action %d (0=Goodbye..11=DoorLock)\n", bmwActionIndex);
#endif
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
#if NOCT_BMW_DEBUG
          Serial.println("[BMW] Run done");
#endif
        }
        needRedraw = true;
      }
      break;
    default:
      break;
    }
  }

  // 5. Render (throttle: needRedraw or guiTimer)
  static unsigned long lastYield = 0;

  if (predatorMode)
  {
    display.clearBuffer();
    display.sendBuffer();
    // Минимальная задержка только при необходимости
    if (now - lastYield > 10)
    {
      yield();
      lastYield = now;
    }
    return;
  }

  // Always render when menu is open so it is never overwritten by a skipped
  // frame
  if (!needRedraw && !guiTimer.check(now) && !quickMenuOpen)
  {
    // Минимальная задержка только при необходимости
    if (now - lastYield > 10)
    {
      yield();
      lastYield = now;
    }
    return;
  }
  needRedraw = false;

  if (lastInputTime == 0)
    lastInputTime = now;
  // Display timeout: dim to NOCT_CONTRAST_MIN (not 0) so user sees device is on
  if (!quickMenuOpen && settings.displayTimeoutSec > 0 &&
      (now - lastInputTime > (unsigned long)settings.displayTimeoutSec * 1000))
    display.u8g2().setContrast(NOCT_CONTRAST_MIN);
  else
    display.u8g2().setContrast(settings.displayContrast);

  bool displayManagerSent = false;
  if (!(currentMode == MODE_BMW_ASSISTANT && !quickMenuOpen))
    display.clearBuffer();
  bool signalLost = splashDone && netManager.isSignalLost(now);
  if (signalLost && netManager.isTcpConnected() && netManager.hasReceivedData())
    netManager.disconnectTcp();

  if (!splashDone)
  {
    display.drawSplash();
  }
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
    case MODE_NORMAL:
    {
      /* Idle = no WiFi or no server (linking). After 10s show wolf screensaver.
       * Do NOT use isSearchMode() here: searchMode_ is true when WiFi is down
       * or when linking, which are exactly the states we want screensaver for.
       */
      bool idleState =
          !netManager.isWifiConnected() || !netManager.isTcpConnected();
      if (signalLost && netManager.isTcpConnected())
        idleState =
            false; /* TCP up but no data → show SEARCH, not screensaver */
      if (!idleState)
      {
        idleStateEnteredMs = 0;
      }
      if (idleState && idleStateEnteredMs == 0)
      {
        idleStateEnteredMs = now;
      }
      bool showScreensaver =
          idleState && idleStateEnteredMs != 0 &&
          (now - idleStateEnteredMs >= (unsigned long)NOCT_IDLE_SCREENSAVER_MS);

      if (!netManager.isWifiConnected())
      {
        if (showScreensaver)
        {
          sceneManager.drawIdleScreensaver(now);
          display.applyGlitch(); /* Always light glitch on screensaver */
        }
        else
        {
          sceneManager.drawNoSignal(false, false, 0, blinkState);
        }
      }
      else if (!netManager.isTcpConnected())
      {
        if (showScreensaver)
        {
          sceneManager.drawIdleScreensaver(now);
          display.applyGlitch(); /* Always light glitch on screensaver */
        }
        else
        {
          sceneManager.drawConnecting(netManager.rssi(), blinkState);
        }
      }
      else if (netManager.isSearchMode() || signalLost)
      {
        int scanPhase = (int)(now / 100) % 12;
        sceneManager.drawSearchMode(scanPhase);
      }
      else
      {
        display.drawGlobalHeader(sceneManager.getSceneName(currentScene),
                                 nullptr, netManager.rssi(),
                                 netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                     state.batteryVoltage);
        if (inTransition)
        {
          unsigned long elapsed = now - transitionStart;
          int progress =
              (int)((elapsed * NOCT_TRANSITION_STEP) / NOCT_TRANSITION_MS);
          if (progress > NOCT_DISP_W)
            progress = NOCT_DISP_W;
          int offsetA = -progress;
          int offsetB = NOCT_DISP_W - progress;
          sceneManager.drawWithOffset(previousScene, offsetA, bootTime,
                                      blinkState, fanAnimFrame);
          sceneManager.drawWithOffset(currentScene, offsetB, bootTime,
                                      blinkState, fanAnimFrame);
          if (progress >= NOCT_DISP_W)
            inTransition = false;
        }
        else
        {
          sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
        }
      }
      break;
    }
    case MODE_CHARGE_ONLY:
      sceneManager.drawChargeOnlyScreen(state.batteryPct, state.isCharging,
                                        state.batteryVoltage);
      break;
    case MODE_BMW_ASSISTANT:
      bmwManager.tick();
#if NOCT_OBD_ENABLED
      if (obdClient.isEnabled())
        obdClient.tick();
#endif
      displayManagerSent = displayManager.update(now);
      break;
    default:
      break;
    }
  }

  if (settings.glitchEnabled)
    display.applyGlitch();
  if (toastUntil && now >= toastUntil)
  {
    toastUntil = 0;
    toastMsg[0] = '\0';
  }
  if (toastUntil && now < toastUntil && toastMsg[0])
    sceneManager.drawToast(toastMsg);
  if (!displayManagerSent || (toastUntil && now < toastUntil && toastMsg[0]))
    display.sendBuffer();

  // Оптимизированная задержка - yield только при необходимости
  static unsigned long lastMainYield = 0;
  if (now - lastMainYield > 10)
  {
    yield();
    lastMainYield = now;
  }
}
