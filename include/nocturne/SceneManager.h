/*
 * NOCTURNE_OS — SceneManager: Cortex, Neural, Weather, Media, Phantom Limb
 * (radar), etc.
 */
#ifndef NOCTURNE_SCENE_MANAGER_H
#define NOCTURNE_SCENE_MANAGER_H

#include "DataManager.h"
#include "DisplayEngine.h"
#include "Types.h"
#include "config.h"


class SceneManager {
public:
  SceneManager(DisplayEngine &disp, DataManager &data);
  void draw(int sceneIndex, unsigned long bootTime, bool blinkState,
            int fanFrame);

  // Per-scene draw (called by draw())
  void drawCortex(unsigned long bootTime);
  void drawNeural();
  void drawThermal(int fanFrame);
  void drawMemBank();
  void drawTaskKill(bool blinkState);
  void drawDeck(bool blinkState);
  void drawWeather(); // WMO pixel-art icons
  void drawRadar();   // Phantom Limb: WiFi strength history sonar/bar

  void drawSearchMode(int scanPhase); // SEARCH_MODE scanning animation
  void drawMenu(int menuItem);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);

  int totalScenes() const { return NOCT_TOTAL_SCENES; }

private:
  void drawWmoIcon(int x, int y, int wmoCode); // Map WMO code to pixel icon

  DisplayEngine &disp_;
  DataManager &data_;
};

#endif
