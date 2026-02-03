/*
 * NOCTURNE_OS — DataManager: parse JSON (2-char keys), deserialize only what we
 * need
 */
#ifndef NOCTURNE_DATAMANAGER_H
#define NOCTURNE_DATAMANAGER_H

#include "Types.h"
#include <ArduinoJson.h>

class DataManager {
public:
  DataManager();
  void parseLine(const String &line); // One JSON line from TCP
  void getPayload(JsonDocument &doc,
                  const String &line); // Fill doc for parsePayload

  HardwareData &hw() { return hw_; }
  WeatherData &weather() { return weather_; }
  bool weatherReceived() const { return weatherReceived_; }
  ProcessData &procs() { return procs_; }
  MediaData &media() { return media_; }

  // Alert from server: CRITICAL + target_screen (CPU/GPU) for auto-switch +
  // flash
  bool alertActive() const { return alertActive_; }
  int alertTargetScene() const {
    return alertTargetScene_;
  } // NOCT_SCENE_CPU or NOCT_SCENE_GPU

private:
  void parsePayload(JsonDocument &doc);

  HardwareData hw_;
  WeatherData weather_;
  bool weatherReceived_ = false;
  ProcessData procs_;
  MediaData media_;
  bool alertActive_ = false;
  int alertTargetScene_ = 1; // 1=CPU, 2=GPU scene index
};

#endif
