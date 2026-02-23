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
  /* packet: [0]=src, [1]=len, [2]=dest, [3]=cmd, [4..]=data, last=checksum */
  if (packet[0] != IBUS_MFL || packet[1] < 4)
    return;
  if (packet[3] != IBUS_MFL_BUTTON)
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
  if (packet[0] != IBUS_PDC || packet[1] < 5)
    return;
  /* PDC distance bytes layout varies by car; typical: 4 bytes for 4 sensors. */
  for (int i = 0; i < kPdcSensors && (int)(2 + 2 + i) < packet[1]; i++)
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

void BmwManager::setObdData(bool connected, int rpm, int coolantC, int oilC) {
  obdConnected_ = connected;
  obdRpm_ = rpm >= 0 ? rpm : 0;
  obdCoolantTempC_ = coolantC;
  obdOilTempC_ = oilC;
}

void BmwManager::onIbusPacket(uint8_t *packet) {
  if (packet[0] == IBUS_MFL)
    parseMflButton(packet);
  else if (packet[0] == IBUS_PDC)
    parsePdcPacket(packet);
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

void BmwManager::begin() {
  active_ = true;
  ibusSynced_ = false;
  s_bmwForIbus = this;
  ibus_.setPacketHandler(ibusPacketForward);
  ibus_.begin(NOCT_IBUS_TX_PIN, NOCT_IBUS_RX_PIN);
  bleKey_.setConnectionCallback([](bool connected) {
    if (s_bmwForIbus)
      s_bmwForIbus->onPhoneConnectionChanged(connected);
  });  /* no capture: uses global s_bmwForIbus */
  bleKey_.begin();
}

void BmwManager::end() {
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
}

void BmwManager::getStatusLine(char *buf, size_t len) const {
  if (!buf || len == 0)
    return;
  if (!active_) {
    snprintf(buf, len, "BMW OFF");
    return;
  }
  snprintf(buf, len, "IBUS %s | BLE %s",
          ibusSynced_ ? "OK" : "--",
          phoneConnected_ ? "ON" : "OFF");
}
