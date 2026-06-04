/*
 * NOCTURNE_OS — BadUsb: USB-HID keyboard injection (Rubber-Ducky-lite).
 *
 * The ESP32-S3 enumerates as a composite CDC + HID device (the USBHIDKeyboard
 * global registers HID before the CDC-on-boot USB.begin()), so the serial debug
 * console survives. A small Ducky-lite script drives keystrokes:
 *   REM <comment>        STRING <text>        DELAY <ms>
 *   ENTER / TAB / ESC / DEL / UP / DOWN / LEFT / RIGHT
 *   GUI <k> / CTRL <k> / ALT <k> / SHIFT <k> (and multi-modifier combos)
 *
 * It fires on ANY host it's plugged into, so the UI requires a deliberate HOLD
 * to run. Compiled only where NOCT_FEATURE_BADUSB is set (hacker + multi).
 */
#ifndef NOCTURNE_BADUSB_H
#define NOCTURNE_BADUSB_H

#include "nocturne/config.h"
#if NOCT_FEATURE_BADUSB
#include <Arduino.h>

class BadUsb {
public:
  void begin();                       // init HID (composite with the CDC console)
  int payloadCount() const;
  const char *payloadName(int i) const;
  void run(int i);                    // execute payload i (blocking; has DELAYs)

  bool isReady() const { return ready_; }
  bool busy() const { return busy_; }
  int lastRunIdx() const { return lastRunIdx_; }
  int lastSteps() const { return lastSteps_; }

private:
  const char *scriptFor(int i) const;
  void execScript(const char *s);

  bool ready_ = false;
  bool busy_ = false;
  int lastRunIdx_ = -1;
  int lastSteps_ = 0;
};

#endif // NOCT_FEATURE_BADUSB
#endif
