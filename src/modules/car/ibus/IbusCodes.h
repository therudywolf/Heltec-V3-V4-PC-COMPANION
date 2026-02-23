/*
 * I-Bus message codes for E39 (locks, lights). Checksum added by driver.
 */
#ifndef IBUS_CODES_H
#define IBUS_CODES_H

#include <stdint.h>

extern const uint8_t REMOTE_UNLOCK[6];
extern const uint8_t REMOTE_LOCK[6];

extern const uint8_t GoodbyeLights[13];
extern const uint8_t FollowMeHome[13];
extern const uint8_t ParkLights_And_Signals[13];
extern const uint8_t Low_Beams[13];
extern const uint8_t TurnOffLights[17];
extern const uint8_t HazardLights[13];

extern const uint8_t Doors_Unlock_Interior[7];
extern const uint8_t Doors_Lock_Key[7];
extern const uint8_t Trunk_Open[7];

/** Request IKE (cluster) status — for periodic I-Bus poll. */
extern const uint8_t IKE_Status_Request[4];

#endif
