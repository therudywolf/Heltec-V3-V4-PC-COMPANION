/*
 * NOCTURNE_OS — SceneManager: 9 screens. MAIN, CPU, GPU, RAM, DISKS, MEDIA,
 * FANS, MOTHERBOARD, WEATHER.
 */
#ifndef NOCTURNE_SCENE_MANAGER_H
#define NOCTURNE_SCENE_MANAGER_H

#include "../../include/nocturne/Types.h"
#include "../../include/nocturne/config.h"
#include "DisplayEngine.h"

class KickManager;
class LoraManager;
class VaultManager;

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
  /** Two-level menu: menuState 0=main, 1=WIFI, 2=RADIO, 3=TOOLS; mainIndex or
   * submenuIndex is selected. */
  void drawMenu(int menuState, int mainIndex, int submenuIndex, bool carouselOn,
                int carouselSec, bool screenRotated, bool glitchEnabled,
                bool rebootConfirmed = false);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);

  /** Idle screensaver: animated wolf + cyberpunk/Blade Runner glitch. Shown
   * after NOCT_IDLE_SCREENSAVER_MS in no-WiFi or linking state. */
  void drawIdleScreensaver(unsigned long now);

  /** Cyberdeck: Daemon mode (Wolf Soul). */
  void drawDaemon(unsigned long bootTime = 0, bool wifiConnected = false,
                  bool tcpConnected = false, int rssi = 0);
  /** Cyberdeck: Netrunner WiFi Scanner — list view with RSSI and security. */
  void drawWiFiScanner(int selectedIndex, int pageOffset,
                       int *sortedIndices = nullptr, int filteredCount = 0);
  /** Cyberdeck: LoRa sniffer (868 MHz) — noise floor, packet count, last hex.
   */
  void drawLoraSniffer(LoraManager &lora);

  /** Battery HUD: icon + percent (or CHG when charging) in header, right of
   * WOOF!. pct: 0-100, isCharging: true if charging, batteryVoltage: voltage in
   * V (for detecting disconnected battery). onWhiteHeader: true when drawn on
   * the white header bar (use black ink); false for dark background (use
   * white). */
  void drawPowerStatus(int pct, bool isCharging, float batteryVoltage = 0.0f,
                       bool onWhiteHeader = true);

  /** BLE Phantom Spammer UI: status bar, pulsing BT icon, packet count. */
  void drawBleSpammer(int packetCount);

  /** BADWOLF USB HID: skull/wolf icon, ARMED status, Short/Long labels. */
  void drawBadWolf();

  /** SILENCE: 868 MHz jammer UI — muted icon, static noise, status. */
  void drawSilenceMode(int8_t power = 22);

  /** TRAP: Evil twin / captive portal — clients, logs, last bite. */
  void drawTrapMode(int clientCount, int logsCaptured, const char *lastPassword,
                    unsigned long passwordShowUntil,
                    const char *clonedSSID = nullptr);

  /** KICK: WiFi deauth — wolf baring teeth, shake when attacking,
   * TARGET/STATUS/PKTS. */
  void drawKickMode(KickManager &kick);

  /** VAULT: TOTP 2FA — locked chest + paw, account name, 6-digit code, 30s bar.
   */
  void drawVaultMode(const char *accountName, const char *code6,
                     int countdownSec);

  /** GHOSTS: 868 MHz sensor sniffer — radar sweep, scrolling list RSSI + hex.
   */
  void drawGhostsMode(LoraManager &lora);

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
