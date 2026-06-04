#include "modules/lora/LoraManager.h"
#if NOCT_FEATURE_LORA

#include <Arduino.h>
#include <SPI.h>

// Dedicated SPI bus for the SX1262 (separate from the OLED's I2C).
static SPIClass loraSpi(FSPI);
static SX1262 radio =
    new Module(NOCT_LORA_NSS, NOCT_LORA_DIO1, NOCT_LORA_RST, NOCT_LORA_BUSY, loraSpi);

bool LoraManager::begin() {
  loraSpi.begin(NOCT_LORA_SCK, NOCT_LORA_MISO, NOCT_LORA_MOSI, NOCT_LORA_NSS);
  // 868 MHz, 125 kHz BW, SF9, CR4/7, private sync, 10 dBm, 8-sym preamble,
  // 1.8V TCXO (Heltec module has a TCXO), DC-DC regulator.
  lastErr_ = radio.begin(NOCT_LORA_FREQ, 125.0f, 9, 7, 0x12, 10, 8, NOCT_LORA_TCXO, false);
  ready_ = (lastErr_ == RADIOLIB_ERR_NONE);
  if (ready_) {
    radio.setDio2AsRfSwitch(true); // Heltec SX1262 uses DIO2 as the RF switch
    radio.startReceive();
  }
  lastPollMs_ = millis();
  return ready_;
}

void LoraManager::startListen() {
  if (ready_) radio.startReceive();
}

void LoraManager::tick() {
  if (!ready_) return;
  unsigned long now = millis();
  if (now - lastPollMs_ < 120) return; // ~8 Hz
  lastPollMs_ = now;

  // Live channel RSSI (not packet RSSI) — a simple spectrum/energy meter.
  rssi_ = radio.getRSSI(false);
  // Slow noise-floor estimate (rises toward quiet level, snaps down on signal).
  if (rssi_ < floor_) floor_ = rssi_;
  else floor_ += (rssi_ - floor_) * 0.02f;

  // Channel Activity Detection: a quick LoRa-preamble probe.
  int cad = radio.scanChannel();
  if (cad == RADIOLIB_LORA_DETECTED) {
    activity_++;
    // A detected preamble often means a frame is incoming — try to read it.
    uint8_t buf[64];
    int len = radio.readData(buf, sizeof(buf));
    if (len > 0 || radio.getPacketLength() > 0) packets_++;
    radio.startReceive();
  }
}

#endif // NOCT_FEATURE_LORA
