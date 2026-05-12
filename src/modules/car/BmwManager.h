/*
 * NOCTURNE_OS — BmwManager: BMW E39 Assistant via I-Bus.
 * Proximity lock/unlock, diagnostics, multimedia, light control, PDC, cluster display.
 */
#ifndef NOCTURNE_BMW_MANAGER_H
#define NOCTURNE_BMW_MANAGER_H

#include <Arduino.h>
#include <atomic>
#include "ibus/IbusDriver.h"
#include "BleKeyService.h"
#include "DemoManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/** Demo state and queue live in DemoManager (Task_DemoMode on Core 1). */

/** Number of I-Bus actions in the assistant menu (lights, locks, trunk, cluster, doors). */
#define BMW_ACTION_COUNT 12

class BmwManager {
 public:
  BmwManager();
  void begin();
  void end();
  void tick();

  /** Enable demo mode from menu: inject fake I-Bus/status so app can be tested without real bus. */
  void setDemoMode(bool enable) {
    demoMode_ = enable;
    isDemoModeActive.store(enable);
  }
  bool isDemoMode() const { return demoMode_; }
  /** E39 variant: true = facelift (e39_fl), false = pre-facelift (e39). Set from prefs in begin(). */
  bool isE39Facelift() const { return e39Facelift_; }
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
  void sendDoorsUnlockGM();  /* Central unlock: 3F 05 00 0C 34 00 */
  void sendDoorsLockKey();
  void sendDoorsHardLock();
  void sendAllExceptDriverLock();
  void sendDriverDoorLock();
  void sendDoorsFuelTrunk();

  /** Placeholder: LCM diagnostic (e.g. blink high beams/indicators, welcome light). Payload per LCM ID 0xD0. */
  void sendLCMDiagnostic(const uint8_t *payload, uint8_t len);

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
  /** For demo/OLED: current light show step name ("Hazard","Park",...) or "" if inactive. */
  const char *getLightShowStepName() const;

  /** Demo only: last cluster text sent (for OLED display when no real cluster). Empty if none. */
  const char *getDemoClusterText() const { return lastClusterTextDemo_; }
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
  /** IKE text (Radio mode): C8 [LEN] 80 23 42 32 [20 chars pad 0x20] [XOR]. Pads string to exactly 20 characters. */
  void sendIkeRadioText(const char *text);
  /** MFL → Radio: Next track / Previous track (I-Bus 0x3B 0x01 and 0x08). */
  void sendMflNext();
  void sendMflPrev();

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

  /** Last action feedback for dashboard (e.g. "Lock sent"). Cleared after timeout. */
  void setLastActionFeedback(const char *msg);
  /** Returns non-empty if feedback is set and not expired (e.g. 3s). */
  const char *getLastActionFeedback() const;

  /** Flex: next cluster text write from app is stored as startup greeting (after BLE cmd 0x98). */
  bool isNextClusterTextGreeting() const { return nextClusterTextIsGreeting_; }
  void storeStartupGreeting(const char *text);

  void setWigWagActive(bool v) { wigWagActive_ = v; if (!v) wigWagStep_ = 0; }
  bool isWigWagActive() const { return wigWagActive_; }
  void setSensoryDark(bool v) { sensoryDark_ = v; }
  void setComfortBlink(bool v) { comfortBlinkEnabled_ = v; }
  void triggerPanic() { panicActive_ = true; wigWagActive_ = true; }
  void setMirrorFoldOnLock(bool v) { mirrorFoldOnLock_ = v; }
  void setNextClusterTextIsGreeting(bool v) { nextClusterTextIsGreeting_ = v; }

 private:
  void parseMflButton(uint8_t *packet);
  void parsePdcPacket(uint8_t *packet);
  void tickWigWag(unsigned long now);
  void tickGreetingOnIgnition(unsigned long now);
  /** Send LCM diagnostic for panel dim 0% (sensory dark). Placeholder payload until LCM dim bytes confirmed. */
  void sendSensoryDarkLcm();
  static const int kMidDisplayChars = 12;

  bool active_ = false;
  bool demoMode_ = false;
  bool e39Facelift_ = false;  /* From prefs bmw_model: e39_fl = true, e39 = false. For future I-Bus variants if needed. */
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
  /** Demo: last cluster text sent (shown on OLED when in demo mode). */
  static const int kDemoClusterTextLen = 21;
  char lastClusterTextDemo_[kDemoClusterTextLen];
  unsigned long lastShiftClusterMs_ = 0;
  static const unsigned long kShiftClusterIntervalMs = 1000;
  static const int kShiftRpmThreshold = 5500;
  IbusDriver ibus_;
  BleKeyService bleKey_;
  static const int kLastActionFeedbackLen = 32;
  static const unsigned long kLastActionFeedbackTimeoutMs = 3000;
  char lastActionFeedback_[kLastActionFeedbackLen];
  unsigned long lastActionFeedbackTime_ = 0;

  /* Flex / Instincts: BLE command state (non-blocking) */
  bool wigWagActive_ = false;
  uint8_t wigWagStep_ = 0;
  unsigned long lastWigWagMs_ = 0;
  static const unsigned long kWigWagIntervalMs = 300;
  bool sensoryDark_ = false;
  bool comfortBlinkEnabled_ = false;
  bool mirrorFoldOnLock_ = false;
  bool panicActive_ = false;
  bool nextClusterTextIsGreeting_ = false;
  static const int kStartupGreetingLen = 21;
  char startupGreeting_[kStartupGreetingLen];
  bool greetingPendingSend_ = false;
  unsigned long greetingSendAtMs_ = 0;
  int lastIgnitionForGreeting_ = -1;
};

#endif
