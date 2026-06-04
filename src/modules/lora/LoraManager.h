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
  void sleep();                  // park the radio (RX off) on mode exit
  void tick();                   // poll: refresh channel RSSI + count detections
  void resetCounters() { activity_ = 0; packets_ = 0; } // clear session tallies

  float rssi() const { return rssi_; }       // live channel RSSI (dBm)
  float floorRssi() const { return floor_; } // slow noise-floor estimate
  int activity() const { return activity_; } // CAD hits this session
  int packets() const { return packets_; }   // LoRa packets seen
  float freqMhz() const { return NOCT_LORA_FREQ; }
  unsigned long lastHitMs() const { return lastHitMs_; } // last CAD-detected ms

  // Rolling RSSI history (oldest..newest) for the on-screen waterfall.
  static const int kHistLen = 120;
  int histLen() const { return kHistLen; }
  int8_t histAt(int i) const { // i in [0..kHistLen): 0 = oldest
    return hist_[(histHead_ + i) % kHistLen];
  }

private:
  bool ready_ = false;
  int lastErr_ = 0;
  float rssi_ = -130.0f;
  float floor_ = -130.0f;
  int activity_ = 0;
  int packets_ = 0;
  unsigned long lastPollMs_ = 0;
  unsigned long lastHitMs_ = 0;
  int8_t hist_[kHistLen] = {0};
  int histHead_ = 0; // index of the oldest sample (ring write position)
};

#endif // NOCT_FEATURE_LORA
#endif
