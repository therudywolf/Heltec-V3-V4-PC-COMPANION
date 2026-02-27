/*
 * I-Bus serial parser 9600 8E1 (ESP32-compatible).
 */
#include "IbusSerial.h"
#include "nocturne/config.h"

IbusSerial::IbusSerial()
    : ibusSerial_(nullptr),
      rxBuffer_(new RingBuffer(128)),
      txBuffer_(new RingBuffer(64)),
      state_(FIND_SOURCE),
      source_(0),
      length_(0),
      destination_(0),
      packetHandler_(nullptr),
      lastRxMs_(0),
      lastTxMs_(0),
      clearToSend_(true) {
  memset(ibusByte_, 0, sizeof(ibusByte_));
}

void IbusSerial::setIbusSerial(HardwareSerial &serial) {
  ibusSerial_ = &serial;
  /* Serial must be initialized once in IbusDriver::begin() with correct pins. */
}

void IbusSerial::setPacketHandler(void (*handler)(uint8_t *packet)) {
  packetHandler_ = handler;
}

void IbusSerial::readIbus() {
  if (!ibusSerial_ || !rxBuffer_)
    return;
  const unsigned long now = millis();
  /* If there is a gap of >=8 ms between bytes while buffer has data, discard partial frame. */
  if (rxBuffer_->available() > 0 && (now - lastRxMs_) >= 8) {
    int av = rxBuffer_->available();
    if (av > 0)
      rxBuffer_->remove(av);
    state_ = FIND_SOURCE;
  }
  while (ibusSerial_->available()) {
    rxBuffer_->write(ibusSerial_->read());
    lastRxMs_ = now;  /* Track last RX time for clear-to-send / frame timeout. */
  }

  switch (state_) {
    case FIND_SOURCE:
      if (rxBuffer_->available() >= 1) {
        source_ = (uint8_t)rxBuffer_->peek(0);
        state_ = FIND_LENGTH;
      }
      break;

    case FIND_LENGTH:
      if (rxBuffer_->available() >= 2) {
        length_ = (uint8_t)rxBuffer_->peek(1);
        if (length_ >= 0x03 && length_ <= 0x24) {
          state_ = FIND_MESSAGE;
        } else {
          rxBuffer_->remove(1);
          state_ = FIND_SOURCE;
        }
      }
      break;

    case FIND_MESSAGE: {
      if (rxBuffer_->available() < (int)(length_ + 2))
        break;
      uint8_t checksumByte = 0;
      for (int i = 0; i <= length_; i++)
        checksumByte ^= (uint8_t)rxBuffer_->peek(i);
      if ((uint8_t)rxBuffer_->peek(length_ + 1) == checksumByte) {
        lastRxMs_ = millis();
        state_ = GOOD_CHECKSUM;
      } else {
        state_ = BAD_CHECKSUM;
      }
      break;
    }

    case GOOD_CHECKSUM:
      for (int i = 0; i <= length_ + 1; i++)
        ibusByte_[i] = (uint8_t)rxBuffer_->read();
      rxCount_++;
      if (packetHandler_)
        packetHandler_(ibusByte_);
      state_ = FIND_SOURCE;
      break;

    case BAD_CHECKSUM:
      errorCount_++;
      rxBuffer_->remove(1);
      state_ = FIND_SOURCE;
      break;
  }
}

void IbusSerial::clearTxQueue() {
  if (!txBuffer_)
    return;
  int av = txBuffer_->available();
  while (av >= 2) {
    int lenByte = txBuffer_->peek(0);
    if (lenByte <= 0 || lenByte > 40) {
      txBuffer_->remove(1);
      av--;
      continue;
    }
    int need = 1 + lenByte;
    if (av < need)
      break;
    txBuffer_->remove(need);
    av -= need;
  }
}

void IbusSerial::write(const uint8_t *message, uint8_t size) {
  if (!txBuffer_ || size == 0)
    return;
  uint8_t checksum = calculateChecksum(message, size);
  txBuffer_->write(size + 1);
  for (uint8_t i = 0; i < size; i++)
    txBuffer_->write(message[i]);
  txBuffer_->write(checksum);
}

void IbusSerial::sendNextPacket() {
  if (!ibusSerial_ || !txBuffer_ || txBuffer_->available() < 2)
    return;
  const unsigned long now = millis();
  /* Strict RTOS: only TX when bus silent >= 5 ms. */
  if ((now - lastRxMs_) < kPacketGapMs)
    return;
  /* Abort TX queue immediately if any RX data (bus not silent). */
  if (ibusSerial_->available() > 0) {
    collisionCount_++;
    clearTxQueue();
    return;
  }

  const int lenByte = txBuffer_->read();
  if (lenByte <= 0 || lenByte > 40)
    return;
  uint8_t buf[40];
  for (int i = 0; i < lenByte; i++) {
    const int b = txBuffer_->read();
    if (b < 0)
      return;
    buf[i] = (uint8_t)b;
  }
#if (NOCT_IBUS_MONITOR_VERBOSE || NOCT_BMW_DEBUG)
  Serial.print("[IBus TX] ");
  for (int i = 0; i < lenByte; i++)
    Serial.printf("%02X ", buf[i]);
  Serial.println();
#endif
  /* Send entire packet in one block (no byte-by-byte; UART FIFO). */
  ibusSerial_->write(buf, (size_t)lenByte);
  txCount_++;
  lastTxMs_ = now;
}

uint8_t IbusSerial::calculateChecksum(const uint8_t *data, uint8_t length) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < length; i++)
    checksum ^= data[i];
  return checksum;
}

void IbusSerial::run() {
  readIbus();
  const unsigned long now = millis();
  clearToSend_ = (now - lastRxMs_ >= kPacketGapMs);
  if (clearToSend_ && txBuffer_ && txBuffer_->available() > 0)
    sendNextPacket();
}
