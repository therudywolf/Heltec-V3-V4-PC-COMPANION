/*
 * NOCTURNE_OS — SceneManager: 9 screens. MAIN, CPU, GPU, RAM, DISKS, MEDIA,
 * FANS, MOTHERBOARD, WEATHER.
 */
#ifndef NOCTURNE_SCENE_MANAGER_H
#define NOCTURNE_SCENE_MANAGER_H

#include "../../include/nocturne/Types.h"
#include "../../include/nocturne/config.h"
#include "DisplayEngine.h"

class LoraManager;

class SceneManager {
public:
  SceneManager(DisplayEngine &disp, AppState &state);
  void draw(int sceneIndex, unsigned long bootTime, bool blinkState,
            int fanFrame);
  /** Draw scene content with X offset (for slide transition). offset 0 =
   * normal. */
  void drawWithOffset(int sceneIndex, int xOffset, unsigned long bootTime,
                      bool blinkState, int fanFrame);

  const char *getSceneName(int sceneIndex) const;
  int totalScenes() const { return NOCT_TOTAL_SCENES; }

  // --- 9 screens (internal: use xOff for transition) ---
  void drawMain(bool blinkState, int xOff = 0);
  void drawCpu(bool blinkState, int xOff = 0);
  void drawGpu(bool blinkState, int xOff = 0);
  void drawRam(bool blinkState, int xOff = 0);
  void drawDisks(int xOff = 0);
  void drawPlayer(int xOff = 0);
  void drawFans(int fanFrame, int xOff = 0);
  void drawMotherboard(int xOff = 0);
  void drawWeather(int xOff = 0);

  // --- Utility / overlay screens ---
  void drawSearchMode(int scanPhase);
  /** Tactical HUD Menu: 6 rows. AUTO, FLIP, GLITCH, RUN:DAEMON, RUN:RADAR,
   * EXIT. menuItem 0..5. */
  void drawMenu(int menuItem, bool carouselOn, int carouselSec,
                bool screenRotated, bool glitchEnabled);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);

  /** Cyberdeck: Daemon mode (Wolf Soul). */
  void drawDaemon();
  /** Cyberdeck: Netrunner WiFi Scanner — list view with RSSI and security. */
  void drawWiFiScanner(int selectedIndex, int pageOffset);
  /** Cyberdeck: LoRa sniffer (868 MHz) — noise floor, packet count, last hex.
   */
  void drawLoraSniffer(LoraManager &lora);

  /** Battery HUD: icon + percent (or CHG when charging) in header, right of
   * WOOF!. */
  void drawPowerStatus(int pct, bool isCharging);

  /** BLE Phantom Spammer UI: status bar, pulsing BT icon, packet count. */
  void drawBleSpammer(int packetCount);

  /** BADWOLF USB HID: skull/wolf icon, ARMED status, Short/Long labels. */
  void drawBadWolf();

  /** SILENCE: 868 MHz jammer UI — muted icon, static noise, status. */
  void drawSilenceMode();

  /** TRAP: Evil twin / captive portal — clients, logs, last bite. */
  void drawTrapMode(int clientCount, int logsCaptured, const char *lastPassword,
                    unsigned long passwordShowUntil);

private:
  /** Unified 2x2 grid cell: bracket + label (top-left) + value (right-aligned).
   * valueYOffset: extra pixels to move value baseline down (e.g. 6 for
   * CPU/GPU/MB).
   */
  void drawGridCell(int x, int y, const char *label, const char *value,
                    int valueYOffset = 0);
  void drawFanIconSmall(int x, int y, int frame);
  void drawWeatherIcon32(int x, int y, int wmoCode);
  void drawNoDataCross(int x, int y, int w, int h);
  void drawNoisePattern(int x, int y, int w, int h);

  static const char *sceneNames_[];

  DisplayEngine &disp_;
  AppState &state_;
};

#endif
