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
#include <Wire.h>

#include "modules/BleManager.h"
#include "modules/BootAnim.h"
#include "modules/DisplayEngine.h"
#include "modules/KickManager.h"
#include "modules/LoraManager.h"
#include "modules/NetManager.h"
#include "modules/SceneManager.h"
#include "modules/TrapManager.h"
#include "modules/UsbManager.h"
#include "modules/VaultManager.h"
#include "nocturne/Types.h"
#include "nocturne/config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Local constants (after includes, before any global object instantiations)
// ---------------------------------------------------------------------------
#define NOCT_REDRAW_INTERVAL_MS 500
#define NOCT_CONFIG_MSG_MS 1500
#define NOCT_BAT_READ_INTERVAL_MS 5000
#define NOCT_BAT_SAMPLES 20
#define NOCT_BAT_CALIBRATION 1.1f
#define WOLF_MENU_ITEMS 9
#define MAIN_IDX_WIFI 3
#define MAIN_IDX_RADIO 4
#define MAIN_IDX_BLE 5
#define MAIN_IDX_USB 6
#define MAIN_IDX_TOOLS 7
#define MAIN_IDX_EXIT 8
#define SUB_WIFI_COUNT 4
#define SUB_RADIO_COUNT 4
#define SUB_TOOLS_COUNT 4

// ---------------------------------------------------------------------------
// Input: Event-based (SHORT = navigate, LONG = select, DOUBLE = back/menu)
// ---------------------------------------------------------------------------
enum ButtonEvent { EV_NONE, EV_SHORT, EV_LONG, EV_DOUBLE };

struct InputSystem {
  const int pin;
  unsigned long pressTime = 0;
  unsigned long releaseTime = 0;
  bool btnState = false;
  int clickCount = 0;

  InputSystem(int p) : pin(p) { pinMode(pin, INPUT_PULLUP); }

  ButtonEvent update() {
    bool down = (digitalRead(pin) == LOW);
    unsigned long now = millis();
    ButtonEvent event = EV_NONE;

    if (down && !btnState) {
      btnState = true;
      pressTime = now;
    } else if (!down && btnState) {
      btnState = false;
      unsigned long duration = now - pressTime;
      if (duration > 50 && duration < 500) {
        clickCount++;
        releaseTime = now;
      } else if (duration >= 500) {
        clickCount = 0;
        return EV_LONG;
      }
    }

    if (clickCount > 0 && !btnState && (now - releaseTime > 250)) {
      if (clickCount == 1)
        event = EV_SHORT;
      else if (clickCount >= 2)
        event = EV_DOUBLE;
      clickCount = 0;
    }
    return event;
  }
};

struct IntervalTimer {
  unsigned long intervalMs;
  unsigned long lastMs = 0;
  IntervalTimer(unsigned long interval) : intervalMs(interval) {}
  bool check(unsigned long now) {
    if (now - lastMs >= intervalMs) {
      lastMs = now;
      return true;
    }
    return false;
  }
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
NetManager netManager;
LoraManager loraManager;
BleManager bleManager;
UsbManager usbManager;
TrapManager trapManager;
KickManager kickManager;
VaultManager vaultManager;
AppState state;
SceneManager sceneManager(display, state);

InputSystem input(NOCT_BUTTON_PIN);
IntervalTimer guiTimer(NOCT_REDRAW_INTERVAL_MS);
IntervalTimer batTimer(NOCT_BAT_READ_INTERVAL_MS);

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
int fanAnimFrame = 0;
bool blinkState = false;
int alertBlinkCounter = 0;
bool lastAlertActive = false;
unsigned long lastBlink = 0;

bool predatorMode = false;
unsigned long predatorEnterTime = 0;

bool quickMenuOpen = false;
int quickMenuItem = 0;

enum MenuState { MENU_MAIN, MENU_SUB_WIFI, MENU_SUB_RADIO, MENU_SUB_TOOLS };
MenuState menuState = MENU_MAIN;
int submenuIndex = 0;

// --- Cyberdeck Modes ---
enum AppMode {
  MODE_NORMAL,
  MODE_DAEMON,
  MODE_RADAR,
  MODE_WIFI_DEAUTH,
  MODE_LORA,
  MODE_LORA_JAM,
  MODE_BLE_SPAM,
  MODE_BADWOLF,
  MODE_WIFI_TRAP,
  MODE_VAULT,
  MODE_LORA_SENSE
};
AppMode currentMode = MODE_NORMAL;

// --- Netrunner WiFi Scanner ---
int wifiScanSelected = 0;
int wifiListPage = 0;

static bool needRedraw = true;

/** Get battery voltage from ADC (GPIO 1). Heltec V4: 1/2 divider → ×2; 1.1
 * calibration. */
static float getBatteryVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < NOCT_BAT_SAMPLES; i++) {
    sum += analogRead(NOCT_BAT_PIN);
    if (NOCT_BAT_READ_DELAY > 0)
      delay(NOCT_BAT_READ_DELAY);
  }
  float avg = (float)sum / (float)NOCT_BAT_SAMPLES;
  return (avg / 4095.0f) * 3.3f * 2.0f * NOCT_BAT_CALIBRATION;
}

/** Update state.batteryVoltage, state.batteryPct, state.isCharging. Call every
 * 5s from loop. */
static void updateBatteryState() {
  state.batteryVoltage = getBatteryVoltage();
  state.isCharging = (state.batteryVoltage > 4.3f);
  float v = state.batteryVoltage;
  int pct =
      (int)((v - NOCT_VOLT_MIN) / (NOCT_VOLT_MAX - NOCT_VOLT_MIN) * 100.0f);
  state.batteryPct = constrain(pct, 0, 100);
}

static void VextON() {
  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  delay(100);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  bootTime = millis();
  setCpuFrequencyMhz(240); // V4: max speed (also set in platformio.ini)
  VextON();
  pinMode(NOCT_RST_PIN, OUTPUT);
  digitalWrite(NOCT_RST_PIN, LOW);
  delay(50);
  digitalWrite(NOCT_RST_PIN, HIGH);
  delay(50);

  display.begin();
  drawBootSequence(display);
  splashDone = true;
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
  delay(200);
  digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  pinMode(NOCT_BUTTON_PIN, INPUT_PULLUP);

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
  settings.displayInverted = prefs.getBool("inverted", false);
  settings.glitchEnabled = prefs.getBool("glitch", false);
  prefs.end();

  display.u8g2().setContrast((uint8_t)settings.displayContrast);
  display.setScreenFlipped(settings.displayInverted);
  randomSeed(esp_random());

  netManager.begin(WIFI_SSID, WIFI_PASS);
  netManager.setServer(PC_IP, TCP_PORT);
  vaultManager.begin();

  updateBatteryState();
}

// ---------------------------------------------------------------------------
// Loop: Event-based input, switch(mode) render, header then battery
// ---------------------------------------------------------------------------
static void exitAppModeToNormal() {
  if (currentMode == MODE_LORA || currentMode == MODE_LORA_JAM ||
      currentMode == MODE_LORA_SENSE) {
    if (currentMode == MODE_LORA_JAM)
      loraManager.stopJamming();
    loraManager.setMode(false);
  }
  if (currentMode == MODE_BLE_SPAM) {
    bleManager.stop();
    WiFi.mode(WIFI_STA);
  }
  if (currentMode == MODE_BADWOLF)
    usbManager.stop();
  if (currentMode == MODE_WIFI_TRAP) {
    trapManager.stop();
    WiFi.mode(WIFI_STA);
  }
  if (currentMode == MODE_WIFI_DEAUTH)
    kickManager.stopAttack();
  currentMode = MODE_NORMAL;
  WiFi.scanDelete();
  netManager.setSuspend(false);
  Serial.println("[SYS] NETMANAGER RESUMED.");
}

void loop() {
  unsigned long now = millis();

  // 1. Critical background tasks
  netManager.tick(now);

  if (netManager.isTcpConnected()) {
    while (netManager.available()) {
      char c = (char)netManager.read();
      if (c == '\n') {
        String &buf = netManager.getLineBuffer();
        if (buf.length() > 0) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, buf);
          if (!err) {
            netManager.markDataReceived(now);
            if (netManager.parsePayload(buf, &state)) {
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
      } else {
        netManager.appendLineBuffer(c);
      }
    }
  }

  // 2. Non-blocking input
  ButtonEvent event = input.update();

  // 3. Global: DOUBLE = back / enter menu
  if (event == EV_DOUBLE) {
    if (currentMode != MODE_NORMAL) {
      exitAppModeToNormal();
    } else {
      quickMenuOpen = !quickMenuOpen;
      if (quickMenuOpen) {
        menuState = MENU_MAIN;
        quickMenuItem = 0;
      }
    }
    needRedraw = true;
  }

  // Alert / carousel / screen sync / fan animation
  if (state.alertActive) {
    currentScene = state.alertTargetScene;
    needRedraw = true;
  }
  if (settings.carouselEnabled && !predatorMode && !state.alertActive) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      needRedraw = true;
      previousScene = currentScene;
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
      if (previousScene != currentScene) {
        inTransition = true;
        transitionStart = now;
      }
      lastCarousel = now;
    }
  }
  if (netManager.isTcpConnected() &&
      netManager.getLastSentScreen() != currentScene) {
    netManager.print("screen:");
    netManager.print(String(currentScene));
    netManager.print("\n");
    netManager.setLastSentScreen(currentScene);
  }
  if (now - lastFanAnim >= 100) {
    fanAnimFrame = (fanAnimFrame + 1) % 12;
    lastFanAnim = now;
  }

  // Battery: periodic update
  if (batTimer.check(now))
    updateBatteryState();

  // LED: predator breath / alert blink / cursor blink
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  if (predatorMode) {
    unsigned long t = (now - predatorEnterTime) / 20;
    int breath = (int)(128 + 127 * sin(t * 0.1f));
    if (breath < 0)
      breath = 0;
    if (settings.ledEnabled)
      analogWrite(NOCT_LED_ALERT_PIN, breath);
    else
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  } else {
    if (state.alertActive && !lastAlertActive) {
      alertBlinkCounter = 0;
      blinkState = true;
      digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
      lastBlink = now;
    }
    lastAlertActive = state.alertActive;
    if (state.alertActive) {
      if (now - lastBlink >= NOCT_ALERT_LED_BLINK_MS) {
        lastBlink = now;
        if (alertBlinkCounter < NOCT_ALERT_MAX_BLINKS * 2) {
          blinkState = !blinkState;
          digitalWrite(NOCT_LED_ALERT_PIN, blinkState ? HIGH : LOW);
          alertBlinkCounter++;
        } else {
          blinkState = false;
          digitalWrite(NOCT_LED_ALERT_PIN, LOW);
        }
      }
    } else {
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
      if (now - lastBlink > 500) {
        blinkState = !blinkState;
        lastBlink = now;
      }
    }
  }

  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= (unsigned long)NOCT_SPLASH_MS) {
      splashDone = true;
      needRedraw = true;
    }
  }

  // 4. Mode handling: menu vs app
  if (quickMenuOpen) {
    if (event == EV_SHORT) {
      if (menuState == MENU_MAIN)
        quickMenuItem = (quickMenuItem + 1) % WOLF_MENU_ITEMS;
      else if (menuState == MENU_SUB_WIFI)
        submenuIndex = (submenuIndex + 1) % SUB_WIFI_COUNT;
      else if (menuState == MENU_SUB_RADIO)
        submenuIndex = (submenuIndex + 1) % SUB_RADIO_COUNT;
      else
        submenuIndex = (submenuIndex + 1) % SUB_TOOLS_COUNT;
      needRedraw = true;
    } else if (event == EV_LONG) {
      if (menuState == MENU_MAIN) {
        if (quickMenuItem == 0) {
          if (!settings.carouselEnabled) {
            settings.carouselEnabled = true;
            settings.carouselIntervalSec = 5;
          } else if (settings.carouselIntervalSec == 5)
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
        } else if (quickMenuItem == 1) {
          display.flipScreen();
          settings.displayInverted = display.isScreenFlipped();
          Preferences prefs;
          prefs.begin("nocturne", false);
          prefs.putBool("inverted", settings.displayInverted);
          prefs.end();
        } else if (quickMenuItem == 2) {
          settings.glitchEnabled = !settings.glitchEnabled;
          Preferences prefs;
          prefs.begin("nocturne", false);
          prefs.putBool("glitch", settings.glitchEnabled);
          prefs.end();
        } else if (quickMenuItem == MAIN_IDX_WIFI) {
          menuState = MENU_SUB_WIFI;
          submenuIndex = 0;
        } else if (quickMenuItem == MAIN_IDX_RADIO) {
          menuState = MENU_SUB_RADIO;
          submenuIndex = 0;
        } else if (quickMenuItem == MAIN_IDX_BLE) {
          currentMode = MODE_BLE_SPAM;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          WiFi.mode(WIFI_OFF);
          bleManager.begin();
          Serial.println("[SYS] BLE SPAM.");
        } else if (quickMenuItem == MAIN_IDX_USB) {
          currentMode = MODE_BADWOLF;
          quickMenuOpen = false;
          usbManager.begin();
          Serial.println("[SYS] USB HID.");
        } else if (quickMenuItem == MAIN_IDX_TOOLS) {
          menuState = MENU_SUB_TOOLS;
          submenuIndex = 0;
        } else if (quickMenuItem == MAIN_IDX_EXIT) {
          quickMenuOpen = false;
        }
      } else if (menuState == MENU_SUB_WIFI) {
        if (submenuIndex == SUB_WIFI_COUNT - 1)
          menuState = MENU_MAIN;
        else if (submenuIndex == 0) {
          currentMode = MODE_RADAR;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          WiFi.disconnect();
          WiFi.mode(WIFI_STA);
          WiFi.scanNetworks(true);
          Serial.println("[SYS] WIFI SCAN.");
        } else if (submenuIndex == 1) {
          currentMode = MODE_WIFI_DEAUTH;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          WiFi.disconnect();
          WiFi.mode(WIFI_STA);
          WiFi.scanNetworks(true);
          wifiScanSelected = 0;
          wifiListPage = 0;
          kickManager.setTargetFromScan(0);
          Serial.println("[SYS] DEAUTH. Short=target Long=Fire 2x Out.");
        } else {
          currentMode = MODE_WIFI_TRAP;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          trapManager.start();
          Serial.println("[SYS] PORTAL ON.");
        }
      } else if (menuState == MENU_SUB_RADIO) {
        if (submenuIndex == SUB_RADIO_COUNT - 1)
          menuState = MENU_MAIN;
        else if (submenuIndex == 0) {
          currentMode = MODE_LORA;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          WiFi.mode(WIFI_OFF);
          loraManager.setMode(true);
          Serial.println("[SYS] SNIFF ON.");
        } else if (submenuIndex == 1) {
          currentMode = MODE_LORA_JAM;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          WiFi.mode(WIFI_OFF);
          loraManager.startJamming(869.525f);
          Serial.println("[SYS] JAM ON.");
        } else {
          currentMode = MODE_LORA_SENSE;
          quickMenuOpen = false;
          netManager.setSuspend(true);
          WiFi.mode(WIFI_OFF);
          loraManager.startSense();
          Serial.println("[SYS] SENSE ON.");
        }
      } else if (menuState == MENU_SUB_TOOLS) {
        if (submenuIndex == SUB_TOOLS_COUNT - 1)
          menuState = MENU_MAIN;
        else if (submenuIndex == 0) {
          currentMode = MODE_VAULT;
          quickMenuOpen = false;
          vaultManager.trySyncNtp();
          Serial.println("[SYS] VAULT OPEN.");
        } else if (submenuIndex == 1) {
          currentMode = MODE_DAEMON;
          quickMenuOpen = false;
          Serial.println("[SYS] DAEMON.");
        } else {
          Serial.println(
              "[SYS] Rebooting for Meshtastic flash. Hold BOOT to enter "
              "download mode.");
          esp_restart();
        }
      }
      needRedraw = true;
    }
  } else {
    // App mode: SHORT = navigate, LONG = action (or predator in normal)
    switch (currentMode) {
    case MODE_NORMAL:
      if (event == EV_SHORT && !state.alertActive) {
        previousScene = currentScene;
        currentScene = (currentScene + 1) % sceneManager.totalScenes();
        if (previousScene != currentScene) {
          inTransition = true;
          transitionStart = now;
        }
        lastCarousel = now;
        needRedraw = true;
      } else if (event == EV_LONG) {
        predatorMode = !predatorMode;
        predatorEnterTime = now;
        needRedraw = true;
      }
      break;
    case MODE_WIFI_DEAUTH:
      if (event == EV_SHORT) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          wifiScanSelected = (wifiScanSelected + 1) % n;
          kickManager.setTargetFromScan(wifiScanSelected);
          if (wifiScanSelected >= wifiListPage + 5)
            wifiListPage = wifiScanSelected - 4;
          else if (wifiScanSelected < wifiListPage)
            wifiListPage = wifiScanSelected;
        }
        needRedraw = true;
      } else if (event == EV_LONG) {
        if (kickManager.isAttacking())
          kickManager.stopAttack();
        else
          kickManager.startAttack();
        needRedraw = true;
      }
      break;
    case MODE_RADAR:
      if (event == EV_SHORT) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          wifiScanSelected = (wifiScanSelected + 1) % n;
          if (wifiScanSelected >= wifiListPage + 5)
            wifiListPage = wifiScanSelected - 4;
          else if (wifiScanSelected < wifiListPage)
            wifiListPage = wifiScanSelected;
        }
        needRedraw = true;
      } else if (event == EV_LONG) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          Serial.println("[RADAR] INITIATING DISCONNECT...");
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          delay(100);
          WiFi.mode(WIFI_STA);
          Serial.println("[RADAR] RADIO RESET.");
          WiFi.scanNetworks(true);
        }
      }
      break;
    case MODE_LORA:
      if (event == EV_SHORT) {
        loraManager.clearBuffer();
        needRedraw = true;
      } else if (event == EV_LONG) {
        loraManager.replayLast();
        needRedraw = true;
      }
      break;
    case MODE_VAULT:
      if (event == EV_SHORT) {
        int n = vaultManager.getAccountCount();
        if (n > 0)
          vaultManager.setCurrentIndex((vaultManager.getCurrentIndex() + 1) %
                                       n);
        needRedraw = true;
      }
      break;
    case MODE_BADWOLF:
      if (event == EV_SHORT) {
        usbManager.runMatrix();
        needRedraw = true;
      } else if (event == EV_LONG) {
        usbManager.runSniffer();
        needRedraw = true;
      }
      break;
    default:
      break;
    }
  }

  // 5. Render (throttle: needRedraw or guiTimer)
  if (predatorMode) {
    display.clearBuffer();
    display.sendBuffer();
    delay(10);
    yield();
    return;
  }

  if (!needRedraw && !guiTimer.check(now)) {
    delay(10);
    yield();
    return;
  }
  needRedraw = false;

  display.clearBuffer();
  bool signalLost = splashDone && netManager.isSignalLost(now);
  if (signalLost && netManager.isTcpConnected() && netManager.hasReceivedData())
    netManager.disconnectTcp();

  if (!splashDone) {
    display.drawSplash();
  } else if (quickMenuOpen) {
    display.drawGlobalHeader("MENU", nullptr, netManager.rssi(),
                             netManager.isWifiConnected());
    sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
    sceneManager.drawMenu((int)menuState, quickMenuItem, submenuIndex,
                          settings.carouselEnabled,
                          settings.carouselIntervalSec,
                          settings.displayInverted, settings.glitchEnabled);
  } else {
    switch (currentMode) {
    case MODE_NORMAL: {
      if (!netManager.isWifiConnected()) {
        display.drawGlobalHeader("NO SIGNAL", nullptr, 0, false);
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
        sceneManager.drawNoSignal(false, false, 0, blinkState);
      } else if (!netManager.isTcpConnected()) {
        display.drawGlobalHeader("LINKING", nullptr, netManager.rssi(), true);
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
        sceneManager.drawConnecting(netManager.rssi(), blinkState);
      } else if (netManager.isSearchMode() || signalLost) {
        display.drawGlobalHeader("SEARCH", nullptr, netManager.rssi(),
                                 netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
        int scanPhase = (int)(now / 100) % 12;
        sceneManager.drawSearchMode(scanPhase);
      } else {
        display.drawGlobalHeader(sceneManager.getSceneName(currentScene),
                                 nullptr, netManager.rssi(),
                                 netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
        if (inTransition) {
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
        } else {
          sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
        }
      }
      break;
    }
    case MODE_DAEMON:
      display.drawGlobalHeader("DAEMON", nullptr, netManager.rssi(), true);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawDaemon();
      break;
    case MODE_RADAR:
      display.drawGlobalHeader("RADAR", nullptr, netManager.rssi(), false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage);
      break;
    case MODE_WIFI_DEAUTH: {
      static bool kickTargetInitialized = false;
      if (WiFi.scanComplete() <= 0)
        kickTargetInitialized = false;
      else if (!kickTargetInitialized) {
        kickManager.setTargetFromScan(wifiScanSelected >= 0 ? wifiScanSelected
                                                            : 0);
        kickTargetInitialized = true;
      }
      kickManager.tick();
      display.drawGlobalHeader("DEAUTH", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawKickMode(kickManager);
      break;
    }
    case MODE_LORA:
      loraManager.tick();
      display.drawGlobalHeader("SIGINT", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawLoraSniffer(loraManager);
      break;
    case MODE_LORA_JAM:
      loraManager.tick();
      display.drawGlobalHeader("JAM", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawSilenceMode();
      break;
    case MODE_BLE_SPAM: {
      static int lastPhantomPayloadIndex = -1;
      bleManager.tick();
      display.drawGlobalHeader("BLE", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawBleSpammer(bleManager.getPacketCount());
      if (lastPhantomPayloadIndex >= 0 &&
          bleManager.getCurrentPayloadIndex() != lastPhantomPayloadIndex)
        display.applyGlitch();
      lastPhantomPayloadIndex = bleManager.getCurrentPayloadIndex();
      break;
    }
    case MODE_BADWOLF:
      display.drawGlobalHeader("USB HID", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawBadWolf();
      break;
    case MODE_WIFI_TRAP:
      trapManager.tick();
      display.drawGlobalHeader("PORTAL", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawTrapMode(trapManager.getClientCount(),
                                trapManager.getLogsCaptured(),
                                trapManager.getLastPassword(),
                                trapManager.getLastPasswordShowUntil());
      break;
    case MODE_VAULT: {
      vaultManager.tick();
      static char codeBuf[8];
      vaultManager.getCurrentCode(codeBuf, sizeof(codeBuf));
      display.drawGlobalHeader("VAULT", nullptr, 0,
                               netManager.isWifiConnected());
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawVaultMode(
          vaultManager.getAccountName(vaultManager.getCurrentIndex()), codeBuf,
          vaultManager.getCountdownSeconds());
      break;
    }
    case MODE_LORA_SENSE:
      loraManager.tick();
      display.drawGlobalHeader("SENSE", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      sceneManager.drawGhostsMode(loraManager);
      break;
    default:
      display.drawGlobalHeader("HUB", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);
      break;
    }
  }

  if (settings.glitchEnabled)
    display.applyGlitch();
  display.sendBuffer();
  delay(10);
  yield();
}
