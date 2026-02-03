/*
 * NOCTURNE_OS — Firmware entry (ESP32 Heltec).
 * Loop: NetManager.tick (TCP/JSON) -> SceneManager.draw (current scene) ->
 * sendBuffer. Button: short = next scene / menu; long = predator (screen off,
 * LED breath). Config: NOCT_* in config.h; WiFi/PC in secrets.h.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
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
unsigned long lastCarousel = 0;
unsigned long lastBlink = 0;
unsigned long lastFanAnim = 0;
int fanAnimFrame = 0;
bool blinkState = false;

bool predatorMode = false;
unsigned long predatorEnterTime = 0;

bool quickMenuOpen = false;
int quickMenuItem = 0;
#define WOLF_MENU_ITEMS 4 /* CAROUSEL: 5s, 10s, OFF, EXIT */

#define NOCT_REDRAW_INTERVAL_MS 500
#define NOCT_CONFIG_MSG_MS 1500 // "CONFIG LOADED: ALERTS ON" display time
#define NOCT_ALERT_LED_BLINK_MS 175
static unsigned long lastRedrawMs = 0;
static bool needRedraw = true;
static bool alertLedCpuDone = false;
static bool alertLedGpuDone = false;
static unsigned long alertStartMs = 0;
static bool lastAlertActive = false;

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
  VextON();
  pinMode(NOCT_RST_PIN, OUTPUT);
  digitalWrite(NOCT_RST_PIN, LOW);
  delay(50);
  digitalWrite(NOCT_RST_PIN, HIGH);
  delay(50);

  display.begin();
  drawBootSequence(display);
  splashDone = true;
  pinMode(NOCT_LED_CPU_PIN, OUTPUT);
  pinMode(NOCT_LED_GPU_PIN, OUTPUT);
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
  prefs.end();

  display.u8g2().setContrast((uint8_t)settings.displayContrast);
  display.u8g2().setFlipMode(settings.displayInverted ? 1 : 0);
  randomSeed(analogRead(0));

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

  // Button: Short (<500ms) = next screen or cycle menu. Long (>=1000ms) = Quick
  // Menu or Toggle/Select.
  int btnState = digitalRead(NOCT_BUTTON_PIN);
  if (btnState == LOW && !btnHeld) {
    btnHeld = true;
    btnPressTime = now;
  }
  if (btnState == HIGH && btnHeld) {
    needRedraw = true;
    unsigned long duration = now - btnPressTime;
    btnHeld = false;
    if (quickMenuOpen) {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        /* Long press: save & exit */
        if (quickMenuItem == 0) {
          settings.carouselEnabled = true;
          settings.carouselIntervalSec = 5;
        } else if (quickMenuItem == 1) {
          settings.carouselEnabled = true;
          settings.carouselIntervalSec = 10;
        } else if (quickMenuItem == 2) {
          settings.carouselEnabled = false;
        }
        if (quickMenuItem <= 2) {
          Preferences prefs;
          prefs.begin("nocturne", false);
          prefs.putBool("carousel", settings.carouselEnabled);
          prefs.putInt("carouselSec", settings.carouselIntervalSec);
          prefs.end();
        }
        quickMenuOpen = false; /* Always exit on long press */
      } else {
        /* Short press: toggle/cycle option (5s / 10s / OFF / EXIT) */
        quickMenuItem = (quickMenuItem + 1) % WOLF_MENU_ITEMS;
      }
    } else {
      if (duration >= NOCT_BUTTON_LONG_MS) {
        /* Long press (>1s): open modal menu CAROUSEL: [5s / 10s / OFF] */
        quickMenuOpen = true;
        /* Highlight current option */
        if (!settings.carouselEnabled)
          quickMenuItem = 2;
        else if (settings.carouselIntervalSec == 5)
          quickMenuItem = 0;
        else
          quickMenuItem = 1;
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
  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  if (state.alertActive && !lastAlertActive) {
    alertLedCpuDone = false;
    alertLedGpuDone = false;
    alertStartMs = now;
  }
  lastAlertActive = state.alertActive;
  if (!state.alertActive) {
    alertLedCpuDone = false;
    alertLedGpuDone = false;
  }

  bool cpuTempAlert = state.alertActive &&
                      state.alertTargetScene == NOCT_SCENE_CPU &&
                      state.alertMetric == NOCT_ALERT_CT;
  bool gpuTempAlert = state.alertActive &&
                      state.alertTargetScene == NOCT_SCENE_GPU &&
                      state.alertMetric == NOCT_ALERT_GT;
  unsigned long alertElapsed = now - alertStartMs;
  bool withinBlinkWindow =
      alertElapsed < (unsigned long)NOCT_ALERT_LED_BLINK_MAX_MS;

  if (predatorMode) {
    digitalWrite(NOCT_LED_GPU_PIN, LOW);
    unsigned long t = (now - predatorEnterTime) / 20;
    int breath = (int)(128 + 127 * sin(t * 0.1f));
    if (breath < 0)
      breath = 0;
    if (settings.ledEnabled)
      analogWrite(NOCT_LED_CPU_PIN, breath);
    else
      digitalWrite(NOCT_LED_CPU_PIN, LOW);
  } else {
    digitalWrite(NOCT_LED_CPU_PIN, LOW);
    digitalWrite(NOCT_LED_GPU_PIN, LOW);
    if (alertElapsed >= (unsigned long)NOCT_ALERT_LED_BLINK_MAX_MS) {
      alertLedCpuDone = true;
      alertLedGpuDone = true;
    }
    if (settings.ledEnabled && withinBlinkWindow) {
      unsigned long phase = alertElapsed / NOCT_ALERT_LED_BLINK_MS;
      bool on = (phase & 1) == 0;
      if (cpuTempAlert && !alertLedCpuDone)
        digitalWrite(NOCT_LED_CPU_PIN, on ? HIGH : LOW);
      if (gpuTempAlert && !alertLedGpuDone)
        digitalWrite(NOCT_LED_GPU_PIN, on ? HIGH : LOW);
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
    static unsigned long lastAlertFlash = 0;
    if (state.alertActive && now - lastAlertFlash >= 1000) {
      lastAlertFlash = now;
      display.clearBuffer();
      display.u8g2().drawBox(0, 0, NOCT_DISP_W, NOCT_DISP_H);
      display.sendBuffer();
      delay(10);
      return;
    }
    display.u8g2().setFlipMode(settings.displayInverted ? 1 : 0);

    display.drawGlobalHeader(
        quickMenuOpen ? "MENU" : sceneManager.getSceneName(currentScene),
        nullptr, netManager.rssi(), netManager.isWifiConnected());

    if (quickMenuOpen) {
      sceneManager.drawMenu(quickMenuItem, settings.carouselEnabled,
                            settings.carouselIntervalSec);
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
      if (state.alertActive)
        display.drawAlertBorder();
    }
  }

  display.sendBuffer();
  delay(10);
}
