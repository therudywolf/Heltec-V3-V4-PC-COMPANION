/*
 * NOCTURNE_OS — BADWOLF USB HID: Chaos & PsyOps.
 * Uses USB.h + USBHIDKeyboard.h + USBHIDMouse.h
 */
#include "UsbManager.h"

// Проверка наличия библиотек.
// Если их нет, код превращается в тыкву (пустышку), чтобы не ломать сборку.
#if __has_include("USBHIDKeyboard.h") && __has_include("USBHIDMouse.h")
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

static USBHIDKeyboard *s_keyboard = nullptr;
static USBHIDMouse *s_mouse = nullptr;
#define HAVE_USB_HID 1
#else
#define HAVE_USB_HID 0
#warning "USB HID libraries not found. BadUSB features disabled."
#endif

UsbManager::UsbManager() {}

void UsbManager::begin() {
#if HAVE_USB_HID
  if (active_)
    return;

  if (!s_keyboard)
    s_keyboard = new USBHIDKeyboard();
  if (!s_mouse)
    s_mouse = new USBHIDMouse();

  if (!s_keyboard || !s_mouse) {
    Serial.println("[BADWOLF] CRITICAL: Alloc failed.");
    return;
  }

  s_keyboard->begin();
  s_mouse->begin();
  delay(300);

  USB.begin();
  delay(1000); // Ждем драйверы

  active_ = true;
  Serial.println("[BADWOLF] HID ARMED. Ready to hunt.");
#else
  Serial.println("[BADWOLF] USB hardware not supported.");
#endif
}

void UsbManager::stop() {
#if HAVE_USB_HID
  if (!active_)
    return;
  if (s_keyboard)
    s_keyboard->end();
  if (s_mouse)
    s_mouse->end();
  active_ = false;
  Serial.println("[BADWOLF] Disarmed.");
#endif
}

#if HAVE_USB_HID
void UsbManager::openRunDialog() {
  if (!s_keyboard)
    return;
  s_keyboard->press(KEY_LEFT_GUI);
  s_keyboard->press('r');
  delay(100);
  s_keyboard->releaseAll();
  delay(BADWOLF_DELAY_AFTER_RUN_MS);
}

void UsbManager::openPowerShell() {
  openRunDialog();
  s_keyboard->print("powershell -WindowStyle Hidden");
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

  openRunDialog();
  s_keyboard->print("powershell");
  delay(200);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  delay(1000);

  s_keyboard->println("color 0a");
  s_keyboard->println("cls");
  const char *msg = "Wake up, Rudy... The System is compromised.";
  for (size_t i = 0; msg[i]; i++) {
    s_keyboard->write((uint8_t)msg[i]);
    delay(random(30, 100));
  }
  delay(800);
  s_keyboard->println("");
  s_keyboard->println("NOCTURNE_OS // INJECTION_COMPLETE");
#endif
}

void UsbManager::runSniffer() {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;
  openPowerShell();
  // Собираем IP, процессы и инфо о системе в файл
  s_keyboard->print(
      "Get-NetIPAddress | Out-File $env:TEMP\\net.txt; Get-Process | "
      "Select-Object Name, CPU | Sort-Object CPU -Descending | Select-Object "
      "-First 10 >> $env:TEMP\\net.txt; notepad $env:TEMP\\net.txt");
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
#endif
}

void UsbManager::runVoice() {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;
  openPowerShell();

  // TTS Injection
  const char *script = "$v = New-Object -ComObject SAPI.SpVoice; "
                       "$v.Volume = 100; "
                       "$v.Rate = 0; "
                       "$v.Speak('Warning. Security breach detected. Bad Wolf "
                       "protocol initiated. Run, Rudy, run.');";

  s_keyboard->print(script);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  Serial.println("[BADWOLF] Voice payload sent.");
#endif
}

void UsbManager::runPoltergeist() {
#if HAVE_USB_HID
  if (!s_keyboard || !s_mouse || !active_)
    return;

  openRunDialog();
  s_keyboard->print("notepad");
  delay(200);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();
  delay(1000);

  s_keyboard->println("DO NOT TOUCH THE MOUSE.");
  s_keyboard->println("I AM IN CONTROL NOW.");
  delay(1000);

  // 10-15 секунд хаоса
  for (int i = 0; i < 30; i++) {
    int x = random(-200, 200);
    int y = random(-200, 200);
    s_mouse->move(x, y);

    if (i % 8 == 0)
      s_keyboard->print(" HELP ");
    if (i % 10 == 0)
      s_mouse->move(0, 0, random(-5, 5)); // Скролл

    delay(200);
  }

  s_keyboard->println("\n\n...System failure imminent.");
  Serial.println("[BADWOLF] Poltergeist finished.");
#endif
}

void UsbManager::runFakeUpdate() {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;

  openRunDialog();
  // Запуск браузера в режиме киоска (на весь экран, без выхода)
  const char *cmd = "start msedge --kiosk \"https://geekprank.com/hacker/\" "
                    "--edge-kiosk-type=fullscreen";

  s_keyboard->print(cmd);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();

  Serial.println("[BADWOLF] Fake Update/Hacker screen launched.");
#endif
}

// RESTORED LEGACY METHOD
void UsbManager::runBackdoor() {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;

  openRunDialog();
  delay(200);

  s_keyboard->print("cmd");
  delay(100);
  s_keyboard->press(KEY_RETURN);
  s_keyboard->releaseAll();

  Serial.println("[BADWOLF] Backdoor (cmd) opened.");
#endif
}

void UsbManager::runDuckyScript(int index) {
#if HAVE_USB_HID
  if (!s_keyboard || !active_)
    return;

  Serial.printf("[BADWOLF] Payload Index: %d\n", index);

  switch (index) {
  case 0:
    runMatrix();
    break;
  case 1:
    runSniffer();
    break;
  case 2:
    runVoice();
    break;
  case 3:
    runPoltergeist();
    break;
  case 4:
    runFakeUpdate();
    break;
  default:
    runMatrix();
    break;
  }
#endif
}