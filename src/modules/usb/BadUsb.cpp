#include "modules/usb/BadUsb.h"
#if NOCT_FEATURE_BADUSB

#include <USB.h>
#include <USBHIDKeyboard.h>
#include <LittleFS.h>
#include <string.h>
#include <strings.h> // strcasecmp / strncasecmp
#include <stdlib.h>  // atoi

// Global so its constructor registers the HID interface BEFORE the CDC-on-boot
// USB.begin() runs — the device then enumerates as composite CDC + HID.
static USBHIDKeyboard kbd;

struct Payload { const char *name; const char *script; };
static const Payload kBuiltins[] = {
    {"Notepad demo",
     "REM open notepad and type a marker\n"
     "GUI r\nDELAY 500\nSTRING notepad\nENTER\nDELAY 900\n"
     "STRING Hello from NOCTURNE OS // BadUSB\n"},
    {"Lock screen", "REM Win+L\nGUI l\n"},
};
static const int kBuiltinCount = (int)(sizeof(kBuiltins) / sizeof(kBuiltins[0]));

// Optional user payload dropped at /badusb/payload.txt on LittleFS.
static String s_custom;

void BadUsb::begin() {
  kbd.begin();
  ready_ = true;
  if (LittleFS.begin(true) && LittleFS.exists("/badusb/payload.txt")) {
    File f = LittleFS.open("/badusb/payload.txt", "r");
    if (f) { s_custom = f.readString(); f.close(); }
  }
  Serial.println("[USB] BadUSB HID ready (composite CDC+HID)");
}

int BadUsb::payloadCount() const {
  return kBuiltinCount + (s_custom.length() > 0 ? 1 : 0);
}

const char *BadUsb::payloadName(int i) const {
  if (i >= 0 && i < kBuiltinCount) return kBuiltins[i].name;
  if (i == kBuiltinCount && s_custom.length() > 0) return "Custom (FS)";
  return "?";
}

const char *BadUsb::scriptFor(int i) const {
  if (i >= 0 && i < kBuiltinCount) return kBuiltins[i].script;
  if (i == kBuiltinCount && s_custom.length() > 0) return s_custom.c_str();
  return nullptr;
}

// Map a Ducky-lite token to a HID keycode (0 = unknown). Single chars pass
// through as their ASCII value (USBHIDKeyboard accepts that).
static uint8_t tokenKey(const char *t) {
  if (!strcasecmp(t, "ENTER") || !strcasecmp(t, "RETURN")) return KEY_RETURN;
  if (!strcasecmp(t, "TAB")) return KEY_TAB;
  if (!strcasecmp(t, "ESC") || !strcasecmp(t, "ESCAPE")) return KEY_ESC;
  if (!strcasecmp(t, "DEL") || !strcasecmp(t, "DELETE")) return KEY_DELETE;
  if (!strcasecmp(t, "SPACE")) return ' ';
  if (!strcasecmp(t, "UP")) return KEY_UP_ARROW;
  if (!strcasecmp(t, "DOWN")) return KEY_DOWN_ARROW;
  if (!strcasecmp(t, "LEFT")) return KEY_LEFT_ARROW;
  if (!strcasecmp(t, "RIGHT")) return KEY_RIGHT_ARROW;
  if (!strcasecmp(t, "GUI") || !strcasecmp(t, "WIN") || !strcasecmp(t, "WINDOWS"))
    return KEY_LEFT_GUI;
  if (!strcasecmp(t, "CTRL") || !strcasecmp(t, "CONTROL")) return KEY_LEFT_CTRL;
  if (!strcasecmp(t, "ALT")) return KEY_LEFT_ALT;
  if (!strcasecmp(t, "SHIFT")) return KEY_LEFT_SHIFT;
  if (strlen(t) == 1) return (uint8_t)t[0]; // single character
  return 0;
}

void BadUsb::execScript(const char *s) {
  if (!s) return;
  char line[160];
  int li = 0;
  for (const char *p = s;; p++) {
    if (*p != '\n' && *p != '\0') {
      if (li < (int)sizeof(line) - 1) line[li++] = *p;
      continue;
    }
    line[li] = '\0';
    // process one line
    char *L = line;
    while (*L == ' ' || *L == '\t') L++;
    if (L[0] && strncasecmp(L, "REM", 3) != 0) {
      if (!strncasecmp(L, "STRING ", 7)) {
        kbd.print(L + 7);
      } else if (!strncasecmp(L, "DELAY ", 6)) {
        delay((unsigned)atoi(L + 6));
      } else {
        // key / modifier-combo line: press all tokens, then release.
        int pressed = 0;
        char *tok = strtok(L, " \t");
        while (tok) {
          uint8_t k = tokenKey(tok);
          if (k) { kbd.press(k); pressed++; delay(8); }
          tok = strtok(nullptr, " \t");
        }
        if (pressed) { delay(20); kbd.releaseAll(); }
      }
      lastSteps_++;
    }
    li = 0;
    if (*p == '\0') break;
  }
}

void BadUsb::run(int i) {
  if (!ready_ || busy_) return;
  const char *s = scriptFor(i);
  if (!s) return;
  busy_ = true;
  lastRunIdx_ = i;
  lastSteps_ = 0;
  Serial.printf("[USB] running payload %d: %s\n", i, payloadName(i));
  execScript(s);
  busy_ = false;
}

#endif // NOCT_FEATURE_BADUSB
