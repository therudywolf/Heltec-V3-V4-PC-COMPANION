/*
 * NOCTURNE_OS — BLE Phantom Spammer + BLE Scan + Clone.
 * Spam: Apple/Google/Samsung/Windows. Scan: list devices. Clone: advertise as
 * selected. WiFi MUST be off while active.
 */
#pragma once
#include <Arduino.h>

#define BLE_PHANTOM_ROTATE_MS 1500
#define BLE_PHANTOM_PAYLOAD_COUNT 6
#define BLE_SCAN_DEVICE_MAX 20
#define BLE_DEVICE_NAME_LEN 32
#define BLE_DEVICE_ADDR_LEN 18

struct BleScanDevice {
  char name[BLE_DEVICE_NAME_LEN];
  char addr[BLE_DEVICE_ADDR_LEN];
  int rssi;
};

class BleManager {
public:
  BleManager();
  void begin();
  void stop();
  void tick();

  bool isActive() const { return active_; }
  int getPacketCount() const { return packetCount_; }
  int getCurrentPayloadIndex() const { return currentPayloadIndex_; }

  void beginScan();
  void stopScan();
  bool isScanning() const { return scanning_; }
  int getScanCount() const { return scanCount_; }
  const BleScanDevice *getScanDevice(int index) const;
  void cloneDevice(int index);
  bool isCloning() const { return cloning_; }
  void onScanResult(void *device);

private:
  void startSpam();
  void setPayload(int index);

  bool active_ = false;
  int packetCount_ = 0;
  int currentPayloadIndex_ = 0;
  unsigned long lastRotateMs_ = 0;

  bool scanning_ = false;
  BleScanDevice scanDevices_[BLE_SCAN_DEVICE_MAX];
  int scanCount_ = 0;
  bool cloning_ = false;
  char cloneName_[BLE_DEVICE_NAME_LEN];
  char cloneAddr_[BLE_DEVICE_ADDR_LEN];
};
