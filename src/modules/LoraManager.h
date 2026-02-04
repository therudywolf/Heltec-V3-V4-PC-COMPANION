#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

// HELTEC V3/V4 PINS for SX1262
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13

struct LoraPacket {
  String data; // Raw hex or text
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

private:
  Module mod_;
  SX1262 radio_;
  bool active_ = false;
  bool isReceiving_ = false;
  float currentRSSI_ = -120.0f;

  LoraPacket lastPacket_;
  int packetCount_ = 0;

  unsigned long lastUpdate_ = 0;
};
