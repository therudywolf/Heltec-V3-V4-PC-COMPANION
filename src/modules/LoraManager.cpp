#include "LoraManager.h"

LoraManager::LoraManager()
    : mod_(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY), radio_(&mod_) {}

void LoraManager::begin() {
  // Hardware init is deferred until setMode(true) to save power
}

void LoraManager::setMode(bool active) {
  active_ = active;
  if (active) {
    Serial.println("[LORA] Powering UP SX1262...");
    // Meshtastic EU_868 LongFast: 869.525 MHz, BW 250 kHz, SF 11, CR 5
    int state = radio_.begin(869.525f, 250.0f, 11, 5,
                             RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22, 8);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LORA] Init Success!");
      radio_.startReceive();
    } else {
      Serial.printf("[LORA] Init Failed, code %d\n", state);
    }
  } else {
    radio_.sleep();
    Serial.println("[LORA] Sleeping...");
  }
}

void LoraManager::tick() {
  if (!active_)
    return;

  // 1. Read instantaneous RSSI (background noise level)
  if (millis() - lastUpdate_ > 500) {
    currentRSSI_ = radio_.getRSSI();
    lastUpdate_ = millis();
  }

  // 2. Check for packet
  uint8_t buf[256];
  int state = radio_.readData(buf, sizeof(buf));

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LORA] PACKET CAPTURED!");
    size_t len = radio_.getPacketLength();
    lastPacket_.rssi = radio_.getRSSI();
    lastPacket_.snr = radio_.getSNR();
    lastPacket_.timestamp = millis();
    packetCount_++;

    // Convert raw bytes to hex string for display
    String hexOut = "";
    for (size_t i = 0; i < len && i < sizeof(buf); i++) {
      char cbuf[4];
      snprintf(cbuf, sizeof(cbuf), "%02X ", (unsigned)buf[i]);
      hexOut += cbuf;
    }
    lastPacket_.data = hexOut;

    radio_.startReceive();
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT &&
             state != RADIOLIB_ERR_CRC_MISMATCH) {
    // Non-timeout error: optionally restart receive
    radio_.startReceive();
  }
}
