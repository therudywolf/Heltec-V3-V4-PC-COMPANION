/*
 * NOCTURNE_OS — BADWOLF USB HID Keyboard payload injection.
 * Requires ESP32-S3 (or board with SOC_USB_OTG_SUPPORTED).
 */
#pragma once
#include <Arduino.h>

#define BADWOLF_DELAY_AFTER_RUN_MS 1000
#define BADWOLF_DELAY_AFTER_POWERSHELL_MS 1000
#define BADWOLF_MATRIX_CHAR_DELAY_MS 80

class UsbManager {
public:
  UsbManager();
  /** Initialize USB and HID Keyboard (call when entering BADWOLF mode). */
  void begin();
  /** De-initialize Keyboard (call on triple-click exit). */
  void stop();
  bool isActive() const { return active_; }

  void runMatrix();
  void runSniffer();
  /** Run Ducky-style script by index (0=Matrix, 1=Sniffer, 2=CMD, 3=Process
   * list, 4=Backdoor). */
  void runDuckyScript(int index);
  /** Minimal backdoor: Win+R -> cmd -> Enter. */
  void runBackdoor();
  static constexpr int DUCKY_SCRIPT_COUNT = 5;

private:
  bool active_ = false;
  void openPowerShell();
  void ensureEnglishLayout();
};
