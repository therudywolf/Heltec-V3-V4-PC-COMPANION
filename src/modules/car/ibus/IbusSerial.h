/*
 * I-Bus serial layer: 9600 8E1, frame [Source][Length][Destination][Data...][XOR].
 * Pins: NOCT_IBUS_TX_PIN 39, NOCT_IBUS_RX_PIN 38 (config.h — single source of truth for Heltec V4).
 * TX only when RX line silent >= 5 ms; abort TX if Serial.available() > 0 before send; send packet in one block.
 */
#ifndef IBUSSERIAL_H
#define IBUSSERIAL_H

#include "Arduino.h"
#include "RingBuffer.h"

class IbusSerial {
 public:
  IbusSerial();
  void setIbusSerial(HardwareSerial &serial);
  void run();
  /** Called from Task_IBus_Read when using FreeRTOS. */
  void runRead() { readIbus(); }
  /** Called from Task_IBus_Write when using FreeRTOS (one packet send attempt). */
  void runSendNext() { sendNextPacket(); }
  void write(const uint8_t *message, uint8_t size);
  void setPacketHandler(void (*handler)(uint8_t *packet));
  uint8_t calculateChecksum(const uint8_t *data, uint8_t length);

  /** Stats for OLED: RX packets (good checksum), TX packets sent, errors (bad checksum + collisions). */
  uint32_t getRxCount() const { return rxCount_; }
  uint32_t getTxCount() const { return txCount_; }
  uint32_t getErrorCount() const { return errorCount_; }
  uint32_t getCollisionCount() const { return collisionCount_; }

 private:
  enum State {
    FIND_SOURCE,
    FIND_LENGTH,
    FIND_MESSAGE,
    GOOD_CHECKSUM,
    BAD_CHECKSUM,
  };

  void readIbus();
  void sendNextPacket();
  /** Abort TX queue when bus is not silent (collision avoidance). */
  void clearTxQueue();

  HardwareSerial *ibusSerial_;
  RingBuffer *rxBuffer_;
  RingBuffer *txBuffer_;

  State state_;
  uint8_t source_;
  uint8_t length_;
  uint8_t destination_;
  uint8_t ibusByte_[40];
  void (*packetHandler_)(uint8_t *packet);

  static const unsigned long kPacketGapMs = 10;  /* Min 10 ms RX silence before TX (I-Bus collision avoidance). */
  unsigned long lastRxMs_;
  unsigned long lastTxMs_;
  bool clearToSend_;

  uint32_t rxCount_ = 0;
  uint32_t txCount_ = 0;
  uint32_t errorCount_ = 0;
  uint32_t collisionCount_ = 0;
};

#endif
