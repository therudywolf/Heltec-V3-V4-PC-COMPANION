/*
 * NOCTURNE_OS — SceneManager: conditional screens based on build profile.
 * Core: 9 PC monitoring screens, menu, charge-only, BMW assistant.
 * Optional: WiFi scanner, BLE spammer, Trap, Forza dash, WiFi sniff.
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
  void draw(int sceneIndex, unsigned long bootTime, bool blinkState, int fanFrame);
  void drawWithOffset(int sceneIndex, int xOffset, unsigned long bootTime,
                      bool blinkState, int fanFrame);

  const char *getSceneName(int sceneIndex) const;
  int totalScenes() const { return NOCT_TOTAL_SCENES; }

  // --- 9 PC monitoring screens ---
  void drawMain(bool blinkState, int xOff = 0);
  void drawCpu(bool blinkState, int xOff = 0);
  void drawGpu(bool blinkState, int xOff = 0);
  void drawRam(bool blinkState, int xOff = 0);
  void drawDisks(int xOff = 0);
  void drawPlayer(int xOff = 0);
  void drawFans(int fanFrame, int xOff = 0);
  void drawMotherboard(int xOff = 0);
  void drawWeather(int xOff = 0);
  void setWeatherExpanded(bool e) { weatherExpanded_ = e; }
  bool weatherExpanded() const { return weatherExpanded_; }
  void drawClaude(int xOff = 0);
  void drawNet(int xOff = 0);
  void drawForest(int xOff = 0);
  void drawEvents(int xOff = 0);
  void drawServices(int xOff = 0);
#if NOCT_FEATURE_WOLFPET
  void drawWolfPet(class WolfPet &pet, int selectedAction);
#endif

  // --- Always available ---
  void drawSearchMode(int scanPhase);
  void drawMenu(int menuLevel, int menuCategory, int mainIndex,
                int menuHackerGroup, bool carouselOn, int carouselSec,
                bool screenRotated, bool glitchEnabled, bool ledEnabled,
                bool lowBrightnessDefault, bool rebootConfirmed = false,
                int displayContrast = 128, int displayTimeoutSec = 0,
                int pinnedScene = -1, bool colorInverted = false);
  void drawChargeOnlyScreen(int pct, bool isCharging, float batteryVoltage);
  void drawToast(const char *msg);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);
  void drawIdleScreensaver(unsigned long now);
  void drawDaemon(unsigned long bootTime = 0, bool wifiConnected = false,
                  bool tcpConnected = false, int rssi = 0);
  void drawPowerStatus(int pct, bool isCharging, float batteryVoltage = 0.0f,
                       bool onWhiteHeader = true);
#if NOCT_FEATURE_BMW
  void drawBmwAssistant(class BmwManager &bmw, int selectedActionIndex);
#endif
  void drawBottomHint(const char *hint = nullptr);

  // --- Conditional: Hacker features ---
#if NOCT_FEATURE_HACKER
  void drawWiFiScanner(int selectedIndex, int pageOffset,
                       int *sortedIndices = nullptr, int filteredCount = 0,
                       const char *footerOverride = nullptr);
  void drawBleScan(class BleManager &mgr);
  void drawBleDevices(class BleManager &mgr, int selected); // passive device browser
  void drawWifiSniffMode(int selected, class WifiSniffManager &mgr);
#endif

  // --- Conditional: LoRa / sub-GHz (#21) ---
#if NOCT_FEATURE_LORA
  void drawLora(class LoraManager &mgr, int view); // view 0=packets 1=nodes
  void drawLoraSweep(class LoraManager &mgr);       // band RSSI spectrum
  void drawLoraArm(const char *what); // antenna-present confirm gate
#if NOCT_FEATURE_LORA_TX
  void drawLoraTx(class LoraManager &mgr, int kind); // beacon/ping/replay sender
#endif
#endif

  // --- Conditional: Forza ---
#if NOCT_FEATURE_FORZA
  void drawForzaDash(class ForzaManager &forza, bool showSplash, uint32_t localIp);
#endif

private:
  void drawGridCell(int x, int y, const char *label, const char *value,
                    int valueYOffset = 0);
  void drawFanIconSmall(int x, int y, int frame);
  void drawNoDataCross(int x, int y, int w, int h);
  void drawNoisePattern(int x, int y, int w, int h);

  static const char *sceneNames_[];

  DisplayEngine &disp_;
  AppState &state_;
  bool weatherExpanded_ = false; // #14: weather forecast drill-in (long-press)
};

#endif
