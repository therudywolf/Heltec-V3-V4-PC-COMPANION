/*
 * NOCTURNE_OS — App mode enum and manager for BMW-only firmware.
 * Modes: BMW_ASSISTANT, CHARGE_ONLY.
 */
#ifndef NOCTURNE_APP_MODE_MANAGER_H
#define NOCTURNE_APP_MODE_MANAGER_H

enum AppMode
{
  MODE_BMW_ASSISTANT,
  MODE_CHARGE_ONLY
};

class BmwManager;

class AppModeManager
{
public:
  explicit AppModeManager(BmwManager &bmw);

  bool switchToMode(AppMode &current, AppMode next);

private:
  void cleanupMode(AppMode mode);
  void manageWiFiState(AppMode mode);
  bool initializeMode(AppMode mode);

  BmwManager &bmw_;
};

#endif
