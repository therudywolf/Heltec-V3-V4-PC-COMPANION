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

#include "modules/BootAnim.h"
#include "modules/DisplayEngine.h"
#include "modules/NetManager.h"
#include "modules/SceneManager.h"
#include "nocturne/Types.h"
#include "nocturne/config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
NetManager netManager;
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
#define WOLF_MENU_ITEMS 6 /* AUTO, FLIP, GLITCH, DAEMON, RADAR, EXIT */

// --- Cyberdeck Modes ---
enum AppMode { MODE_NORMAL, MODE_DAEMON, MODE_RADAR };
AppMode currentMode = MODE_NORMAL;

// --- Netrunner WiFi Scanner ---
int wifiScanSelected = 0;
int wifiListPage = 0;

#define NOCT_REDRAW_INTERVAL_MS 500
#define NOCT_CONFIG_MSG_MS 1500 // "CONFIG LOADED: ALERTS ON" display time
static unsigned long lastRedrawMs = 0;
static bool needRedraw = true;

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
          currentMode = MODE_NORMAL;
          WiFi.scanDelete();
          Serial.println("[UI] Triple Click: EXIT APP");
        } else {
          quickMenuOpen = false;
        }
        return;
      }
    }

    // STANDARD LOGIC
    if (currentMode != MODE_NORMAL) {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        if (currentMode == MODE_RADAR) {
          int n = WiFi.scanComplete();
          if (n > 0 && wifiScanSelected < n) {
            if (WiFi.SSID(wifiScanSelected) == WiFi.SSID()) {
              Serial.println("[NETRUNNER] Disconnecting from current AP.");
              WiFi.disconnect();
            } else {
              Serial.println("[NETRUNNER] Rescanning...");
              WiFi.scanNetworks(true);
              wifiScanSelected = 0;
              wifiListPage = 0;
            }
          } else {
            WiFi.scanNetworks(true);
          }
        } else {
          currentMode = MODE_NORMAL;
        }
      } else {
        if (currentMode == MODE_RADAR) {
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
        } else if (quickMenuItem == 3) {
          currentMode = MODE_DAEMON;
          quickMenuOpen = false;
        } else if (quickMenuItem == 4) {
          currentMode = MODE_RADAR;
          quickMenuOpen = false;
        } else if (quickMenuItem == 5) {
          quickMenuOpen = false;
        }
      } else {
        quickMenuItem = (quickMenuItem + 1) % WOLF_MENU_ITEMS;
      }
    } else {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        quickMenuOpen = true;
        quickMenuItem = 0;
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
  } else if (!netManager.isWifiConnected()) {
    display.drawGlobalHeader("NO SIGNAL", nullptr, 0, false);
    sceneManager.drawNoSignal(false, false, 0, blinkState);
  } else if (!netManager.isTcpConnected()) {
    display.drawGlobalHeader("LINKING", nullptr, netManager.rssi(), true);
    sceneManager.drawConnecting(netManager.rssi(), blinkState);
  } else if (netManager.isSearchMode() || signalLost) {
    display.drawGlobalHeader("SEARCH", nullptr, netManager.rssi(),
                             netManager.isWifiConnected());
    int scanPhase = (int)(now / 100) % 12;
    sceneManager.drawSearchMode(scanPhase);
  } else {
    if (currentMode == MODE_DAEMON) {
      sceneManager.drawDaemon();
    } else if (currentMode == MODE_RADAR) {
      sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage);
    } else {
      display.drawGlobalHeader(
          quickMenuOpen ? "MENU" : sceneManager.getSceneName(currentScene),
          nullptr, netManager.rssi(), netManager.isWifiConnected());

      if (quickMenuOpen) {
        sceneManager.drawMenu(quickMenuItem, settings.carouselEnabled,
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
        sceneManager.drawWithOffset(previousScene, offsetA, bootTime,
                                    blinkState, fanAnimFrame);
        sceneManager.drawWithOffset(currentScene, offsetB, bootTime, blinkState,
                                    fanAnimFrame);
        if (progress >= NOCT_DISP_W)
          inTransition = false;
      } else {
        sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
      }
    }
  }

  if (settings.glitchEnabled)
    display.applyGlitch();
  display.sendBuffer();
  delay(10);
}
