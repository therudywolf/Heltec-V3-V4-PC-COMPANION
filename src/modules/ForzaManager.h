/*
 * NOCTURNE_OS — ForzaManager: UDP listener for Forza Horizon 4/5 and Motorsport
 * "Data Out" telemetry. Receives on port 5300, parses RPM, Speed, Gear.
 */
#ifndef NOCTURNE_FORZA_MANAGER_H
#define NOCTURNE_FORZA_MANAGER_H

#include <WiFiUdp.h>

#define FORZA_UDP_PORT 5300
#define FORZA_PACKET_MIN_SIZE 311   /* Dash: FM7 311, FM8 331, FH4 323 */
#define FORZA_PACKET_DASH_FM 311
#define FORZA_PACKET_DASH_FH 323
#define FORZA_TIMEOUT_MS 3000
#define FORZA_SHIFT_THRESHOLD 0.90f  // RPM % for shift light

// --- Forza Dashboard: FIXED layout with NO collisions ---
#define FORZA_RPM_JITTER_RANGE 1
#define FORZA_RED_ZONE_FLICKER_MS 80
#define FORZA_GEAR_CHANGE_VIBRATE_RANGE 1
#define FORZA_SPEED_BRAKE_DROP_PX 1
#define FORZA_EMA_RPM 0.35f
#define FORZA_EMA_SPEED 0.28f
#define FORZA_GEAR_CHANGE_MS 200
#define FORZA_RED_ZONE_RPM_PCT 0.85f

// Layout (128x64) — F1 style: thin RPM bar | Gear left (huge), Speed right | Shift bottom
#define RPM_BAR_HEIGHT 10
#define FORZA_CONTENT_TOP (RPM_BAR_HEIGHT + 4)
#define FORZA_CONTENT_BOTTOM 64
// Zones: Gear (0-55), Speed (60-128). No boxes to avoid collisions.
#define FORZA_GEAR_ZONE_W 55
#define FORZA_SPEED_ZONE_X 60
#define FORZA_SPEED_ANCHOR_X 126
#define FORZA_SEPARATOR_X 57
#define FORZA_SHIFT_Y 58

// Fonts (F1-style: massive gear, big speed)
#define FORZA_RPM_FONT u8g2_font_profont10_tf
#define FORZA_GEAR_FONT u8g2_font_logisoso32_tr
#define FORZA_SPEED_FONT u8g2_font_logisoso28_tn
#define FORZA_KMH_FONT u8g2_font_profont12_tf
#define FORZA_SHIFT_FONT u8g2_font_logisoso16_tr

// Offsets from Forza packet (little-endian)
#define FORZA_OFF_IS_RACE_ON 0
#define FORZA_OFF_ENGINE_MAX_RPM 8
#define FORZA_OFF_ENGINE_IDLE_RPM 12
#define FORZA_OFF_CURRENT_ENGINE_RPM 16
// FM7/FM8 Dash (311 bytes)
#define FORZA_OFF_SPEED 244
#define FORZA_OFF_TIRE_FL 256
#define FORZA_OFF_FUEL 280
#define FORZA_OFF_LAP 296
#define FORZA_OFF_RACE_POS 298
#define FORZA_OFF_GEAR 307
// FH4/FH5 Dash (323 bytes): +12 after NumCylinders
#define FORZA_OFF_SPEED_FH 256
#define FORZA_OFF_TIRE_FL_FH 268
#define FORZA_OFF_FUEL_FH 292
#define FORZA_OFF_LAP_FH 308
#define FORZA_OFF_RACE_POS_FH 310
#define FORZA_OFF_GEAR_FH 319

struct ForzaState {
  float currentRpm;
  float maxRpm;
  float idleRpm;
  float speedMs;
  int gear;
  float tireFL, tireFR, tireRL, tireRR;
  float fuel;
  uint16_t lapNumber;
  uint8_t racePosition;
  bool connected;
  unsigned long lastPacketMs;
};

class ForzaManager {
public:
  ForzaManager();
  void begin();
  void stop();
  void tick();
  bool isConnected() const { return state_.connected; }
  const ForzaState &getState() const { return state_; }
  int getSpeedKmh() const;
  float getCurrentRpm() const { return state_.currentRpm; }
  float getMaxRpm() const { return state_.maxRpm; }
  void getGearString(char *buf, size_t bufSize) const;
  char getGearChar() const;
  float getFuelPct() const;
  int getRacePosition() const;
  uint16_t getLapNumber() const;

private:
  void parsePacket(const uint8_t *buf, size_t len);
  float readFloatLE(const uint8_t *p);
  int32_t readS32LE(const uint8_t *p);
  uint16_t readU16LE(const uint8_t *p);

  WiFiUDP udp_;
  ForzaState state_;
  uint8_t rxBuf_[400];
};

#endif
