/*
 * I-Bus message codes for E39 (locks, lights, requests).
 *
 * Each array is a complete I-Bus message WITHOUT the trailing XOR checksum —
 * IbusSerial::write() appends the checksum. On the wire a frame is:
 *     [SRC] [LEN] [DST] [ ...data... ] [CHK]
 * where LEN counts every byte from DST through CHK inclusive. Since the array
 * omits CHK, the invariant is:  array[1] (LEN) == sizeof(array) - 1.
 */
#ifndef IBUS_CODES_H
#define IBUS_CODES_H

#include <stdint.h>

extern const uint8_t REMOTE_UNLOCK[5];
extern const uint8_t REMOTE_LOCK[5];

extern const uint8_t GoodbyeLights[12];
extern const uint8_t FollowMeHome[12];
extern const uint8_t ParkLights_And_Signals[12];
extern const uint8_t Low_Beams[12];
extern const uint8_t TurnOffLights[16];
extern const uint8_t HazardLights[12];

extern const uint8_t Doors_Unlock_Interior[6];
extern const uint8_t Doors_Unlock_GM[5];  /* Central unlock GM3: 3F 04 00 0C 34 */
extern const uint8_t Doors_Lock_Key[6];
extern const uint8_t Trunk_Open[6];

extern const uint8_t Window_FrontDriver_Open[6];
extern const uint8_t Window_FrontDriver_Close[6];
extern const uint8_t Window_FrontPassenger_Open[6];
extern const uint8_t Window_FrontPassenger_Close[6];
extern const uint8_t Window_RearDriver_Open[6];
extern const uint8_t Window_RearDriver_Close[6];
extern const uint8_t Window_RearPassenger_Open[6];
extern const uint8_t Window_RearPassenger_Close[6];

extern const uint8_t Wipers_Front[6];
extern const uint8_t Washer_Front[6];

extern const uint8_t Interior_Off[6];
extern const uint8_t Interior_On3s[6];
extern const uint8_t Clown_Flash[6];

extern const uint8_t Doors_HardLock[6];
extern const uint8_t AllExceptDriver_Lock[6];
extern const uint8_t DriverDoor_Lock[6];
extern const uint8_t Doors_Fuel_Trunk[6];

/** IKE Ping (keep-alive) for periodic I-Bus poll. Wilhelm 02.md. */
extern const uint8_t IKE_Ping[4];
/** MFL -> Radio: Next track 0x3B 0x01, Prev 0x3B 0x08. Wilhelm mfl/3b. */
extern const uint8_t MflNext[5];
extern const uint8_t MflPrev[5];
/** Request door/lid status 0x7a from GM. Wilhelm gm/79.md; we use DIA as sender. */
extern const uint8_t GM_Status_Request[4];
/** Request ignition status 0x11 from IKE. Wilhelm ike/10.md. */
extern const uint8_t IKE_Ignition_Request[4];
/** Request odometer 0x17 from IKE. Wilhelm ike/16.md. */
extern const uint8_t IKE_Odometer_Request[4];

#endif
