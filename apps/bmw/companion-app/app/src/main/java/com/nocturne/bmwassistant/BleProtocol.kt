package com.nocturne.bmwassistant

import java.util.UUID

/**
 * Single source of truth for the BLE protocol shared with the Nocturne firmware
 * (src/modules/car/BleKeyService.{h,cpp}). Keep these in lockstep with the
 * device — UUIDs, control command bytes, and the status-packet byte layout.
 */
object BleProtocol {
    const val DEVICE_NAME = "BMW E39 Key"

    // GATT service + characteristics (must equal firmware createService/createCharacteristic).
    val SERVICE_UUID: UUID = UUID.fromString("1a2b0001-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    val CONTROL_CHAR_UUID: UUID = UUID.fromString("1a2b0002-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    val STATUS_CHAR_UUID: UUID = UUID.fromString("1a2b0003-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    val NOW_PLAYING_CHAR_UUID: UUID = UUID.fromString("1a2b0004-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    val CLUSTER_TEXT_CHAR_UUID: UUID = UUID.fromString("1a2b0005-5e6f-4a5b-8c9d-0e1f2a3b4c5d")

    // Status packet: firmware kStatusPacketLen = 16. Core 10 bytes always present;
    // bytes 10..15 (doors/lock/ignition/odometer) present when size >= 16.
    const val STATUS_PACKET_MIN = 10
    const val STATUS_PACKET_FULL = 16

    // Control command bytes the firmware's onLightCommandReceived maps (0..11 here;
    // firmware also handles 0x80/0x81 light-show + 0x90.. flex commands).
    object Cmd {
        const val GOODBYE_LIGHTS = 0
        const val FOLLOW_ME_HOME = 1
        const val PARK_LIGHTS = 2
        const val HAZARD = 3
        const val LOW_BEAMS = 4
        const val LIGHTS_OFF = 5
        const val UNLOCK = 6
        const val LOCK = 7
        const val TRUNK = 8
        const val CLUSTER_TEXT = 9
        const val DOORS_UNLOCK_INTERIOR = 10
        const val DOORS_LOCK_KEY = 11

        // Light show + flex (firmware switch cases 0x80+).
        const val LIGHT_SHOW_START = 0x80
        const val LIGHT_SHOW_STOP = 0x81
    }

    // Cluster text payloads are clamped to 20 bytes on the device.
    const val CLUSTER_TEXT_MAX = 20
}
