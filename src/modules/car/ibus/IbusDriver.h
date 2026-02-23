/*
 * NOCTURNE_OS — I-Bus driver: UART 9600 8E1, packet handler, write.
 */
#ifndef NOCTURNE_IBUS_DRIVER_H
#define NOCTURNE_IBUS_DRIVER_H

#include <Arduino.h>
#include "IbusSerial.h"
#include "IbusDefines.h"

class IbusDriver {
 public:
  IbusDriver();
  /** Init UART on given TX/RX pins. Call once. */
  void begin(int txPin, int rxPin);
  void end();
  /** Call every loop. Reads bus and sends queued packets. */
  void tick();
  /** Send raw message (checksum added by IbusSerial). */
  void write(const uint8_t *data, uint8_t len);
  /** Set callback for each received packet: packet[0]=src, [1]=len, [2]=dest, ... */
  void setPacketHandler(void (*handler)(uint8_t *packet));
  bool isSynced() const { return synced_; }

 private:
  static void onPacket(uint8_t *packet);

  HardwareSerial *serial_;
  IbusSerial ibus_;
  bool begun_;
  bool synced_;
  void (*userHandler_)(uint8_t *packet);
  static IbusDriver *instance_;
};

#endif
