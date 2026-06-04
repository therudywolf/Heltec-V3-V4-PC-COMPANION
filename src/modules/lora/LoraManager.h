/*
 * NOCTURNE_OS — LoraManager: SX1262 sub-GHz tools (#21, EU868).
 * Passive first: radio init + live channel RSSI + CAD (LoRa activity detect).
 * TX features (replay/beacon) stay behind a mandatory antenna-present gate.
 */
#ifndef NOCTURNE_LORA_MANAGER_H
#define NOCTURNE_LORA_MANAGER_H

#include "nocturne/config.h"
#if NOCT_FEATURE_LORA
#include <RadioLib.h>

class LoraManager {
public:
  bool begin();                  // init SX1262 @ NOCT_LORA_FREQ; false if not found
  bool isReady() const { return ready_; }
  int lastError() const { return lastErr_; }

  void startListen();            // enter continuous RX (for RSSI/sniff)
  void tick();                   // poll: refresh channel RSSI + count detections

  float rssi() const { return rssi_; }       // live channel RSSI (dBm)
  float floorRssi() const { return floor_; } // slow noise-floor estimate
  int activity() const { return activity_; } // CAD hits in the last window
  int packets() const { return packets_; }   // LoRa packets seen
  float freqMhz() const { return NOCT_LORA_FREQ; }

private:
  bool ready_ = false;
  int lastErr_ = 0;
  float rssi_ = -130.0f;
  float floor_ = -130.0f;
  int activity_ = 0;
  int packets_ = 0;
  unsigned long lastPollMs_ = 0;
};

#endif // NOCT_FEATURE_LORA
#endif
