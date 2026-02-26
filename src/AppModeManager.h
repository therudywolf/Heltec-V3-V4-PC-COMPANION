/*
 * NOCTURNE_OS — App mode enum and mode manager: cleanup, WiFi state,
 * initialize, switch. BMW-only firmware: NORMAL, BMW_ASSISTANT, CHARGE_ONLY.
 */
#ifndef NOCTURNE_APP_MODE_MANAGER_H
#define NOCTURNE_APP_MODE_MANAGER_H

enum AppMode
{
  MODE_NORMAL,
  MODE_BMW_ASSISTANT,
  MODE_CHARGE_ONLY
};

class NetManager;
class BleManager;
class BmwManager;

class AppModeManager
{
public:
  AppModeManager(NetManager &net, BleManager &ble, BmwManager &bmw);

  bool switchToMode(AppMode &current, AppMode next);

  void exitToNormal(AppMode &current);

private:
  void cleanupMode(AppMode mode);
  void manageWiFiState(AppMode mode);
  bool initializeMode(AppMode mode);

  NetManager &net_;
  BleManager &ble_;
  BmwManager &bmw_;
};

#endif
