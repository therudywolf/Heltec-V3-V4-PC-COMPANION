/*
 * NOCTURNE_OS — Menu structure helpers: category/submenu counts,
 * map hacker item to AppMode. Actions (handleMenuActionByCategory etc.) stay in main.
 */
#ifndef NOCTURNE_MENU_HANDLER_H
#define NOCTURNE_MENU_HANDLER_H

#include "AppModeManager.h"

// Categories: 0=BMW, 1=Config, 2=System (BMW-only firmware)
#define MENU_CATEGORIES 3

/** Number of items in a category (level 1 submenu). */
int submenuCount(int category);

/** Number of items in a hacker group (level 2). */
int submenuCountForHackerGroup(int group);

/** Map (hacker group, item index) to AppMode. Used when menuLevel==2. */
AppMode getModeForHackerItem(int group, int item);

#endif
