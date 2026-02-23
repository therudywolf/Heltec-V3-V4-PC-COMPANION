/*
 * NOCTURNE_OS — I-Bus driver.
 */
#include "IbusDriver.h"
#include "nocturne/config.h"

IbusDriver *IbusDriver::instance_ = nullptr;

IbusDriver::IbusDriver()
    : serial_(nullptr),
      begun_(false),
      synced_(false),
      userHandler_(nullptr) {}

void IbusDriver::onPacket(uint8_t *packet) {
  if (instance_ && instance_->userHandler_)
    instance_->userHandler_(packet);
  instance_->synced_ = true;
}

void IbusDriver::begin(int txPin, int rxPin) {
  if (begun_)
    return;
  if (txPin < 0 || rxPin < 0)
    return;  /* I-Bus disabled (pins not configured) */
  instance_ = this;
  serial_ = &Serial1;
  serial_->begin(9600, SERIAL_8E1, rxPin, txPin);
  ibus_.setIbusSerial(*serial_);
  ibus_.setPacketHandler(onPacket);
  begun_ = true;
  synced_ = false;
}

void IbusDriver::end() {
  if (!begun_)
    return;
  if (serial_)
    serial_->end();
  begun_ = false;
  synced_ = false;
  instance_ = nullptr;
}

void IbusDriver::tick() {
  if (!begun_)
    return;
  ibus_.run();
}

void IbusDriver::write(const uint8_t *data, uint8_t len) {
  if (!begun_ || !data || len == 0)
    return;
  ibus_.write(data, len);
}

void IbusDriver::setPacketHandler(void (*handler)(uint8_t *packet)) {
  userHandler_ = handler;
}
