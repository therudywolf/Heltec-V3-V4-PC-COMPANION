#include "modules/lora/LoraManager.h"
#if NOCT_FEATURE_LORA

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <string.h>

// Minimum gap between manual transmits — keeps us comfortably inside the EU868
// duty-cycle even though 869.4-869.65 MHz (where LongFast lives) allows 10%.
static const unsigned long kTxCooldownMs = 3000;

// Dedicated SPI bus for the SX1262 (separate from the OLED's I2C).
static SPIClass loraSpi(FSPI);
static SX1262 radio =
    new Module(NOCT_LORA_NSS, NOCT_LORA_DIO1, NOCT_LORA_RST, NOCT_LORA_BUSY, loraSpi);

// DIO1 fires on RX-done; the ISR just flags it and tick() does the SPI read.
static volatile bool s_rxFlag = false;
static void IRAM_ATTR loraRxIsr() { s_rxFlag = true; }

// EU868 mesh-oriented modem presets. Meshtastic uses sync word 0x2b; its EU868
// default channel ("LongFast") is 869.525 MHz / BW250 / SF11 / CR4-5. Meshcore
// and most generic stacks use the private sync 0x12. Cycle these to lock on.
const LoraPreset LoraManager::kPresets[] = {
    {"Mesh LF", 869.525f, 250.0f, 11, 5, 0x2b}, // Meshtastic LongFast (EU868 default)
    {"Mesh MF", 869.525f, 250.0f,  9, 5, 0x2b}, // Meshtastic MediumFast
    {"Mesh SF", 869.525f, 250.0f,  7, 5, 0x2b}, // Meshtastic ShortFast
    {"Core LF", 869.525f, 250.0f, 11, 5, 0x12}, // Meshcore / generic (private sync)
    {"Raw 868", 868.000f, 125.0f,  7, 5, 0x12}, // generic LoRa
};
const int LoraManager::kPresetCount =
    (int)(sizeof(kPresets) / sizeof(kPresets[0]));

bool LoraManager::begin() {
  loraSpi.begin(NOCT_LORA_SCK, NOCT_LORA_MISO, NOCT_LORA_MOSI, NOCT_LORA_NSS);
  const LoraPreset &p = kPresets[preset_];
  // freq, bw, sf, cr, sync, power(dBm, RX-irrelevant), preamble=16 (Meshtastic),
  // 1.8V TCXO (Heltec module), DC-DC regulator. CRC defaults on (Meshtastic).
  lastErr_ = radio.begin(p.freq, p.bw, p.sf, p.cr, p.sync, 10, 16, NOCT_LORA_TCXO, false);
  ready_ = (lastErr_ == RADIOLIB_ERR_NONE);
  if (ready_) {
    radio.setDio2AsRfSwitch(true); // Heltec SX1262 drives the RF switch off DIO2
    radio.setPacketReceivedAction(loraRxIsr);
    radio.startReceive();
  }
  for (int i = 0; i < LORA_SPEC_BINS; i++) spec_[i] = -128;
  loadNodes(); // restore the unique-node table across reboots
  return ready_;
}

void LoraManager::loadNodes() {
  Preferences p;
  if (!p.begin("loranodes", true)) return;
  int n = p.getInt("n", 0);
  if (n < 0) n = 0;
  if (n > LORA_NODE_MAX) n = LORA_NODE_MAX;
  size_t want = (size_t)n * sizeof(LoraNode);
  size_t got = (want > 0) ? p.getBytes("nodes", nodes_, want) : 0;
  nodeCount_ = (got == want) ? n : 0;
  p.end();
}

void LoraManager::saveNodes() {
  Preferences p;
  if (!p.begin("loranodes", false)) return;
  p.putInt("n", nodeCount_);
  if (nodeCount_ > 0)
    p.putBytes("nodes", nodes_, (size_t)nodeCount_ * sizeof(LoraNode));
  p.end();
}

void LoraManager::applyPreset() {
  if (!ready_) return;
  const LoraPreset &p = kPresets[preset_];
  radio.standby();
  radio.setFrequency(p.freq);
  radio.setBandwidth(p.bw);
  radio.setSpreadingFactor(p.sf);
  radio.setCodingRate(p.cr);
  radio.setSyncWord(p.sync);
  radio.setPreambleLength(16);
  radio.startReceive();
}

void LoraManager::startListen() {
  if (ready_) applyPreset(); // restores the listen config (e.g. after a sweep)
}

void LoraManager::nextPreset() {
  preset_ = (preset_ + 1) % kPresetCount;
  applyPreset();
}

void LoraManager::sleep() {
  saveNodes(); // persist the node table before parking the radio
  if (ready_) radio.sleep();
}

void LoraManager::tick() {
  if (!ready_) return;
  rssi_ = radio.getRSSI(false); // instantaneous channel RSSI for the meter

  if (!s_rxFlag) return;
  s_rxFlag = false;
  uint8_t buf[256];
  int len = (int)radio.getPacketLength();
  if (len > (int)sizeof(buf)) len = sizeof(buf);
  int st = radio.readData(buf, len);
  if (st == RADIOLIB_ERR_NONE && len > 0) {
    recordPacket(buf, len, radio.getRSSI(), radio.getSNR());
    packets_++;
    lastHitMs_ = millis();
  }
  radio.startReceive(); // re-arm
}

void LoraManager::recordPacket(const uint8_t *buf, int len, float rssi, float snr) {
  // Keep the full frame for TX_REPLAY.
  lastRxLen_ = (len < (int)sizeof(lastRx_)) ? len : (int)sizeof(lastRx_);
  memcpy(lastRx_, buf, lastRxLen_);

  LoraPacket &pk = ring_[ringHead_];
  pk.len = (uint16_t)len;
  pk.rssi = (int16_t)rssi;
  pk.snr = (int8_t)snr;
  pk.ms = millis();
  pk.from = pk.to = 0;
  pk.flags = pk.chanHash = 0;
  for (int i = 0; i < 4; i++) pk.head[i] = (i < len) ? buf[i] : 0;
  // Meshtastic LoRa header (unencrypted): to[0..3] from[4..7] id[8..11]
  // flags[12] channelHash[13]. Little-endian node IDs.
  if (len >= 16) {
    pk.to = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
            ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    pk.from = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) |
              ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
    pk.flags = buf[12];
    pk.chanHash = buf[13];
  }
  ringHead_ = (ringHead_ + 1) % LORA_PKT_RING;
  if (ringCount_ < LORA_PKT_RING) ringCount_++;

  if (pk.from != 0) { // track unique senders (Meshtastic-looking frames)
    int idx = findNode(pk.from);
    if (idx < 0 && nodeCount_ < LORA_NODE_MAX) {
      idx = nodeCount_++;
      nodes_[idx].id = pk.from;
      nodes_[idx].count = 0;
    }
    if (idx >= 0) {
      nodes_[idx].count++;
      nodes_[idx].rssi = (int16_t)rssi;
      nodes_[idx].lastMs = pk.ms;
    }
  }
}

const LoraPacket *LoraManager::recent(int i) const {
  if (i < 0 || i >= ringCount_) return nullptr;
  int idx = (ringHead_ - 1 - i + LORA_PKT_RING * 2) % LORA_PKT_RING;
  return &ring_[idx];
}

const LoraNode *LoraManager::node(int i) const {
  if (i < 0 || i >= nodeCount_) return nullptr;
  return &nodes_[i];
}

int LoraManager::findNode(uint32_t id) {
  for (int i = 0; i < nodeCount_; i++)
    if (nodes_[i].id == id) return i;
  return -1;
}

void LoraManager::resetStats() {
  packets_ = 0;
  ringCount_ = 0;
  ringHead_ = 0;
  nodeCount_ = 0;
}

// ── Spectrum sweep ────────────────────────────────────────────────────────
// Wide-band energy view: hop the synth across 863..870 MHz reading the
// instantaneous RSSI at each bin. A narrow modem (BW125/SF7) keeps the RBW
// tight; only a few bins are sampled per call so the UI stays responsive.
void LoraManager::beginSweep() {
  if (!ready_) return;
  radio.standby();
  radio.setBandwidth(125.0f);
  radio.setSpreadingFactor(7);
  sweepIdx_ = 0;
  for (int i = 0; i < LORA_SPEC_BINS; i++) spec_[i] = -128;
}

void LoraManager::sweepTick() {
  if (!ready_) return;
  const int binsPerTick = 6;
  for (int k = 0; k < binsPerTick; k++) {
    float f = kBandStart + (sweepIdx_ + 0.5f) * kBinMHz;
    radio.standby();
    radio.setFrequency(f);
    radio.startReceive();
    delayMicroseconds(400); // let the instantaneous-RSSI register settle
    float r = radio.getRSSI(false);
    if (r > 0) r = 0;
    else if (r < -128) r = -128;
    spec_[sweepIdx_] = (int8_t)r;
    sweepIdx_ = (sweepIdx_ + 1) % LORA_SPEC_BINS;
  }
}

#if NOCT_FEATURE_LORA_TX
const char *LoraManager::txKindName(int k) {
  switch (k) {
    case TX_BEACON:    return "Beacon";
    case TX_MESH_PING: return "MeshPing";
    case TX_REPLAY:    return "Replay";
    default:           return "?";
  }
}

int LoraManager::txCooldownLeft() const {
  if (lastTxMs_ == 0) return 0;
  unsigned long since = millis() - lastTxMs_;
  if (since >= kTxCooldownMs) return 0;
  return (int)((kTxCooldownMs - since + 999) / 1000);
}

bool LoraManager::transmit(int kind) {
  if (!ready_) return false;
  unsigned long now = millis();
  if (lastTxMs_ != 0 && (now - lastTxMs_) < kTxCooldownMs) return false; // cooldown

  uint8_t buf[64];
  int len = 0;
  if (kind == TX_BEACON) {
    const char tag[] = "NOCTURNE";
    for (int i = 0; i < 8; i++) buf[len++] = (uint8_t)tag[i];
    buf[len++] = (uint8_t)(txCount_ & 0xFF);
  } else if (kind == TX_MESH_PING) {
    // A valid-LENGTH Meshtastic frame: real nodes RECEIVE it (a packet from an
    // unknown node !4e4f4354) though the payload won't decrypt. Header layout:
    // to[4]=broadcast, from[4]=myNodeId, id[4], flags(hop_limit=3), chanHash.
    buf[0] = buf[1] = buf[2] = buf[3] = 0xFF;
    buf[4] = myNodeId_ & 0xFF; buf[5] = (myNodeId_ >> 8) & 0xFF;
    buf[6] = (myNodeId_ >> 16) & 0xFF; buf[7] = (myNodeId_ >> 24) & 0xFF;
    uint32_t id = txPktId_++;
    buf[8] = id & 0xFF; buf[9] = (id >> 8) & 0xFF;
    buf[10] = (id >> 16) & 0xFF; buf[11] = (id >> 24) & 0xFF;
    buf[12] = 0x03; // hop_limit = 3
    buf[13] = 0x08; // channel hash (plausible LongFast-ish)
    for (int i = 14; i < 24; i++) buf[i] = (uint8_t)(0xA0 ^ i); // opaque payload
    len = 24;
  } else if (kind == TX_REPLAY) {
    if (lastRxLen_ <= 0) return false;
    len = lastRxLen_;
    memcpy(buf, lastRx_, len);
  } else {
    return false;
  }

  radio.standby();
  lastTxResult_ = radio.transmit(buf, len); // blocking
  lastTxLen_ = len;
  lastTxMs_ = now;
  txCount_++;
  radio.startReceive(); // back to listening
  return (lastTxResult_ == RADIOLIB_ERR_NONE);
}
#endif // NOCT_FEATURE_LORA_TX

#endif // NOCT_FEATURE_LORA
