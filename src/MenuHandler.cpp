/*
 * NOCTURNE_OS — Menu structure: submenu counts and mode mapping.
 */
#include "MenuHandler.h"

int submenuCount(int category)
{
  switch (category)
  {
  case 0:
    return 2; // Monitoring: PC, Forza
  case 1:
    return 7; // Config: AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT
  case 2:
    return HACKER_GROUP_COUNT; // Hacker: WiFi, BLE
  case 3:
    return 1; // BMW: BMW Assistant
  case 4:
    return 4; // System: REBOOT, CHARGE ONLY, POWER OFF, VERSION
  default:
    return 2;
  }
}

int submenuCountForHackerGroup(int group)
{
  switch (group)
  {
  case HACKER_GROUP_WIFI:
    return 14; // 13 scan + 1 duplicator
  case HACKER_GROUP_BLE:
    return 6; // mimic only
  default:
    return 1;
  }
}

AppMode getModeForHackerItem(int group, int item)
{
  if (group == HACKER_GROUP_WIFI)
  {
    switch (item)
    {
    case 0: return MODE_RADAR;
    case 1: return MODE_WIFI_PROBE_SCAN;
    case 2: return MODE_WIFI_EAPOL_SCAN;
    case 3: return MODE_WIFI_STATION_SCAN;
    case 4: return MODE_WIFI_PACKET_MONITOR;
    case 5: return MODE_WIFI_CHANNEL_ANALYZER;
    case 6: return MODE_WIFI_CHANNEL_ACTIVITY;
    case 7: return MODE_WIFI_PACKET_RATE;
    case 8: return MODE_WIFI_PINESCAN;
    case 9: return MODE_WIFI_MULTISSID;
    case 10: return MODE_WIFI_SIGNAL_STRENGTH;
    case 11: return MODE_WIFI_RAW_CAPTURE;
    case 12: return MODE_WIFI_AP_STA;
    case 13: return MODE_WIFI_TRAP;
    default: return MODE_NORMAL;
    }
  }
  if (group == HACKER_GROUP_BLE)
  {
    switch (item)
    {
    case 0: return MODE_BLE_SPAM;
    case 1: return MODE_BLE_SOUR_APPLE;
    case 2: return MODE_BLE_SWIFTPAIR_MICROSOFT;
    case 3: return MODE_BLE_SWIFTPAIR_GOOGLE;
    case 4: return MODE_BLE_SWIFTPAIR_SAMSUNG;
    case 5: return MODE_BLE_FLIPPER_SPAM;
    default: return MODE_NORMAL;
    }
  }
  return MODE_NORMAL;
}
