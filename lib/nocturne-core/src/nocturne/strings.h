/*
 * NOCTURNE_OS — Centralized user-facing UI strings.
 *
 * Menu text and label tables live here, in one place. snprintf format strings
 * stay at their call sites — they are code, not translatable content.
 */
#ifndef NOCTURNE_STRINGS_H
#define NOCTURNE_STRINGS_H

#include "nocturne/config.h"

/* ── Menu category headers ─────────────────────────────────────────────── */
#define STR_CAT_MONITOR "Monitoring"
#define STR_CAT_HACKER  "Hacker"
#define STR_CAT_BMW     "BMW"
#define STR_CAT_CONFIG  "Settings"
#define STR_CAT_SYSTEM  "System"

/* ── Submenu items ─────────────────────────────────────────────────────── */
#define STR_MENU_PC_MONITOR  "PC Monitor"
#define STR_MENU_FORZA       "Forza"
#define STR_MENU_WIFI        "WiFi"
#define STR_MENU_BLE         "BLE"
#define STR_MENU_LORA        "LoRa"
#define STR_MENU_BMW_ASSIST  "BMW Assistant"
#define STR_MENU_BMW_DEMO    "BMW Demo"
#define STR_MENU_DEMO        "Demo"
#define STR_MENU_REBOOT      "REBOOT"
#define STR_MENU_CHARGE_ONLY "Charge only"
#define STR_MENU_POWER_OFF   "Power off"
#define STR_MENU_VERSION     "Version"

/* ── BMW Assistant action names (indexed by action order) ──────────────── */
static const char* const kBmwActionNames[] = {
    "Goodbye", "FollowMe", "Park", "Hazard", "LowBeam",
    "LightsOff", "Unlock", "Lock", "Trunk", "Cluster",
    "DoorUnlk", "DoorLock"};

#if NOCT_FEATURE_HACKER
/* ── Hacker WiFi submenu (indexed by item order) ───────────────────────── */
static const char* const kHackerWifiModes[] = {
    "Radar", "Probe", "EAPOL", "Station", "PktMon",
    "ChAnalyz", "ChActiv", "PktRate", "Pine",
    "MultiSSID", "Signal", "RawCap", "AP+STA"};

/* ── Hacker BLE submenu ────────────────────────────────────────────────── */
static const char* const kHackerBleModes[] = {
    "Trackers", "Devices"};

#if NOCT_FEATURE_LORA
/* ── Hacker LoRa / sub-GHz submenu (EU868) ─────────────────────────────── */
static const char* const kHackerLoraModes[] = {
    "Listen", "Spectrum"};
#endif
#endif

#endif
