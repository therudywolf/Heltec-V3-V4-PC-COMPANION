/*
 * NOCTURNE_OS — AppModeManager: unified cleanup, WiFi state, init, switch.
 * Conditional compilation via NOCT_FEATURE_* flags.
 */
#include "AppModeManager.h"
#if NOCT_FEATURE_BMW
#include "modules/car/BmwManager.h"
#endif
#include "nocturne/config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#if NOCT_FEATURE_MONITORING
#include "modules/network/NetManager.h"
#endif
#if NOCT_FEATURE_FORZA
#include "modules/car/ForzaManager.h"
#endif
#if NOCT_FEATURE_HACKER
#include "modules/network/WifiSniffManager.h"
#include "modules/ble/BleManager.h"
#endif
#if NOCT_FEATURE_LORA
#include "modules/lora/LoraManager.h"
#endif

AppModeManager::AppModeManager(
#if NOCT_FEATURE_BMW
    BmwManager &bmw,
#endif
#if NOCT_FEATURE_MONITORING
    NetManager &net,
#endif
#if NOCT_FEATURE_FORZA
    ForzaManager &forza,
#endif
#if NOCT_FEATURE_HACKER
    WifiSniffManager &wifiSniff,
    BleManager &ble,
#endif
#if NOCT_FEATURE_LORA
    LoraManager &lora,
#endif
    int reserved)
    : reserved_(reserved)
#if NOCT_FEATURE_BMW
      , bmw_(bmw)
#endif
#if NOCT_FEATURE_MONITORING
      , net_(net)
#endif
#if NOCT_FEATURE_FORZA
      , forza_(forza)
#endif
#if NOCT_FEATURE_HACKER
      , wifiSniff_(wifiSniff)
      , ble_(ble)
#endif
#if NOCT_FEATURE_LORA
      , lora_(lora)
#endif
{
  (void)reserved_;
}

void AppModeManager::cleanupMode(AppMode mode)
{
  switch (mode)
  {
#if NOCT_FEATURE_BMW
  case MODE_BMW_ASSISTANT:
    bmw_.end();
    break;
#endif
  case MODE_CHARGE_ONLY:
    break;
#if NOCT_FEATURE_MONITORING
  case MODE_NORMAL:
    break;
#endif
#if NOCT_FEATURE_FORZA
  case MODE_GAME_FORZA:
    forza_.stop();
    break;
#endif
#if NOCT_FEATURE_HACKER
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
  case MODE_BLE_SCAN:
  case MODE_BLE_DEVICES:
    ble_.stopScan();
    ble_.stop();
    WiFi.mode(WIFI_STA);
    break;
#endif
#if NOCT_FEATURE_LORA
  case MODE_LORA:
  case MODE_LORA_SWEEP:
    lora_.sleep(); // park the SX1262 (RX off) on exit
    break;
#endif
  default:
    break;
  }
}

void AppModeManager::manageWiFiState(AppMode mode)
{
  switch (mode)
  {
#if NOCT_FEATURE_BMW
  case MODE_BMW_ASSISTANT:
#endif
  case MODE_CHARGE_ONLY:
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF");
    }
#if NOCT_FEATURE_MONITORING
    net_.setSuspend(true);
#endif
    break;

#if NOCT_FEATURE_MONITORING
  case MODE_NORMAL:
    if (WiFi.getMode() != WIFI_STA)
      WiFi.mode(WIFI_STA);
    net_.setSuspend(false);
    break;
#endif

#if NOCT_FEATURE_FORZA
  case MODE_GAME_FORZA:
    if (WiFi.getMode() != WIFI_STA)
    {
      WiFi.mode(WIFI_STA);
      Serial.println("[SYS] WiFi STA for Forza");
    }
#if NOCT_FEATURE_MONITORING
    net_.setSuspend(true);
#endif
    break;
#endif

#if NOCT_FEATURE_HACKER
  case MODE_BLE_SCAN:
  case MODE_BLE_DEVICES:
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for BLE");
    }
#if NOCT_FEATURE_MONITORING
    net_.setSuspend(true);
#endif
    break;
#endif
#if NOCT_FEATURE_LORA
  case MODE_LORA:
  case MODE_LORA_SWEEP:
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for LoRa");
    }
#if NOCT_FEATURE_MONITORING
    net_.setSuspend(true);
#endif
    break;
#endif
#if NOCT_FEATURE_HACKER
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
    if (WiFi.getMode() != WIFI_STA)
    {
      WiFi.mode(WIFI_STA);
      Serial.println("[SYS] WiFi STA for WiFi mode");
    }
#if NOCT_FEATURE_MONITORING
    net_.setSuspend(true);
#endif
    break;
#endif

  default:
    break;
  }
}

bool AppModeManager::initializeMode(AppMode mode
#if NOCT_FEATURE_HACKER
    , int trapWifiSelected, int trapFilteredCount, const int *trapSortedIndices
#endif
)
{
  switch (mode)
  {
#if NOCT_FEATURE_BMW
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
#endif

  case MODE_CHARGE_ONLY:
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[SYS] CHARGE_ONLY mode initialized");
    return true;

#if NOCT_FEATURE_MONITORING
  case MODE_NORMAL:
    manageWiFiState(mode);
    WiFi.scanDelete();
    Serial.println("[SYS] NORMAL mode initialized");
    return true;
#endif

#if NOCT_FEATURE_FORZA
  case MODE_GAME_FORZA:
    manageWiFiState(mode);
    forza_.begin();
    Serial.println("[SYS] FORZA mode initialized");
    return true;
#endif

#if NOCT_FEATURE_HACKER
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

  case MODE_BLE_SCAN:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.beginScan(BLE_SCAN_BASIC);
    Serial.println("[SYS] BLE TRACKER SCAN mode initialized");
    return true;

  case MODE_BLE_DEVICES:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF)
    {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    ble_.beginScan(BLE_SCAN_BASIC); // same passive scan; the scene lists every advertiser
    Serial.println("[SYS] BLE DEVICE BROWSER mode initialized");
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
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_PINESCAN);
    Serial.println("[SYS] PINESCAN mode initialized");
    return true;
  case MODE_WIFI_MULTISSID:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_MULTISSID);
    Serial.println("[SYS] MULTISSID mode initialized");
    return true;
  case MODE_WIFI_SIGNAL_STRENGTH:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_SIGNAL_STRENGTH);
    Serial.println("[SYS] SIGNAL STRENGTH mode initialized");
    return true;
  case MODE_WIFI_RAW_CAPTURE:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_RAW_CAPTURE);
    Serial.println("[SYS] RAW CAPTURE mode initialized");
    return true;
  case MODE_WIFI_AP_STA:
    manageWiFiState(mode);
    wifiSniff_.begin(SNIFF_MODE_AP_STA);
    Serial.println("[SYS] AP+STA mode initialized");
    return true;

#endif

#if NOCT_FEATURE_LORA
  case MODE_LORA:
    manageWiFiState(mode);            // WiFi off so the SX1262 owns the RF path
    if (lora_.begin())                // init @ EU868; lazy, so re-entry re-arms it
      lora_.startListen();
    Serial.printf("[SYS] LoRa LISTEN initialized (radio %s)\n",
                  lora_.isReady() ? "OK" : "NOT FOUND");
    return true;                      // enter even if radio missing — scene shows the error

  case MODE_LORA_SWEEP:
    manageWiFiState(mode);
    if (lora_.begin())
      lora_.beginSweep();
    Serial.printf("[SYS] LoRa SWEEP initialized (radio %s)\n",
                  lora_.isReady() ? "OK" : "NOT FOUND");
    return true;
#endif

#if NOCT_FEATURE_WOLFPET
  case MODE_WOLFPET:
    Serial.println("[SYS] WolfPet mode initialized");
    return true;
#endif

  default:
    Serial.println("[SYS] Unknown mode");
    return false;
  }
}

bool AppModeManager::switchToMode(AppMode &current, AppMode next
#if NOCT_FEATURE_HACKER
    , int trapWifiSelected, int trapFilteredCount, const int *trapSortedIndices
#endif
)
{
  if (next == current)
    return true;

  Serial.printf("[SYS] Switching from MODE_%d to MODE_%d\n", (int)current, (int)next);
  cleanupMode(current);

  if (!initializeMode(next
#if NOCT_FEATURE_HACKER
      , trapWifiSelected, trapFilteredCount, trapSortedIndices
#endif
  ))
  {
    Serial.println("[SYS] Mode init failed, staying in current");
    return false;
  }

  current = next;
  Serial.printf("[SYS] Switched to MODE_%d\n", (int)current);
  return true;
}

#if NOCT_FEATURE_MONITORING
void AppModeManager::exitToNormal(AppMode &current)
{
  switchToMode(current, MODE_NORMAL);
  Serial.println("[SYS] NETMANAGER RESUMED.");
}
#endif
