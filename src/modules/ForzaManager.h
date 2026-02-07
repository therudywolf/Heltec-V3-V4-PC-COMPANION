/*
 * NOCTURNE_OS — ForzaManager: UDP listener for Forza Horizon 4/5 and Motorsport
 * "Data Out" telemetry. Receives on port 5300, parses RPM, Speed, Gear.
 */
#ifndef NOCTURNE_FORZA_MANAGER_H
#define NOCTURNE_FORZA_MANAGER_H

#include <WiFiUdp.h>

#define FORZA_UDP_PORT 5300
#define FORZA_PACKET_MIN_SIZE 232
#define FORZA_PACKET_SLED_SIZE 232
#define FORZA_PACKET_DASH_SIZE 311
#define FORZA_TIMEOUT_MS 3000
#define FORZA_SHIFT_THRESHOLD 0.90f  // RPM % for shift light

// Offsets from Forza packet (little-endian)
#define FORZA_OFF_IS_RACE_ON 0
#define FORZA_OFF_ENGINE_MAX_RPM 8
#define FORZA_OFF_ENGINE_IDLE_RPM 12
#define FORZA_OFF_CURRENT_ENGINE_RPM 16
#define FORZA_OFF_VELOCITY_X 32  // Sled: Velocity in car space (m/s)
#define FORZA_OFF_VELOCITY_Y 36
#define FORZA_OFF_VELOCITY_Z 40
#define FORZA_OFF_SPEED 244
#define FORZA_OFF_TIRE_FL 256
#define FORZA_OFF_TIRE_FR 260
#define FORZA_OFF_TIRE_RL 264
#define FORZA_OFF_TIRE_RR 268
#define FORZA_OFF_FUEL 280
#define FORZA_OFF_LAP 296
#define FORZA_OFF_RACE_POS 298
#define FORZA_OFF_GEAR 307

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
  uint8_t rxBuf_[FORZA_PACKET_DASH_SIZE];
};

#endif
