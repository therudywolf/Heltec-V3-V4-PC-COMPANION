/*
 * NOCTURNE_OS — App mode enum and mode manager: cleanup, WiFi state,
 * initialize, switch. Caller provides trap context when switching to TRAP.
 */
#ifndef NOCTURNE_APP_MODE_MANAGER_H
#define NOCTURNE_APP_MODE_MANAGER_H

// --- Modes: Monitoring, Config, Hacker x4, BMW, System ---
enum AppMode
{
  MODE_NORMAL,
  MODE_RADAR,           // WiFi scan for clone target
  MODE_WIFI_TRAP,       // WiFi clone (AP + captive portal)
  MODE_WIFI_EAPOL,      // Infosec: EAPOL handshake capture
  MODE_BLE_SPAM,
  MODE_BLE_CLONE,       // BLE scan -> select -> advertise as clone
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
