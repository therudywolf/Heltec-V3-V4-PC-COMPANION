/*
 * NOCTURNE_OS — DisplayManager: Terminal-style 128x64 OLED. U8g2, strict monospace, no overlap.
 */
#include "DisplayManager.h"
#include "DisplayEngine.h"
#include "modules/car/BmwManager.h"
#include "nocturne/config.h"
#include <U8g2lib.h>
#include <stdio.h>
#include <string.h>

/* Strict small monospace for all text (avoids overlap). Cyber-terminal aesthetic. */
#define DISPLAY_FONT u8g2_font_5x7_tr

/* Terminal zones: 128x64. Line height ~8px. */
#define ROW1_BASELINE  8   /* BLE status | Uptime */
#define ROW2_BASELINE 18   /* RX / TX / ERR */
#define ROW3_BASELINE 28   /* Last event line 1 */
#define ROW4_BASELINE 38   /* Last event line 2 */
#define DATA_MAX_CHARS 20  /* Max chars per line to avoid overflow */

DisplayManager::DisplayManager(DisplayEngine &display, BmwManager &bmw)
    : display_(display), bmw_(bmw) {}

void DisplayManager::requestRefresh() {
  lastRefreshMs_ = 0;
}

bool DisplayManager::update(unsigned long nowMs) {
  if (nowMs - lastRefreshMs_ < kMinRefreshIntervalMs)
    return false;
  lastRefreshMs_ = nowMs;
  drawFrame(nowMs);
  return true;
}

void DisplayManager::drawFrame(unsigned long nowMs) {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2 = display_.u8g2();

  display_.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(0);
  u8g2.setBitmapMode(0);
  u8g2.setFont(DISPLAY_FONT);

  /* --- Row 1: BLE status (left) | Uptime (right). Cyber-terminal layout. --- */
  const char *bleStr = bmw_.isPhoneConnected() ? "BLE: CONN" : "BLE: DISCONN";
  u8g2.drawUTF8(0, ROW1_BASELINE, bleStr);

  unsigned long uptimeSec = nowMs / 1000;
  char uptimeBuf[12];
  if (uptimeSec >= 3600)
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lu:%02lu", uptimeSec / 3600, (uptimeSec % 3600) / 60);
  else if (uptimeSec >= 60)
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lu:%02lu", uptimeSec / 60, uptimeSec % 60);
  else
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lus", (unsigned long)uptimeSec);
  int uptimeW = u8g2.getUTF8Width(uptimeBuf);
  u8g2.drawUTF8(NOCT_DISP_W - uptimeW, ROW1_BASELINE, uptimeBuf);

  /* --- Row 2: I-Bus stats (RX | TX | ERR). Format: RX: 000 | TX: 000 | ERR: 00 --- */
  uint32_t rx = 0, tx = 0, err = 0;
  IbusDriver *ibus = bmw_.ibus();
  if (ibus) {
    rx = ibus->getRxCount();
    tx = ibus->getTxCount();
    err = ibus->getErrorCount() + ibus->getCollisionCount();
  }
  char statsBuf[32];
  snprintf(statsBuf, sizeof(statsBuf), "RX:%lu | TX:%lu | ERR:%lu", (unsigned long)rx, (unsigned long)tx, (unsigned long)err);
  u8g2.drawUTF8(0, ROW2_BASELINE, statsBuf);

  /* --- Row 3 & 4: Last significant I-Bus event (feedback, RPM, TEMP, lock cmd, etc.) --- */
  char line1[DATA_MAX_CHARS + 1];
  char line2[DATA_MAX_CHARS + 1];
  line1[0] = '\0';
  line2[0] = '\0';

  const char *feedback = bmw_.getLastActionFeedback();
  if (feedback && feedback[0]) {
    strncpy(line1, feedback, DATA_MAX_CHARS);
    line1[DATA_MAX_CHARS] = '\0';
    /* Optional second line: OBD/IKE data when available */
    if (bmw_.isObdConnected()) {
      int rpm = bmw_.getObdRpm();
      int cool = bmw_.getObdCoolantTempC();
      int oil = bmw_.getObdOilTempC();
      if (rpm > 0 || cool >= 0 || oil >= 0)
        snprintf(line2, sizeof(line2), "RPM:%d T:%d", rpm, cool >= 0 ? cool : oil);
    } else if (bmw_.getIkeCoolantC() > -128) {
      snprintf(line2, sizeof(line2), "TEMP:%dC", bmw_.getIkeCoolantC());
    }
  } else {
    if (!bmw_.isIbusSynced()) {
      strncpy(line1, "I-Bus: connect", sizeof(line1) - 1);
      line1[sizeof(line1) - 1] = '\0';
    } else if (bmw_.isObdConnected()) {
      int rpm = bmw_.getObdRpm();
      int cool = bmw_.getObdCoolantTempC();
      int oil = bmw_.getObdOilTempC();
      snprintf(line1, sizeof(line1), "RPM:%d", rpm);
      if (cool >= 0 || oil >= 0)
        snprintf(line2, sizeof(line2), "TEMP:%dC", cool >= 0 ? cool : oil);
    } else if (bmw_.getIkeCoolantC() > -128) {
      snprintf(line1, sizeof(line1), "TEMP:%dC", bmw_.getIkeCoolantC());
    } else if (bmw_.getIgnitionState() >= 0) {
      snprintf(line1, sizeof(line1), "IGN:%d", bmw_.getIgnitionState());
    } else if (bmw_.hasPdcData()) {
      int d[4];
      bmw_.getPdcDistances(d, 4);
      snprintf(line1, sizeof(line1), "PDC %d %d %d %d", d[0], d[1], d[2], d[3]);
    } else {
      const char *t = bmw_.getNowPlayingTrack();
      if (t && t[0]) {
        strncpy(line1, t, DATA_MAX_CHARS);
        line1[DATA_MAX_CHARS] = '\0';
        const char *a = bmw_.getNowPlayingArtist();
        if (a && a[0]) {
          strncpy(line2, a, DATA_MAX_CHARS);
          line2[DATA_MAX_CHARS] = '\0';
        }
      } else {
        strncpy(line1, "--", sizeof(line1) - 1);
        line1[sizeof(line1) - 1] = '\0';
      }
    }
  }

  if (line1[0])
    u8g2.drawUTF8(0, ROW3_BASELINE, line1);
  if (line2[0])
    u8g2.drawUTF8(0, ROW4_BASELINE, line2);

  display_.sendBuffer();
}
