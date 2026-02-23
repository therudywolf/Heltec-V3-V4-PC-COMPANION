/*
 * NOCTURNE_OS — BatteryManager: voltage read, smoothing, adaptive interval.
 */
#include "BatteryManager.h"
#include "nocturne/Types.h"
#include <Arduino.h>

BatteryManager::BatteryManager()
{
  for (int i = 0; i < NOCT_BAT_SMOOTHING_SAMPLES; i++)
    history_[i] = 0.0f;
}

float BatteryManager::readVoltage()
{
  if (!batCtrlPinState_)
  {
    pinMode(NOCT_BAT_CTRL_PIN, OUTPUT);
    digitalWrite(NOCT_BAT_CTRL_PIN, HIGH);
    batCtrlPinState_ = true;
    delay(10);
  }

  uint32_t mvSum = 0;
  const int readCount = 3;
  for (int i = 0; i < readCount; i++)
  {
    mvSum += analogReadMilliVolts(NOCT_BAT_PIN);
    if (i < readCount - 1)
      delay(2);
  }
  uint32_t mv = mvSum / readCount;

  if (batCtrlPinState_)
  {
    digitalWrite(NOCT_BAT_CTRL_PIN, LOW);
    batCtrlPinState_ = false;
  }

  float voltage =
      (mv * NOCT_BAT_DIVIDER_FACTOR) / 1000.0f + NOCT_BAT_CALIBRATION_OFFSET;

  history_[historyIndex_] = voltage;
  historyIndex_ = (historyIndex_ + 1) % NOCT_BAT_SMOOTHING_SAMPLES;
  if (historyIndex_ == 0)
    historyFilled_ = true;

  float sum = 0.0f;
  int count = historyFilled_ ? NOCT_BAT_SMOOTHING_SAMPLES : historyIndex_;
  for (int i = 0; i < count; i++)
    sum += history_[i];
  return count > 0 ? (sum / count) : voltage;
}

unsigned long BatteryManager::update(AppState &state)
{
  state.batteryVoltage = readVoltage();
  float v = state.batteryVoltage;
  unsigned long nextInterval = NOCT_BAT_READ_INTERVAL_STABLE_MS;

  if (v < 1.0f || v > 5.5f)
  {
    state.batteryPct = 0;
    state.isCharging = false;
  }
  else if (v < 3.5f)
  {
    state.batteryPct = 0;
    state.isCharging = false;
  }
  else
  {
    state.isCharging = (v > NOCT_VOLT_CHARGING);
    if (v < NOCT_VOLT_MIN)
      state.batteryPct = 0;
    else
    {
      int pct = (int)((v - NOCT_VOLT_MIN) / (NOCT_VOLT_MAX - NOCT_VOLT_MIN) *
                      100.0f);
      state.batteryPct = constrain(pct, 0, 100);
    }
    if (state.isCharging)
      nextInterval = NOCT_BAT_READ_INTERVAL_CHARGING_MS;
    else if (state.batteryPct < 20)
      nextInterval = NOCT_BAT_READ_INTERVAL_LOW_MS;
    else
      nextInterval = NOCT_BAT_READ_INTERVAL_STABLE_MS;
  }

  return nextInterval;
}
