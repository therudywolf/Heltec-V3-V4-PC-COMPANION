/*
 * NOCTURNE_OS — BmwManager: BMW E39 Assistant via I-Bus.
 * Proximity lock/unlock, diagnostics, multimedia, light control, PDC, cluster display.
 */
#ifndef NOCTURNE_BMW_MANAGER_H
#define NOCTURNE_BMW_MANAGER_H

#include <Arduino.h>
#include "ibus/IbusDriver.h"
#include "BleKeyService.h"
#include "A2dpSink.h"

/** Number of I-Bus actions in the assistant menu (lights, locks, trunk, cluster, doors). */
#define BMW_ACTION_COUNT 12

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
  void sendDoorsUnlockInterior();
  void sendDoorsLockKey();
  void sendDoorsHardLock();
  void sendAllExceptDriverLock();
  void sendDriverDoorLock();
  void sendDoorsFuelTrunk();

  void sendWindowFrontDriverOpen();
  void sendWindowFrontDriverClose();
  void sendWindowFrontPassengerOpen();
  void sendWindowFrontPassengerClose();
  void sendWindowRearDriverOpen();
  void sendWindowRearDriverClose();
  void sendWindowRearPassengerOpen();
  void sendWindowRearPassengerClose();
  void sendWipersFront();
  void sendWasherFront();
  void sendInteriorOff();
  void sendInteriorOn3s();
  void sendClownFlash();

  /** Cyclic light show (e.g. for shows/video): cycles Hazard/Park/Goodbye/LowBeam/LightsOff. */
  void startLightShow();
  void stopLightShow();
  bool isLightShowActive() const { return lightShowActive_; }

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
  /** Send Now Playing to head unit display (MID) via I-Bus UPDATE_MID. Max 12 chars. */
  void sendUpdateMid();

  /** OBD (stub): when ELM327/obd is connected, fill these for display/shift lamp. */
  bool isObdConnected() const { return obdConnected_; }
  int getObdRpm() const { return obdRpm_; }
  int getObdCoolantTempC() const { return obdCoolantTempC_; }
  int getObdOilTempC() const { return obdOilTempC_; }
  void setObdData(bool connected, int rpm, int coolantC, int oilC);
  /** Last coolant from I-Bus IKE (0x19), -128 = no data. Use when OBD not connected. */
  int getIkeCoolantC() const { return lastIkeCoolantC_; }

  /** Door/lid status from GM 0x7a: byte1 (doors, lock, interior lamp), byte2 (windows, sunroof, trunk). 0xFF = no data. */
  uint8_t getDoorLidByte1() const { return lastDoorLidByte1_; }
  uint8_t getDoorLidByte2() const { return lastDoorLidByte2_; }
  /** Ignition from IKE 0x11: 0=off, 1=pos1, 2=pos2 (run). -1 = no data. */
  int getIgnitionState() const { return lastIgnition_; }
  /** Odometer from IKE 0x17 (km), -1 = no data. */
  int getOdometerKm() const { return lastOdometerKm_; }

  void onIbusPacket(uint8_t *packet);
  void onPhoneConnectionChanged(bool connected);

 private:
  void parseMflButton(uint8_t *packet);
  void parsePdcPacket(uint8_t *packet);
  static const int kMidDisplayChars = 12;

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
  int lastIkeCoolantC_ = -128;
  uint8_t lastDoorLidByte1_ = 0xFF;
  uint8_t lastDoorLidByte2_ = 0xFF;
  int lastIgnition_ = -1;
  int lastOdometerKm_ = -1;
  unsigned long lastPollMs_ = 0;
  uint8_t pollAlternate_ = 0;
  bool welcomeSentOnConnect_ = false;
  bool lightShowActive_ = false;
  uint8_t lightShowStep_ = 0;
  unsigned long lastLightShowMs_ = 0;
  static const unsigned long kLightShowIntervalMs = 800;
  unsigned long lastShiftClusterMs_ = 0;
  static const unsigned long kShiftClusterIntervalMs = 1000;
  static const int kShiftRpmThreshold = 5500;
  IbusDriver ibus_;
  BleKeyService bleKey_;
  A2dpSink a2dpSink_;
};

#endif
