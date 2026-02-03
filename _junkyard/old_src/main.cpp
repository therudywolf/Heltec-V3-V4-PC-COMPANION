/*
 * NOCTURNE_OS — Cyberdeck firmware
 * Modular: DisplayEngine, NetLink, DataManager, SceneManager
 * Identity Integrity: a living, breathing digital organism.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>

#include "nocturne/DataManager.h"
#include "nocturne/DisplayEngine.h"
#include "nocturne/NetLink.h"
#include "nocturne/SceneManager.h"
#include "nocturne/config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
NetLink netLink;
DataManager dataManager;
SceneManager sceneManager(display, dataManager);

Settings settings;
unsigned long bootTime = 0;
bool splashDone = false;
unsigned long splashStart = 0;
int currentScene = 0;

unsigned long btnPressTime = 0;
bool btnHeld = false;
unsigned long lastCarousel = 0;
unsigned long lastBlink = 0;
unsigned long lastFanAnim = 0;
int fanAnimFrame = 0;
bool blinkState = false;

// Predator mode: OLED off, LED breathing (long press ~2.5s)
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

  netLink.begin(WIFI_SSID, WIFI_PASS);
  netLink.setServer(PC_IP, TCP_PORT);
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  netLink.tick(now);

  // TCP read and parse
  if (netLink.tcpConnected()) {
    while (netLink.available()) {
      char c = (char)netLink.read();
      if (c == '\n') {
        String &buf = netLink.getLineBuffer();
        if (buf.length() > 0) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, buf);
          if (!err) {
            netLink.markDataReceived(now);
            dataManager.parseLine(buf);
            HardwareData &hw = dataManager.hw();
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
        netLink.clearLineBuffer();
      } else {
        netLink.appendLineBuffer(c);
      }
    }
  }

  // Button: click = next screen, long press = SLEEP (Predator)
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

  // Alert from server: force scene to CPU or GPU and flash every 1s
  if (dataManager.alertActive())
    currentScene = dataManager.alertTargetScene();

  if (settings.carouselEnabled && !predatorMode) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
      lastCarousel = now;
    }
  }

  if (netLink.tcpConnected() && netLink.getLastSentScreen() != currentScene) {
    netLink.print("screen:");
    netLink.print(String(currentScene));
    netLink.print("\n");
    netLink.setLastSentScreen(currentScene);
  }

  if (now - lastFanAnim >= 100) {
    fanAnimFrame = (fanAnimFrame + 1) % 12;
    lastFanAnim = now;
  }
  if (now - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  bool anyAlarm = (dataManager.hw().ct >= CPU_TEMP_ALERT) ||
                  (dataManager.hw().gt >= GPU_TEMP_ALERT);
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
  bool signalLost = splashDone && netLink.isSignalLost(now);

  if (signalLost && netLink.tcpConnected() && netLink.hasReceivedData()) {
    netLink.disconnectTcp();
  }

  if (!splashDone) {
    display.drawBiosPost(now, bootTime, netLink.isWifiConnected(),
                         netLink.rssi());
  } else if (!netLink.isWifiConnected()) {
    sceneManager.drawNoSignal(false, false, 0, blinkState);
  } else if (!netLink.isTcpConnected()) {
    sceneManager.drawConnecting(netLink.rssi(), blinkState);
  } else if (netLink.isSearchMode() || signalLost) {
    int scanPhase = (int)(now / 100) % 12;
    sceneManager.drawSearchMode(scanPhase);
  } else {
    // Alert flash: invert display every 1s when CRITICAL
    static unsigned long lastAlertFlash = 0;
    static bool alertFlashInverted = false;
    if (dataManager.alertActive()) {
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

    display.drawCornerCrosshairs();
    sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);

    if (netLink.isWifiConnected())
      display.drawWiFiIcon(NOCT_DISP_W - 16, 2, netLink.rssi());

    display.drawLinkStatus(2, NOCT_DISP_H - 2,
                           netLink.isTcpConnected() &&
                               netLink.hasReceivedData());

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
