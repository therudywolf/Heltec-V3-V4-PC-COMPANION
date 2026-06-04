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
  {
    int n = 1; // PC Monitor
#if NOCT_FEATURE_FORZA
    n++; // Forza
#endif
#if NOCT_FEATURE_WOLFPET
    n++; // Wolf Pet
#endif
    return n;
  }
#endif

#if NOCT_FEATURE_HACKER
  case MCAT_HACKER:
    return HACKER_GROUP_COUNT; // WiFi, BLE
#endif

#if NOCT_FEATURE_BMW
  case MCAT_BMW:
    return 2; // BMW Assistant, BMW Demo
#endif

  case MCAT_CONFIG:
#if NOCT_FEATURE_MONITORING
    return 9; // AUTO, FLIP, FX, LED, DIM, CONTRAST, TIMEOUT, INVERT, PIN
#else
    return 8; // AUTO, FLIP, FX, LED, DIM, CONTRAST, TIMEOUT, INVERT
#endif

  case MCAT_SYSTEM:
#if NOCT_FEATURE_BMW
    return 6; // Demo, REBOOT, CHARGE ONLY, SCREENSAVER, POWER OFF, VERSION
#else
    return 5; // REBOOT, CHARGE ONLY, SCREENSAVER, POWER OFF, VERSION
#endif

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
    return 14; // 13 sniff tools + Export
  case HACKER_GROUP_BLE:
    return 2; // Trackers, Devices
#if NOCT_FEATURE_LORA
  case HACKER_GROUP_LORA:
#if NOCT_FEATURE_LORA_TX
    return 3; // Listen, Spectrum, TX
#else
    return 2; // Listen (mesh packet RX), Spectrum (band sweep)
#endif
#endif
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
    case 13: return MODE_EXPORT;
    default: return NOCT_DEFAULT_MODE;
    }
  }
  if (group == HACKER_GROUP_BLE)
  {
    switch (item)
    {
    case 0: return MODE_BLE_SCAN;
    case 1: return MODE_BLE_DEVICES;
    default: return NOCT_DEFAULT_MODE;
    }
  }
#if NOCT_FEATURE_LORA
  if (group == HACKER_GROUP_LORA)
  {
    switch (item)
    {
    case 0: return MODE_LORA;
    case 1: return MODE_LORA_SWEEP;
#if NOCT_FEATURE_LORA_TX
    case 2: return MODE_LORA_TX;
#endif
    default: return NOCT_DEFAULT_MODE;
    }
  }
#endif
#else
  (void)group;
  (void)item;
#endif
  return NOCT_DEFAULT_MODE;
}
