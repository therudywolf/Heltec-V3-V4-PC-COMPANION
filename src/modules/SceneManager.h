/*
 * NOCTURNE_OS — SceneManager: state machine for HUB, CPU, GPU, NET, ATMOS,
 * MEDIA, SETTINGS.
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

  void drawHub(unsigned long bootTime);
  void drawCpu(unsigned long bootTime, int fanFrame);
  void drawGpu(int fanFrame);
  void drawNet();
  void drawAtmos();
  void drawMedia(bool blinkState);

  void drawSearchMode(int scanPhase);
  void drawMenu(int menuItem);
  void drawNoSignal(bool wifiOk, bool tcpOk, int rssi, bool blinkState);
  void drawConnecting(int rssi, bool blinkState);

  int totalScenes() const { return NOCT_TOTAL_SCENES; }
  const char *getSceneName(int sceneIndex) const;

private:
  void drawWmoIconXbm(int x, int y, int wmoCode);
  void drawNoDataCross(int x, int y, int w, int h);
  void drawCassetteIcon(int x, int y);
  void drawNoisePattern(int x, int y, int w, int h);

  static const char *sceneNames_[];

  DisplayEngine &disp_;
  AppState &state_;
};

#endif
