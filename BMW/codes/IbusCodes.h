/*
 * I-Bus message codes for E39/E46 (lights, locks, MFL, etc.).
 * From I-K_Bus E46_Codes.h; no PROGMEM (ESP32).
 * Checksum is added by IbusSerial::write().
 */
#ifndef IBUS_CODES_H
#define IBUS_CODES_H

#include <stdint.h>

// Remote / locks (GM/FBZV)
extern const uint8_t REMOTE_UNLOCK[6];
extern const uint8_t REMOTE_LOCK[6];

// Lights (LCM / vehicle control)
extern const uint8_t GoodbyeLights[13];
extern const uint8_t FollowMeHome[13];
extern const uint8_t ParkLights_And_Signals[13];
extern const uint8_t Low_Beams[13];
extern const uint8_t TurnOffLights[17];
extern const uint8_t HazardLights[13];

// Doors (GM5)
extern const uint8_t Doors_Unlock_Interior[7];
extern const uint8_t Doors_Lock_Key[7];
extern const uint8_t Trunk_Open[7];

#endif
