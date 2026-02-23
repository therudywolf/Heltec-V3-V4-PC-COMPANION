/*
 * NOCTURNE_OS — Menu structure: submenu counts and mode mapping.
 * Hacker: 4 items — WiFi Clone, BLE Clone, BLE Spam, Infosec (EAPOL).
 */
#include "MenuHandler.h"

#define HACKER_ITEM_COUNT 4

int submenuCount(int category)
{
  switch (category)
  {
  case 0:
    return 2; // Monitoring: PC, Forza
  case 1:
    return HACKER_ITEM_COUNT; // Hacker: WiFi Clone, BLE Clone, BLE Spam, Infosec
  case 2:
    return 1; // BMW: BMW Assistant
  case 3:
    return 7; // Config: AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT
  case 4:
    return 4; // System: REBOOT, CHARGE ONLY, POWER OFF, VERSION
  default:
    return 2;
  }
}

int submenuCountForHackerGroup(int group)
{
  (void)group;
  return HACKER_ITEM_COUNT;
}

/** Map Hacker menu item (0..3) to AppMode. */
AppMode getModeForHackerItem(int group, int item)
{
  (void)group;
  switch (item)
  {
  case 0: return MODE_RADAR;        // WiFi Clone: scan & select, long-press in RADAR starts Trap
  case 1: return MODE_BLE_CLONE;
  case 2: return MODE_BLE_SPAM;
  case 3: return MODE_WIFI_EAPOL;   // Infosec: EAPOL handshake capture
  default: return MODE_NORMAL;
  }
}
