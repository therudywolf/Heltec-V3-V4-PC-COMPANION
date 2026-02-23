/*
 * NOCTURNE_OS — BmwManager: BMW E39 Assistant via I-Bus.
 * Proximity lock/unlock, diagnostics, multimedia, light control, PDC, cluster display.
 */
#ifndef NOCTURNE_BMW_MANAGER_H
#define NOCTURNE_BMW_MANAGER_H

#include <Arduino.h>
#include "ibus/IbusDriver.h"
#include "BleKeyService.h"

/** Number of I-Bus actions in the assistant menu (lights, locks, trunk, cluster). */
#define BMW_ACTION_COUNT 10

class BmwManager {
 public:
  BmwManager();
  void begin();
  void end();
  void tick();

  bool isActive() const { return active_; }
  bool isIbusSynced() const { return ibusSynced_; }
  bool isPhoneConnected() const { return phoneConnected_; }
  void setPhoneConnected(bool connected) { phoneConnected_ = connected; }

  void getStatusLine(char *buf, size_t len) const;

  IbusDriver *ibus() { return &ibus_; }

  void sendGoodbyeLights();
  void sendFollowMeHome();
  void sendParkLights();
  void sendHazardLights();
  void sendLowBeams();
  void sendLightsOff();
  void sendLock();
  void sendUnlock();
  void sendTrunkOpen();

  /** MFL: last button from steering wheel (for AVRCP/media). */
  enum MflAction { MFL_NONE = 0, MFL_NEXT, MFL_PREV, MFL_PLAY_PAUSE, MFL_VOL_UP, MFL_VOL_DOWN };
  MflAction getLastMflAction() const { return lastMflAction_; }
  void clearLastMflAction() { lastMflAction_ = MFL_NONE; }

  /** Now playing (for OLED / MID). Set from app or AVRCP when available. */
  void setNowPlaying(const char *track, const char *artist);
  const char *getNowPlayingTrack() const { return nowPlayingTrack_; }
  const char *getNowPlayingArtist() const { return nowPlayingArtist_; }

  /** PDC distances (cm), -1 = no data. Index: FL, FR, RL, RR or similar. */
  void getPdcDistances(int *dists, int maxCount) const;
  bool hasPdcData() const { return pdcValid_; }

  void sendClusterText(const char *text);

  /** OBD (stub): when ELM327/obd is connected, fill these for display/shift lamp. */
  bool isObdConnected() const { return obdConnected_; }
  int getObdRpm() const { return obdRpm_; }
  int getObdCoolantTempC() const { return obdCoolantTempC_; }
  int getObdOilTempC() const { return obdOilTempC_; }
  void setObdData(bool connected, int rpm, int coolantC, int oilC);

  void onIbusPacket(uint8_t *packet);
  void onPhoneConnectionChanged(bool connected);

 private:
  void parseMflButton(uint8_t *packet);
  void parsePdcPacket(uint8_t *packet);

  bool active_ = false;
  bool ibusSynced_ = false;
  bool phoneConnected_ = false;
  MflAction lastMflAction_ = MFL_NONE;
  static const int kNowPlayingLen = 48;
  char nowPlayingTrack_[kNowPlayingLen];
  char nowPlayingArtist_[kNowPlayingLen];
  static const int kPdcSensors = 4;
  int pdcDists_[kPdcSensors];
  bool pdcValid_ = false;
  bool obdConnected_ = false;
  int obdRpm_ = 0;
  int obdCoolantTempC_ = -1;
  int obdOilTempC_ = -1;
  unsigned long lastPollMs_ = 0;
  IbusDriver ibus_;
  BleKeyService bleKey_;
};

#endif
