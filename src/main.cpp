/*
 * NOCTURNE_OS — Firmware entry (ESP32 Heltec).
 * Loop: NetManager.tick (TCP/JSON) -> SceneManager.draw (current scene) ->
 * sendBuffer. Button: short = next scene / menu; long = predator (screen off,
 * LED breath). Config: NOCT_* in config.h; WiFi/PC in secrets.h.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>

// --- FORCE DEPENDENCY VISIBILITY ---
// Required here so LDF finds them for the sub-modules
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <NimBLEDevice.h>

// -----------------------------------

#include "modules/BeaconManager.h"
#include "modules/BleManager.h"
#include "modules/BootAnim.h"
#include "modules/DisplayEngine.h"
#include "modules/KickManager.h"
#include "modules/MdnsManager.h"
#include "modules/NetManager.h"
#include "modules/NetworkScanManager.h"
#include "modules/SceneManager.h"
#include "modules/TrapManager.h"
#include "modules/UsbManager.h"
#include "modules/VaultManager.h"
#include "modules/WifiAttackManager.h"
#include "modules/WifiSniffManager.h"
#include "nocturne/Types.h"
#include "nocturne/config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Local constants (after includes, before any global object instantiations)
// ---------------------------------------------------------------------------
#define NOCT_CONFIG_MSG_MS 1500
#define NOCT_BAT_READ_INTERVAL_MS 5000
#define NOCT_BAT_SAMPLES 20
#define NOCT_BAT_CALIBRATION 1.1f
// Flipper-style: level 0 = 4 categories, level 1 = submenu items
#define MENU_CATEGORIES 4
#define WOLF_MENU_ITEMS 12

// ---------------------------------------------------------------------------
// Input: Event-based (SHORT = navigate, LONG = select, DOUBLE = back/menu)
// ---------------------------------------------------------------------------
enum ButtonEvent { EV_NONE, EV_SHORT, EV_LONG, EV_DOUBLE };

struct InputSystem {
  const int pin;
  unsigned long pressTime = 0;
  unsigned long releaseTime = 0;
  bool btnState = false;
  int clickCount = 0;

  InputSystem(int p) : pin(p) { pinMode(pin, INPUT_PULLUP); }

  ButtonEvent update() {
    bool down = (digitalRead(pin) == LOW);
    unsigned long now = millis();
    ButtonEvent event = EV_NONE;

    if (down && !btnState) {
      btnState = true;
      pressTime = now;
    } else if (!down && btnState) {
      btnState = false;
      unsigned long duration = now - pressTime;
      if (duration > 50 && duration < 500) {
        clickCount++;
        releaseTime = now;
        // Do NOT return here for 2 clicks — wait for multi-tap window to expire
        if (clickCount >= 4) {
          clickCount = 0;
          return EV_DOUBLE;
        }
      } else if (duration >= 500) {
        clickCount = 0;
        return EV_LONG;
      }
    }

    const unsigned long multiTapWindowMs = 300;
    if (clickCount > 0 && !btnState && (now - releaseTime > multiTapWindowMs)) {
      if (clickCount == 1)
        event = EV_SHORT;
      else if (clickCount >= 2)
        event = EV_DOUBLE; // Double-click = 2 (or more) short presses in window
      clickCount = 0;
    }
    return event;
  }
};

struct IntervalTimer {
  unsigned long intervalMs;
  unsigned long lastMs = 0;
  IntervalTimer(unsigned long interval) : intervalMs(interval) {}
  bool check(unsigned long now) {
    if (now - lastMs >= intervalMs) {
      lastMs = now;
      return true;
    }
    return false;
  }
  void reset(unsigned long now) { lastMs = now; }
};

// Non-blocking timer for replacing delay() calls
class NonBlockingTimer {
  unsigned long targetTime_;
  bool active_;

public:
  NonBlockingTimer() : targetTime_(0), active_(false) {}
  void start(unsigned long durationMs) {
    targetTime_ = millis() + durationMs;
    active_ = true;
  }
  bool isReady() const { return active_ && (millis() >= targetTime_); }
  void reset() {
    active_ = false;
    targetTime_ = 0;
  }
  bool isActive() const { return active_; }
  unsigned long remaining() const {
    if (!active_)
      return 0;
    unsigned long now = millis();
    return (now >= targetTime_) ? 0 : (targetTime_ - now);
  }
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
DisplayEngine display(NOCT_RST_PIN, NOCT_SDA_PIN, NOCT_SCL_PIN);
NetManager netManager;
BleManager bleManager;
UsbManager usbManager;
TrapManager trapManager;
KickManager kickManager;
BeaconManager beaconManager;
WifiSniffManager wifiSniffManager;
WifiAttackManager wifiAttackManager;
NetworkScanManager networkScanManager;
MdnsManager mdnsManager;
VaultManager vaultManager;
AppState state;
SceneManager sceneManager(display, state);

InputSystem input(NOCT_BUTTON_PIN);
IntervalTimer guiTimer(NOCT_REDRAW_INTERVAL_MS);
IntervalTimer
    batTimer(NOCT_BAT_READ_INTERVAL_STABLE_MS); // Will be updated adaptively

Settings &settings = state.settings;
unsigned long bootTime = 0;
unsigned long splashStart = 0;
bool splashDone = false;
int currentScene = 0;
int previousScene = 0;
unsigned long transitionStart = 0;
bool inTransition = false;

unsigned long lastCarousel = 0;
unsigned long lastFanAnim = 0;
static unsigned long idleStateEnteredMs = 0; /* For 10s -> wolf screensaver */
int fanAnimFrame = 0;
bool blinkState = false;
int alertBlinkCounter = 0;
bool lastAlertActive = false;
unsigned long lastBlink = 0;

bool predatorMode = false;
unsigned long predatorEnterTime = 0;

bool quickMenuOpen = false;
int quickMenuItem = 0;
int menuLevel = 0;    // 0 = categories (Config/WiFi/Tools/System), 1 = submenu
int menuCategory = 0; // which category when in submenu (0..3)

enum MenuState { MENU_MAIN };
MenuState menuState = MENU_MAIN;
int submenuIndex =
    0; // Оставлено для совместимости с drawMenu, но не используется
bool rebootConfirmed = false;
unsigned long rebootConfirmTime = 0;

// Защита от множественных срабатываний событий
unsigned long lastMenuEventTime = 0;
#define MENU_EVENT_DEBOUNCE_MS 150

// Toast: brief on-screen message (FAIL on mode error, Saved on Config save)
static char toastMsg[20] = "";
static unsigned long toastUntil = 0;

// --- Cyberdeck Modes --- Full Marauder integration
enum AppMode {
  MODE_NORMAL,
  MODE_DAEMON,
  // WiFi Scans
  MODE_RADAR,                 // AP Scan (existing)
  MODE_WIFI_PROBE_SCAN,       // Probe requests
  MODE_WIFI_EAPOL_SCAN,       // EAPOL handshake capture
  MODE_WIFI_STATION_SCAN,     // Station scan (existing)
  MODE_WIFI_PACKET_MONITOR,   // Packet Monitor (existing)
  MODE_WIFI_CHANNEL_ANALYZER, // Channel Analyzer
  MODE_WIFI_CHANNEL_ACTIVITY, // Channel Activity
  MODE_WIFI_PACKET_RATE,      // Packet Rate
  MODE_WIFI_PINESCAN,         // Pineapple detection
  MODE_WIFI_MULTISSID,        // MultiSSID detection
  MODE_WIFI_SIGNAL_STRENGTH,  // Signal Strength scan
  MODE_WIFI_RAW_CAPTURE,      // Raw packet capture
  MODE_WIFI_AP_STA,           // AP + Station scan
  MODE_WIFI_SNIFF,            // Basic sniff (existing)
  // WiFi Attacks
  MODE_WIFI_DEAUTH,               // Deauth (existing)
  MODE_WIFI_DEAUTH_TARGETED,      // Targeted deauth
  MODE_WIFI_DEAUTH_MANUAL,        // Manual deauth
  MODE_WIFI_BEACON,               // Beacon spam (existing)
  MODE_WIFI_BEACON_RICKROLL,      // Rick Roll beacon
  MODE_WIFI_BEACON_LIST,          // Beacon list spam
  MODE_WIFI_BEACON_FUNNY,         // Funny beacon
  MODE_WIFI_AUTH_ATTACK,          // Auth attack
  MODE_WIFI_MIMIC_FLOOD,          // Mimic flood
  MODE_WIFI_AP_SPAM,              // AP spam
  MODE_WIFI_BAD_MESSAGE,          // Bad message attack
  MODE_WIFI_BAD_MESSAGE_TARGETED, // Bad message targeted
  MODE_WIFI_SLEEP_ATTACK,         // Sleep attack
  MODE_WIFI_SLEEP_TARGETED,       // Sleep targeted
  MODE_WIFI_SAE_COMMIT,           // SAE commit attack
  MODE_WIFI_TRAP,                 // Evil Portal (existing)
  // BLE Scans
  MODE_BLE_SCAN,            // Basic scan (existing)
  MODE_BLE_SCAN_SKIMMERS,   // Skimmers scan
  MODE_BLE_SCAN_AIRTAG,     // AirTag scan
  MODE_BLE_SCAN_AIRTAG_MON, // AirTag monitor
  MODE_BLE_SCAN_FLIPPER,    // Flipper scan
  MODE_BLE_SCAN_FLOCK,      // Flock detection
  MODE_BLE_SCAN_ANALYZER,   // BLE Analyzer
  MODE_BLE_SCAN_SIMPLE,     // Simple scan
  MODE_BLE_SCAN_SIMPLE_TWO, // Simple scan variant
  // BLE Attacks
  MODE_BLE_SPAM,                // Basic spam (existing)
  MODE_BLE_SOUR_APPLE,          // Sour Apple (existing)
  MODE_BLE_SWIFTPAIR_MICROSOFT, // SwiftPair Microsoft
  MODE_BLE_SWIFTPAIR_GOOGLE,    // SwiftPair Google
  MODE_BLE_SWIFTPAIR_SAMSUNG,   // SwiftPair Samsung
  MODE_BLE_SPAM_ALL,            // Spam all types
  MODE_BLE_FLIPPER_SPAM,        // Flipper spam
  MODE_BLE_AIRTAG_SPOOF,        // AirTag spoof
  // Network Scans
  MODE_NETWORK_ARP_SCAN,    // ARP scan
  MODE_NETWORK_PORT_SCAN,   // Port scan
  MODE_NETWORK_PING_SCAN,   // Ping scan
  MODE_NETWORK_DNS_SCAN,    // DNS scan
  MODE_NETWORK_HTTP_SCAN,   // HTTP scan
  MODE_NETWORK_HTTPS_SCAN,  // HTTPS scan
  MODE_NETWORK_SMTP_SCAN,   // SMTP scan
  MODE_NETWORK_RDP_SCAN,    // RDP scan
  MODE_NETWORK_TELNET_SCAN, // Telnet scan
  MODE_NETWORK_SSH_SCAN,    // SSH scan
  // Tools
  MODE_BADWOLF,    // USB HID (existing)
  MODE_VAULT,      // TOTP Vault (existing)
  MODE_FAKE_LOGIN, // Fake Login (existing)
  MODE_QR,         // QR code (existing)
  MODE_MDNS        // mDNS spoof (existing)
};
AppMode currentMode = MODE_NORMAL;

// --- Netrunner WiFi Scanner ---
int wifiScanSelected = 0;
int wifiListPage = 0;
int wifiSortMode = 0;      // 0=RSSI desc, 1=alphabetical, 2=RSSI asc
int wifiRssiFilter = -100; // Минимальный RSSI для отображения (-100 = все)
int wifiSortedIndices[32]; // Индексы отсортированных сетей
int wifiFilteredCount = 0; // Количество отфильтрованных сетей
// DEAUTH: индекс скана выбранной цели (-1 = ещё не выбирали долгим нажатием).
static int lastDeauthTargetScanIndex = -1;
static int wifiSniffSelected = 0;
static int bleScanSelected = 0;
static int badWolfScriptIndex = 0;

// Функция сортировки и фильтрации WiFi сетей
static void sortAndFilterWiFiNetworks() {
  // Статические переменные для кэширования
  static int lastScanCount = -1;
  static int lastSortMode = -1;
  static int lastRssiFilter = -101;

  int n = WiFi.scanComplete();
  if (n <= 0 || n > 32) {
    wifiFilteredCount = 0;
    lastScanCount = -1;
    return;
  }

  // Optimized caching: пересчитываем только если изменились данные
  // Also check if scan is still valid (not expired)
  static unsigned long lastScanTime = 0;
  unsigned long now = millis();
  const unsigned long SCAN_CACHE_TIMEOUT_MS =
      30000; // Cache valid for 30 seconds

  if (n == lastScanCount && wifiSortMode == lastSortMode &&
      wifiRssiFilter == lastRssiFilter && wifiFilteredCount > 0 &&
      (now - lastScanTime < SCAN_CACHE_TIMEOUT_MS)) {
    return; // Данные не изменились и cache is still valid, используем кэш
  }

  lastScanTime = now;

  lastScanCount = n;
  lastSortMode = wifiSortMode;
  lastRssiFilter = wifiRssiFilter;

  // Инициализация индексов
  for (int i = 0; i < n; i++) {
    wifiSortedIndices[i] = i;
  }

  // Фильтрация по RSSI
  int filtered = 0;
  for (int i = 0; i < n; i++) {
    if (WiFi.RSSI(i) >= wifiRssiFilter) {
      wifiSortedIndices[filtered++] = i;
    }
  }
  wifiFilteredCount = filtered;

  // Optimized sorting: Insertion sort for small arrays (<32 items), more
  // efficient than bubble sort
  if (wifiSortMode == 0) {
    // По RSSI (убывание) - Insertion sort
    for (int i = 1; i < filtered; i++) {
      int key = wifiSortedIndices[i];
      int keyRssi = WiFi.RSSI(key);
      int j = i - 1;
      while (j >= 0 && WiFi.RSSI(wifiSortedIndices[j]) < keyRssi) {
        wifiSortedIndices[j + 1] = wifiSortedIndices[j];
        j--;
      }
      wifiSortedIndices[j + 1] = key;
    }
  } else if (wifiSortMode == 1) {
    // По алфавиту - Insertion sort with cached SSID comparisons (optimized: use
    // c_str())
    static char ssidBuf1[33],
        ssidBuf2[33]; // Max SSID length is 32 + null terminator
    for (int i = 1; i < filtered; i++) {
      int key = wifiSortedIndices[i];
      strncpy(ssidBuf1, WiFi.SSID(key).c_str(), sizeof(ssidBuf1) - 1);
      ssidBuf1[sizeof(ssidBuf1) - 1] = '\0';
      int j = i - 1;
      while (j >= 0) {
        strncpy(ssidBuf2, WiFi.SSID(wifiSortedIndices[j]).c_str(),
                sizeof(ssidBuf2) - 1);
        ssidBuf2[sizeof(ssidBuf2) - 1] = '\0';
        if (strcmp(ssidBuf2, ssidBuf1) > 0) {
          wifiSortedIndices[j + 1] = wifiSortedIndices[j];
          j--;
        } else {
          break;
        }
      }
      wifiSortedIndices[j + 1] = key;
    }
  } else if (wifiSortMode == 2) {
    // По RSSI (возрастание) - Insertion sort
    for (int i = 1; i < filtered; i++) {
      int key = wifiSortedIndices[i];
      int keyRssi = WiFi.RSSI(key);
      int j = i - 1;
      while (j >= 0 && WiFi.RSSI(wifiSortedIndices[j]) > keyRssi) {
        wifiSortedIndices[j + 1] = wifiSortedIndices[j];
        j--;
      }
      wifiSortedIndices[j + 1] = key;
    }
  }
}

static bool needRedraw = true;

// Battery reading optimization: moving average buffer
static float batteryVoltageHistory[NOCT_BAT_SMOOTHING_SAMPLES] = {0};
static int batteryHistoryIndex = 0;
static bool batteryHistoryFilled = false;
static bool batCtrlPinState =
    false; // Track GPIO 37 state to avoid unnecessary writes
static bool vextPinState = false; // Track Vext pin (never drive HIGH)

/** Get battery voltage from ADC (GPIO 1). Heltec V4: divider factor 4.9
 * (390k/100k resistors). Requires GPIO 37 (ADC control) to be HIGH to enable
 * divider. Optimized: only enables divider when needed, uses multiple readings
 * for accuracy.
 */
static float getBatteryVoltage() {
  // Step 1: Enable voltage divider by setting GPIO 37 to HIGH (open transistor)
  // Only change state if needed (optimization: avoid unnecessary GPIO writes)
  if (!batCtrlPinState) {
    pinMode(NOCT_BAT_CTRL_PIN, OUTPUT);
    digitalWrite(NOCT_BAT_CTRL_PIN, HIGH);
    batCtrlPinState = true;
    delay(
        10); // Wait for voltage to stabilize (acceptable: called infrequently)
  }

  // Step 2: Read voltage multiple times and average for accuracy
  uint32_t mvSum = 0;
  const int readCount = 3; // Read 3 times for better accuracy
  for (int i = 0; i < readCount; i++) {
    mvSum += analogReadMilliVolts(NOCT_BAT_PIN);
    if (i < readCount - 1) {
      delay(2); // Small delay between readings (acceptable: very short)
    }
  }
  uint32_t mv = mvSum / readCount;

  // Step 3: Disable divider by setting GPIO 37 to LOW (close transistor)
  // This prevents battery drain through the divider circuit
  if (batCtrlPinState) {
    digitalWrite(NOCT_BAT_CTRL_PIN, LOW);
    batCtrlPinState = false;
  }

  // Step 4: Calculate actual battery voltage: multiply by divider factor 4.9
  float voltage = (mv * NOCT_BAT_DIVIDER_FACTOR) / 1000.0f;

  // Step 5: Add to moving average buffer
  batteryVoltageHistory[batteryHistoryIndex] = voltage;
  batteryHistoryIndex = (batteryHistoryIndex + 1) % NOCT_BAT_SMOOTHING_SAMPLES;
  if (batteryHistoryIndex == 0) {
    batteryHistoryFilled = true;
  }

  // Step 6: Calculate smoothed average
  float sum = 0.0f;
  int count =
      batteryHistoryFilled ? NOCT_BAT_SMOOTHING_SAMPLES : batteryHistoryIndex;
  for (int i = 0; i < count; i++) {
    sum += batteryVoltageHistory[i];
  }
  float smoothedVoltage = sum / count;

  // Debug output
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    Serial.printf("[BAT] Raw_mV: %d | Calculated_V: %.2f | Smoothed_V: %.2f\n",
                  mv, voltage, smoothedVoltage);
    lastDebug = millis();
  }

  return smoothedVoltage;
}

/** Update state.batteryVoltage, state.batteryPct, state.isCharging.
 * Uses adaptive reading intervals based on battery state.
 * Returns the next reading interval in milliseconds.
 */
static unsigned long updateBatteryState() {
  state.batteryVoltage = getBatteryVoltage();
  float v = state.batteryVoltage;

  // Determine next reading interval based on battery state
  unsigned long nextInterval = NOCT_BAT_READ_INTERVAL_STABLE_MS;

  // Определение статуса батареи
  // Если напряжение очень низкое (< 1.0V) или очень высокое (> 5.5V),
  // батарея не подключена или некорректное значение ADC
  // При работе от USB без аккумулятора напряжение может быть в
  // диапазоне 3.3-4.2V Но если напряжение стабильно в диапазоне 3.3-3.5V, это
  // может быть USB питание
  if (v < 1.0f || v > 5.5f) {
    // Батарея не подключена или некорректное значение ADC
    state.batteryPct = 0;
    state.isCharging = false;
    nextInterval =
        NOCT_BAT_READ_INTERVAL_STABLE_MS; // Check less frequently if invalid
  } else {
    // Проверка на USB питание без аккумулятора
    // При работе от USB без аккумулятора напряжение может быть стабильным в
    // диапазоне 3.3-4.2V Но если напряжение близко к 3.3V или ниже 3.5V, это
    // может быть USB питание Для простоты считаем, что если напряжение в
    // диапазоне 3.3-3.5V, это USB питание без аккумулятора
    if (v < 3.5f) {
      // Напряжение ниже 3.5V - возможно USB питание без аккумулятора
      // Показываем 0% и не показываем зарядку
      state.batteryPct = 0;
      state.isCharging = false;
      nextInterval = NOCT_BAT_READ_INTERVAL_STABLE_MS; // USB power is stable
    } else {
      // Батарея подключена
      // Если напряжение > 4.4V, батарея заряжается
      state.isCharging = (v > NOCT_VOLT_CHARGING);

      // Расчет процента заряда: 3.3V = 0%, 4.2V = 100%
      // Если напряжение ниже 3.3V, считаем что батарея разряжена
      if (v < NOCT_VOLT_MIN) {
        state.batteryPct = 0;
      } else {
        int pct = (int)((v - NOCT_VOLT_MIN) / (NOCT_VOLT_MAX - NOCT_VOLT_MIN) *
                        100.0f);
        state.batteryPct = constrain(pct, 0, 100);
      }

      // Adaptive reading interval based on battery state
      if (state.isCharging) {
        // Charging: read more frequently to track charge progress
        nextInterval = NOCT_BAT_READ_INTERVAL_CHARGING_MS;
      } else if (state.batteryPct < 20) {
        // Low battery: read more frequently to monitor discharge
        nextInterval = NOCT_BAT_READ_INTERVAL_LOW_MS;
      } else {
        // Stable: read less frequently to save power
        nextInterval = NOCT_BAT_READ_INTERVAL_STABLE_MS;
      }
    }

    // Debug output: Raw_mV | Calculated_V | Pct | NextInterval
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 5000) {
      // Calculate raw mV from voltage (reverse calculation)
      uint32_t raw_mv = (uint32_t)(v * 1000.0f / NOCT_BAT_DIVIDER_FACTOR);
      Serial.printf(
          "[BAT] Raw_mV: %d | Calculated_V: %.2f | Pct: %d%% | Next: %lums\n",
          raw_mv, v, state.batteryPct, nextInterval);
      lastDebug = millis();
    }
  }

  return nextInterval;
}

static void VextON() {
  // Vext (NOCT_VEXT_PIN): LOW = OLED powered, HIGH = off. Never drive HIGH.
  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  delay(100);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  bootTime = millis();
  Serial.begin(115200);
  Serial.println("[NOCT] Nocturne OS " NOCTURNE_VERSION);
  setCpuFrequencyMhz(240); // V4: max speed (also set in platformio.ini)

  // ========================================================================
  // Power-On Kick: Vext (36) LOW = power OLED. RST (21) pulse, then begin().
  // Do NOT use GPIO 18 as RST — 18 is SCL (I2C). Standard V4: Vext=36, RST=21.
  // ========================================================================
  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  vextPinState = true;
  delay(100);

  pinMode(NOCT_RST_PIN, OUTPUT);
  digitalWrite(NOCT_RST_PIN, LOW);
  delay(50);
  digitalWrite(NOCT_RST_PIN, HIGH);
  delay(50);

  display.begin();
  delay(100);

  if (digitalRead(NOCT_VEXT_PIN) != LOW)
    digitalWrite(NOCT_VEXT_PIN, LOW);
  delay(50);
  drawBootSequence(display);
  splashDone = true;
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
  delay(200);
  digitalWrite(NOCT_LED_ALERT_PIN, LOW);

  // Настройка ADC для чтения батареи (GPIO 1 на ESP32-S3)
  // На ESP32-S3 GPIO 1 соответствует ADC1_CH0
  analogReadResolution(
      12); // 12-bit resolution (0-4095)
// Устанавливаем максимальное затухание для измерения до ~3.1V (с учетом
// делителя 1/2 это ~6.2V) Для ESP32-S3 используем analogSetPinAttenuation, для
// других версий - analogSetAttenuation
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
           // ESP32-S3: используем analogSetPinAttenuation с константой ADC_11db
  analogSetPinAttenuation(NOCT_BAT_PIN, ADC_11db);
#else
           // Для других версий ESP32
  analogSetAttenuation(ADC_11db);
#endif

  // Первое чтение для инициализации ADC
  delay(100);
  analogRead(NOCT_BAT_PIN);
  delay(50);

  pinMode(NOCT_BUTTON_PIN, INPUT_PULLUP);

  Preferences prefs;
  prefs.begin("nocturne", true);
  settings.ledEnabled = prefs.getBool("led", true);
  settings.carouselEnabled = prefs.getBool("carousel", false);
  settings.carouselIntervalSec = prefs.getInt("carouselSec", 10);
  if (settings.carouselIntervalSec != 5 && settings.carouselIntervalSec != 10 &&
      settings.carouselIntervalSec != 15)
    settings.carouselIntervalSec = 10;
  settings.displayContrast = prefs.getInt("contrast", 128);
  if (settings.displayContrast > 255)
    settings.displayContrast = 255;
  if (settings.displayContrast < 0)
    settings.displayContrast = 0;
  settings.displayInverted = prefs.getBool("inverted", true);
  settings.glitchEnabled = prefs.getBool("glitch", false);
  settings.lowBrightnessDefault = prefs.getBool("lowBright", false);
  prefs.end();

  {
    uint8_t contrast =
        settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
    display.u8g2().setContrast(contrast);
  }
  display.setScreenFlipped(settings.displayInverted);
  randomSeed(esp_random());

  netManager.begin(WIFI_SSID, WIFI_PASS);
  netManager.setServer(PC_IP, TCP_PORT);
  vaultManager.begin();

  updateBatteryState();
}

// ---------------------------------------------------------------------------
// Mode Management System - единая система управления режимами
// ---------------------------------------------------------------------------

/** Очистка ресурсов режима перед переключением */
static void cleanupMode(AppMode mode) {
  switch (mode) {
  // WiFi Scans
  case MODE_RADAR:
  case MODE_WIFI_PROBE_SCAN:
  case MODE_WIFI_EAPOL_SCAN:
  case MODE_WIFI_STATION_SCAN:
  case MODE_WIFI_PACKET_MONITOR:
  case MODE_WIFI_CHANNEL_ANALYZER:
  case MODE_WIFI_CHANNEL_ACTIVITY:
  case MODE_WIFI_PACKET_RATE:
  case MODE_WIFI_PINESCAN:
  case MODE_WIFI_MULTISSID:
  case MODE_WIFI_SIGNAL_STRENGTH:
  case MODE_WIFI_RAW_CAPTURE:
  case MODE_WIFI_AP_STA:
  case MODE_WIFI_SNIFF:
    wifiSniffManager.stop();
    WiFi.scanDelete();
    break;
  // WiFi Attacks
  case MODE_WIFI_DEAUTH:
  case MODE_WIFI_DEAUTH_TARGETED:
  case MODE_WIFI_DEAUTH_MANUAL:
    kickManager.stopAttack();
    lastDeauthTargetScanIndex = -1;
    WiFi.scanDelete();
    break;
  case MODE_WIFI_BEACON:
  case MODE_WIFI_BEACON_RICKROLL:
  case MODE_WIFI_BEACON_LIST:
  case MODE_WIFI_BEACON_FUNNY:
    beaconManager.stop();
    break;
  case MODE_WIFI_AUTH_ATTACK:
  case MODE_WIFI_MIMIC_FLOOD:
  case MODE_WIFI_AP_SPAM:
  case MODE_WIFI_BAD_MESSAGE:
  case MODE_WIFI_BAD_MESSAGE_TARGETED:
  case MODE_WIFI_SLEEP_ATTACK:
  case MODE_WIFI_SLEEP_TARGETED:
  case MODE_WIFI_SAE_COMMIT:
    wifiAttackManager.stop();
    WiFi.scanDelete();
    break;
  case MODE_WIFI_TRAP:
    trapManager.stop();
    WiFi.mode(WIFI_STA);
    break;
  // BLE Scans
  case MODE_BLE_SCAN:
  case MODE_BLE_SCAN_SKIMMERS:
  case MODE_BLE_SCAN_AIRTAG:
  case MODE_BLE_SCAN_AIRTAG_MON:
  case MODE_BLE_SCAN_FLIPPER:
  case MODE_BLE_SCAN_FLOCK:
  case MODE_BLE_SCAN_ANALYZER:
  case MODE_BLE_SCAN_SIMPLE:
  case MODE_BLE_SCAN_SIMPLE_TWO:
    bleManager.stopScan();
    WiFi.mode(WIFI_STA);
    break;
  // BLE Attacks
  case MODE_BLE_SPAM:
  case MODE_BLE_SOUR_APPLE:
  case MODE_BLE_SWIFTPAIR_MICROSOFT:
  case MODE_BLE_SWIFTPAIR_GOOGLE:
  case MODE_BLE_SWIFTPAIR_SAMSUNG:
  case MODE_BLE_SPAM_ALL:
  case MODE_BLE_FLIPPER_SPAM:
  case MODE_BLE_AIRTAG_SPOOF:
    bleManager.stop();
    WiFi.mode(WIFI_STA);
    break;
  // Network Scans
  case MODE_NETWORK_ARP_SCAN:
  case MODE_NETWORK_PORT_SCAN:
  case MODE_NETWORK_PING_SCAN:
  case MODE_NETWORK_DNS_SCAN:
  case MODE_NETWORK_HTTP_SCAN:
  case MODE_NETWORK_HTTPS_SCAN:
  case MODE_NETWORK_SMTP_SCAN:
  case MODE_NETWORK_RDP_SCAN:
  case MODE_NETWORK_TELNET_SCAN:
  case MODE_NETWORK_SSH_SCAN:
    networkScanManager.stop();
    WiFi.scanDelete();
    break;
  // Tools
  case MODE_BADWOLF:
    usbManager.stop();
    break;
  case MODE_FAKE_LOGIN:
  case MODE_QR:
    break;
  case MODE_MDNS:
    mdnsManager.stop();
    break;
  case MODE_VAULT:
  case MODE_DAEMON:
    // Эти режимы не требуют специальной очистки
    break;
  case MODE_NORMAL:
  default:
    break;
  }
}

/** Управление состоянием WiFi в зависимости от режима */
static void manageWiFiState(AppMode mode) {
  switch (mode) {
  // BLE modes - WiFi OFF
  case MODE_BLE_SPAM:
  case MODE_BLE_SCAN:
  case MODE_BLE_SCAN_SKIMMERS:
  case MODE_BLE_SCAN_AIRTAG:
  case MODE_BLE_SCAN_AIRTAG_MON:
  case MODE_BLE_SCAN_FLIPPER:
  case MODE_BLE_SCAN_FLOCK:
  case MODE_BLE_SCAN_ANALYZER:
  case MODE_BLE_SCAN_SIMPLE:
  case MODE_BLE_SCAN_SIMPLE_TWO:
  case MODE_BLE_SOUR_APPLE:
  case MODE_BLE_SWIFTPAIR_MICROSOFT:
  case MODE_BLE_SWIFTPAIR_GOOGLE:
  case MODE_BLE_SWIFTPAIR_SAMSUNG:
  case MODE_BLE_SPAM_ALL:
  case MODE_BLE_FLIPPER_SPAM:
  case MODE_BLE_AIRTAG_SPOOF:
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for BLE");
    }
    netManager.setSuspend(true);
    break;
  // WiFi modes - WiFi STA
  case MODE_RADAR:
  case MODE_WIFI_PROBE_SCAN:
  case MODE_WIFI_EAPOL_SCAN:
  case MODE_WIFI_STATION_SCAN:
  case MODE_WIFI_PACKET_MONITOR:
  case MODE_WIFI_CHANNEL_ANALYZER:
  case MODE_WIFI_CHANNEL_ACTIVITY:
  case MODE_WIFI_PACKET_RATE:
  case MODE_WIFI_PINESCAN:
  case MODE_WIFI_MULTISSID:
  case MODE_WIFI_SIGNAL_STRENGTH:
  case MODE_WIFI_RAW_CAPTURE:
  case MODE_WIFI_AP_STA:
  case MODE_WIFI_SNIFF:
  case MODE_WIFI_DEAUTH:
  case MODE_WIFI_DEAUTH_TARGETED:
  case MODE_WIFI_DEAUTH_MANUAL:
  case MODE_WIFI_BEACON:
  case MODE_WIFI_BEACON_RICKROLL:
  case MODE_WIFI_BEACON_LIST:
  case MODE_WIFI_BEACON_FUNNY:
  case MODE_WIFI_AUTH_ATTACK:
  case MODE_WIFI_MIMIC_FLOOD:
  case MODE_WIFI_AP_SPAM:
  case MODE_WIFI_BAD_MESSAGE:
  case MODE_WIFI_BAD_MESSAGE_TARGETED:
  case MODE_WIFI_SLEEP_ATTACK:
  case MODE_WIFI_SLEEP_TARGETED:
  case MODE_WIFI_SAE_COMMIT:
  case MODE_WIFI_TRAP:
  case MODE_MDNS:
  case MODE_NETWORK_ARP_SCAN:
  case MODE_NETWORK_PORT_SCAN:
  case MODE_NETWORK_PING_SCAN:
  case MODE_NETWORK_DNS_SCAN:
  case MODE_NETWORK_HTTP_SCAN:
  case MODE_NETWORK_HTTPS_SCAN:
  case MODE_NETWORK_SMTP_SCAN:
  case MODE_NETWORK_RDP_SCAN:
  case MODE_NETWORK_TELNET_SCAN:
  case MODE_NETWORK_SSH_SCAN:
    if (WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
      Serial.println("[SYS] WiFi STA for WiFi mode");
    }
    netManager.setSuspend(true);
    break;
  // Normal modes - WiFi STA, NetManager active
  case MODE_VAULT:
  case MODE_DAEMON:
  case MODE_FAKE_LOGIN:
  case MODE_QR:
    netManager.setSuspend(false);
    break;
  case MODE_BADWOLF:
    netManager.setSuspend(true);
    break;
  case MODE_NORMAL:
  default:
    // Нормальный режим - WiFi включен, NetManager активен
    if (WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
    }
    netManager.setSuspend(false);
    break;
  }
}

/** Инициализация режима с проверкой ошибок */
static bool initializeMode(AppMode mode) {
  switch (mode) {
  case MODE_BLE_SPAM:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      Serial.println("[SYS] ERROR: WiFi not off before BLE start");
      WiFi.disconnect(true);
      yield(); // Allow WiFi stack to process disconnect
      WiFi.mode(WIFI_OFF);
    }
    bleManager.begin();
    if (!bleManager.isActive()) {
      Serial.println("[SYS] BLE init failed");
      return false;
    }
    Serial.println("[SYS] BLE SPAM mode initialized");
    return true;

  case MODE_RADAR:
    manageWiFiState(mode);
    WiFi.scanNetworks(true, true);
    wifiScanSelected = 0;
    wifiListPage = 0;
    wifiFilteredCount = 0;
    Serial.println("[SYS] RADAR mode initialized");
    return true;

  case MODE_WIFI_DEAUTH: {
    manageWiFiState(mode);
    WiFi.disconnect(true);
    delay(80);
    WiFi.scanNetworks(true, true);
    wifiScanSelected = 0;
    wifiListPage = 0;
    wifiFilteredCount = 0;
    lastDeauthTargetScanIndex = -1;
    Serial.println(
        "[SYS] DEAUTH mode initialized (WiFi disconnected for injection)");
    return true;
  }

  case MODE_WIFI_BEACON:
    manageWiFiState(mode);
    beaconManager.begin();
    Serial.println("[SYS] BEACON mode initialized");
    return true;

  case MODE_WIFI_SNIFF:
    manageWiFiState(mode);
    wifiSniffManager.begin();
    Serial.println("[SYS] SNIFF mode initialized");
    return true;

  case MODE_BLE_SCAN:
    manageWiFiState(mode);
    bleManager.beginScan();
    Serial.println("[SYS] BLE SCAN mode initialized");
    return true;

  case MODE_FAKE_LOGIN:
    manageWiFiState(mode);
    Serial.println("[SYS] FAKE LOGIN mode initialized");
    return true;

  case MODE_QR:
    manageWiFiState(mode);
    Serial.println("[SYS] QR mode initialized");
    return true;

  case MODE_MDNS:
    manageWiFiState(mode);
    mdnsManager.begin();
    Serial.println("[SYS] MDNS mode initialized");
    return true;

  case MODE_WIFI_TRAP: {
    manageWiFiState(mode);
    // Перед запуском AP можно клонировать SSID из сканирования
    // Если сканирование еще не завершено, запустим его
    int n = WiFi.scanComplete();
    if (n == -1) {
      WiFi.scanNetworks(true, true);
      Serial.println("[TRAP] Starting scan for SSID cloning...");
    }
    // Если есть завершенное сканирование и выбран индекс, клонируем SSID
    if (n > 0 && wifiScanSelected >= 0 && wifiScanSelected < n) {
      sortAndFilterWiFiNetworks();
      int actualIndex =
          (wifiFilteredCount > 0 && wifiScanSelected < wifiFilteredCount)
              ? wifiSortedIndices[wifiScanSelected]
              : wifiScanSelected;
      trapManager.setClonedSSID(actualIndex);
    }
    trapManager.start();
    yield(); // Allow AP to start (non-blocking)
    if (!trapManager.isActive()) {
      Serial.println("[SYS] TRAP init failed - AP not started");
      return false;
    }
    Serial.println("[SYS] TRAP mode initialized");
    return true;
  }

  case MODE_BADWOLF:
    manageWiFiState(mode);
    usbManager.begin();
    Serial.println("[SYS] BADWOLF mode initialized");
    return true;

  case MODE_VAULT:
    manageWiFiState(mode);
    vaultManager.trySyncNtp();
    Serial.println("[SYS] VAULT mode initialized");
    return true;

  case MODE_DAEMON:
    manageWiFiState(mode);
    Serial.println("[SYS] DAEMON mode initialized");
    return true;

  // WiFi Scans
  case MODE_WIFI_PROBE_SCAN:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_PROBE_SCAN);
    Serial.println("[SYS] PROBE SCAN mode initialized");
    return true;
  case MODE_WIFI_EAPOL_SCAN:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_EAPOL_CAPTURE);
    Serial.println("[SYS] EAPOL SCAN mode initialized");
    return true;
  case MODE_WIFI_STATION_SCAN:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_STATION_SCAN);
    Serial.println("[SYS] STATION SCAN mode initialized");
    return true;
  case MODE_WIFI_PACKET_MONITOR:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_PACKET_MONITOR);
    Serial.println("[SYS] PACKET MONITOR mode initialized");
    return true;
  case MODE_WIFI_CHANNEL_ANALYZER:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_CHANNEL_ANALYZER);
    Serial.println("[SYS] CHANNEL ANALYZER mode initialized");
    return true;
  case MODE_WIFI_CHANNEL_ACTIVITY:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_CHANNEL_ACTIVITY);
    Serial.println("[SYS] CHANNEL ACTIVITY mode initialized");
    return true;
  case MODE_WIFI_PACKET_RATE:
    manageWiFiState(mode);
    wifiSniffManager.begin(SNIFF_MODE_PACKET_RATE);
    Serial.println("[SYS] PACKET RATE mode initialized");
    return true;
  case MODE_WIFI_PINESCAN:
  case MODE_WIFI_MULTISSID:
  case MODE_WIFI_SIGNAL_STRENGTH:
  case MODE_WIFI_RAW_CAPTURE:
  case MODE_WIFI_AP_STA:
    manageWiFiState(mode);
    wifiSniffManager.begin(
        SNIFF_MODE_AP); // Use AP mode as base, can be extended
    Serial.printf("[SYS] WiFi scan mode %d initialized\n", mode);
    return true;
  // WiFi Attacks
  case MODE_WIFI_DEAUTH_TARGETED:
  case MODE_WIFI_DEAUTH_MANUAL:
    manageWiFiState(mode);
    WiFi.disconnect(true);
    delay(80);
    WiFi.scanNetworks(true, true);
    wifiScanSelected = 0;
    wifiListPage = 0;
    wifiFilteredCount = 0;
    lastDeauthTargetScanIndex = -1;
    Serial.printf("[SYS] DEAUTH mode %d initialized\n", mode);
    return true;
  case MODE_WIFI_BEACON_RICKROLL:
    manageWiFiState(mode);
    beaconManager.setMode(BEACON_MODE_RICK_ROLL);
    beaconManager.begin();
    Serial.println("[SYS] BEACON RICKROLL mode initialized");
    return true;
  case MODE_WIFI_BEACON_LIST:
    manageWiFiState(mode);
    beaconManager.setMode(BEACON_MODE_CUSTOM_LIST);
    beaconManager.begin();
    Serial.println("[SYS] BEACON LIST mode initialized");
    return true;
  case MODE_WIFI_BEACON_FUNNY:
    manageWiFiState(mode);
    beaconManager.setMode(BEACON_MODE_FUNNY);
    beaconManager.begin();
    Serial.println("[SYS] BEACON FUNNY mode initialized");
    return true;
  case MODE_WIFI_AUTH_ATTACK:
  case MODE_WIFI_MIMIC_FLOOD:
  case MODE_WIFI_AP_SPAM:
  case MODE_WIFI_BAD_MESSAGE:
  case MODE_WIFI_BAD_MESSAGE_TARGETED:
  case MODE_WIFI_SLEEP_ATTACK:
  case MODE_WIFI_SLEEP_TARGETED:
  case MODE_WIFI_SAE_COMMIT:
    manageWiFiState(mode);
    WiFi.scanNetworks(true, true);
    Serial.printf("[SYS] WiFi attack mode %d initialized (TODO: implement)\n",
                  mode);
    return true; // TODO: Implement these attacks
  // BLE Scans
  case MODE_BLE_SCAN_SKIMMERS:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_SKIMMERS);
    Serial.println("[SYS] BLE SKIMMERS scan initialized");
    return true;
  case MODE_BLE_SCAN_AIRTAG:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_AIRTAG);
    Serial.println("[SYS] BLE AIRTAG scan initialized");
    return true;
  case MODE_BLE_SCAN_AIRTAG_MON:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_BASIC);
    bleManager.startAirTagMonitor();
    Serial.println("[SYS] BLE AIRTAG MONITOR initialized");
    return true;
  case MODE_BLE_SCAN_FLIPPER:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_FLIPPER);
    Serial.println("[SYS] BLE FLIPPER scan initialized");
    return true;
  case MODE_BLE_SCAN_FLOCK:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_FLOCK);
    Serial.println("[SYS] BLE FLOCK scan initialized");
    return true;
  case MODE_BLE_SCAN_ANALYZER:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_ANALYZER);
    Serial.println("[SYS] BLE ANALYZER initialized");
    return true;
  case MODE_BLE_SCAN_SIMPLE:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_SIMPLE);
    Serial.println("[SYS] BLE SIMPLE scan initialized");
    return true;
  case MODE_BLE_SCAN_SIMPLE_TWO:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.beginScan(BLE_SCAN_SIMPLE_TWO);
    Serial.println("[SYS] BLE SIMPLE TWO scan initialized");
    return true;
  // BLE Attacks
  case MODE_BLE_SOUR_APPLE:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.startAttack(BLE_ATTACK_SOUR_APPLE);
    Serial.println("[SYS] SOUR APPLE mode initialized");
    return true;
  case MODE_BLE_SWIFTPAIR_MICROSOFT:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.startAttack(BLE_ATTACK_SWIFTPAIR_MICROSOFT);
    Serial.println("[SYS] SWIFTPAIR MS mode initialized");
    return true;
  case MODE_BLE_SWIFTPAIR_GOOGLE:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.startAttack(BLE_ATTACK_SWIFTPAIR_GOOGLE);
    Serial.println("[SYS] SWIFTPAIR GOOGLE mode initialized");
    return true;
  case MODE_BLE_SWIFTPAIR_SAMSUNG:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.startAttack(BLE_ATTACK_SWIFTPAIR_SAMSUNG);
    Serial.println("[SYS] SWIFTPAIR SAMSUNG mode initialized");
    return true;
  case MODE_BLE_SPAM_ALL:
  case MODE_BLE_FLIPPER_SPAM:
  case MODE_BLE_AIRTAG_SPOOF:
    manageWiFiState(mode);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      yield();
      WiFi.mode(WIFI_OFF);
    }
    bleManager.startAttack(BLE_ATTACK_SPAM); // TODO: Add specific attack types
    Serial.printf("[SYS] BLE attack mode %d initialized\n", mode);
    return true;
  // Network Scans
  case MODE_NETWORK_ARP_SCAN:
  case MODE_NETWORK_PORT_SCAN:
  case MODE_NETWORK_PING_SCAN:
  case MODE_NETWORK_DNS_SCAN:
  case MODE_NETWORK_HTTP_SCAN:
  case MODE_NETWORK_HTTPS_SCAN:
  case MODE_NETWORK_SMTP_SCAN:
  case MODE_NETWORK_RDP_SCAN:
  case MODE_NETWORK_TELNET_SCAN:
  case MODE_NETWORK_SSH_SCAN:
    manageWiFiState(mode);
    Serial.printf("[SYS] Network scan mode %d initialized (TODO: implement)\n",
                  mode);
    return true; // TODO: Implement network scans
  case MODE_NORMAL:
    manageWiFiState(mode);
    WiFi.scanDelete();
    Serial.println("[SYS] NORMAL mode initialized");
    return true;

  default:
    Serial.println("[SYS] Unknown mode");
    return false;
  }
}

/** Безопасное переключение режима с очисткой и инициализацией */
static bool switchToMode(AppMode newMode) {
  if (newMode == currentMode) {
    return true; // Уже в этом режиме
  }

  Serial.printf("[SYS] Switching from MODE_%d to MODE_%d\n", currentMode,
                newMode);

  // 1. Очистка текущего режима
  cleanupMode(currentMode);

  // 2. Освобождение ресурсов
  // WiFi и NetManager управляются в manageWiFiState

  // 3. Инициализация нового режима
  if (!initializeMode(newMode)) {
    Serial.println("[SYS] Mode initialization failed, falling back to NORMAL");
    // Fallback: вернуться в NORMAL при ошибке
    cleanupMode(currentMode);
    currentMode = MODE_NORMAL;
    initializeMode(MODE_NORMAL);
    return false;
  }

  // 4. Установка нового режима
  currentMode = newMode;
  Serial.printf("[SYS] Successfully switched to MODE_%d\n", currentMode);
  return true;
}

/** Старая функция для обратной совместимости */
static void exitAppModeToNormal() {
  switchToMode(MODE_NORMAL);
  Serial.println("[SYS] NETMANAGER RESUMED.");
}

// ---------------------------------------------------------------------------
// Flipper-style menu: categories (0=Config, 1=WiFi, 2=Tools, 3=System) ->
// submenus
// ---------------------------------------------------------------------------
static int submenuCount(int category) {
  switch (category) {
  case 0:
    return 5; // Config: AUTO, FLIP, GLITCH, LED, DIM
  case 1:
    return 20; // WiFi: Full Marauder menu
  case 2:
    return 22; // Tools: BLE + Network + Other tools (no GPS)
  case 3:
    return 3; // System: REBOOT, VERSION, EXIT
  default:
    return 3;
  }
}

/** Execute action for (category, item). Called when menuLevel==1 and user
 * Long-press. */
static bool handleMenuActionByCategory(int cat, int item, unsigned long now) {
  if (cat == 0) {
    // Config: AUTO = cycle OFF -> 5s -> 10s -> 15s -> OFF
    if (item == 0) {
      if (!settings.carouselEnabled) {
        settings.carouselEnabled = true;
        settings.carouselIntervalSec = 5;
      } else if (settings.carouselIntervalSec == 5) {
        settings.carouselIntervalSec = 10;
      } else if (settings.carouselIntervalSec == 10) {
        settings.carouselIntervalSec = 15;
      } else {
        settings.carouselEnabled = false;
      }
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("carousel", settings.carouselEnabled);
      prefs.putInt("carouselSec", settings.carouselIntervalSec);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    } else if (item == 1) {
      settings.displayInverted = !settings.displayInverted;
      display.setScreenFlipped(settings.displayInverted);
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("inverted", settings.displayInverted);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    } else if (item == 2) {
      settings.glitchEnabled = !settings.glitchEnabled;
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("glitch", settings.glitchEnabled);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    } else if (item == 3) {
      settings.ledEnabled = !settings.ledEnabled;
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("led", settings.ledEnabled);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    } else if (item == 4) {
      settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
      display.u8g2().setContrast(settings.lowBrightnessDefault
                                     ? NOCT_CONTRAST_MIN
                                     : NOCT_CONTRAST_MAX);
      Preferences prefs;
      prefs.begin("nocturne", false);
      prefs.putBool("lowBright", settings.lowBrightnessDefault);
      prefs.end();
      snprintf(toastMsg, sizeof(toastMsg), "Saved");
      toastUntil = now + 800;
    }
    return true;
  }
  if (cat == 1) {
    quickMenuOpen = false;
    rebootConfirmed = false;
    // WiFi Scans
    if (item == 0) {
      if (!switchToMode(MODE_RADAR)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 1) {
      if (!switchToMode(MODE_WIFI_PROBE_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 2) {
      if (!switchToMode(MODE_WIFI_EAPOL_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 3) {
      if (!switchToMode(MODE_WIFI_STATION_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 4) {
      if (!switchToMode(MODE_WIFI_PACKET_MONITOR)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 5) {
      if (!switchToMode(MODE_WIFI_CHANNEL_ANALYZER)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 6) {
      if (!switchToMode(MODE_WIFI_CHANNEL_ACTIVITY)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 7) {
      if (!switchToMode(MODE_WIFI_PACKET_RATE)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 8) {
      if (!switchToMode(MODE_WIFI_PINESCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 9) {
      if (!switchToMode(MODE_WIFI_MULTISSID)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 10) {
      if (!switchToMode(MODE_WIFI_SIGNAL_STRENGTH)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 11) {
      if (!switchToMode(MODE_WIFI_RAW_CAPTURE)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 12) {
      if (!switchToMode(MODE_WIFI_AP_STA)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    // WiFi Attacks
    else if (item == 13) {
      wifiScanSelected = 0;
      wifiListPage = 0;
      if (!switchToMode(MODE_WIFI_DEAUTH)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
      kickManager.setTargetFromScan(0);
    } else if (item == 14) {
      wifiScanSelected = 0;
      wifiListPage = 0;
      if (!switchToMode(MODE_WIFI_DEAUTH_TARGETED)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 15) {
      wifiScanSelected = 0;
      wifiListPage = 0;
      if (!switchToMode(MODE_WIFI_DEAUTH_MANUAL)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 16) {
      if (!switchToMode(MODE_WIFI_BEACON)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 17) {
      if (!switchToMode(MODE_WIFI_BEACON_RICKROLL)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 18) {
      wifiScanSelected = 0;
      wifiListPage = 0;
      if (!switchToMode(MODE_WIFI_AUTH_ATTACK)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
      // Set target from scan if available
      int n = WiFi.scanComplete();
      if (n > 0) {
        sortAndFilterWiFiNetworks();
        if (wifiFilteredCount > 0) {
          int idx = wifiSortedIndices[0];
          String ssid = WiFi.SSID(idx);
          uint8_t bssid[6];
          uint8_t *bssidPtr = WiFi.BSSID(idx);
          if (bssidPtr) {
            memcpy(bssid, bssidPtr, 6);
            wifiAttackManager.setTargetBSSID(bssid);
            wifiAttackManager.setTargetSSID(ssid.c_str());
            wifiAttackManager.setTargetChannel(WiFi.channel(idx));
          }
        }
      }
    } else if (item == 19) {
      if (!switchToMode(MODE_WIFI_TRAP)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    return true;
  }
  if (cat == 2) {
    quickMenuOpen = false;
    rebootConfirmed = false;
    // BLE Scans
    if (item == 0) {
      if (!switchToMode(MODE_BLE_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 1) {
      if (!switchToMode(MODE_BLE_SCAN_SKIMMERS)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 2) {
      if (!switchToMode(MODE_BLE_SCAN_AIRTAG)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 3) {
      if (!switchToMode(MODE_BLE_SCAN_AIRTAG_MON)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 4) {
      if (!switchToMode(MODE_BLE_SCAN_FLIPPER)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 5) {
      if (!switchToMode(MODE_BLE_SCAN_ANALYZER)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    // BLE Attacks
    else if (item == 6) {
      if (!switchToMode(MODE_BLE_SPAM)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 7) {
      if (!switchToMode(MODE_BLE_SOUR_APPLE)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 8) {
      if (!switchToMode(MODE_BLE_SWIFTPAIR_MICROSOFT)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 9) {
      if (!switchToMode(MODE_BLE_SWIFTPAIR_GOOGLE)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 10) {
      if (!switchToMode(MODE_BLE_SWIFTPAIR_SAMSUNG)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 11) {
      if (!switchToMode(MODE_BLE_FLIPPER_SPAM)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    // Network Scans
    else if (item == 12) {
      if (!switchToMode(MODE_NETWORK_ARP_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 13) {
      if (!switchToMode(MODE_NETWORK_PORT_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 14) {
      if (!switchToMode(MODE_NETWORK_PING_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 15) {
      if (!switchToMode(MODE_NETWORK_DNS_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 16) {
      if (!switchToMode(MODE_NETWORK_HTTP_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 17) {
      if (!switchToMode(MODE_NETWORK_HTTPS_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 18) {
      if (!switchToMode(MODE_NETWORK_SMTP_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 19) {
      if (!switchToMode(MODE_NETWORK_RDP_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 20) {
      if (!switchToMode(MODE_NETWORK_TELNET_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    } else if (item == 21) {
      if (!switchToMode(MODE_NETWORK_SSH_SCAN)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    // Tools
    else if (item == 22) {
      if (!switchToMode(MODE_BADWOLF)) {
        snprintf(toastMsg, sizeof(toastMsg), "FAIL");
        toastUntil = now + 1500;
        return false;
      }
    }
    return true;
  }
  if (cat == 3) {
    if (item == 0) { // REBOOT
      if (!rebootConfirmed) {
        rebootConfirmed = true;
        rebootConfirmTime = now;
      } else {
        rebootConfirmed = false;
        esp_restart();
      }
      return true;
    }
    if (item == 1) { // VERSION
      snprintf(toastMsg, sizeof(toastMsg), "v" NOCTURNE_VERSION);
      toastUntil = now + 2000;
      return true;
    }
    if (item == 2) { // EXIT
      quickMenuOpen = false;
      return true;
    }
  }
  return false;
}

void loop() {
  unsigned long now = millis();

  // 1. Critical background tasks
  netManager.tick(now);

  if (netManager.isTcpConnected()) {
    while (netManager.available()) {
      char c = (char)netManager.read();
      if (c == '\n') {
        char *buf = netManager.getLineBuffer();
        size_t bufLen = netManager.getLineBufferLen();
        if (bufLen > 0 && bufLen <= NOCT_TCP_LINE_MAX) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, buf, bufLen);
          if (!err) {
            netManager.markDataReceived(now);
            if (netManager.parsePayload(buf, bufLen, &state)) {
              needRedraw = true;
              HardwareData &hw = state.hw;
              display.cpuGraph.push((float)hw.cl);
              display.gpuGraph.push((float)hw.gl);
              display.netDownGraph.setMax(2048);
              display.netDownGraph.push((float)hw.nd);
              display.netUpGraph.setMax(2048);
              display.netUpGraph.push((float)hw.nu);
            }
          }
        }
        netManager.clearLineBuffer();
      } else {
        netManager.appendLineBuffer(c);
      }
    }
  }

  // 2. Non-blocking input
  ButtonEvent event = input.update();

  // 3. Global: DOUBLE = open menu when closed; when menu already open, leave
  // EV_DOUBLE for menu block (back / close). Menu is available regardless of
  // WiFi or TCP connection state.
  if (event == EV_DOUBLE && !quickMenuOpen) {
    if (currentMode != MODE_NORMAL)
      exitAppModeToNormal();
    quickMenuOpen = true;
    menuState = MENU_MAIN;
    menuLevel = 0;
    menuCategory = 0;
    quickMenuItem = 0;
    lastMenuEventTime = now;
    needRedraw = true;
    event = EV_NONE;
  } else if (event == EV_LONG && currentMode == MODE_NORMAL && !quickMenuOpen) {
    settings.lowBrightnessDefault = !settings.lowBrightnessDefault;
    uint8_t contrast =
        settings.lowBrightnessDefault ? NOCT_CONTRAST_MIN : NOCT_CONTRAST_MAX;
    display.u8g2().setContrast(contrast);
    Preferences prefs;
    prefs.begin("nocturne", false);
    prefs.putBool("lowBright", settings.lowBrightnessDefault);
    prefs.end();
    needRedraw = true;
    event = EV_NONE;
  }

  // Alert / carousel / screen sync / fan animation
  if (state.alertActive) {
    currentScene = state.alertTargetScene;
    needRedraw = true;
  }
  if (settings.carouselEnabled && !predatorMode && !state.alertActive) {
    unsigned long intervalMs =
        (unsigned long)settings.carouselIntervalSec * 1000;
    if (now - lastCarousel > intervalMs) {
      needRedraw = true;
      previousScene = currentScene;
      currentScene = (currentScene + 1) % sceneManager.totalScenes();
      if (previousScene != currentScene) {
        inTransition = true;
        transitionStart = now;
      }
      lastCarousel = now;
    }
  }
  if (netManager.isTcpConnected() &&
      netManager.getLastSentScreen() != currentScene) {
    char screenMsg[16];
    snprintf(screenMsg, sizeof(screenMsg), "screen:%d\n", currentScene);
    netManager.print(screenMsg);
    netManager.setLastSentScreen(currentScene);
  }
  if (now - lastFanAnim >= 50) {
    fanAnimFrame = (fanAnimFrame + 1) % 12;
    lastFanAnim = now;
  }

  // Battery: periodic update with adaptive interval
  if (batTimer.check(now)) {
    unsigned long nextInterval = updateBatteryState();
    // Update timer interval for next reading
    batTimer.intervalMs = nextInterval;
    batTimer.lastMs = now; // Reset timer with new interval
  }

  // LED: predator breath / alert blink / cursor blink
  pinMode(NOCT_LED_ALERT_PIN, OUTPUT);
  if (predatorMode) {
    unsigned long t = (now - predatorEnterTime) / 20;
    int breath = (int)(128 + 127 * sin(t * 0.1f));
    if (breath < 0)
      breath = 0;
    if (settings.ledEnabled)
      analogWrite(NOCT_LED_ALERT_PIN, breath);
    else
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  } else {
    if (state.alertActive && !lastAlertActive) {
      alertBlinkCounter = 0;
      blinkState = true;
      if (settings.ledEnabled)
        digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
      lastBlink = now;
    }
    lastAlertActive = state.alertActive;
    if (state.alertActive) {
      if (now - lastBlink >= NOCT_ALERT_LED_BLINK_MS) {
        lastBlink = now;
        if (alertBlinkCounter < NOCT_ALERT_MAX_BLINKS * 2) {
          blinkState = !blinkState;
          if (settings.ledEnabled)
            digitalWrite(NOCT_LED_ALERT_PIN, blinkState ? HIGH : LOW);
          alertBlinkCounter++;
        } else {
          blinkState = false;
          if (settings.ledEnabled)
            digitalWrite(NOCT_LED_ALERT_PIN, LOW);
        }
      }
    } else {
      if (settings.ledEnabled)
        digitalWrite(NOCT_LED_ALERT_PIN, LOW);
      if (now - lastBlink > 500) {
        blinkState = !blinkState;
        lastBlink = now;
      }
    }
    if (!settings.ledEnabled)
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
  }

  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= (unsigned long)NOCT_SPLASH_MS) {
      splashDone = true;
      needRedraw = true;
    }
  }
  // Render guarantee: menu has highest priority; never block menu with splash
  if (quickMenuOpen)
    splashDone = true;

  // 4. Mode handling: menu vs app
  if (quickMenuOpen) {
    // Сброс подтверждения перезагрузки при таймауте (5 секунд)
    if (rebootConfirmed && (now - rebootConfirmTime > 5000)) {
      rebootConfirmed = false;
      needRedraw = true;
    }

    // Защита от множественных срабатываний событий
    if (event != EV_NONE &&
        (now - lastMenuEventTime < MENU_EVENT_DEBOUNCE_MS)) {
      event = EV_NONE; // Игнорируем событие, если прошло слишком мало времени
    }

    // --- MENU LOGIC (Flipper-style: level 0 = categories, level 1 = submenu)
    // ---
    if (event == EV_DOUBLE) {
      lastMenuEventTime = now;
      if (menuLevel == 1) {
        menuLevel = 0;
        quickMenuItem = menuCategory;
        rebootConfirmed = false;
      } else {
        quickMenuOpen = false;
        rebootConfirmed = false;
      }
      needRedraw = true;
    } else if (event == EV_SHORT) {
      lastMenuEventTime = now;
      if (menuLevel == 0) {
        quickMenuItem = (quickMenuItem + 1) % MENU_CATEGORIES;
      } else {
        int count = submenuCount(menuCategory);
        quickMenuItem = (quickMenuItem + 1) % count;
        if (menuCategory == 3 && quickMenuItem != 0)
          rebootConfirmed = false;
      }
      needRedraw = true; // Ensure redraw when quickMenuItem changes
    } else if (event == EV_LONG) {
      lastMenuEventTime = now;
      if (menuLevel == 0) {
        menuLevel = 1;
        menuCategory = quickMenuItem;
        quickMenuItem = 0;
      } else {
        bool ok = handleMenuActionByCategory(menuCategory, quickMenuItem, now);
        if (ok)
          needRedraw = true;
      }
    }
  } else {
    // App mode: SHORT = navigate, LONG = action (or predator in normal)
    switch (currentMode) {
    case MODE_NORMAL:
      if (event == EV_SHORT && !state.alertActive) {
        previousScene = currentScene;
        currentScene = (currentScene + 1) % sceneManager.totalScenes();
        if (previousScene != currentScene) {
          inTransition = true;
          transitionStart = now;
        }
        lastCarousel = now;
        needRedraw = true;
      }
      break;
    case MODE_WIFI_DEAUTH: {
      if (event == EV_SHORT) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          sortAndFilterWiFiNetworks();
          int count = wifiFilteredCount > 0 ? wifiFilteredCount : n;
          if (count > 0) {
            wifiScanSelected = (wifiScanSelected + 1) % count;
            if (wifiScanSelected >= wifiListPage + 5)
              wifiListPage = wifiScanSelected - 4;
            else if (wifiScanSelected < wifiListPage)
              wifiListPage = wifiScanSelected;
          }
        }
        needRedraw = true;
      } else if (event == EV_LONG) {
        if (kickManager.isAttacking()) {
          kickManager.stopAttack();
        } else {
          int n = WiFi.scanComplete();
          if (n > 0) {
            sortAndFilterWiFiNetworks();
            int count = wifiFilteredCount > 0 ? wifiFilteredCount : n;
            if (count > 0) {
              int actualIndex =
                  (wifiFilteredCount > 0 &&
                   wifiScanSelected < wifiFilteredCount)
                      ? wifiSortedIndices[wifiScanSelected]
                      : (wifiScanSelected >= 0 && wifiScanSelected < n
                             ? wifiScanSelected
                             : 0);
              if (lastDeauthTargetScanIndex != actualIndex) {
                kickManager.setTargetFromScan(actualIndex);
                lastDeauthTargetScanIndex = actualIndex;
              } else {
                kickManager.startAttack();
              }
            }
          }
        }
        needRedraw = true;
      }
      break;
    }
    case MODE_RADAR:
      if (event == EV_SHORT) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          sortAndFilterWiFiNetworks();
          int count = wifiFilteredCount > 0 ? wifiFilteredCount : n;
          if (count > 0) {
            wifiScanSelected = (wifiScanSelected + 1) % count;
            if (wifiScanSelected >= wifiListPage + 5)
              wifiListPage = wifiScanSelected - 4;
            else if (wifiScanSelected < wifiListPage)
              wifiListPage = wifiScanSelected;
          }
        }
        needRedraw = true;
      } else if (event == EV_LONG) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          // Переключение режима сортировки/фильтрации
          wifiSortMode = (wifiSortMode + 1) % 3;
          sortAndFilterWiFiNetworks();
          wifiScanSelected = 0;
          wifiListPage = 0;
          Serial.printf("[RADAR] Sort mode: %d\n", wifiSortMode);
        } else {
          // Если нет сканирования, запустить новое
          Serial.println("[RADAR] INITIATING DISCONNECT...");
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          delay(100);
          WiFi.mode(WIFI_STA);
          Serial.println("[RADAR] RADIO RESET.");
          WiFi.scanNetworks(true, true);
          wifiFilteredCount = 0;
        }
        needRedraw = true;
      }
      break;
    case MODE_VAULT:
      if (event == EV_SHORT) {
        int n = vaultManager.getAccountCount();
        if (n > 0)
          vaultManager.setCurrentIndex((vaultManager.getCurrentIndex() + 1) %
                                       n);
        needRedraw = true;
      }
      break;
    case MODE_BADWOLF:
      if (event == EV_SHORT) {
        badWolfScriptIndex =
            (badWolfScriptIndex + 1) % UsbManager::DUCKY_SCRIPT_COUNT;
        needRedraw = true;
      } else if (event == EV_LONG) {
        if (badWolfScriptIndex == 4)
          usbManager.runBackdoor();
        else
          usbManager.runDuckyScript(badWolfScriptIndex);
        needRedraw = true;
      }
      break;
    case MODE_WIFI_BEACON:
      if (event == EV_SHORT) {
        beaconManager.nextSSID();
        needRedraw = true;
      }
      break;
    case MODE_WIFI_SNIFF:
      if (event == EV_SHORT) {
        int n = wifiSniffManager.getApCount();
        if (n > 0) {
          wifiSniffSelected = (wifiSniffSelected + 1) % n;
          needRedraw = true;
        }
      }
      break;
    case MODE_BLE_SCAN:
      if (event == EV_SHORT) {
        int n = bleManager.getScanCount();
        if (n > 0) {
          bleScanSelected = (bleScanSelected + 1) % n;
          needRedraw = true;
        }
      } else if (event == EV_LONG) {
        int n = bleManager.getScanCount();
        if (n > 0) {
          bleManager.cloneDevice(bleScanSelected);
          needRedraw = true;
        }
      }
      break;
    case MODE_FAKE_LOGIN:
    case MODE_QR:
      if (event == EV_SHORT)
        needRedraw = true;
      break;
    case MODE_MDNS:
      if (event == EV_SHORT) {
        static int mdnsNameIdx = 0;
        const char *names[] = {"NOCTURNE", "HP-Print", "Chromecast", "AirPlay"};
        mdnsNameIdx = (mdnsNameIdx + 1) % 4;
        mdnsManager.setServiceName(names[mdnsNameIdx]);
        if (mdnsManager.isActive())
          mdnsManager.stop();
        mdnsManager.begin();
        needRedraw = true;
      }
      break;
    default:
      break;
    }
  }

  // 5. Render (throttle: needRedraw or guiTimer)
  static unsigned long lastYield = 0;

  if (predatorMode) {
    display.clearBuffer();
    display.sendBuffer();
    // Минимальная задержка только при необходимости
    if (now - lastYield > 10) {
      yield();
      lastYield = now;
    }
    return;
  }

  // Always render when menu is open so it is never overwritten by a skipped
  // frame
  if (!needRedraw && !guiTimer.check(now) && !quickMenuOpen) {
    // Минимальная задержка только при необходимости
    if (now - lastYield > 10) {
      yield();
      lastYield = now;
    }
    return;
  }
  needRedraw = false;

  display.clearBuffer();
  bool signalLost = splashDone && netManager.isSignalLost(now);
  if (signalLost && netManager.isTcpConnected() && netManager.hasReceivedData())
    netManager.disconnectTcp();

  if (!splashDone) {
    display.drawSplash();
  } else if (quickMenuOpen) {
    sceneManager.drawMenu(
        menuLevel, menuCategory, quickMenuItem, settings.carouselEnabled,
        settings.carouselIntervalSec, settings.displayInverted,
        settings.glitchEnabled, settings.ledEnabled,
        settings.lowBrightnessDefault, rebootConfirmed);
  } else {
    switch (currentMode) {
    case MODE_NORMAL: {
      /* Idle = no WiFi or no server (linking). After 10s show wolf screensaver.
       * Do NOT use isSearchMode() here: searchMode_ is true when WiFi is down
       * or when linking, which are exactly the states we want screensaver for.
       */
      bool idleState =
          !netManager.isWifiConnected() || !netManager.isTcpConnected();
      if (signalLost && netManager.isTcpConnected())
        idleState =
            false; /* TCP up but no data → show SEARCH, not screensaver */
      if (!idleState) {
        idleStateEnteredMs = 0;
      }
      if (idleState && idleStateEnteredMs == 0) {
        idleStateEnteredMs = now;
      }
      bool showScreensaver =
          idleState && idleStateEnteredMs != 0 &&
          (now - idleStateEnteredMs >= (unsigned long)NOCT_IDLE_SCREENSAVER_MS);

      if (!netManager.isWifiConnected()) {
        if (showScreensaver) {
          sceneManager.drawIdleScreensaver(now);
          display.applyGlitch(); /* Always light glitch on screensaver */
        } else {
          sceneManager.drawNoSignal(false, false, 0, blinkState);
        }
      } else if (!netManager.isTcpConnected()) {
        if (showScreensaver) {
          sceneManager.drawIdleScreensaver(now);
          display.applyGlitch(); /* Always light glitch on screensaver */
        } else {
          sceneManager.drawConnecting(netManager.rssi(), blinkState);
        }
      } else if (netManager.isSearchMode() || signalLost) {
        int scanPhase = (int)(now / 100) % 12;
        sceneManager.drawSearchMode(scanPhase);
      } else {
        display.drawGlobalHeader(sceneManager.getSceneName(currentScene),
                                 nullptr, netManager.rssi(),
                                 netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                     state.batteryVoltage);
        if (inTransition) {
          unsigned long elapsed = now - transitionStart;
          int progress =
              (int)((elapsed * NOCT_TRANSITION_STEP) / NOCT_TRANSITION_MS);
          if (progress > NOCT_DISP_W)
            progress = NOCT_DISP_W;
          int offsetA = -progress;
          int offsetB = NOCT_DISP_W - progress;
          sceneManager.drawWithOffset(previousScene, offsetA, bootTime,
                                      blinkState, fanAnimFrame);
          sceneManager.drawWithOffset(currentScene, offsetB, bootTime,
                                      blinkState, fanAnimFrame);
          if (progress >= NOCT_DISP_W)
            inTransition = false;
        } else {
          sceneManager.draw(currentScene, bootTime, blinkState, fanAnimFrame);
        }
      }
      break;
    }
    case MODE_DAEMON:
      // Полноэкранный режим без хедера; батарея не рисуем
      sceneManager.drawDaemon(bootTime, netManager.isWifiConnected(),
                              netManager.isTcpConnected(), netManager.rssi());
      break;
    case MODE_RADAR: {
      int n = WiFi.scanComplete();
      if (n > 0 && wifiFilteredCount == 0) {
        sortAndFilterWiFiNetworks();
      }
      sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage,
                                   wifiFilteredCount > 0 ? wifiSortedIndices
                                                         : nullptr,
                                   wifiFilteredCount);
      break;
    }
    case MODE_WIFI_PROBE_SCAN:
    case MODE_WIFI_EAPOL_SCAN:
    case MODE_WIFI_STATION_SCAN:
    case MODE_WIFI_PACKET_MONITOR:
    case MODE_WIFI_CHANNEL_ANALYZER:
    case MODE_WIFI_CHANNEL_ACTIVITY:
    case MODE_WIFI_PACKET_RATE:
    case MODE_WIFI_PINESCAN:
    case MODE_WIFI_MULTISSID:
    case MODE_WIFI_SIGNAL_STRENGTH:
    case MODE_WIFI_RAW_CAPTURE:
    case MODE_WIFI_AP_STA:
      wifiSniffManager.tick();
      sceneManager.drawWifiSniffMode(wifiSniffSelected, wifiSniffManager);
      break;
    case MODE_WIFI_DEAUTH: {
      int n = WiFi.scanComplete();
      if (n > 0 && wifiFilteredCount == 0) {
        sortAndFilterWiFiNetworks();
      }
      kickManager.tick();
      static char deauthFooter[48];
      snprintf(deauthFooter, sizeof(deauthFooter), "%s PKTS:%d  L=sel L=atk",
               kickManager.isAttacking() ? "INJ" : "IDLE",
               kickManager.getPacketCount());
      sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage,
                                   wifiFilteredCount > 0 ? wifiSortedIndices
                                                         : nullptr,
                                   wifiFilteredCount, deauthFooter);
      break;
    }
    case MODE_WIFI_BEACON:
    case MODE_WIFI_BEACON_RICKROLL:
    case MODE_WIFI_BEACON_LIST:
    case MODE_WIFI_BEACON_FUNNY:
      beaconManager.tick();
      sceneManager.drawBeaconMode(beaconManager.getCurrentSSID(),
                                  beaconManager.getBeaconCount(),
                                  beaconManager.getCurrentIndex(), 8);
      break;
    case MODE_WIFI_PROBE_SCAN:
    case MODE_WIFI_EAPOL_SCAN:
    case MODE_WIFI_STATION_SCAN:
    case MODE_WIFI_PACKET_MONITOR:
    case MODE_WIFI_CHANNEL_ANALYZER:
    case MODE_WIFI_CHANNEL_ACTIVITY:
    case MODE_WIFI_PACKET_RATE:
    case MODE_WIFI_PINESCAN:
    case MODE_WIFI_MULTISSID:
    case MODE_WIFI_SIGNAL_STRENGTH:
    case MODE_WIFI_RAW_CAPTURE:
    case MODE_WIFI_AP_STA:
      wifiSniffManager.tick();
      sceneManager.drawWifiSniffMode(wifiSniffSelected, wifiSniffManager);
      break;
    case MODE_WIFI_AUTH_ATTACK:
    case MODE_WIFI_MIMIC_FLOOD:
    case MODE_WIFI_AP_SPAM:
    case MODE_WIFI_BAD_MESSAGE:
    case MODE_WIFI_BAD_MESSAGE_TARGETED:
    case MODE_WIFI_SLEEP_ATTACK:
    case MODE_WIFI_SLEEP_TARGETED:
    case MODE_WIFI_SAE_COMMIT: {
      wifiAttackManager.tick();
      static char attackFooter[48];
      const char *attackNames[] = {"AUTH",      "MIMIC", "AP SPAM", "BAD MSG",
                                   "BAD MSG T", "SLEEP", "SLEEP T", "SAE"};
      int attackIdx = 0;
      switch (currentMode) {
      case MODE_WIFI_AUTH_ATTACK:
        attackIdx = 0;
        break;
      case MODE_WIFI_MIMIC_FLOOD:
        attackIdx = 1;
        break;
      case MODE_WIFI_AP_SPAM:
        attackIdx = 2;
        break;
      case MODE_WIFI_BAD_MESSAGE:
        attackIdx = 3;
        break;
      case MODE_WIFI_BAD_MESSAGE_TARGETED:
        attackIdx = 4;
        break;
      case MODE_WIFI_SLEEP_ATTACK:
        attackIdx = 5;
        break;
      case MODE_WIFI_SLEEP_TARGETED:
        attackIdx = 6;
        break;
      case MODE_WIFI_SAE_COMMIT:
        attackIdx = 7;
        break;
      default:
        break;
      }
      snprintf(attackFooter, sizeof(attackFooter), "%s PKTS:%lu",
               attackNames[attackIdx], wifiAttackManager.getPacketsSent());
      sceneManager.drawWiFiScanner(0, 0, nullptr, 0, attackFooter);
      break;
    }
    case MODE_NETWORK_ARP_SCAN:
    case MODE_NETWORK_PORT_SCAN:
    case MODE_NETWORK_PING_SCAN:
    case MODE_NETWORK_DNS_SCAN:
    case MODE_NETWORK_HTTP_SCAN:
    case MODE_NETWORK_HTTPS_SCAN:
    case MODE_NETWORK_SMTP_SCAN:
    case MODE_NETWORK_RDP_SCAN:
    case MODE_NETWORK_TELNET_SCAN:
    case MODE_NETWORK_SSH_SCAN: {
      networkScanManager.tick();
      static char scanFooter[48];
      const char *scanNames[] = {"ARP",   "PORT", "PING", "DNS",    "HTTP",
                                 "HTTPS", "SMTP", "RDP",  "TELNET", "SSH"};
      int scanIdx = 0;
      switch (currentMode) {
      case MODE_NETWORK_ARP_SCAN:
        scanIdx = 0;
        break;
      case MODE_NETWORK_PORT_SCAN:
        scanIdx = 1;
        break;
      case MODE_NETWORK_PING_SCAN:
        scanIdx = 2;
        break;
      case MODE_NETWORK_DNS_SCAN:
        scanIdx = 3;
        break;
      case MODE_NETWORK_HTTP_SCAN:
        scanIdx = 4;
        break;
      case MODE_NETWORK_HTTPS_SCAN:
        scanIdx = 5;
        break;
      case MODE_NETWORK_SMTP_SCAN:
        scanIdx = 6;
        break;
      case MODE_NETWORK_RDP_SCAN:
        scanIdx = 7;
        break;
      case MODE_NETWORK_TELNET_SCAN:
        scanIdx = 8;
        break;
      case MODE_NETWORK_SSH_SCAN:
        scanIdx = 9;
        break;
      default:
        break;
      }
      snprintf(scanFooter, sizeof(scanFooter), "%s HOSTS:%d %s",
               scanNames[scanIdx], networkScanManager.getHostCount(),
               networkScanManager.isScanComplete() ? "DONE" : "SCAN");
      sceneManager.drawWiFiScanner(0, 0, nullptr, 0, scanFooter);
      break;
    }
    case MODE_BLE_SCAN_SKIMMERS:
    case MODE_BLE_SCAN_AIRTAG:
    case MODE_BLE_SCAN_FLIPPER:
    case MODE_BLE_SCAN_FLOCK:
    case MODE_BLE_SCAN_ANALYZER:
    case MODE_BLE_SCAN_SIMPLE:
    case MODE_BLE_SCAN_SIMPLE_TWO: {
      static char bleFooter[48];
      const char *scanNames[] = {"SKIMMERS", "AIRTAG", "FLIPPER", "FLOCK",
                                 "ANALYZER", "SIMPLE", "SIMPLE2"};
      int scanIdx = 0;
      int count = 0;
      switch (currentMode) {
      case MODE_BLE_SCAN_SKIMMERS:
        scanIdx = 0;
        count = bleManager.getSkimmerCount();
        break;
      case MODE_BLE_SCAN_AIRTAG:
        scanIdx = 1;
        count = bleManager.getAirTagCount();
        break;
      case MODE_BLE_SCAN_FLIPPER:
        scanIdx = 2;
        count = bleManager.getFlipperCount();
        break;
      case MODE_BLE_SCAN_FLOCK:
        scanIdx = 3;
        count = bleManager.getFlockCount();
        break;
      case MODE_BLE_SCAN_ANALYZER:
        scanIdx = 4;
        count = bleManager.getAnalyzerValue();
        break;
      case MODE_BLE_SCAN_SIMPLE:
      case MODE_BLE_SCAN_SIMPLE_TWO:
        scanIdx = currentMode == MODE_BLE_SCAN_SIMPLE ? 5 : 6;
        count = bleManager.getScanCount();
        break;
      default:
        break;
      }
      snprintf(bleFooter, sizeof(bleFooter), "%s CNT:%d", scanNames[scanIdx],
               count);
      sceneManager.drawBleScanMode(bleScanSelected, bleManager);
      break;
    }
    case MODE_BLE_SCAN_AIRTAG_MON: {
      int count = bleManager.getAirTagCount();
      static char airtagFooter[48];
      snprintf(airtagFooter, sizeof(airtagFooter), "AIRTAG MON CNT:%d", count);
      sceneManager.drawBleScanMode(bleScanSelected, bleManager);
      break;
    }
    case MODE_WIFI_SNIFF:
      wifiSniffManager.tick();
      sceneManager.drawWifiSniffMode(wifiSniffSelected, wifiSniffManager);
      break;
    case MODE_BLE_SCAN:
      sceneManager.drawBleScanMode(bleScanSelected, bleManager);
      break;
    case MODE_BLE_SPAM: {
      static int lastPhantomPayloadIndex = -1;
      if (bleManager.isActive()) {
        bleManager.tick();
      }
      sceneManager.drawBleSpammer(bleManager.getPacketCount());
      if (lastPhantomPayloadIndex >= 0 &&
          bleManager.getCurrentPayloadIndex() != lastPhantomPayloadIndex)
        display.applyGlitch();
      lastPhantomPayloadIndex = bleManager.getCurrentPayloadIndex();
      break;
    }
    case MODE_BADWOLF:
      sceneManager.drawBadWolf(badWolfScriptIndex);
      break;
    case MODE_WIFI_TRAP:
      if (trapManager.isActive()) {
        trapManager.tick();
      }
      sceneManager.drawTrapMode(
          trapManager.getClientCount(), trapManager.getLogsCaptured(),
          trapManager.getLastPassword(), trapManager.getLastPasswordShowUntil(),
          trapManager.getClonedSSID());
      break;
    case MODE_VAULT: {
      vaultManager.tick();
      static char codeBuf[8];
      vaultManager.getCurrentCode(codeBuf, sizeof(codeBuf));
      sceneManager.drawVaultMode(
          vaultManager.getAccountName(vaultManager.getCurrentIndex()), codeBuf,
          vaultManager.getCountdownSeconds());
      break;
    }
    case MODE_FAKE_LOGIN:
      sceneManager.drawFakeLoginMode();
      break;
    case MODE_QR:
      sceneManager.drawQrMode("NOCTURNE_OS");
      break;
    case MODE_MDNS:
      mdnsManager.tick();
      sceneManager.drawMdnsMode(mdnsManager.getServiceName(),
                                mdnsManager.isActive());
      break;
    default:
      break;
    }
  }

  if (settings.glitchEnabled)
    display.applyGlitch();
  if (toastUntil && now >= toastUntil) {
    toastUntil = 0;
    toastMsg[0] = '\0';
  }
  if (toastUntil && now < toastUntil && toastMsg[0])
    sceneManager.drawToast(toastMsg);
  display.sendBuffer();

  // Оптимизированная задержка - yield только при необходимости
  static unsigned long lastMainYield = 0;
  if (now - lastMainYield > 10) {
    yield();
    lastMainYield = now;
  }
}
