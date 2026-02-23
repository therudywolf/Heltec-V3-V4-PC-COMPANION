/*
 * NOCTURNE_OS — App mode enum and mode manager: cleanup, WiFi state,
 * initialize, switch. Caller provides trap context when switching to TRAP.
 */
#ifndef NOCTURNE_APP_MODE_MANAGER_H
#define NOCTURNE_APP_MODE_MANAGER_H

// --- Cyberdeck Modes ---
enum AppMode
{
  MODE_NORMAL,
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
  MODE_GAME_FORZA,
  MODE_BMW_ASSISTANT,
  MODE_CHARGE_ONLY
};

class NetManager;
class TrapManager;
class WifiSniffManager;
class BleManager;
class ForzaManager;
class BmwManager;

class AppModeManager
{
public:
  AppModeManager(NetManager &net, TrapManager &trap, WifiSniffManager &wifiSniff,
                 BleManager &ble, ForzaManager &forza, BmwManager &bmw);

  /** Switch from current to next. On TRAP, pass trapWifiSelected, trapFilteredCount, trapSortedIndices (main calls sortAndFilterWiFiNetworks() first). Returns true on success and sets current = next. */
  bool switchToMode(AppMode &current, AppMode next, int trapWifiSelected = -1,
                    int trapFilteredCount = 0, const int *trapSortedIndices = nullptr);

  void exitToNormal(AppMode &current);

private:
  void cleanupMode(AppMode mode);
  void manageWiFiState(AppMode mode);
  bool initializeMode(AppMode mode, int trapWifiSelected, int trapFilteredCount,
                      const int *trapSortedIndices);

  NetManager &net_;
  TrapManager &trap_;
  WifiSniffManager &wifiSniff_;
  BleManager &ble_;
  ForzaManager &forza_;
  BmwManager &bmw_;
};

#endif
