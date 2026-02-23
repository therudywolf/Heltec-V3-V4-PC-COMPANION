/*
 * NOCTURNE_OS — AppModeManager: cleanup, manage WiFi, initialize, switch.
 */
#include "AppModeManager.h"
#include "modules/network/NetManager.h"
#include "modules/network/TrapManager.h"
#include "modules/network/WifiSniffManager.h"
#include "modules/ble/BleManager.h"
#include "modules/car/ForzaManager.h"
#include "modules/car/BmwManager.h"
#include "nocturne/config.h"
#include <Arduino.h>
#include <WiFi.h>

AppModeManager::AppModeManager(NetManager &net, TrapManager &trap,
                               WifiSniffManager &wifiSniff, BleManager &ble,
                               ForzaManager &forza, BmwManager &bmw)
    : net_(net), trap_(trap), wifiSniff_(wifiSniff), ble_(ble), forza_(forza),
      bmw_(bmw)
{
}

void AppModeManager::cleanupMode(AppMode mode)
{
  switch (mode)
  {
  case MODE_RADAR:
    WiFi.scanDelete();
    break;
  case MODE_WIFI_EAPOL:
    wifiSniff_.stop();
    WiFi.scanDelete();
    break;
  case MODE_WIFI_TRAP:
    trap_.stop();
    WiFi.mode(WIFI_STA);
    break;
  case MODE_BLE_SPAM:
  case MODE_BLE_CLONE:
    ble_.stop();
    WiFi.mode(WIFI_STA);
    break;
  case MODE_GAME_FORZA:
    forza_.stop();
    break;
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
  case MODE_BLE_SPAM:
  case MODE_BLE_CLONE:
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for BLE");
    }
    net_.setSuspend(true);
    break;
  case MODE_RADAR:
  case MODE_WIFI_EAPOL:
  case MODE_WIFI_TRAP:
    if (WiFi.getMode() != WIFI_STA)
    {
      WiFi.mode(WIFI_STA);
      Serial.println("[SYS] WiFi STA for WiFi mode");
    }
    net_.setSuspend(true);
    break;
  case MODE_GAME_FORZA:
    if (WiFi.getMode() != WIFI_STA)
    {
      WiFi.mode(WIFI_STA);
      Serial.println("[SYS] WiFi STA for Forza");
    }
    net_.setSuspend(true);
    break;
  case MODE_BMW_ASSISTANT:
  case MODE_CHARGE_ONLY:
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for BMW Assistant (BLE key) / Charge only");
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

bool AppModeManager::initializeMode(AppMode mode, int trapWifiSelected,
                                    int trapFilteredCount,
                                    const int *trapSortedIndices)
{
  switch (mode)
  {
  case MODE_BLE_SPAM:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.begin();
    if (!ble_.isActive())
    {
      Serial.println("[SYS] BLE init failed");
      return false;
    }
    ble_.startAttack(BLE_ATTACK_SPAM);
    Serial.println("[SYS] BLE SPAM mode initialized");
    return true;

  case MODE_BLE_CLONE:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.begin();
    if (!ble_.isActive())
    {
      Serial.println("[SYS] BLE init failed");
      return false;
    }
    ble_.beginScan();
    Serial.println("[SYS] BLE CLONE mode initialized (scan)");
    return true;

  case MODE_RADAR:
    manageWiFiState(mode);
    WiFi.scanNetworks(true, true);
    Serial.println("[SYS] RADAR mode initialized");
    return true;

  case MODE_WIFI_EAPOL:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_EAPOL_CAPTURE);
    Serial.println("[SYS] EAPOL (Infosec) mode initialized");
    return true;

  case MODE_WIFI_TRAP:
  {
    manageWiFiState(mode);
    int n = WiFi.scanComplete();
    if (n == -1)
    {
      WiFi.scanNetworks(true, true);
      Serial.println("[TRAP] Starting scan for SSID cloning...");
    }
    if (n > 0 && trapFilteredCount > 0 && trapWifiSelected >= 0 &&
        trapWifiSelected < trapFilteredCount && trapSortedIndices)
    {
      int actualIndex = trapSortedIndices[trapWifiSelected];
      trap_.setClonedSSID(actualIndex);
    }
    trap_.start();
    yield();
    if (!trap_.isActive())
    {
      Serial.println("[SYS] TRAP init failed - AP not started");
      return false;
    }
    Serial.println("[SYS] TRAP mode initialized");
    return true;
  }

  case MODE_GAME_FORZA:
    manageWiFiState(mode);
    forza_.begin();
    Serial.println("[SYS] FORZA mode initialized");
    return true;

  case MODE_BMW_ASSISTANT:
    manageWiFiState(mode);
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

bool AppModeManager::switchToMode(AppMode &current, AppMode next,
                                  int trapWifiSelected, int trapFilteredCount,
                                  const int *trapSortedIndices)
{
  if (next == current)
    return true;

  Serial.printf("[SYS] Switching from MODE_%d to MODE_%d\n", (int)current,
                (int)next);

  /* Always cleanup current mode first, then init next — avoids double init and leftover peripherals. */
  cleanupMode(current);

  if (!initializeMode(next, trapWifiSelected, trapFilteredCount,
                     trapSortedIndices))
  {
    Serial.println("[SYS] Mode initialization failed, falling back to NORMAL");
    cleanupMode(current);
    current = MODE_NORMAL;
    initializeMode(MODE_NORMAL, -1, 0, nullptr);
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
