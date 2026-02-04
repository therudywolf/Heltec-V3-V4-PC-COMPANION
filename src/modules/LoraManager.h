#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

// HELTEC V3/V4 PINS for SX1262
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13

#define LORA_RSSI_HISTORY_LEN 128
#define LORA_PACKET_BUF_SIZE 3
#define LORA_RAW_PACKET_MAX 256

struct LoraPacket {
  String data; // Raw hex or text
  float rssi;
  float snr;
  unsigned long timestamp;
};

/** One raw packet for replay: bytes + metadata for display */
struct LoraRawPacket {
  uint8_t data[LORA_RAW_PACKET_MAX];
  size_t len;
  float rssi;
  float snr;
  unsigned long timestamp;
};

class LoraManager {
public:
  LoraManager();
  void begin();
  void tick();
  void setMode(bool active);

  bool isReceiving() const { return isReceiving_; }
  float getCurrentRSSI() const { return currentRSSI_; }
  LoraPacket getLastPacket() const { return lastPacket_; }
  int getPacketCount() const { return packetCount_; }

  /** Spectrum: RSSI samples every 100ms for waterfall/bar */
  const int8_t *getRssiHistory(int &outLen) const;
  int getRssiHistoryHead() const { return rssiHistoryHead_; }
  static constexpr int rssiHistoryLen() { return LORA_RSSI_HISTORY_LEN; }

  /** Packet buffer for SIGINT: last 3 packets (0 = newest) */
  int getPacketBufferCount() const { return packetBufferCount_; }
  const LoraRawPacket *getPacketInBuffer(int index) const;

  /** Replay last captured packet (REPLAY attack) */
  void replayLast();
  /** Clear packet buffer and reset count (Short press) */
  void clearBuffer();

private:
  void captureAndStore(const uint8_t *rawData, size_t len, float rssi,
                       float snr);

  Module mod_;
  SX1262 radio_;
  bool active_ = false;
  bool isReceiving_ = false;
  float currentRSSI_ = -120.0f;

  LoraPacket lastPacket_;
  int packetCount_ = 0;

  unsigned long lastUpdate_ = 0;
  unsigned long lastRssiSampleMs_ = 0;
  int8_t rssiHistory_[LORA_RSSI_HISTORY_LEN];
  int rssiHistoryHead_ = 0;
  int rssiHistoryCount_ = 0;

  LoraRawPacket packetBuffer_[LORA_PACKET_BUF_SIZE];
  int packetBufferHead_ = 0;
  int packetBufferCount_ = 0;
  uint8_t lastPacketRaw_[LORA_RAW_PACKET_MAX];
  size_t lastPacketRawLen_ = 0;
};
