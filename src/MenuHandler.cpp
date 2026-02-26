/*
 * NOCTURNE_OS — Menu structure: submenu counts. BMW-only: 3 categories (BMW, Config, System).
 */
#include "MenuHandler.h"

#define MENU_CATEGORIES 3

int submenuCount(int category)
{
  switch (category)
  {
  case 0:
    return 2; // BMW: BMW Assistant, BMW Demo
  case 1:
    return 7; // Config: AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT
  case 2:
    return 5; // System: Demo, REBOOT, CHARGE ONLY, POWER OFF, VERSION
  default:
    return 2;
  }
}

int submenuCountForHackerGroup(int group)
{
  (void)group;
  return 0;
}

AppMode getModeForHackerItem(int group, int item)
{
  (void)group;
  (void)item;
  return MODE_BMW_ASSISTANT;
}
