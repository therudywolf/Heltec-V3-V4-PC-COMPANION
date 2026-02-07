/*
 * NOCTURNE_OS — ForzaManager: UDP listener for Forza Data Out telemetry.
 */
#include "ForzaManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

ForzaManager::ForzaManager() {
  state_.currentRpm = 0.0f;
  state_.maxRpm = 10000.0f;
  state_.idleRpm = 0.0f;
  state_.speedMs = 0.0f;
  state_.gear = 0;
  state_.tireFL = state_.tireFR = state_.tireRL = state_.tireRR = 0.0f;
  state_.fuel = 1.0f;
  state_.lapNumber = 0;
  state_.racePosition = 0;
  state_.connected = false;
  state_.lastPacketMs = 0;
}

float ForzaManager::readFloatLE(const uint8_t *p) {
  union {
    uint8_t b[4];
    float f;
  } u;
  u.b[0] = p[0];
  u.b[1] = p[1];
  u.b[2] = p[2];
  u.b[3] = p[3];
  return u.f;
}

int32_t ForzaManager::readS32LE(const uint8_t *p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

uint16_t ForzaManager::readU16LE(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

void ForzaManager::begin() {
  if (udp_.begin(FORZA_UDP_PORT)) {
    Serial.printf("[FORZA] UDP listening on port %d\n", FORZA_UDP_PORT);
  } else {
    Serial.println("[FORZA] UDP begin failed");
  }
  state_.connected = false;
  state_.lastPacketMs = 0;
}

void ForzaManager::stop() {
  udp_.stop();
  state_.connected = false;
}

void ForzaManager::parsePacket(const uint8_t *buf, size_t len) {
  if (len < FORZA_PACKET_MIN_SIZE)
    return;

  int32_t isRaceOn = readS32LE(buf + FORZA_OFF_IS_RACE_ON);
  if (isRaceOn <= 0)
    return;

  state_.maxRpm = readFloatLE(buf + FORZA_OFF_ENGINE_MAX_RPM);
  state_.idleRpm = readFloatLE(buf + FORZA_OFF_ENGINE_IDLE_RPM);
  state_.currentRpm = readFloatLE(buf + FORZA_OFF_CURRENT_ENGINE_RPM);

  if (state_.maxRpm < 100.0f || state_.maxRpm > 25000.0f)
    state_.maxRpm = 10000.0f;

  // Speed: Dash has it at 244; Sled (232 bytes) must use Velocity
  if (len >= 248) {
    state_.speedMs = readFloatLE(buf + FORZA_OFF_SPEED);
  } else if (len >= 44) {
    float vx = readFloatLE(buf + FORZA_OFF_VELOCITY_X);
    float vy = readFloatLE(buf + FORZA_OFF_VELOCITY_Y);
    float vz = readFloatLE(buf + FORZA_OFF_VELOCITY_Z);
    state_.speedMs = sqrtf(vx * vx + vy * vy + vz * vz);
  }

  if (len >= 284) {
    state_.tireFL = readFloatLE(buf + FORZA_OFF_TIRE_FL);
    state_.tireFR = readFloatLE(buf + FORZA_OFF_TIRE_FR);
    state_.tireRL = readFloatLE(buf + FORZA_OFF_TIRE_RL);
    state_.tireRR = readFloatLE(buf + FORZA_OFF_TIRE_RR);
  }
  if (len >= 284) {
    state_.fuel = readFloatLE(buf + FORZA_OFF_FUEL);
    if (state_.fuel < 0.0f || state_.fuel > 1.0f)
      state_.fuel = 1.0f;
  }
  if (len >= 298) {
    state_.lapNumber = readU16LE(buf + FORZA_OFF_LAP);
  }
  if (len >= 299) {
    state_.racePosition = buf[FORZA_OFF_RACE_POS] & 0xFF;
  }
  if (len >= 308) {
    state_.gear = (int)(buf[FORZA_OFF_GEAR] & 0xFF);
  }
}

void ForzaManager::tick() {
  unsigned long now = millis();

  int len = udp_.parsePacket();
  if (len > 0) {
    size_t toRead = (len > (int)sizeof(rxBuf_)) ? sizeof(rxBuf_) : (size_t)len;
    int n = udp_.read(rxBuf_, toRead);
    if (n >= (int)FORZA_PACKET_MIN_SIZE) {
      parsePacket(rxBuf_, (size_t)n);
      state_.lastPacketMs = now;
      state_.connected = true;
    }
  }

  if (state_.connected && (now - state_.lastPacketMs > FORZA_TIMEOUT_MS)) {
    state_.connected = false;
  }
}

int ForzaManager::getSpeedKmh() const {
  return (int)(state_.speedMs * 3.6f + 0.5f);
}

char ForzaManager::getGearChar() const {
  if (state_.gear == 0)
    return 'R';
  if (state_.gear == 11)
    return 'N';
  if (state_.gear >= 1 && state_.gear <= 10)
    return (char)('0' + state_.gear);
  return '-';
}

float ForzaManager::getFuelPct() const {
  return (state_.fuel >= 0.0f && state_.fuel <= 1.0f) ? state_.fuel : 1.0f;
}

int ForzaManager::getRacePosition() const {
  return (int)state_.racePosition;
}

uint16_t ForzaManager::getLapNumber() const {
  return state_.lapNumber;
}
