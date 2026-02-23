/*
 * I-Bus serial parser 9600 8E1 (ESP32-compatible).
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
  void write(const uint8_t *message, uint8_t size);
  void setPacketHandler(void (*handler)(uint8_t *packet));
  uint8_t calculateChecksum(const uint8_t *data, uint8_t length);

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

  HardwareSerial *ibusSerial_;
  RingBuffer *rxBuffer_;
  RingBuffer *txBuffer_;

  State state_;
  uint8_t source_;
  uint8_t length_;
  uint8_t destination_;
  uint8_t ibusByte_[40];
  void (*packetHandler_)(uint8_t *packet);

  static const unsigned long kPacketGapMs = 10;
  unsigned long lastRxMs_;
  unsigned long lastTxMs_;
  bool clearToSend_;
};

#endif
