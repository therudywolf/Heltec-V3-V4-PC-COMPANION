/*
 * NOCTURNE_OS — Menu structure: conditional categories based on features.
 *
 * BMW-only:      BMW | Config | System          (3 categories)
 * PC Companion:  Monitoring | BMW | Config | System  (4 categories)
 * Full:          Monitoring | Hacker | BMW | Config | System  (5 categories)
 */
#ifndef NOCTURNE_MENU_HANDLER_H
#define NOCTURNE_MENU_HANDLER_H

#include "AppModeManager.h"

/*
 * Category indices are computed at compile time so that menu navigation
 * code uses a single numbering scheme regardless of build profile.
 */
enum MenuCategory
{
#if NOCT_FEATURE_MONITORING
  MCAT_MONITORING,
#endif
#if NOCT_FEATURE_HACKER
  MCAT_HACKER,
#endif
#if NOCT_FEATURE_BMW
  MCAT_BMW,
#endif
  MCAT_CONFIG,
  MCAT_SYSTEM,
  MENU_CATEGORIES
};

#if NOCT_FEATURE_HACKER
#define HACKER_GROUP_WIFI 0
#define HACKER_GROUP_BLE 1
#if NOCT_FEATURE_LORA
#define HACKER_GROUP_LORA 2
#define NOCT_HG_N_LORA 1
#else
#define NOCT_HG_N_LORA 0
#endif
#if NOCT_FEATURE_BADUSB
#define HACKER_GROUP_USB (2 + NOCT_HG_N_LORA)
#define NOCT_HG_N_USB 1
#else
#define NOCT_HG_N_USB 0
#endif
#define HACKER_GROUP_COUNT (2 + NOCT_HG_N_LORA + NOCT_HG_N_USB)
#endif

int submenuCount(int category);
int submenuCountForHackerGroup(int group);
AppMode getModeForHackerItem(int group, int item);

#endif
