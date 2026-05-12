/*
 * NOCTURNE_OS — Unified app mode enum and manager.
 * Modes compiled conditionally via NOCT_FEATURE_* flags.
 */
#ifndef NOCTURNE_APP_MODE_MANAGER_H
#define NOCTURNE_APP_MODE_MANAGER_H

#include "nocturne/config.h"

enum AppMode
{
  MODE_BMW_ASSISTANT,
  MODE_CHARGE_ONLY,
#if NOCT_FEATURE_MONITORING
  MODE_NORMAL,
#endif
#if NOCT_FEATURE_FORZA
  MODE_GAME_FORZA,
#endif
#if NOCT_FEATURE_HACKER
  MODE_RADAR,
  MODE_WIFI_PROBE_SCAN,
  MODE_WIFI_EAPOL_SCAN,
  MODE_WIFI_STATION_SCAN,
  MODE_WIFI_PACKET_MONITOR,
  MODE_WIFI_CHANNEL_ANALYZER,
  MODE_WIFI_CHANNEL_ACTIVITY,
  MODE_WIFI_PACKET_RATE,
  MODE_WIFI_PINESCAN,
  MODE_WIFI_MULTISSID,
  MODE_WIFI_SIGNAL_STRENGTH,
  MODE_WIFI_RAW_CAPTURE,
  MODE_WIFI_AP_STA,
  MODE_WIFI_SNIFF,
  MODE_WIFI_TRAP,
  MODE_BLE_SPAM,
  MODE_BLE_SOUR_APPLE,
  MODE_BLE_SWIFTPAIR_MICROSOFT,
  MODE_BLE_SWIFTPAIR_GOOGLE,
  MODE_BLE_SWIFTPAIR_SAMSUNG,
  MODE_BLE_FLIPPER_SPAM,
#endif
};

class BmwManager;
#if NOCT_FEATURE_MONITORING
class NetManager;
#endif
#if NOCT_FEATURE_FORZA
class ForzaManager;
#endif
#if NOCT_FEATURE_HACKER
class TrapManager;
class WifiSniffManager;
class BleManager;
#endif

class AppModeManager
{
public:
  AppModeManager(
      BmwManager &bmw
#if NOCT_FEATURE_MONITORING
      , NetManager &net
#endif
#if NOCT_FEATURE_FORZA
      , ForzaManager &forza
#endif
#if NOCT_FEATURE_HACKER
      , TrapManager &trap
      , WifiSniffManager &wifiSniff
      , BleManager &ble
#endif
  );

  bool switchToMode(AppMode &current, AppMode next
#if NOCT_FEATURE_HACKER
      , int trapWifiSelected = -1
      , int trapFilteredCount = 0
      , const int *trapSortedIndices = nullptr
#endif
  );

#if NOCT_FEATURE_MONITORING
  void exitToNormal(AppMode &current);
#endif

private:
  void cleanupMode(AppMode mode);
  void manageWiFiState(AppMode mode);
  bool initializeMode(AppMode mode
#if NOCT_FEATURE_HACKER
      , int trapWifiSelected
      , int trapFilteredCount
      , const int *trapSortedIndices
#endif
  );

  BmwManager &bmw_;
#if NOCT_FEATURE_MONITORING
  NetManager &net_;
#endif
#if NOCT_FEATURE_FORZA
  ForzaManager &forza_;
#endif
#if NOCT_FEATURE_HACKER
  TrapManager &trap_;
  WifiSniffManager &wifiSniff_;
  BleManager &ble_;
#endif
};

#endif
