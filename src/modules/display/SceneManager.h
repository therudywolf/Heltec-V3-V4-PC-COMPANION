/*
 * NOCTURNE_OS — SceneManager: 9 screens. MAIN, CPU, GPU, RAM, DISKS, MEDIA,
 * FANS, MOTHERBOARD, WEATHER.
 */
#ifndef NOCTURNE_SCENE_MANAGER_H
#define NOCTURNE_SCENE_MANAGER_H

#include "nocturne/Types.h"
#include "nocturne/config.h"
#include "DisplayEngine.h"

class SceneManager
{
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
  /** Menu: level 0 = 5 categories (Monitoring/Config/Hacker/BMW/System),
   * level 1 = submenu, level 2 = Hacker group items. menuHackerGroup when
   * menuLevel==2. */
  void drawMenu(int menuLevel, int menuCategory, int mainIndex,
                int menuHackerGroup, bool carouselOn, int carouselSec,
                bool screenRotated, bool glitchEnabled, bool ledEnabled,
                bool lowBrightnessDefault, bool rebootConfirmed = false,
                int displayContrast = 128, int displayTimeoutSec = 0);
  /** Charge-only full screen: battery %, charging indicator, voltage. */
  void drawChargeOnlyScreen(int pct, bool isCharging, float batteryVoltage);
  /** Brief toast overlay (e.g. "FAIL", "Saved"). Call after main content. */
  void drawToast(const char *msg);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);

  /** Idle screensaver: animated wolf + cyberpunk/Blade Runner glitch. Shown
   * after NOCT_IDLE_SCREENSAVER_MS in no-WiFi or linking state. */
  void drawIdleScreensaver(unsigned long now);

  /** Cyberdeck: Daemon mode (Wolf Soul). */
  void drawDaemon(unsigned long bootTime = 0, bool wifiConnected = false,
                  bool tcpConnected = false, int rssi = 0);
  /** Cyberdeck: Netrunner WiFi Scanner — list view with RSSI and security.
   * footerOverride: if non-null, used instead of "LINK: ONLINE" (e.g. DEAUTH).
   */
  void drawWiFiScanner(int selectedIndex, int pageOffset,
                       int *sortedIndices = nullptr, int filteredCount = 0,
                       const char *footerOverride = nullptr);

  /** Battery HUD: icon + percent (or CHG when charging) in header, right of
   * WOOF!. pct: 0-100, isCharging: true if charging, batteryVoltage: voltage in
   * V (for detecting disconnected battery). onWhiteHeader: true when drawn on
   * the white header bar (use black ink); false for dark background (use
   * white). */
  void drawPowerStatus(int pct, bool isCharging, float batteryVoltage = 0.0f,
                       bool onWhiteHeader = true);

  /** BLE Phantom Spammer UI: status bar, pulsing BT icon, packet count. */
  void drawBleSpammer(int packetCount);

  /** BLE Clone: scan list, selected device, long-press to clone. */
  void drawBleClone(class BleManager &ble, int selectedIndex);

  /** TRAP: Evil twin / captive portal — clients, logs, last bite. cloneApPassword: AP password when cloning encrypted (for display). */
  void drawTrapMode(int clientCount, int logsCaptured, const char *lastPassword,
                    unsigned long passwordShowUntil,
                    const char *clonedSSID = nullptr,
                    const char *cloneApPassword = nullptr,
                    bool apFailed = false);

  void drawWifiSniffMode(int selected, class WifiSniffManager &mgr);

  /** Forza Horizon/Motorsport telemetry dashboard. showSplash: IP|PORT|WAITING
   * for 3s on enter. localIp: WiFi.localIP() as uint32_t. */
  void drawForzaDash(class ForzaManager &forza, bool showSplash,
                     uint32_t localIp);

  /** BMW E39 Assistant: I-Bus status, BLE, selected action (1x next, 2s run). */
  void drawBmwAssistant(class BmwManager &bmw, int selectedActionIndex);

  /** Bottom hint line for fullscreen modes (1x next  2s ok  2x menu). hint==nullptr = default. */
  void drawBottomHint(const char *hint = nullptr);

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
