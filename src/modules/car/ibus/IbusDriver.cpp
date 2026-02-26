/*
 * NOCTURNE_OS — I-Bus driver (optional FreeRTOS tasks).
 */
#include "IbusDriver.h"
#include "nocturne/config.h"
#include <string.h>

IbusDriver *IbusDriver::instance_ = nullptr;

IbusDriver::IbusDriver()
    : serial_(nullptr),
      begun_(false),
      synced_(false),
      userHandler_(nullptr) {
#if NOCT_IBUS_ENABLED
  rxQueue_ = nullptr;
  txQueue_ = nullptr;
  mutex_ = nullptr;
  taskReadHandle_ = nullptr;
  taskWriteHandle_ = nullptr;
#endif
}

void IbusDriver::onPacket(uint8_t *packet) {
  if (instance_ && instance_->userHandler_)
    instance_->userHandler_(packet);
  instance_->synced_ = true;
}

#if NOCT_IBUS_ENABLED
void IbusDriver::pushToRxQueue(uint8_t *packet) {
  if (!packet || !rxQueue_)
    return;
  uint8_t plen = packet[1] + 2;  /* source + length + (dest + data... + checksum) */
  if (plen > IBUS_PACKET_MAX)
    return;
  IbusRxItem item;
  item.len = plen;
  memcpy(item.data, packet, plen);
  xQueueSend(rxQueue_, &item, 0);
}

void IbusDriver::taskReadEntry(void *pv) {
  IbusDriver *d = (IbusDriver *)pv;
  if (d)
    d->taskReadLoop();
  vTaskDelete(nullptr);
}

void IbusDriver::taskWriteEntry(void *pv) {
  IbusDriver *d = (IbusDriver *)pv;
  if (d)
    d->taskWriteLoop();
  vTaskDelete(nullptr);
}

void IbusDriver::taskReadLoop() {
  for (;;) {
    /* Bounded timeout to avoid TWDT: do not use portMAX_DELAY. */
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
      ibus_.runRead();
      xSemaphoreGive(mutex_);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void IbusDriver::taskWriteLoop() {
  IbusTxItem item;
  for (;;) {
    /* Wait for next TX packet (blocking OK here); mutex take is bounded. */
    if (txQueue_ != nullptr && xQueueReceive(txQueue_, &item, portMAX_DELAY) == pdTRUE) {
      if (item.len > 0 && item.len <= IBUS_PACKET_MAX && mutex_ != nullptr &&
          xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
        ibus_.write(item.data, item.len);
        ibus_.runSendNext();
        xSemaphoreGive(mutex_);
      }
    }
  }
}

static void onPacketToRxQueue(uint8_t *packet) {
  if (IbusDriver::instance_)
    IbusDriver::instance_->pushToRxQueue(packet);
  if (IbusDriver::instance_)
    IbusDriver::instance_->synced_ = true;
}
#endif

void IbusDriver::begin(int txPin, int rxPin) {
  if (begun_)
    return;
  if (txPin < 0 || rxPin < 0)
    return;
  instance_ = this;
  serial_ = &Serial1;
  serial_->begin(9600, SERIAL_8E1, rxPin, txPin);
  ibus_.setIbusSerial(*serial_);

#if NOCT_IBUS_ENABLED
  rxQueue_ = xQueueCreate(IBUS_RX_QUEUE_LEN, sizeof(IbusRxItem));
  txQueue_ = xQueueCreate(IBUS_TX_QUEUE_LEN, sizeof(IbusTxItem));
  mutex_ = xSemaphoreCreateMutex();
  if (rxQueue_ && txQueue_ && mutex_) {
    ibus_.setPacketHandler(onPacketToRxQueue);
    xTaskCreate(taskReadEntry, "ibus_rx", 2048, this, 2, &taskReadHandle_);
    xTaskCreate(taskWriteEntry, "ibus_tx", 2048, this, 2, &taskWriteHandle_);
  } else {
    ibus_.setPacketHandler(onPacket);
  }
#else
  ibus_.setPacketHandler(onPacket);
#endif

  begun_ = true;
  synced_ = false;
}

void IbusDriver::end() {
  if (!begun_)
    return;
#if NOCT_IBUS_ENABLED
  if (taskReadHandle_ != nullptr) {
    vTaskDelete(taskReadHandle_);
    taskReadHandle_ = nullptr;
  }
  if (taskWriteHandle_ != nullptr) {
    vTaskDelete(taskWriteHandle_);
    taskWriteHandle_ = nullptr;
  }
  if (rxQueue_ != nullptr) {
    vQueueDelete(rxQueue_);
    rxQueue_ = nullptr;
  }
  if (txQueue_ != nullptr) {
    vQueueDelete(txQueue_);
    txQueue_ = nullptr;
  }
  if (mutex_ != nullptr) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
#endif
  if (serial_)
    serial_->end();
  begun_ = false;
  synced_ = false;
  instance_ = nullptr;
}

void IbusDriver::tick() {
  if (!begun_)
    return;
#if NOCT_IBUS_ENABLED
  if (rxQueue_ && userHandler_) {
    IbusRxItem item;
    while (xQueueReceive(rxQueue_, &item, 0) == pdTRUE) {
      if (item.len > 0 && item.len <= IBUS_PACKET_MAX)
        userHandler_(item.data);
    }
  }
#else
  ibus_.run();
#endif
}

void IbusDriver::write(const uint8_t *data, uint8_t len) {
  if (!begun_ || !data || len == 0)
    return;
#if NOCT_IBUS_ENABLED
  if (txQueue_ != nullptr && len <= IBUS_PACKET_MAX) {
    IbusTxItem item;
    item.len = len;
    memcpy(item.data, data, len);
    xQueueSend(txQueue_, &item, 0);
  }
#else
  ibus_.write(data, len);
#endif
}

void IbusDriver::setPacketHandler(void (*handler)(uint8_t *packet)) {
  userHandler_ = handler;
}
