/*
 * NOCTURNE_OS — AppModeManager: cleanup, WiFi off, initialize, switch.
 * BMW-only: BMW_ASSISTANT, CHARGE_ONLY.
 */
#include "AppModeManager.h"
#include "modules/car/BmwManager.h"
#include "nocturne/config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

AppModeManager::AppModeManager(BmwManager &bmw)
    : bmw_(bmw)
{
}

void AppModeManager::cleanupMode(AppMode mode)
{
  switch (mode)
  {
  case MODE_BMW_ASSISTANT:
    bmw_.end();
    break;
  case MODE_CHARGE_ONLY:
  default:
    break;
  }
}

void AppModeManager::manageWiFiState(AppMode mode)
{
  (void)mode;
  if (WiFi.getMode() != WIFI_OFF)
  {
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_OFF);
    Serial.println("[SYS] WiFi OFF");
  }
}

bool AppModeManager::initializeMode(AppMode mode)
{
  switch (mode)
  {
  case MODE_BMW_ASSISTANT:
    manageWiFiState(mode);
    {
      Preferences prefs;
      prefs.begin("nocturne", true);
      bool demo = prefs.getBool("bmw_demo", false);
      prefs.end();
      bmw_.setDemoMode(demo);
    }
    bmw_.begin();
    Serial.println("[SYS] BMW Assistant mode initialized");
    return true;

  case MODE_CHARGE_ONLY:
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[SYS] CHARGE_ONLY mode initialized");
    return true;

  default:
    Serial.println("[SYS] Unknown mode");
    return false;
  }
}

bool AppModeManager::switchToMode(AppMode &current, AppMode next)
{
  if (next == current)
    return true;

  Serial.printf("[SYS] Switching from MODE_%d to MODE_%d\n", (int)current,
                (int)next);

  cleanupMode(current);

  if (!initializeMode(next))
  {
    Serial.println("[SYS] Mode init failed, staying in current");
    return false;
  }

  current = next;
  Serial.printf("[SYS] Switched to MODE_%d\n", (int)current);
  return true;
}
