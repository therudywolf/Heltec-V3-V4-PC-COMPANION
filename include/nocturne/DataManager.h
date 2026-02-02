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
  ProcessData &procs() { return procs_; }
  MediaData &media() { return media_; }

private:
  void parsePayload(JsonDocument &doc);

  HardwareData hw_;
  WeatherData weather_;
  ProcessData procs_;
  MediaData media_;
};

#endif
