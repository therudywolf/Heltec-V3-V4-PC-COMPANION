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
#define FORZA_OFF_SPEED 244     // Dash only: Speed m/s
#define FORZA_OFF_GEAR 307      // Dash only: u8 0=R, 1-10=gears, 11=N

struct ForzaState {
  float currentRpm;
  float maxRpm;
  float idleRpm;
  float speedMs;   // m/s
  int gear;        // 0=R, 1-10=gear, 11=N
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
  int getSpeedKmh() const;   // Speed in km/h
  char getGearChar() const;  // 'R','N','1'-'9'

private:
  void parsePacket(const uint8_t *buf, size_t len);
  float readFloatLE(const uint8_t *p);
  int32_t readS32LE(const uint8_t *p);

  WiFiUDP udp_;
  ForzaState state_;
  uint8_t rxBuf_[FORZA_PACKET_DASH_SIZE];
};

#endif
