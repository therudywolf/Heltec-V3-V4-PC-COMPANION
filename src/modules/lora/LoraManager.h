/*
 * NOCTURNE_OS — LoraManager: SX1262 sub-GHz tools (#21, EU868).
 *
 *  Listen  : real LoRa packet RX with cyclable Meshtastic/Meshcore modem
 *            presets, interrupt-driven receive, Meshtastic header decode
 *            (sender/dest node IDs + hop limit + channel hash), a recent-
 *            packet ring and a unique-node table.
 *  Spectrum: incremental RSSI sweep across the 863-870 MHz EU band so the
 *            whole sub-GHz neighbourhood is visible at a glance.
 *
 * Passive RX only — the SX1262 never transmits here.
 */
#ifndef NOCTURNE_LORA_MANAGER_H
#define NOCTURNE_LORA_MANAGER_H

#include "nocturne/config.h"
#if NOCT_FEATURE_LORA
#include <RadioLib.h>

#define LORA_PKT_RING  8    // recent packets kept for the live view
#define LORA_NODE_MAX  16   // unique Meshtastic senders tracked
#define LORA_SPEC_BINS 56   // spectrum sweep resolution (bins across the band)

// One modem configuration the user can cycle through while listening. Receiving
// LoRa requires an EXACT match of freq/bw/sf/cr/sync/preamble, so we ship the
// known mesh presets and let the user flip between them to lock onto traffic.
struct LoraPreset {
  const char *name;
  float freq;   // MHz
  float bw;     // kHz
  uint8_t sf;   // spreading factor
  uint8_t cr;   // coding-rate denominator (5 = 4/5 .. 8 = 4/8)
  uint8_t sync; // LoRa sync word (0x2b Meshtastic, 0x12 private, 0x34 public)
};

// A captured frame. The first 16 bytes of a Meshtastic LoRa packet are an
// unencrypted header (to/from/id/flags/channel-hash) — we decode those; the
// payload stays encrypted and is not stored.
struct LoraPacket {
  uint32_t from;    // sender node id (header bytes 4..7, little-endian)
  uint32_t to;      // dest (0xFFFFFFFF = broadcast)
  uint8_t  flags;   // hop_limit (low 3 bits) + want_ack + via_mqtt
  uint8_t  chanHash;
  uint16_t len;
  int16_t  rssi;
  int8_t   snr;
  uint8_t  head[4]; // first raw bytes (useful for non-Meshtastic frames)
  uint32_t ms;
};

struct LoraNode {
  uint32_t id;
  uint16_t count;
  int16_t  rssi;
  uint32_t lastMs;
};

class LoraManager {
public:
  bool begin();                          // init SX1262 with the current preset
  bool isReady() const { return ready_; }
  int  lastError() const { return lastErr_; }
  void sleep();                          // park the radio on mode exit

  // --- Listen mode ---
  void startListen();                    // (re)arm RX with the active preset
  void tick();                           // poll RX flag, decode, update tables
  void nextPreset();                     // cycle modem config (re-arms RX)
  const char *presetName() const { return kPresets[preset_].name; }
  float freqMhz() const { return kPresets[preset_].freq; }
  int  spreadingFactor() const { return kPresets[preset_].sf; }

  int  packetCount() const { return packets_; }
  int  recentCount() const { return ringCount_; }
  const LoraPacket *recent(int i) const; // i = 0 is the newest
  int  nodeCount() const { return nodeCount_; }
  const LoraNode *node(int i) const;
  float rssi() const { return rssi_; }   // live channel RSSI for the header bar
  unsigned long lastHitMs() const { return lastHitMs_; }
  void resetStats();

  // --- Spectrum sweep mode ---
  void beginSweep();
  void sweepTick();                      // advance a few bins per call
  int  specBins() const { return LORA_SPEC_BINS; }
  int8_t specAt(int i) const { return spec_[i]; }
  float specFreq(int i) const { return kBandStart + (i + 0.5f) * kBinMHz; }
  int  sweepCursor() const { return sweepIdx_; }
  float bandStart() const { return kBandStart; }
  float bandEnd() const { return kBandEnd; }

private:
  void applyPreset();
  void recordPacket(const uint8_t *buf, int len, float rssi, float snr);
  int  findNode(uint32_t id);

  static const LoraPreset kPresets[];
  static const int kPresetCount;
  static constexpr float kBandStart = 863.0f;
  static constexpr float kBandEnd   = 870.0f;
  static constexpr float kBinMHz    = (kBandEnd - kBandStart) / LORA_SPEC_BINS;

  bool ready_ = false;
  int  lastErr_ = 0;
  int  preset_ = 0;
  float rssi_ = -130.0f;
  int  packets_ = 0;
  unsigned long lastHitMs_ = 0;

  LoraPacket ring_[LORA_PKT_RING];
  int ringHead_ = 0, ringCount_ = 0;
  LoraNode nodes_[LORA_NODE_MAX];
  int nodeCount_ = 0;

  int8_t spec_[LORA_SPEC_BINS];
  int sweepIdx_ = 0;
};

#endif // NOCT_FEATURE_LORA
#endif
