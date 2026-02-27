/*
 * NOCTURNE_OS — BMW Assistant firmware (Heltec V4 / ESP32-S3).
 * Loop: BmwManager.tick -> DisplayManager (OLED). Button: short = next action,
 * long = execute. Double = menu. Config: NOCT_* in config.h.
 */
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <NimBLEDevice.h>

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

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
BmwManager bmwManager;
ObdClient obdClient;
AppState state;
SceneManager sceneManager(display, state);
DisplayManager displayManager(display, bmwManager);
AppModeManager appModeManager(bmwManager);
BatteryManager batteryManager;

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

static int bmwActionIndex = 0;

AppMode currentMode = MODE_BMW_ASSISTANT;

static bool needRedraw = true;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup()
{
  bootTime = millis();
  Serial.begin(115200);
  Serial.println("[NOCT] Nocturne OS " NOCTURNE_VERSION " (BMW Assistant)");
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

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

#if NOCT_OBD_ENABLED
  obdClient.begin(NOCT_OBD_TX_PIN, NOCT_OBD_RX_PIN);
  obdClient.setDataCallback(
      [](bool c, int r, int co, int o) { bmwManager.setObdData(c, r, co, o); });
#endif

  batteryManager.update(state);

  appModeManager.switchToMode(currentMode, MODE_BMW_ASSISTANT);
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
    if (item == 0)
    {
      if (!settings.carouselEnabled)
      {
        settings.carouselEnabled = true;
        settings.carouselIntervalSec = 5;
      }
      else if (settings.carouselIntervalSec == 5)
        settings.carouselIntervalSec = 10;
      else if (settings.carouselIntervalSec == 10)
        settings.carouselIntervalSec = 15;
      else
        settings.carouselEnabled = false;
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
    if (item == 0)
    {
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
    {
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
    {
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

void loop()
{
  unsigned long now = millis();

  if (currentMode == MODE_BMW_ASSISTANT)
    bmwManager.tick();

  ButtonEvent event = input.update();
  if (event != EV_NONE)
    lastInputTime = now;

  if (event == EV_DOUBLE && !quickMenuOpen)
  {
    quickMenuOpen = true;
    menuState = MENU_MAIN;
    menuLevel = 1;
    menuCategory = 2;
    quickMenuItem = 0;
    lastMenuEventTime = now;
    needRedraw = true;
    event = EV_NONE;
  }

  if (batTimer.check(now))
  {
    unsigned long nextInterval = batteryManager.update(state);
    batTimer.intervalMs = nextInterval;
    batTimer.lastMs = now;
  }

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
    if (settings.ledEnabled)
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
  if (quickMenuOpen)
    splashDone = true;

  if (quickMenuOpen)
  {
    if (rebootConfirmed && (now - rebootConfirmTime > 5000))
    {
      rebootConfirmed = false;
      needRedraw = true;
    }

    if (event != EV_NONE &&
        (now - lastMenuEventTime < MENU_EVENT_DEBOUNCE_MS))
      event = EV_NONE;

    if (event == EV_DOUBLE)
    {
      lastMenuEventTime = now;
      if (menuLevel == 1 && menuCategory == 2)
      {
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
        quickMenuItem = menuCategory;
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
    switch (currentMode)
    {
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
          Serial.printf("[BMW] Run action %d\n", bmwActionIndex);
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
    case MODE_CHARGE_ONLY:
      break;
    default:
      break;
    }
  }

  static unsigned long lastYield = 0;

  if (predatorMode)
  {
    display.clearBuffer();
    display.sendBuffer();
    if (now - lastYield > 10)
    {
      yield();
      lastYield = now;
    }
    return;
  }

  if (!needRedraw && !guiTimer.check(now) && !quickMenuOpen)
  {
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
      sceneManager.drawChargeOnlyScreen(state.batteryPct, state.isCharging,
                                        state.batteryVoltage);
      break;
    case MODE_BMW_ASSISTANT:
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

  static unsigned long lastMainYield = 0;
  if (now - lastMainYield > 10)
  {
    yield();
    lastMainYield = now;
  }
}
