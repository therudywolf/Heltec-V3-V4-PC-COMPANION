/*
 * NOCTURNE_OS — SceneManager: 7 screens, grid coordinates, cyberpunk layout.
 * HUB, CPU, GPU, RAM, DISKS, ATMOS, MEDIA.
 */
#ifndef NOCTURNE_SCENE_MANAGER_H
#define NOCTURNE_SCENE_MANAGER_H

#include "../../include/nocturne/Types.h"
#include "../../include/nocturne/config.h"
#include "DisplayEngine.h"

class SceneManager {
public:
  SceneManager(DisplayEngine &disp, AppState &state);
  void draw(int sceneIndex, unsigned long bootTime, bool blinkState,
            int fanFrame);

  const char *getSceneName(int sceneIndex) const;
  int totalScenes() const { return NOCT_TOTAL_SCENES; }

  // --- 5 brutalist screens (5m rule) ---
  void drawThermal(bool blinkState);
  void drawLoad();
  void drawMemory();
  void drawStorage();
  void drawFans();

  // --- Utility / overlay screens ---
  void drawSearchMode(int scanPhase);
  void drawMenu(int menuItem, bool carouselOn, bool screenOff);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);

private:
  void drawFanIconSmall(int x, int y, int frame);
  void drawWeatherIcon32(int x, int y, int wmoCode);
  void drawNoDataCross(int x, int y, int w, int h);
  void drawNoisePattern(int x, int y, int w, int h);

  static const char *sceneNames_[];

  DisplayEngine &disp_;
  AppState &state_;
};

#endif
