/*
 * NOCTURNE_OS — Cyberdeck firmware
 * Modules: NetManager (WiFi + TCP + JSON), DisplayEngine, SceneManager.
 * Predator Mode: long press -> screen OFF, LED breathing.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>

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

unsigned long btnPressTime = 0;
bool btnHeld = false;
unsigned long lastCarousel = 0;
unsigned long lastBlink = 0;
unsigned long lastFanAnim = 0;
int fanAnimFrame = 0;
bool blinkState = false;

bool predatorMode = false;
unsigned long predatorEnterTime = 0;

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
  pinMode(NOCT_LED_PIN, OUTPUT);
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
              HardwareData &hw = state.hw;
              display.cpuGraph.push((float)hw.cl);
              display.gpuGraph.push((float)hw.gl);
              display.netDownGraph.setMax(2048);
              display.netDownGraph.push((float)hw.nd);
              display.netUpGraph.setMax(2048);
              display.netUpGraph.push((float)hw.nu);
              bool spike =
                  (hw.cl > 85 || hw.gl > 85 || hw.nd > 1500 || hw.nu > 1500);
              display.setDataSpike(spike);
            }
          }
        }
        netManager.clearLineBuffer();
      } else {
        netManager.appendLineBuffer(c);
      }
    }
  }

  // Button: short = next scene, long (2.5s) = Predator mode (screen OFF, LED
  // breath)
  int btnState = digitalRead(NOCT_BUTTON_PIN);
  if (btnState == LOW && !btnHeld) {
    btnHeld = true;
    btnPressTime = now;
  }
  if (btnState == HIGH && btnHeld) {
    unsigned long duration = now - btnPressTime;
    btnHeld = false;
    if (duration >= NOCT_BUTTON_PREDATOR_MS) {
      predatorMode = !predatorMode;
      if (predatorMode)
        predatorEnterTime = now;
    } else {
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
      lastCarousel = now;
    }
  }

  if (state.alertActive)
    currentScene = state.alertTargetScene;

  if (settings.carouselEnabled && !predatorMode) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
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

  bool anyAlarm =
      (state.hw.ct >= CPU_TEMP_ALERT) || (state.hw.gt >= GPU_TEMP_ALERT);
  if (predatorMode) {
    digitalWrite(NOCT_LED_PIN, LOW);
    unsigned long t = (now - predatorEnterTime) / 20;
    int breath = (int)(128 + 127 * sin(t * 0.1f));
    if (breath < 0)
      breath = 0;
    if (settings.ledEnabled)
      analogWrite(NOCT_LED_PIN, breath);
  } else if (settings.ledEnabled && anyAlarm && blinkState) {
    digitalWrite(NOCT_LED_PIN, HIGH);
  } else {
    digitalWrite(NOCT_LED_PIN, LOW);
  }

  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= (unsigned long)NOCT_SPLASH_MS)
      splashDone = true;
  }

  // ----- Render -----
  if (predatorMode) {
    display.clearBuffer();
    display.sendBuffer();
    delay(10);
    return;
  }

  display.clearBuffer();
  bool signalLost = splashDone && netManager.isSignalLost(now);

  if (signalLost && netManager.isTcpConnected() && netManager.hasReceivedData())
    netManager.disconnectTcp();

  if (!splashDone) {
    display.drawBiosPost(now, bootTime, netManager.isWifiConnected(),
                         netManager.rssi());
  } else if (!netManager.isWifiConnected()) {
    sceneManager.drawNoSignal(false, false, 0, blinkState);
  } else if (!netManager.isTcpConnected()) {
    sceneManager.drawConnecting(netManager.rssi(), blinkState);
  } else if (netManager.isSearchMode() || signalLost) {
    int scanPhase = (int)(now / 100) % 12;
    sceneManager.drawSearchMode(scanPhase);
  } else {
    static unsigned long lastAlertFlash = 0;
    static bool alertFlashInverted = false;
    if (state.alertActive) {
      if (now - lastAlertFlash >= 1000) {
        lastAlertFlash = now;
        alertFlashInverted = !alertFlashInverted;
      }
      display.u8g2().setFlipMode(alertFlashInverted
                                     ? (1 - (settings.displayInverted ? 1 : 0))
                                     : (settings.displayInverted ? 1 : 0));
    } else {
      display.u8g2().setFlipMode(settings.displayInverted ? 1 : 0);
    }

    display.drawOverlay(
        sceneManager.getSceneName(currentScene), netManager.rssi(),
        netManager.isTcpConnected() && netManager.hasReceivedData());
    display.drawCornerCrosshairs();
    sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);

    display.drawLinkStatus(2, NOCT_DISP_H - 2,
                           netManager.isTcpConnected() &&
                               netManager.hasReceivedData());

    if (anyAlarm && blinkState)
      display.u8g2().drawFrame(0, 0, NOCT_DISP_W, NOCT_DISP_H);

    display.drawGlitchEffect();

    int n = sceneManager.totalScenes();
    int dotsW = n * 6 - 2;
    for (int i = 0; i < n; i++) {
      int x = NOCT_DISP_W / 2 - dotsW / 2 + (i * 6);
      if (i == currentScene)
        display.u8g2().drawBox(x, NOCT_DISP_H - 2, 3, 2);
      else
        display.u8g2().drawPixel(x + 1, NOCT_DISP_H - 1);
    }
  }

  display.sendBuffer();
  delay(10);
}
