/*
 * NOCTURNE_OS — Menu structure: submenu counts and mode mapping.
 * Categories and items adapt to NOCT_FEATURE_* build flags.
 */
#include "MenuHandler.h"

int submenuCount(int category)
{
  switch (category)
  {
#if NOCT_FEATURE_MONITORING
  case MCAT_MONITORING:
#if NOCT_FEATURE_FORZA
    return 2; // PC Monitor, Forza
#else
    return 1; // PC Monitor only
#endif
#endif

#if NOCT_FEATURE_HACKER
  case MCAT_HACKER:
    return HACKER_GROUP_COUNT; // WiFi, BLE
#endif

  case MCAT_BMW:
    return 2; // BMW Assistant, BMW Demo

  case MCAT_CONFIG:
    return 7; // AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT

  case MCAT_SYSTEM:
    return 5; // Demo toggle, REBOOT, CHARGE ONLY, POWER OFF, VERSION

  default:
    return 1;
  }
}

int submenuCountForHackerGroup(int group)
{
#if NOCT_FEATURE_HACKER
  switch (group)
  {
  case HACKER_GROUP_WIFI:
    return 14;
  case HACKER_GROUP_BLE:
    return 6;
  default:
    return 1;
  }
#else
  (void)group;
  return 0;
#endif
}

AppMode getModeForHackerItem(int group, int item)
{
#if NOCT_FEATURE_HACKER
  if (group == HACKER_GROUP_WIFI)
  {
    switch (item)
    {
    case 0:  return MODE_RADAR;
    case 1:  return MODE_WIFI_PROBE_SCAN;
    case 2:  return MODE_WIFI_EAPOL_SCAN;
    case 3:  return MODE_WIFI_STATION_SCAN;
    case 4:  return MODE_WIFI_PACKET_MONITOR;
    case 5:  return MODE_WIFI_CHANNEL_ANALYZER;
    case 6:  return MODE_WIFI_CHANNEL_ACTIVITY;
    case 7:  return MODE_WIFI_PACKET_RATE;
    case 8:  return MODE_WIFI_PINESCAN;
    case 9:  return MODE_WIFI_MULTISSID;
    case 10: return MODE_WIFI_SIGNAL_STRENGTH;
    case 11: return MODE_WIFI_RAW_CAPTURE;
    case 12: return MODE_WIFI_AP_STA;
    case 13: return MODE_WIFI_TRAP;
    default: return MODE_BMW_ASSISTANT;
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
    default: return MODE_BMW_ASSISTANT;
    }
  }
#else
  (void)group;
  (void)item;
#endif
  return MODE_BMW_ASSISTANT;
}
