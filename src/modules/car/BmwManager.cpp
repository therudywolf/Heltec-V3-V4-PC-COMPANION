/*
 * NOCTURNE_OS — BmwManager: BMW E39 Assistant (I-Bus + BLE key).
 */
#include "BmwManager.h"
#include "ibus/IbusDriver.h"
#include "ibus/IbusCodes.h"
#include "ibus/IbusDefines.h"
#include "nocturne/config.h"
#include <stdio.h>
#include <string.h>

static BmwManager *s_bmwForIbus = nullptr;

static void ibusPacketForward(uint8_t *packet) {
  if (s_bmwForIbus)
    s_bmwForIbus->onIbusPacket(packet);
}

BmwManager::BmwManager() {
  nowPlayingTrack_[0] = '\0';
  nowPlayingArtist_[0] = '\0';
  for (int i = 0; i < kPdcSensors; i++)
    pdcDists_[i] = -1;
}

void BmwManager::parseMflButton(uint8_t *packet) {
  /* MFL 0x50, cmd 0x3B. Wilhelm mfl/3b.md: button+state bitfield (FORWARD 0x01, BACK 0x08, PRESS/HOLD/RELEASE, R/T 0x40, TEL 0x80). We use simplified byte mapping. */
  if (!packet || packet[0] != IBUS_MFL || packet[1] < 3)
    return;
  size_t totalLen = 2u + (size_t)packet[1];
  if (totalLen < 5 || packet[3] != IBUS_MFL_BUTTON)
    return;
  uint8_t b = packet[4];
  if (b == 0x3B || b == 0x02)   /* next / R/T */
    lastMflAction_ = MFL_NEXT;
  else if (b == 0x3A || b == 0x01)
    lastMflAction_ = MFL_PREV;
  else if (b == 0x80 || b == 0x00)
    lastMflAction_ = MFL_PLAY_PAUSE;
  else if (b == 0x11)
    lastMflAction_ = MFL_VOL_UP;
  else if (b == 0x10)
    lastMflAction_ = MFL_VOL_DOWN;
}

void BmwManager::setNowPlaying(const char *track, const char *artist) {
#if NOCT_BMW_DEBUG
  Serial.printf("[BMW] setNowPlaying: \"%s\" - \"%s\"\n", track ? track : "", artist ? artist : "");
#endif
  if (track) {
    strncpy(nowPlayingTrack_, track, kNowPlayingLen - 1);
    nowPlayingTrack_[kNowPlayingLen - 1] = '\0';
  } else
    nowPlayingTrack_[0] = '\0';
  if (artist) {
    strncpy(nowPlayingArtist_, artist, kNowPlayingLen - 1);
    nowPlayingArtist_[kNowPlayingLen - 1] = '\0';
  } else
    nowPlayingArtist_[0] = '\0';
}

void BmwManager::getPdcDistances(int *dists, int maxCount) const {
  if (!dists || maxCount <= 0)
    return;
  int n = maxCount < kPdcSensors ? maxCount : kPdcSensors;
  for (int i = 0; i < n; i++)
    dists[i] = pdcDists_[i];
}

void BmwManager::parsePdcPacket(uint8_t *packet) {
  /* PDC 0x60: distance packet format not documented in wilhelm (model/reverse-engineered). We assume bytes 4..7 = sensor distances. */
  if (!packet || packet[0] != IBUS_PDC || packet[1] < 7)
    return;
  /* PDC: need at least dest, cmd, 4 distance bytes, checksum. Indices 4..7 = data. */
  for (int i = 0; i < kPdcSensors && (4 + i) <= (int)(packet[1]); i++)
    pdcDists_[i] = packet[4 + i] <= 0xFE ? (int)packet[4 + i] : -1;
  pdcValid_ = true;
}

void BmwManager::sendClusterText(const char *text) {
  /* Cluster text: cmd 0x1A to IKE 0x80. Wilhelm lcm/1a.md describes CCM 0x30→IKE; we use DIA 0x3F as sender (diagnostic). */
#if NOCT_BMW_DEBUG
  Serial.printf("[BMW] I-Bus sendClusterText: \"%s\"\n", text ? text : "");
#endif
  if (!text || !ibus_.isSynced())
    return;
  uint8_t msg[24];
  int len = 0;
  while (text[len] && len < 20)
    len++;
  if (len == 0)
    return;
  msg[0] = IBUS_DIA;
  msg[1] = (uint8_t)(3 + len);
  msg[2] = IBUS_IKE;
  msg[3] = IBUS_IKE_TXT_GONG;
  for (int i = 0; i < len; i++)
    msg[4 + i] = (uint8_t)text[i];
  ibus_.write(msg, 4 + len);
}

void BmwManager::sendUpdateMid() {
#if NOCT_BMW_DEBUG
  Serial.println("[BMW] I-Bus sendUpdateMid (track/artist to MID)");
#endif
  if (!ibus_.isSynced())
    return;
  char line[kMidDisplayChars + 1];
  int n = 0;
  const char *a = nowPlayingArtist_;
  if (nowPlayingTrack_[0]) {
    for (; n < kMidDisplayChars && nowPlayingTrack_[n]; n++)
      line[n] = nowPlayingTrack_[n];
  }
  if (n < kMidDisplayChars && a && *a) {
    if (n > 0)
      line[n++] = ' ';
    for (; n < kMidDisplayChars && *a; a++)
      line[n++] = *a;
  }
  if (n == 0)
    return;
  line[n] = '\0';
  uint8_t msg[4 + kMidDisplayChars];
  msg[0] = IBUS_CDC;
  msg[1] = (uint8_t)(2 + n);
  msg[2] = IBUS_MID;
  msg[3] = IBUS_UPDATE_MID;
  for (int i = 0; i < n; i++)
    msg[4 + i] = (uint8_t)(line[i] & 0x7F);
  ibus_.write(msg, 4 + n);
}

void BmwManager::setObdData(bool connected, int rpm, int coolantC, int oilC) {
  obdConnected_ = connected;
  obdRpm_ = rpm >= 0 ? rpm : 0;
  obdCoolantTempC_ = coolantC;
  obdOilTempC_ = oilC;
}

void BmwManager::onIbusPacket(uint8_t *packet) {
  if (!packet || packet[1] < 3 || packet[1] > 0x24)
    return;
#if NOCT_IBUS_MONITOR_VERBOSE
  {
    size_t total = 2u + (size_t)packet[1];
    Serial.printf("[IBUS] %02X %02X", packet[0], packet[1]);
    for (size_t i = 2; i < total && i < 24; i++)
      Serial.printf(" %02X", packet[i]);
    Serial.println();
  }
#endif
  if (packet[0] == IBUS_MFL)
    parseMflButton(packet);
  else if (packet[0] == IBUS_PDC)
    parsePdcPacket(packet);
  else if (packet[0] == IBUS_IKE && packet[1] >= 5 && packet[3] == IBUS_TEMP) {
    /* IKE 0x19 temperature: byte0 = ambient °C, byte1 = coolant °C. Wilhelm ike/19.md. */
    lastIkeCoolantC_ = (int)packet[5];
  }
  else if (packet[0] == IBUS_GM && packet[1] >= 5 && packet[2] == 0xBF && packet[3] == IBUS_GM_STAT_RPLY) {
    /* GM door/lid status 0x7a: byte1 = doors/lock/lamp, byte2 = windows/sunroof/trunk. Wilhelm gm/7a.md. */
    lastDoorLidByte1_ = packet[4];
    lastDoorLidByte2_ = packet[5];
  }
  else if (packet[0] == IBUS_IKE && packet[1] >= 4 && packet[3] == IBUS_IGN_STAT_RPLY) {
    /* IKE ignition status 0x11: 0=off, 1=pos1, 2=pos2 (run), etc. */
    lastIgnition_ = (int)packet[4];
  }
  else if (packet[0] == IBUS_IKE && packet[1] >= 6 && packet[3] == IBUS_ODMTR_STAT_RPLY) {
    /* IKE odometer 0x17: 3 bytes km = b1 + b2*256 + b3*65536. Wilhelm ike/17.md. */
    lastOdometerKm_ = (int)packet[4] | ((int)packet[5] << 8) | ((int)packet[6] << 16);
  }
  /* CDC emulation: RAD requests CD status -> reply as CDC so head unit shows CD source. */
  else if (packet[0] == IBUS_RAD && packet[1] >= 3 && packet[2] == IBUS_CDC && packet[3] == IBUS_CD_CTRL_REQ) {
    uint8_t reply[] = { IBUS_CDC, 3, IBUS_RAD, IBUS_CD_STAT_RPLY, 0x01 };
    ibus_.write(reply, sizeof(reply));
  }
}

void BmwManager::onPhoneConnectionChanged(bool connected) {
  phoneConnected_ = connected;
  if (!ibus_.isSynced())
    return;
  if (connected)
    ibus_.write(REMOTE_UNLOCK, sizeof(REMOTE_UNLOCK));
  else
    ibus_.write(REMOTE_LOCK, sizeof(REMOTE_LOCK));
}

void BmwManager::sendGoodbyeLights() {
  ibus_.write(GoodbyeLights, sizeof(GoodbyeLights));
}

void BmwManager::sendFollowMeHome() {
  ibus_.write(FollowMeHome, sizeof(FollowMeHome));
}

void BmwManager::sendParkLights() {
  ibus_.write(ParkLights_And_Signals, sizeof(ParkLights_And_Signals));
}

void BmwManager::sendHazardLights() {
  ibus_.write(HazardLights, sizeof(HazardLights));
}

void BmwManager::sendLowBeams() {
  ibus_.write(Low_Beams, sizeof(Low_Beams));
}

void BmwManager::sendLightsOff() {
  ibus_.write(TurnOffLights, sizeof(TurnOffLights));
}

void BmwManager::sendLock() {
  ibus_.write(REMOTE_LOCK, sizeof(REMOTE_LOCK));
}

void BmwManager::sendUnlock() {
  ibus_.write(REMOTE_UNLOCK, sizeof(REMOTE_UNLOCK));
}

void BmwManager::sendTrunkOpen() {
  ibus_.write(Trunk_Open, sizeof(Trunk_Open));
}

void BmwManager::sendDoorsUnlockInterior() {
  ibus_.write(Doors_Unlock_Interior, sizeof(Doors_Unlock_Interior));
}

void BmwManager::sendDoorsLockKey() {
  ibus_.write(Doors_Lock_Key, sizeof(Doors_Lock_Key));
}

void BmwManager::sendDoorsHardLock() {
  ibus_.write(Doors_HardLock, sizeof(Doors_HardLock));
}
void BmwManager::sendAllExceptDriverLock() {
  ibus_.write(AllExceptDriver_Lock, sizeof(AllExceptDriver_Lock));
}
void BmwManager::sendDriverDoorLock() {
  ibus_.write(DriverDoor_Lock, sizeof(DriverDoor_Lock));
}
void BmwManager::sendDoorsFuelTrunk() {
  ibus_.write(Doors_Fuel_Trunk, sizeof(Doors_Fuel_Trunk));
}

void BmwManager::sendWindowFrontDriverOpen() {
  ibus_.write(Window_FrontDriver_Open, sizeof(Window_FrontDriver_Open));
}
void BmwManager::sendWindowFrontDriverClose() {
  ibus_.write(Window_FrontDriver_Close, sizeof(Window_FrontDriver_Close));
}
void BmwManager::sendWindowFrontPassengerOpen() {
  ibus_.write(Window_FrontPassenger_Open, sizeof(Window_FrontPassenger_Open));
}
void BmwManager::sendWindowFrontPassengerClose() {
  ibus_.write(Window_FrontPassenger_Close, sizeof(Window_FrontPassenger_Close));
}
void BmwManager::sendWindowRearDriverOpen() {
  ibus_.write(Window_RearDriver_Open, sizeof(Window_RearDriver_Open));
}
void BmwManager::sendWindowRearDriverClose() {
  ibus_.write(Window_RearDriver_Close, sizeof(Window_RearDriver_Close));
}
void BmwManager::sendWindowRearPassengerOpen() {
  ibus_.write(Window_RearPassenger_Open, sizeof(Window_RearPassenger_Open));
}
void BmwManager::sendWindowRearPassengerClose() {
  ibus_.write(Window_RearPassenger_Close, sizeof(Window_RearPassenger_Close));
}
void BmwManager::sendWipersFront() {
  ibus_.write(Wipers_Front, sizeof(Wipers_Front));
}
void BmwManager::sendWasherFront() {
  ibus_.write(Washer_Front, sizeof(Washer_Front));
}
void BmwManager::sendInteriorOff() {
  ibus_.write(Interior_Off, sizeof(Interior_Off));
}
void BmwManager::sendInteriorOn3s() {
  ibus_.write(Interior_On3s, sizeof(Interior_On3s));
}
void BmwManager::sendClownFlash() {
  ibus_.write(Clown_Flash, sizeof(Clown_Flash));
}

void BmwManager::startLightShow() {
  lightShowActive_ = true;
  lightShowStep_ = 0;
  lastLightShowMs_ = millis();
}

void BmwManager::stopLightShow() {
  lightShowActive_ = false;
  ibus_.write(TurnOffLights, sizeof(TurnOffLights));
}

void BmwManager::begin() {
  active_ = true;
  ibusSynced_ = false;
  s_bmwForIbus = this;
#if NOCT_BMW_DEBUG
  Serial.println("[BMW] begin: I-Bus + BLE key");
#endif
#if NOCT_IBUS_ENABLED
  ibus_.setPacketHandler(ibusPacketForward);
  ibus_.begin(NOCT_IBUS_TX_PIN, NOCT_IBUS_RX_PIN);
#endif
  bleKey_.setConnectionCallback([](bool connected) {
    if (s_bmwForIbus)
      s_bmwForIbus->onPhoneConnectionChanged(connected);
  });
  bleKey_.setLightCommandCallback([](uint8_t cmd) {
    if (!s_bmwForIbus || !s_bmwForIbus->isIbusSynced())
      return;
    switch (cmd) {
      case 0: s_bmwForIbus->sendGoodbyeLights(); break;
      case 1: s_bmwForIbus->sendFollowMeHome(); break;
      case 2: s_bmwForIbus->sendParkLights(); break;
      case 3: s_bmwForIbus->sendHazardLights(); break;
      case 4: s_bmwForIbus->sendLowBeams(); break;
      case 5: s_bmwForIbus->sendLightsOff(); break;
      case 6: s_bmwForIbus->sendUnlock(); break;
      case 7: s_bmwForIbus->sendLock(); break;
      case 8: s_bmwForIbus->sendTrunkOpen(); break;
      case 9: s_bmwForIbus->sendClusterText("NOCT"); break;
      case 10: s_bmwForIbus->sendDoorsUnlockInterior(); break;
      case 11: s_bmwForIbus->sendDoorsLockKey(); break;
      case 12: s_bmwForIbus->sendWindowFrontDriverOpen(); break;
      case 13: s_bmwForIbus->sendWindowFrontDriverClose(); break;
      case 14: s_bmwForIbus->sendWindowFrontPassengerOpen(); break;
      case 15: s_bmwForIbus->sendWindowFrontPassengerClose(); break;
      case 16: s_bmwForIbus->sendWindowRearDriverOpen(); break;
      case 17: s_bmwForIbus->sendWindowRearDriverClose(); break;
      case 18: s_bmwForIbus->sendWindowRearPassengerOpen(); break;
      case 19: s_bmwForIbus->sendWindowRearPassengerClose(); break;
      case 20: s_bmwForIbus->sendWipersFront(); break;
      case 21: s_bmwForIbus->sendWasherFront(); break;
      case 22: s_bmwForIbus->sendInteriorOff(); break;
      case 23: s_bmwForIbus->sendInteriorOn3s(); break;
      case 24: s_bmwForIbus->sendClownFlash(); break;
      case 25: s_bmwForIbus->sendDoorsHardLock(); break;
      case 26: s_bmwForIbus->sendAllExceptDriverLock(); break;
      case 27: s_bmwForIbus->sendDriverDoorLock(); break;
      case 28: s_bmwForIbus->sendDoorsFuelTrunk(); break;
      case 0x80: s_bmwForIbus->startLightShow(); break;
      case 0x81: s_bmwForIbus->stopLightShow(); break;
      default: break;
    }
  });
  bleKey_.setNowPlayingCallback([](const char *track, const char *artist) {
    if (s_bmwForIbus) {
      s_bmwForIbus->setNowPlaying(track, artist);
      s_bmwForIbus->sendUpdateMid();
      /* Also send Now Playing to cluster (IKE) up to 20 chars — for AUX, track name is visible on cluster. */
      char clusterLine[21];
      int n = 0;
      if (track) {
        for (; n < 20 && track[n]; n++)
          clusterLine[n] = track[n];
      }
      if (n < 20 && artist && *artist) {
        if (n > 0) {
          clusterLine[n++] = ' ';
          clusterLine[n++] = '-';
          clusterLine[n++] = ' ';
        }
        for (; n < 20 && *artist; artist++)
          clusterLine[n++] = *artist;
      }
      clusterLine[n] = '\0';
      if (n > 0)
        s_bmwForIbus->sendClusterText(clusterLine);
    }
  });
  bleKey_.setClusterTextCallback([](const char *text) {
    if (s_bmwForIbus)
      s_bmwForIbus->sendClusterText(text);
  });
  bleKey_.begin();
#if NOCT_A2DP_SINK_ENABLED
  a2dpSink_.begin();
#endif
}

void BmwManager::end() {
#if NOCT_A2DP_SINK_ENABLED
  a2dpSink_.end();
#endif
  bleKey_.end();
  ibus_.end();
  s_bmwForIbus = nullptr;
  active_ = false;
  ibusSynced_ = false;
}

void BmwManager::tick() {
  if (!active_)
    return;
  ibus_.tick();
  bleKey_.tick();
  if (demoMode_)
    ibusSynced_ = true;
  else
    ibusSynced_ = ibus_.isSynced();
  const bool wasConnected = phoneConnected_;
  if (bleKey_.isConnected() != phoneConnected_)
    phoneConnected_ = bleKey_.isConnected();
  unsigned long now = millis();
  if (demoMode_)
  {
    /* Inject fake status data so app shows live-looking values (interval from config). */
    static unsigned long lastDemoMs = 0;
    static uint8_t demoMflCycle = 0;
    if (now - lastDemoMs >= (unsigned long)NOCT_BMW_DEMO_INTERVAL_MS) {
      lastDemoMs = now;
      lastIkeCoolantC_ = 85;
      obdCoolantTempC_ = 85;
      obdOilTempC_ = 90;
      obdRpm_ = 2000;
      obdConnected_ = true;
      pdcDists_[0] = 50;
      pdcDists_[1] = 45;
      pdcDists_[2] = 30;
      pdcDists_[3] = 25;
      pdcValid_ = true;
      demoMflCycle = (demoMflCycle + 1) % 6;
      lastMflAction_ = (MflAction)demoMflCycle;  /* 0=None, 1=Next, 2=Prev, 3=Play, 4=Vol+, 5=Vol- */
      lastDoorLidByte1_ = 0x10;
      lastDoorLidByte2_ = 0x00;
      lastIgnition_ = 2;
      lastOdometerKm_ = 123456;
#if NOCT_BMW_DEBUG
      const char *mflStr = "None";
      if (lastMflAction_ == MFL_NEXT) mflStr = "Next";
      else if (lastMflAction_ == MFL_PREV) mflStr = "Prev";
      else if (lastMflAction_ == MFL_PLAY_PAUSE) mflStr = "Play";
      else if (lastMflAction_ == MFL_VOL_UP) mflStr = "Vol+";
      else if (lastMflAction_ == MFL_VOL_DOWN) mflStr = "Vol-";
      Serial.printf("[BMW DEMO] status: coolant=85 oil=90 rpm=2000 PDC 50/45/30/25 MFL=%s doors=0x10 ign=2 odom=123456\n", mflStr);
#endif
    }
  }
#if NOCT_BMW_DEBUG
  static bool lastLoggedConnected = false;
  if (phoneConnected_ != lastLoggedConnected) {
    lastLoggedConnected = phoneConnected_;
    Serial.printf("[BMW] BLE phoneConnected=%d ibusSynced=%d\n", phoneConnected_ ? 1 : 0, ibusSynced_ ? 1 : 0);
  }
#endif
  /* Welcome message on cluster when BLE connects (once per connection). */
  if (!wasConnected && phoneConnected_ && ibusSynced_ && !welcomeSentOnConnect_) {
    sendClusterText("BMW Nocturne");
    welcomeSentOnConnect_ = true;
  }
  if (!phoneConnected_)
    welcomeSentOnConnect_ = false;
  /* Periodic I-Bus poll: rotate IKE ping, GM door/lid, IKE ignition. */
  if (ibusSynced_ && now - lastPollMs_ >= (unsigned long)NOCT_IBUS_POLL_INTERVAL_MS) {
    switch (pollAlternate_ % 3) {
      case 0: ibus_.write(IKE_Ping, sizeof(IKE_Ping)); break;
      case 1: ibus_.write(GM_Status_Request, sizeof(GM_Status_Request)); break;
      case 2: ibus_.write(IKE_Ignition_Request, sizeof(IKE_Ignition_Request)); break;
      default: break;
    }
    pollAlternate_++;
    lastPollMs_ = now;
  }
  /* Light show: configurable sequence (Hazard -> Park -> Goodbye -> LowBeam -> Off). */
  static const uint8_t kLightShowSequence[] = { 0, 1, 2, 3, 4 };
  static const unsigned int kLightShowDelayMs = 800;
  const size_t kLightShowSteps = sizeof(kLightShowSequence) / sizeof(kLightShowSequence[0]);
  if (lightShowActive_ && ibusSynced_ && now - lastLightShowMs_ >= kLightShowDelayMs) {
    lastLightShowMs_ = now;
    size_t idx = (size_t)(lightShowStep_ % (int)kLightShowSteps);
    switch (kLightShowSequence[idx]) {
      case 0: ibus_.write(HazardLights, sizeof(HazardLights)); break;
      case 1: ibus_.write(ParkLights_And_Signals, sizeof(ParkLights_And_Signals)); break;
      case 2: ibus_.write(GoodbyeLights, sizeof(GoodbyeLights)); break;
      case 3: ibus_.write(Low_Beams, sizeof(Low_Beams)); break;
      case 4: ibus_.write(TurnOffLights, sizeof(TurnOffLights)); break;
      default: break;
    }
    lightShowStep_++;
  }
  /* Shift indicator on cluster: when OBD connected and RPM >= threshold, send "SHIFT!" to IKE periodically. */
  if (obdConnected_ && obdRpm_ >= kShiftRpmThreshold && ibusSynced_ && now - lastShiftClusterMs_ >= kShiftClusterIntervalMs) {
    sendClusterText("SHIFT!");
    lastShiftClusterMs_ = now;
  }
  /* BLE status characteristic: flags, coolant, oil, rpm, PDC. */
  int coolantC = obdConnected_ ? obdCoolantTempC_ : lastIkeCoolantC_;
  if (coolantC < -40 || coolantC > 127)
    coolantC = -1;
  int oilC = obdOilTempC_;
  if (oilC < -40 || oilC > 127)
    oilC = -1;
  int rpm = (obdRpm_ >= 0 && obdRpm_ <= 65535) ? obdRpm_ : -1;
  /* Lock state from 0x7a byte1: 0x10=unlocked, 0x20=locked, 0x30=double. */
  uint8_t lockState = 0xFF;
  if (lastDoorLidByte1_ != 0xFF) {
    uint8_t cl = (lastDoorLidByte1_ >> 4) & 0x30;
    if (cl == 0x10) lockState = 0;
    else if (cl == 0x20) lockState = 1;
    else if (cl == 0x30) lockState = 2;
  }
  int odom = lastOdometerKm_;
  if (odom < 0 || odom > 65535)
    odom = -1;
  bleKey_.updateStatus(ibusSynced_, phoneConnected_, pdcValid_, obdConnected_,
                      coolantC, oilC, rpm, pdcDists_, (uint8_t)lastMflAction_,
                      lastDoorLidByte1_, lastDoorLidByte2_, lockState,
                      lastIgnition_ >= 0 ? lastIgnition_ : -1, odom);
}

void BmwManager::getStatusLine(char *buf, size_t len) const {
  if (!buf || len == 0)
    return;
  if (!active_) {
    snprintf(buf, len, "BMW OFF");
    return;
  }
  if (obdConnected_ && len >= 32) {
    snprintf(buf, len, "IBUS %s | BLE %s | RPM %d",
            ibusSynced_ ? "OK" : "--",
            phoneConnected_ ? "ON" : "OFF",
            obdRpm_);
    return;
  }
  snprintf(buf, len, "IBUS %s | BLE %s",
          ibusSynced_ ? "OK" : "--",
          phoneConnected_ ? "ON" : "OFF");
}
