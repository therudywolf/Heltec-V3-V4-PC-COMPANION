/*
 * NOCTURNE_OS — Input: button events (SHORT/LONG/DOUBLE), interval timer,
 * non-blocking timer. Used by main loop.
 */
#ifndef NOCTURNE_INPUT_HANDLER_H
#define NOCTURNE_INPUT_HANDLER_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Input: Event-based (SHORT = navigate, LONG = select, DOUBLE = back/menu)
// ---------------------------------------------------------------------------
enum ButtonEvent
{
  EV_NONE,
  EV_SHORT,
  EV_LONG,
  EV_DOUBLE
};

struct InputSystem
{
  const int pin;
  unsigned long pressTime = 0;
  unsigned long releaseTime = 0;
  bool btnState = false;
  int clickCount = 0;

  InputSystem(int p) : pin(p) { pinMode(pin, INPUT_PULLUP); }

  ButtonEvent update()
  {
    bool down = (digitalRead(pin) == LOW);
    unsigned long now = millis();
    ButtonEvent event = EV_NONE;

    if (down && !btnState)
    {
      btnState = true;
      pressTime = now;
    }
    else if (!down && btnState)
    {
      btnState = false;
      unsigned long duration = now - pressTime;
      if (duration > 50 && duration < 500)
      {
        clickCount++;
        releaseTime = now;
        if (clickCount >= 4)
        {
          clickCount = 0;
          return EV_DOUBLE;
        }
      }
      else if (duration >= 500)
      {
        clickCount = 0;
        return EV_LONG;
      }
    }

    const unsigned long multiTapWindowMs = 300;
    if (clickCount > 0 && !btnState && (now - releaseTime > multiTapWindowMs))
    {
      if (clickCount == 1)
        event = EV_SHORT;
      else if (clickCount >= 2)
        event = EV_DOUBLE;
      clickCount = 0;
    }
    return event;
  }
};

struct IntervalTimer
{
  unsigned long intervalMs;
  unsigned long lastMs = 0;
  IntervalTimer(unsigned long interval) : intervalMs(interval) {}
  bool check(unsigned long now)
  {
    if (now - lastMs >= intervalMs)
    {
      lastMs = now;
      return true;
    }
    return false;
  }
  void reset(unsigned long now) { lastMs = now; }
};

class NonBlockingTimer
{
  unsigned long targetTime_;
  bool active_;

public:
  NonBlockingTimer() : targetTime_(0), active_(false) {}
  void start(unsigned long durationMs)
  {
    targetTime_ = millis() + durationMs;
    active_ = true;
  }
  bool isReady() const { return active_ && (millis() >= targetTime_); }
  void reset()
  {
    active_ = false;
    targetTime_ = 0;
  }
  bool isActive() const { return active_; }
  unsigned long remaining() const
  {
    if (!active_)
      return 0;
    unsigned long now = millis();
    return (now >= targetTime_) ? 0 : (targetTime_ - now);
  }
};

#endif
