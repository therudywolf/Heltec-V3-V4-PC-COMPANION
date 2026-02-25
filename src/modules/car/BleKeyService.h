/*
 * NOCTURNE_OS — BLE key: peripheral for proximity unlock/lock.
 * Phone connects = unlock, disconnects (after debounce) = lock.
 */
#ifndef NOCTURNE_BLE_KEY_SERVICE_H
#define NOCTURNE_BLE_KEY_SERVICE_H

#include <Arduino.h>
#include <cstdint>

class BleKeyService {
 public:
  BleKeyService();
  void begin();
  void end();
  void tick();

  bool isConnected() const { return connected_; }
  bool isActive() const { return active_; }

  void setConnectionCallback(void (*cb)(bool connected)) { connectionCb_ = cb; }

  /** Optional: when phone writes to BMW control characteristic, this is called with cmd 0..11 (Goodbye..DoorLock). */
  void setLightCommandCallback(void (*cb)(uint8_t cmd)) { lightCommandCb_ = cb; }

  /** Optional: when phone writes to Now Playing characteristic (track\\0artist), this is called. */
  void setNowPlayingCallback(void (*cb)(const char *track, const char *artist)) { nowPlayingCb_ = cb; }

  /** Update status characteristic (READ/NOTIFY). Call from BmwManager::tick().
   * lastMflAction: 0=none, 1=next, 2=prev, 3=play_pause, 4=vol_up, 5=vol_down.
   * doorByte1, doorByte2: GM 0x7a; lockState: 0=unlocked, 1=locked, 2=double, 0xFF=unknown;
   * ignition: 0=off, 1=pos1, 2=pos2, -1=unknown; odometerKm: -1 = unknown. */
  void updateStatus(bool ibusSynced, bool phoneConnected, bool pdcValid, bool obdConnected,
                   int coolantC, int oilC, int rpm, const int *pdcDists, uint8_t lastMflAction,
                   uint8_t doorByte1 = 0xFF, uint8_t doorByte2 = 0xFF, uint8_t lockState = 0xFF,
                   int ignition = -1, int odometerKm = -1);

  /** Optional: when phone writes to cluster-text characteristic, this is called with UTF-8 string (max 20 bytes). */
  void setClusterTextCallback(void (*cb)(const char *text)) { clusterTextCb_ = cb; }
  /** Called from NimBLE when cluster text characteristic is written (internal). */
  void onClusterTextReceived(const uint8_t *data, size_t len);

  /** Called from NimBLE when control characteristic is written (internal). */
  void onLightCommandReceived(uint8_t cmd);
  /** Called from NimBLE when Now Playing characteristic is written (internal). */
  void onNowPlayingReceived(const uint8_t *data, size_t len);

  /** Called from NimBLE server callbacks (internal). */
  void onConnect();
  void onDisconnect();

  /** Call from onConnect so next updateStatus() will notify (phone gets current status immediately). */
  void requestStatusNotifyOnNextUpdate() { lastStatusPacketValid_ = false; }

 private:
  bool active_ = false;
  bool connected_ = false;
  void (*connectionCb_)(bool) = nullptr;
  void (*lightCommandCb_)(uint8_t) = nullptr;
  void (*nowPlayingCb_)(const char *track, const char *artist) = nullptr;
  void (*clusterTextCb_)(const char *text) = nullptr;

  static const size_t kStatusPacketLen = 16;
  uint8_t lastStatusPacket_[kStatusPacketLen];
  bool lastStatusPacketValid_ = false;

  /** Debounce: delay before reporting disconnect (avoid brief dropouts). */
  static const unsigned long kDisconnectDebounceMs = 2500;
  unsigned long disconnectReportedAt_ = 0;
  bool disconnectPending_ = false;
};

#endif
