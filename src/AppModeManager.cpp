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
  case MODE_WIFI_PROBE_SCAN:
  case MODE_WIFI_EAPOL_SCAN:
  case MODE_WIFI_STATION_SCAN:
  case MODE_WIFI_PACKET_MONITOR:
  case MODE_WIFI_CHANNEL_ANALYZER:
  case MODE_WIFI_CHANNEL_ACTIVITY:
  case MODE_WIFI_PACKET_RATE:
  case MODE_WIFI_PINESCAN:
  case MODE_WIFI_MULTISSID:
  case MODE_WIFI_SIGNAL_STRENGTH:
  case MODE_WIFI_RAW_CAPTURE:
  case MODE_WIFI_AP_STA:
  case MODE_WIFI_SNIFF:
    wifiSniff_.stop();
    WiFi.scanDelete();
    break;
  case MODE_WIFI_TRAP:
    trap_.stop();
    WiFi.mode(WIFI_STA);
    break;
  case MODE_BLE_SPAM:
  case MODE_BLE_SOUR_APPLE:
  case MODE_BLE_SWIFTPAIR_MICROSOFT:
  case MODE_BLE_SWIFTPAIR_GOOGLE:
  case MODE_BLE_SWIFTPAIR_SAMSUNG:
  case MODE_BLE_FLIPPER_SPAM:
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
  case MODE_BLE_SOUR_APPLE:
  case MODE_BLE_SWIFTPAIR_MICROSOFT:
  case MODE_BLE_SWIFTPAIR_GOOGLE:
  case MODE_BLE_SWIFTPAIR_SAMSUNG:
  case MODE_BLE_FLIPPER_SPAM:
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
  case MODE_WIFI_PROBE_SCAN:
  case MODE_WIFI_EAPOL_SCAN:
  case MODE_WIFI_STATION_SCAN:
  case MODE_WIFI_PACKET_MONITOR:
  case MODE_WIFI_CHANNEL_ANALYZER:
  case MODE_WIFI_CHANNEL_ACTIVITY:
  case MODE_WIFI_PACKET_RATE:
  case MODE_WIFI_PINESCAN:
  case MODE_WIFI_MULTISSID:
  case MODE_WIFI_SIGNAL_STRENGTH:
  case MODE_WIFI_RAW_CAPTURE:
  case MODE_WIFI_AP_STA:
  case MODE_WIFI_SNIFF:
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
    Serial.println("[SYS] BLE SPAM mode initialized");
    return true;

  case MODE_RADAR:
    manageWiFiState(mode);
    WiFi.scanNetworks(true, true);
    Serial.println("[SYS] RADAR mode initialized");
    return true;

  case MODE_WIFI_SNIFF:
    manageWiFiState(mode);
    wifiSniff_.begin();
    Serial.println("[SYS] SNIFF mode initialized");
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

  case MODE_WIFI_PROBE_SCAN:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_PROBE_SCAN);
    Serial.println("[SYS] PROBE SCAN mode initialized");
    return true;
  case MODE_WIFI_EAPOL_SCAN:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_EAPOL_CAPTURE);
    Serial.println("[SYS] EAPOL SCAN mode initialized");
    return true;
  case MODE_WIFI_STATION_SCAN:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_STATION_SCAN);
    Serial.println("[SYS] STATION SCAN mode initialized");
    return true;
  case MODE_WIFI_PACKET_MONITOR:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_PACKET_MONITOR);
    Serial.println("[SYS] PACKET MONITOR mode initialized");
    return true;
  case MODE_WIFI_CHANNEL_ANALYZER:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_CHANNEL_ANALYZER);
    Serial.println("[SYS] CHANNEL ANALYZER mode initialized");
    return true;
  case MODE_WIFI_CHANNEL_ACTIVITY:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_CHANNEL_ACTIVITY);
    Serial.println("[SYS] CHANNEL ACTIVITY mode initialized");
    return true;
  case MODE_WIFI_PACKET_RATE:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_PACKET_RATE);
    Serial.println("[SYS] PACKET RATE mode initialized");
    return true;
  case MODE_WIFI_PINESCAN:
  case MODE_WIFI_MULTISSID:
  case MODE_WIFI_SIGNAL_STRENGTH:
  case MODE_WIFI_RAW_CAPTURE:
  case MODE_WIFI_AP_STA:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_AP);
    Serial.printf("[SYS] WiFi scan mode %d initialized\n", mode);
    return true;
  case MODE_BLE_SOUR_APPLE:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.startAttack(BLE_ATTACK_SOUR_APPLE);
    Serial.println("[SYS] SOUR APPLE mode initialized");
    return true;
  case MODE_BLE_SWIFTPAIR_MICROSOFT:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.startAttack(BLE_ATTACK_SWIFTPAIR_MICROSOFT);
    Serial.println("[SYS] SWIFTPAIR MS mode initialized");
    return true;
  case MODE_BLE_SWIFTPAIR_GOOGLE:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.startAttack(BLE_ATTACK_SWIFTPAIR_GOOGLE);
    Serial.println("[SYS] SWIFTPAIR GOOGLE mode initialized");
    return true;
  case MODE_BLE_SWIFTPAIR_SAMSUNG:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.startAttack(BLE_ATTACK_SWIFTPAIR_SAMSUNG);
    Serial.println("[SYS] SWIFTPAIR SAMSUNG mode initialized");
    return true;
  case MODE_BLE_FLIPPER_SPAM:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.startAttack(BLE_ATTACK_FLIPPER_SPAM);
    Serial.println("[SYS] BLE FLIPPER SPAM mode initialized");
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
