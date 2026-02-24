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

extern const uint8_t Window_FrontDriver_Open[7];
extern const uint8_t Window_FrontDriver_Close[7];
extern const uint8_t Window_FrontPassenger_Open[7];
extern const uint8_t Window_FrontPassenger_Close[7];
extern const uint8_t Window_RearDriver_Open[7];
extern const uint8_t Window_RearDriver_Close[7];
extern const uint8_t Window_RearPassenger_Open[7];
extern const uint8_t Window_RearPassenger_Close[7];

extern const uint8_t Wipers_Front[7];
extern const uint8_t Washer_Front[7];

extern const uint8_t Interior_Off[7];
extern const uint8_t Interior_On3s[7];
extern const uint8_t Clown_Flash[7];

extern const uint8_t Doors_HardLock[7];
extern const uint8_t AllExceptDriver_Lock[7];
extern const uint8_t DriverDoor_Lock[7];
extern const uint8_t Doors_Fuel_Trunk[7];

/** Request IKE (cluster) status — for periodic I-Bus poll. */
extern const uint8_t IKE_Status_Request[4];
/** Request door/lid status 0x7a from GM. */
extern const uint8_t GM_Status_Request[4];
/** Request ignition status 0x11 from IKE. */
extern const uint8_t IKE_Ignition_Request[4];

#endif
