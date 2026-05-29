/*
 * NOCTURNE_OS — BatteryManager: ADC read (GPIO 1, divider 4.9), moving average,
 * adaptive interval. Updates AppState.batteryVoltage, batteryPct, isCharging.
 */
#ifndef NOCTURNE_BATTERY_MANAGER_H
#define NOCTURNE_BATTERY_MANAGER_H

#include "nocturne/config.h"

struct AppState;

class BatteryManager
{
public:
  BatteryManager();

  /** Update state.batteryVoltage, state.batteryPct, state.isCharging.
   * Returns the next reading interval in ms. */
  unsigned long update(AppState &state);

private:
  float readVoltage();

  float history_[NOCT_BAT_SMOOTHING_SAMPLES];
  int historyIndex_ = 0;
  bool historyFilled_ = false;
  bool batCtrlPinState_ = false;
};

#endif
