/*
 * NOCTURNE_OS — AppModeManager: cleanup, manage WiFi, initialize, switch.
 * BMW-only: NORMAL, BMW_ASSISTANT, CHARGE_ONLY.
 */
#include "AppModeManager.h"
#include "modules/network/NetManager.h"
#include "modules/ble/BleManager.h"
#include "modules/car/BmwManager.h"
#include "nocturne/config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

AppModeManager::AppModeManager(NetManager &net, BleManager &ble, BmwManager &bmw)
    : net_(net), ble_(ble), bmw_(bmw)
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
  case MODE_NORMAL:
  default:
    break;
  }
}

void AppModeManager::manageWiFiState(AppMode mode)
{
  switch (mode)
  {
  case MODE_BMW_ASSISTANT:
  case MODE_CHARGE_ONLY:
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for BMW Assistant / Charge only");
    }
    net_.setSuspend(true);
    break;
  case MODE_NORMAL:
  default:
    if (WiFi.getMode() != WIFI_STA)
      WiFi.mode(WIFI_STA);
    net_.setSuspend(false);
    break;
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

  case MODE_NORMAL:
    manageWiFiState(mode);
    WiFi.scanDelete();
    Serial.println("[SYS] NORMAL mode initialized");
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
    Serial.println("[SYS] Mode initialization failed, falling back to NORMAL");
    cleanupMode(current);
    current = MODE_NORMAL;
    initializeMode(MODE_NORMAL);
    return false;
  }

  current = next;
  Serial.printf("[SYS] Successfully switched to MODE_%d\n", (int)current);
  return true;
}

void AppModeManager::exitToNormal(AppMode &current)
{
  switchToMode(current, MODE_NORMAL);
  Serial.println("[SYS] NETMANAGER RESUMED.");
}
