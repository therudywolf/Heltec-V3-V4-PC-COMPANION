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
  /* packet: [0]=src, [1]=len (bytes after len), [2]=dest, [3]=cmd, [4..]=data */
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
  if (!packet || packet[0] != IBUS_PDC || packet[1] < 7)
    return;
  /* PDC: need at least dest, cmd, 4 distance bytes, checksum. Indices 4..7 = data. */
  for (int i = 0; i < kPdcSensors && (4 + i) <= (int)(packet[1]); i++)
    pdcDists_[i] = packet[4 + i] <= 0xFE ? (int)packet[4 + i] : -1;
  pdcValid_ = true;
}

void BmwManager::sendClusterText(const char *text) {
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
  else if (packet[0] == IBUS_IKE && packet[1] >= 4 && packet[3] == IBUS_TEMP) {
    /* IKE temperature broadcast: optional extraction for bus-sourced coolant. */
    if (packet[1] >= 5)
      lastIkeCoolantC_ = (int)(packet[4]) - 40;
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
      case 0x80: s_bmwForIbus->startLightShow(); break;
      case 0x81: s_bmwForIbus->stopLightShow(); break;
      default: break;
    }
  });
  bleKey_.setNowPlayingCallback([](const char *track, const char *artist) {
    if (s_bmwForIbus) {
      s_bmwForIbus->setNowPlaying(track, artist);
      s_bmwForIbus->sendUpdateMid();
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
  ibusSynced_ = ibus_.isSynced();
  if (bleKey_.isConnected() != phoneConnected_)
    phoneConnected_ = bleKey_.isConnected();
  /* Periodic I-Bus poll: request IKE status every 3 s to keep bus active. */
  unsigned long now = millis();
  if (ibusSynced_ && now - lastPollMs_ >= 3000) {
    ibus_.write(IKE_Status_Request, sizeof(IKE_Status_Request));
    lastPollMs_ = now;
  }
  /* Cyclic light show: Hazard -> Park -> Goodbye -> LowBeam -> LightsOff -> repeat. */
  if (lightShowActive_ && ibusSynced_ && now - lastLightShowMs_ >= kLightShowIntervalMs) {
    lastLightShowMs_ = now;
    switch (lightShowStep_ % 5) {
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
  bleKey_.updateStatus(ibusSynced_, phoneConnected_, pdcValid_, obdConnected_,
                      coolantC, oilC, rpm, pdcDists_, (uint8_t)lastMflAction_);
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
