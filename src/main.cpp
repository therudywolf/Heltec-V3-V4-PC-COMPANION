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

Settings &settings = state.settings;
unsigned long bootTime = 0;
unsigned long splashStart = 0;
bool splashDone = false;
int currentScene = 0;
int previousScene = 0;
unsigned long transitionStart = 0;
bool inTransition = false;

unsigned long btnPressTime = 0;
bool btnHeld = false;
int clickCount = 0;
unsigned long lastClickTime = 0;
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
#define WOLF_MENU_ITEMS 9

enum MenuState { MENU_MAIN, MENU_SUB_WIFI, MENU_SUB_RADIO, MENU_SUB_TOOLS };
MenuState menuState = MENU_MAIN;
int submenuIndex = 0;

#define MAIN_IDX_WIFI 3
#define MAIN_IDX_RADIO 4
#define MAIN_IDX_BLE 5
#define MAIN_IDX_USB 6
#define MAIN_IDX_TOOLS 7
#define MAIN_IDX_EXIT 8
#define SUB_WIFI_COUNT 4
#define SUB_RADIO_COUNT 4
#define SUB_TOOLS_COUNT 4

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

#define NOCT_REDRAW_INTERVAL_MS 500
#define NOCT_CONFIG_MSG_MS 1500 // "CONFIG LOADED: ALERTS ON" display time
#define NOCT_BAT_READ_INTERVAL_MS 5000
#define NOCT_BAT_SAMPLES 20
#define NOCT_BAT_CALIBRATION 1.1f
static unsigned long lastRedrawMs = 0;
static bool needRedraw = true;
static unsigned long lastBatteryReadMs = 0;

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
  lastBatteryReadMs = millis();
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

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

  // --- BUTTON LOGIC WITH TRIPLE CLICK ---
  int btnState = digitalRead(NOCT_BUTTON_PIN);

  if (btnState == LOW && !btnHeld) {
    btnHeld = true;
    btnPressTime = now;
  }

  if (btnState == HIGH && btnHeld) {
    btnHeld = false;
    unsigned long duration = now - btnPressTime;
    needRedraw = true;

    // TRIPLE CLICK: Count fast clicks
    if (duration < 500) {
      if (now - lastClickTime > 600)
        clickCount = 0;
      clickCount++;
      lastClickTime = now;

      if (clickCount >= 3) {
        clickCount = 0;
        if (currentMode != MODE_NORMAL) {
          if (currentMode == MODE_LORA || currentMode == MODE_LORA_JAM ||
              currentMode == MODE_LORA_SENSE)
            loraManager.setMode(false);
          if (currentMode == MODE_LORA_JAM) {
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
        } else if (quickMenuOpen) {
          if (menuState != MENU_MAIN)
            menuState = MENU_MAIN;
          else
            quickMenuOpen = false;
        }
        return;
      }
    }

    // IN APP MODE: short = navigate/clear, long = action (REPLAY in LORA)
    if (currentMode != MODE_NORMAL) {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        if (currentMode == MODE_WIFI_DEAUTH) {
          if (kickManager.isAttacking())
            kickManager.stopAttack();
          else
            kickManager.startAttack();
          needRedraw = true;
        } else if (currentMode == MODE_LORA) {
          loraManager.replayLast();
          needRedraw = true;
        } else if (currentMode == MODE_BADWOLF) {
          usbManager.runSniffer();
          needRedraw = true;
        } else if (currentMode == MODE_RADAR) {
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
      } else {
        if (currentMode == MODE_VAULT) {
          int n = vaultManager.getAccountCount();
          if (n > 0)
            vaultManager.setCurrentIndex((vaultManager.getCurrentIndex() + 1) %
                                         n);
          needRedraw = true;
        } else if (currentMode == MODE_WIFI_DEAUTH) {
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
        } else if (currentMode == MODE_LORA) {
          loraManager.clearBuffer();
          needRedraw = true;
        } else if (currentMode == MODE_BADWOLF) {
          usbManager.runMatrix();
          needRedraw = true;
        } else if (currentMode == MODE_RADAR) {
          int n = WiFi.scanComplete();
          if (n > 0) {
            wifiScanSelected = (wifiScanSelected + 1) % n;
            if (wifiScanSelected >= wifiListPage + 5)
              wifiListPage = wifiScanSelected - 4;
            else if (wifiScanSelected < wifiListPage)
              wifiListPage = wifiScanSelected;
          }
        }
      }
    } else if (quickMenuOpen) {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        if (menuState == MENU_SUB_WIFI) {
          if (submenuIndex == SUB_WIFI_COUNT - 1) {
            menuState = MENU_MAIN;
          } else if (submenuIndex == 0) {
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
            Serial.println("[SYS] DEAUTH. Short=target Long=Fire 3x Out.");
          } else {
            currentMode = MODE_WIFI_TRAP;
            quickMenuOpen = false;
            netManager.setSuspend(true);
            trapManager.start();
            Serial.println("[SYS] PORTAL ON.");
          }
        } else if (menuState == MENU_SUB_RADIO) {
          if (submenuIndex == SUB_RADIO_COUNT - 1) {
            menuState = MENU_MAIN;
          } else if (submenuIndex == 0) {
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
          if (submenuIndex == SUB_TOOLS_COUNT - 1) {
            menuState = MENU_MAIN;
          } else if (submenuIndex == 0) {
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
        } else {
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
        }
      } else {
        if (menuState == MENU_MAIN) {
          quickMenuItem = (quickMenuItem + 1) % WOLF_MENU_ITEMS;
        } else if (menuState == MENU_SUB_WIFI) {
          submenuIndex = (submenuIndex + 1) % SUB_WIFI_COUNT;
        } else if (menuState == MENU_SUB_RADIO) {
          submenuIndex = (submenuIndex + 1) % SUB_RADIO_COUNT;
        } else {
          submenuIndex = (submenuIndex + 1) % SUB_TOOLS_COUNT;
        }
      }
    } else {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        quickMenuOpen = true;
        quickMenuItem = 0;
        menuState = MENU_MAIN;
      } else if (!state.alertActive) {
        previousScene = currentScene;
        currentScene = (currentScene + 1) % sceneManager.totalScenes();
        if (previousScene != currentScene) {
          inTransition = true;
          transitionStart = now;
        }
        lastCarousel = now;
      }
    }
  }

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

  /* Battery: read every 5s to save power */
  if (now - lastBatteryReadMs >= (unsigned long)NOCT_BAT_READ_INTERVAL_MS) {
    lastBatteryReadMs = now;
    updateBatteryState();
  }

  /* Alert LED: double-tap (2 blinks then silence); predator breath. */
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
    // --- ALERT LED LOGIC (DOUBLE TAP) ---

    // 1. Detect New Alert Edge -> Reset Counter
    if (state.alertActive && !lastAlertActive) {
      alertBlinkCounter = 0;
      blinkState = true; // Start ON
      digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
      lastBlink = now;
    }
    lastAlertActive = state.alertActive;

    // 2. Process Blinking
    if (state.alertActive) {
      if (now - lastBlink >= NOCT_ALERT_LED_BLINK_MS) {
        lastBlink = now;

        // Count phases: 2 blinks = 4 phases (On, Off, On, Off)
        if (alertBlinkCounter < NOCT_ALERT_MAX_BLINKS * 2) {
          blinkState = !blinkState;
          digitalWrite(NOCT_LED_ALERT_PIN, blinkState ? HIGH : LOW);
          alertBlinkCounter++;
        } else {
          // Limit reached: Silence
          blinkState = false;                    // Value stays solid on screen
          digitalWrite(NOCT_LED_ALERT_PIN, LOW); // LED Off
        }
      }
    } else {
      // No Alert -> LED Off
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);

      // Standard UI cursor blink (non-alert)
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

  // ----- Render (throttle: only when needed or every NOCT_REDRAW_INTERVAL_MS)
  // -----
  if (predatorMode) {
    display.clearBuffer();
    display.sendBuffer();
    delay(10);
    return;
  }

  if (!needRedraw &&
      (now - lastRedrawMs < (unsigned long)NOCT_REDRAW_INTERVAL_MS)) {
    delay(10);
    return;
  }
  lastRedrawMs = now;
  needRedraw = false;

  display.clearBuffer();
  bool signalLost = splashDone && netManager.isSignalLost(now);

  if (signalLost && netManager.isTcpConnected() && netManager.hasReceivedData())
    netManager.disconnectTcp();

  if (!splashDone) {
    display.drawSplash();
  } else if (currentMode == MODE_DAEMON) {
    sceneManager.drawDaemon();
  } else if (currentMode == MODE_RADAR) {
    sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage);
  } else if (currentMode == MODE_WIFI_DEAUTH) {
    static bool kickTargetInitialized = false;
    if (WiFi.scanComplete() <= 0)
      kickTargetInitialized = false;
    else if (!kickTargetInitialized) {
      kickManager.setTargetFromScan(wifiScanSelected >= 0 ? wifiScanSelected
                                                          : 0);
      kickTargetInitialized = true;
    }
    kickManager.tick();
    sceneManager.drawKickMode(kickManager);
  } else if (currentMode == MODE_LORA) {
    loraManager.tick();
    sceneManager.drawLoraSniffer(loraManager);
  } else if (currentMode == MODE_LORA_JAM) {
    loraManager.tick();
    sceneManager.drawSilenceMode();
  } else if (currentMode == MODE_BLE_SPAM) {
    static int lastPhantomPayloadIndex = -1;
    bleManager.tick();
    sceneManager.drawBleSpammer(bleManager.getPacketCount());
    if (lastPhantomPayloadIndex >= 0 &&
        bleManager.getCurrentPayloadIndex() != lastPhantomPayloadIndex)
      display.applyGlitch();
    lastPhantomPayloadIndex = bleManager.getCurrentPayloadIndex();
  } else if (currentMode == MODE_BADWOLF) {
    sceneManager.drawBadWolf();
  } else if (currentMode == MODE_WIFI_TRAP) {
    trapManager.tick();
    sceneManager.drawTrapMode(
        trapManager.getClientCount(), trapManager.getLogsCaptured(),
        trapManager.getLastPassword(), trapManager.getLastPasswordShowUntil());
  } else if (currentMode == MODE_VAULT) {
    vaultManager.tick();
    static char codeBuf[8];
    vaultManager.getCurrentCode(codeBuf, sizeof(codeBuf));
    sceneManager.drawVaultMode(
        vaultManager.getAccountName(vaultManager.getCurrentIndex()), codeBuf,
        vaultManager.getCountdownSeconds());
  } else if (currentMode == MODE_LORA_SENSE) {
    loraManager.tick();
    sceneManager.drawGhostsMode(loraManager);
  } else if (!netManager.isWifiConnected()) {
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
    display.drawGlobalHeader(
        quickMenuOpen ? "MENU" : sceneManager.getSceneName(currentScene),
        nullptr, netManager.rssi(), netManager.isWifiConnected());
    sceneManager.drawPowerStatus(state.batteryPct, state.isCharging);

    if (quickMenuOpen) {
      sceneManager.drawMenu((int)menuState, quickMenuItem, submenuIndex,
                            settings.carouselEnabled,
                            settings.carouselIntervalSec,
                            settings.displayInverted, settings.glitchEnabled);
    } else if (inTransition) {
      unsigned long elapsed = now - transitionStart;
      int progress =
          (int)((elapsed * NOCT_TRANSITION_STEP) / NOCT_TRANSITION_MS);
      if (progress > NOCT_DISP_W)
        progress = NOCT_DISP_W;
      int offsetA = -progress;
      int offsetB = NOCT_DISP_W - progress;
      sceneManager.drawWithOffset(previousScene, offsetA, bootTime, blinkState,
                                  fanAnimFrame);
      sceneManager.drawWithOffset(currentScene, offsetB, bootTime, blinkState,
                                  fanAnimFrame);
      if (progress >= NOCT_DISP_W)
        inTransition = false;
    } else {
      sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
    }
  }

  if (settings.glitchEnabled)
    display.applyGlitch();
  display.sendBuffer();
  delay(10);
}
