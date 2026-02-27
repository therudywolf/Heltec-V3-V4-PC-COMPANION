/*
 * NOCTURNE_OS — I-Bus driver: UART 9600 8E1, packet handler, write.
 * When I-Bus enabled: two FreeRTOS tasks (Read → rx_queue, Write ← tx_queue); tick() drains rx_queue.
 */
#ifndef NOCTURNE_IBUS_DRIVER_H
#define NOCTURNE_IBUS_DRIVER_H

#include <Arduino.h>
#include "nocturne/config.h"
#include "IbusSerial.h"
#include "IbusDefines.h"

#if NOCT_IBUS_ENABLED
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

#define IBUS_RX_QUEUE_LEN 16
#define IBUS_TX_QUEUE_LEN 16
#define IBUS_PACKET_MAX  40

/** One received I-Bus packet (for rx_queue). */
struct IbusRxItem {
  uint8_t len;
  uint8_t data[IBUS_PACKET_MAX];
};
/** One message to send (for tx_queue); checksum added by sender task. */
struct IbusTxItem {
  uint8_t len;
  uint8_t data[IBUS_PACKET_MAX];
};

class IbusDriver {
 public:
  IbusDriver();
  /** Init UART on given TX/RX pins. Call once. When NOCT_IBUS_ENABLED, creates FreeRTOS tasks and queues. */
  void begin(int txPin, int rxPin);
  void end();
  /** Call every loop. When using FreeRTOS: drains rx_queue and calls packet handler for each; otherwise runs ibus_.run(). */
  void tick();
  /** Send raw message (checksum added by IbusSerial). When FreeRTOS: posts to tx_queue. */
  void write(const uint8_t *data, uint8_t len);
  /** Set callback for each received packet: packet[0]=src, [1]=len, [2]=dest, ... */
  void setPacketHandler(void (*handler)(uint8_t *packet));
  bool isSynced() const { return synced_; }

  /** I-Bus stats for OLED (from IbusSerial). */
  uint32_t getRxCount() const { return ibus_.getRxCount(); }
  uint32_t getTxCount() const { return ibus_.getTxCount(); }
  uint32_t getErrorCount() const { return ibus_.getErrorCount(); }
  uint32_t getCollisionCount() const { return ibus_.getCollisionCount(); }

#if NOCT_IBUS_ENABLED
  /** Called from packet handler to enqueue received packet (used by tasks). */
  void pushToRxQueue(uint8_t *packet);
  /** For FreeRTOS Read task: run parser only. */
  void runRead() { ibus_.runRead(); }
  /** For FreeRTOS Write task: try send one packet from internal TX buffer. */
  void runSendNext() { ibus_.runSendNext(); }
#endif

 private:
  static void onPacket(uint8_t *packet);
  friend void onPacketToRxQueue(uint8_t *packet);
#if defined(NOCT_IBUS_ENABLED) && (NOCT_IBUS_ENABLED) == 1
  static void taskReadEntry(void *pv);
  static void taskWriteEntry(void *pv);
  void taskReadLoop();
  void taskWriteLoop();
#endif

  HardwareSerial *serial_;
  IbusSerial ibus_;
  bool begun_;
  bool synced_;
  void (*userHandler_)(uint8_t *packet);
  static IbusDriver *instance_;

#if NOCT_IBUS_ENABLED
  QueueHandle_t rxQueue_;
  QueueHandle_t txQueue_;
  SemaphoreHandle_t mutex_;
  TaskHandle_t taskReadHandle_;
  TaskHandle_t taskWriteHandle_;
#endif
};

#endif
