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

#include "modules/BleManager.h"
#include "modules/BootAnim.h"
#include "modules/DisplayEngine.h"
#include "modules/KickManager.h"
#include "modules/LoraManager.h"
#include "modules/NetManager.h"
#include "modules/SceneManager.h"
#include "modules/TrapManager.h"
#include "modules/UsbManager.h"
#include "modules/VaultManager.h"
#include "nocturne/Types.h"
#include "nocturne/config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Local constants (after includes, before any global object instantiations)
// ---------------------------------------------------------------------------
#define NOCT_REDRAW_INTERVAL_MS 500
#define NOCT_CONFIG_MSG_MS 1500
#define NOCT_BAT_READ_INTERVAL_MS 5000
#define NOCT_BAT_SAMPLES 20
#define NOCT_BAT_CALIBRATION 1.1f
#define WOLF_MENU_ITEMS 15

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
      } else if (duration >= 500) {
        clickCount = 0;
        return EV_LONG;
      }
    }

    if (clickCount > 0 && !btnState && (now - releaseTime > 250)) {
      if (clickCount == 1)
        event = EV_SHORT;
      else if (clickCount >= 2)
        event = EV_DOUBLE;
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
LoraManager loraManager;
BleManager bleManager;
UsbManager usbManager;
TrapManager trapManager;
KickManager kickManager;
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

enum MenuState { MENU_MAIN };
MenuState menuState = MENU_MAIN;
int submenuIndex =
    0; // Оставлено для совместимости с drawMenu, но не используется
bool rebootConfirmed = false;
unsigned long rebootConfirmTime = 0;

// Защита от множественных срабатываний событий
unsigned long lastMenuEventTime = 0;
#define MENU_EVENT_DEBOUNCE_MS 150

// --- Cyberdeck Modes ---
enum AppMode {
  MODE_NORMAL,
  MODE_DAEMON,
  MODE_RADAR,
  MODE_WIFI_DEAUTH,
  MODE_LORA,
  MODE_LORA_JAM,
  MODE_BLE_SPAM,
  MODE_BADWOLF,
  MODE_WIFI_TRAP,
  MODE_VAULT,
  MODE_LORA_SENSE
};
AppMode currentMode = MODE_NORMAL;

// --- Netrunner WiFi Scanner ---
int wifiScanSelected = 0;
int wifiListPage = 0;
int wifiSortMode = 0;      // 0=RSSI desc, 1=alphabetical, 2=RSSI asc
int wifiRssiFilter = -100; // Минимальный RSSI для отображения (-100 = все)
int wifiSortedIndices[32]; // Индексы отсортированных сетей
int wifiFilteredCount = 0; // Количество отфильтрованных сетей

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
static bool vextPinState =
    false; // Track GPIO 36 (Vext) state to avoid unnecessary writes

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
  // GPIO 36 (Vext) controls external power for OLED display
  // LOW = Enable power, HIGH = Disable power
  // MUST be set to LOW to power OLED display
  pinMode(NOCT_VEXT_PIN, OUTPUT);
  digitalWrite(NOCT_VEXT_PIN, LOW);
  delay(100);
  // CRITICAL: GPIO 36 must remain LOW permanently - do NOT set it HIGH
  // anywhere!
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  bootTime = millis();
  setCpuFrequencyMhz(240); // V4: max speed (also set in platformio.ini)

  // ========================================================================
  // CRITICAL: GPIO 36 (Vext) controls external power for OLED display
  // GPIO 21 is OLED RST pin (separate from Vext)
  // On Heltec V4: GPIO 36 LOW = Enable external power (OLED), HIGH = Disable
  // ========================================================================

  // Enable external power by setting GPIO 36 to LOW (Vext)
  // This must be done BEFORE display.begin() so the display has power
  // Optimized: only change state if needed
  if (!vextPinState || digitalRead(NOCT_VEXT_PIN) != LOW) {
    pinMode(NOCT_VEXT_PIN, OUTPUT);
    digitalWrite(NOCT_VEXT_PIN, LOW); // Enable power to OLED display
    vextPinState = true;
  }
  delay(150); // Allow power to stabilize before initializing display

  // Initialize OLED display
  // u8g2 library will handle the reset sequence (GPIO 21 LOW -> HIGH -> LOW)
  // automatically during begin()
  display.begin();
  delay(100); // Additional delay to ensure display is ready

  // CRITICAL: Ensure GPIO 36 remains LOW after display initialization
  // This maintains power to the OLED display
  // Optimized: only write if state changed
  if (!vextPinState || digitalRead(NOCT_VEXT_PIN) != LOW) {
    digitalWrite(NOCT_VEXT_PIN, LOW);
    vextPinState = true;
  }
  delay(50); // Small delay to ensure pin state is stable
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
  settings.displayInverted = prefs.getBool("inverted", false);
  settings.glitchEnabled = prefs.getBool("glitch", false);
  prefs.end();

  display.u8g2().setContrast((uint8_t)settings.displayContrast);
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
  case MODE_LORA:
  case MODE_LORA_JAM:
  case MODE_LORA_SENSE:
    if (mode == MODE_LORA_JAM)
      loraManager.stopJamming();
    else if (mode == MODE_LORA_SENSE)
      loraManager.stopSense();
    loraManager.setMode(false);
    break;
  case MODE_BLE_SPAM:
    bleManager.stop();
    WiFi.mode(WIFI_STA);
    break;
  case MODE_BADWOLF:
    usbManager.stop();
    break;
  case MODE_WIFI_TRAP:
    trapManager.stop();
    WiFi.mode(WIFI_STA);
    break;
  case MODE_WIFI_DEAUTH:
    kickManager.stopAttack();
    WiFi.scanDelete();
    break;
  case MODE_RADAR:
    WiFi.scanDelete();
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
  case MODE_BLE_SPAM:
  case MODE_LORA:
  case MODE_LORA_JAM:
  case MODE_LORA_SENSE:
    // WiFi должен быть выключен для этих режимов
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      // Non-blocking: use yield() instead of delay() for WiFi state change
      yield(); // Allow WiFi stack to process disconnect
      WiFi.mode(WIFI_OFF);
      Serial.println("[SYS] WiFi OFF for radio mode");
    }
    netManager.setSuspend(true);
    break;
  case MODE_RADAR:
  case MODE_WIFI_DEAUTH:
  case MODE_WIFI_TRAP:
    // WiFi нужен для этих режимов
    if (WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
      Serial.println("[SYS] WiFi STA for WiFi mode");
    }
    netManager.setSuspend(true);
    break;
  case MODE_VAULT:
  case MODE_DAEMON:
    // WiFi может быть включен, но NetManager приостановлен
    netManager.setSuspend(false);
    break;
  case MODE_BADWOLF:
    // USB режим - WiFi не нужен
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
  case MODE_LORA:
    manageWiFiState(mode);
    // Остановить другие LoRa режимы перед запуском SNIFF
    loraManager.stopJamming();
    loraManager.stopSense();
    yield(); // Allow LoRa stack to process stop commands
    loraManager.setMode(true);
    // Проверка успешности инициализации через проверку активного состояния
    yield(); // Allow LoRa stack to initialize
    Serial.println("[SYS] LoRa SNIFF mode initialized");
    return true;

  case MODE_LORA_JAM:
    manageWiFiState(mode);
    loraManager.stopSense();    // Остановить SENSE если активен
    loraManager.setMode(false); // Остановить SNIFF если активен
    yield();                    // Allow LoRa stack to process stop commands
    loraManager.startJamming(869.5f);
    if (!loraManager.isJamming()) {
      Serial.println("[SYS] LoRa JAM init failed");
      return false;
    }
    Serial.println("[SYS] LoRa JAM mode initialized");
    return true;

  case MODE_LORA_SENSE:
    manageWiFiState(mode);
    loraManager.stopJamming();  // Остановить JAM если активен
    loraManager.setMode(false); // Остановить SNIFF если активен
    yield();                    // Allow LoRa stack to process stop commands
    loraManager.startSense();
    if (!loraManager.isSensing()) {
      Serial.println("[SYS] LoRa SENSE init failed");
      return false;
    }
    Serial.println("[SYS] LoRa SENSE mode initialized");
    return true;

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
    WiFi.scanNetworks(true);
    wifiScanSelected = 0;
    wifiListPage = 0;
    wifiFilteredCount = 0;
    Serial.println("[SYS] RADAR mode initialized");
    return true;

  case MODE_WIFI_DEAUTH:
    manageWiFiState(mode);
    WiFi.scanNetworks(true);
    wifiScanSelected = 0;
    wifiListPage = 0;
    wifiFilteredCount = 0;
    kickManager.setTargetFromScan(0);
    Serial.println("[SYS] DEAUTH mode initialized");
    return true;

  case MODE_WIFI_TRAP: {
    manageWiFiState(mode);
    // Перед запуском AP можно клонировать SSID из сканирования
    // Если сканирование еще не завершено, запустим его
    int n = WiFi.scanComplete();
    if (n == -1) {
      WiFi.scanNetworks(true);
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
// Menu handlers - рефакторинг логики меню
// ---------------------------------------------------------------------------
static bool handleMainMenuAction(int item, unsigned long now) {
  // Защита от выхода за границы
  if (item < 0 || item >= WOLF_MENU_ITEMS) {
    Serial.printf("[MENU] Invalid item index: %d (max: %d)\n", item,
                  WOLF_MENU_ITEMS - 1);
    return false;
  }

  if (item == 0) { // AUTO
    settings.carouselEnabled = !settings.carouselEnabled;
    Preferences prefs;
    prefs.begin("nocturne", false);
    prefs.putBool("carousel", settings.carouselEnabled);
    prefs.end();
    return true;
  } else if (item == 1) { // FLIP
    settings.displayInverted = !settings.displayInverted;
    display.setScreenFlipped(settings.displayInverted);
    Preferences prefs;
    prefs.begin("nocturne", false);
    prefs.putBool("inverted", settings.displayInverted);
    prefs.end();
    return true;
  } else if (item == 2) { // GLITCH
    settings.glitchEnabled = !settings.glitchEnabled;
    Preferences prefs;
    prefs.begin("nocturne", false);
    prefs.putBool("glitch", settings.glitchEnabled);
    prefs.end();
    return true;
  }
  // WiFi режимы (3-5)
  else if (item == 3) { // WiFi SCAN
    quickMenuOpen = false;
    if (!switchToMode(MODE_RADAR)) {
      Serial.println("[MENU] Failed to switch to RADAR");
      return false;
    }
  } else if (item == 4) { // WiFi DEAUTH
    quickMenuOpen = false;
    wifiScanSelected = 0;
    wifiListPage = 0;
    rebootConfirmed = false;
    if (!switchToMode(MODE_WIFI_DEAUTH)) {
      Serial.println("[MENU] Failed to switch to DEAUTH");
      return false;
    }
    kickManager.setTargetFromScan(0);
  } else if (item == 5) { // WiFi PORTAL
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_WIFI_TRAP)) {
      Serial.println("[MENU] Failed to switch to TRAP");
      return false;
    }
  }
  // LoRa режимы (6-8)
  else if (item == 6) { // LoRa MESH
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_LORA)) {
      Serial.println("[MENU] Failed to switch to MESH");
      return false;
    }
  } else if (item == 7) { // LoRa JAM
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_LORA_JAM)) {
      Serial.println("[MENU] Failed to switch to JAM");
      return false;
    }
  } else if (item == 8) { // LoRa SENSE
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_LORA_SENSE)) {
      Serial.println("[MENU] Failed to switch to SENSE");
      return false;
    }
  }
  // Прочие режимы (9-12)
  else if (item == 9) { // BLE SPAM
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_BLE_SPAM)) {
      Serial.println("[MENU] Failed to switch to BLE_SPAM");
      return false;
    }
  } else if (item == 10) { // USB HID
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_BADWOLF)) {
      Serial.println("[MENU] Failed to switch to BADWOLF");
      return false;
    }
  } else if (item == 11) { // VAULT
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_VAULT)) {
      Serial.println("[MENU] Failed to switch to VAULT");
      return false;
    }
  } else if (item == 12) { // DAEMON
    quickMenuOpen = false;
    rebootConfirmed = false;
    if (!switchToMode(MODE_DAEMON)) {
      Serial.println("[MENU] Failed to switch to DAEMON");
      return false;
    }
  } else if (item == 13) { // REBOOT
    if (!rebootConfirmed) {
      rebootConfirmed = true;
      rebootConfirmTime = now;
    } else {
      rebootConfirmed = false;
      esp_restart();
    }
  } else if (item == 14) { // EXIT
    quickMenuOpen = false;
    return true;
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
        if (bufLen > 0) {
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

  // 3. Global: DOUBLE = back / enter menu
  if (event == EV_DOUBLE) {
    if (currentMode != MODE_NORMAL) {
      exitAppModeToNormal();
    } else {
      quickMenuOpen = !quickMenuOpen;
      if (quickMenuOpen) {
        menuState = MENU_MAIN;
        quickMenuItem = 0;
        rebootConfirmed = false; // Сброс подтверждения при открытии меню
      } else {
        rebootConfirmed = false; // Сброс подтверждения при закрытии меню
      }
    }
    needRedraw = true;
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
  if (now - lastFanAnim >= 100) {
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
      digitalWrite(NOCT_LED_ALERT_PIN, HIGH);
      lastBlink = now;
    }
    lastAlertActive = state.alertActive;
    if (state.alertActive) {
      if (now - lastBlink >= NOCT_ALERT_LED_BLINK_MS) {
        lastBlink = now;
        if (alertBlinkCounter < NOCT_ALERT_MAX_BLINKS * 2) {
          blinkState = !blinkState;
          digitalWrite(NOCT_LED_ALERT_PIN, blinkState ? HIGH : LOW);
          alertBlinkCounter++;
        } else {
          blinkState = false;
          digitalWrite(NOCT_LED_ALERT_PIN, LOW);
        }
      }
    } else {
      digitalWrite(NOCT_LED_ALERT_PIN, LOW);
      if (now - lastBlink > 500) {
        blinkState = !blinkState;
        lastBlink = now;
      }
    }
  }

  if (!splashDone) {
    if (splashStart == 0)
      splashStart = now;
    if (now - splashStart >= (unsigned long)NOCT_SPLASH_MS) {
      splashDone = true;
      needRedraw = true;
    }
  }

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

    // --- MENU LOGIC (FLAT STRUCTURE) ---
    if (event == EV_SHORT) {
      lastMenuEventTime = now;
      // SCROLL DOWN - только по главному меню
      int oldItem = quickMenuItem;
      quickMenuItem = (quickMenuItem + 1) % WOLF_MENU_ITEMS;
      // Сброс подтверждения при прокрутке от REBOOT
      if (oldItem == 13 && quickMenuItem != 13) {
        rebootConfirmed = false;
      }
      needRedraw = true;
    } else if (event == EV_LONG) { // SELECT / ENTER
      lastMenuEventTime = now;
      bool actionHandled = handleMainMenuAction(quickMenuItem, now);
      if (actionHandled) {
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
      } else if (event == EV_LONG) {
        predatorMode = !predatorMode;
        predatorEnterTime = now;
        needRedraw = true;
      }
      break;
    case MODE_WIFI_DEAUTH:
      if (event == EV_SHORT) {
        int n = WiFi.scanComplete();
        if (n > 0) {
          wifiScanSelected = (wifiScanSelected + 1) % n;
          kickManager.setTargetFromScan(wifiScanSelected);
          if (wifiScanSelected >= wifiListPage + 5)
            wifiListPage = wifiScanSelected - 4;
          else if (wifiScanSelected < wifiListPage)
            wifiListPage = wifiScanSelected;
        }
        needRedraw = true;
      } else if (event == EV_LONG) {
        if (kickManager.isAttacking())
          kickManager.stopAttack();
        else
          kickManager.startAttack();
        needRedraw = true;
      }
      break;
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
          WiFi.scanNetworks(true);
          wifiFilteredCount = 0;
        }
        needRedraw = true;
      }
      break;
    case MODE_LORA:
      if (event == EV_SHORT) {
        loraManager.clearBuffer();
        needRedraw = true;
      } else if (event == EV_LONG) {
        loraManager.replayLast();
        needRedraw = true;
      } else if (event == EV_DOUBLE) {
        // Двойное нажатие для переключения частотного слота
        loraManager.switchFreqSlot();
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
        usbManager.runMatrix();
        needRedraw = true;
      } else if (event == EV_LONG) {
        usbManager.runSniffer();
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

  if (!needRedraw && !guiTimer.check(now)) {
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
    display.drawGlobalHeader("MENU", nullptr, netManager.rssi(),
                             netManager.isWifiConnected());
    sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                 state.batteryVoltage);
    sceneManager.drawMenu(
        (int)menuState, quickMenuItem, 0, settings.carouselEnabled,
        settings.carouselIntervalSec, settings.displayInverted,
        settings.glitchEnabled, rebootConfirmed);
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
          display.drawGlobalHeader("NO SIGNAL", nullptr, 0, false);
          sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                       state.batteryVoltage);
          sceneManager.drawNoSignal(false, false, 0, blinkState);
        }
      } else if (!netManager.isTcpConnected()) {
        if (showScreensaver) {
          sceneManager.drawIdleScreensaver(now);
          display.applyGlitch(); /* Always light glitch on screensaver */
        } else {
          display.drawGlobalHeader("LINKING", nullptr, netManager.rssi(), true);
          sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                       state.batteryVoltage);
          sceneManager.drawConnecting(netManager.rssi(), blinkState);
        }
      } else if (netManager.isSearchMode() || signalLost) {
        display.drawGlobalHeader("SEARCH", nullptr, netManager.rssi(),
                                 netManager.isWifiConnected());
        sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                     state.batteryVoltage);
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
      display.drawGlobalHeader("RADAR", nullptr, netManager.rssi(), false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawWiFiScanner(wifiScanSelected, wifiListPage,
                                   wifiFilteredCount > 0 ? wifiSortedIndices
                                                         : nullptr,
                                   wifiFilteredCount);
      break;
    }
    case MODE_WIFI_DEAUTH: {
      int n = WiFi.scanComplete();
      static bool kickTargetInitialized = false;
      if (n <= 0) {
        kickTargetInitialized = false;
      } else {
        if (wifiFilteredCount == 0) {
          sortAndFilterWiFiNetworks();
        }
        if (!kickTargetInitialized) {
          int actualIndex =
              (wifiFilteredCount > 0 && wifiScanSelected < wifiFilteredCount)
                  ? wifiSortedIndices[wifiScanSelected]
                  : (wifiScanSelected >= 0 && wifiScanSelected < n
                         ? wifiScanSelected
                         : 0);
          kickManager.setTargetFromScan(actualIndex);
          kickTargetInitialized = true;
        }
      }
      // Автоматический запуск атаки если цель установлена и атака не активна
      if (!kickManager.isAttacking() && kickManager.isTargetSet() &&
          !kickManager.isTargetOwnAP()) {
        kickManager.startAttack();
      }

      // tick() должен вызываться всегда в активном режиме для обработки атаки
      kickManager.tick();
      display.drawGlobalHeader("DEAUTH", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawKickMode(kickManager);
      break;
    }
    case MODE_LORA:
      if (loraManager.isReceiving()) {
        loraManager.tick();
      }
      display.drawGlobalHeader("SIGINT", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawLoraSniffer(loraManager);
      break;
    case MODE_LORA_JAM:
      if (loraManager.isJamming()) {
        loraManager.tick();
      }
      display.drawGlobalHeader("JAM", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawSilenceMode(loraManager.getJamPower());
      break;
    case MODE_BLE_SPAM: {
      static int lastPhantomPayloadIndex = -1;
      if (bleManager.isActive()) {
        bleManager.tick();
      }
      display.drawGlobalHeader("BLE", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawBleSpammer(bleManager.getPacketCount());
      if (lastPhantomPayloadIndex >= 0 &&
          bleManager.getCurrentPayloadIndex() != lastPhantomPayloadIndex)
        display.applyGlitch();
      lastPhantomPayloadIndex = bleManager.getCurrentPayloadIndex();
      break;
    }
    case MODE_BADWOLF:
      display.drawGlobalHeader("USB HID", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawBadWolf();
      break;
    case MODE_WIFI_TRAP:
      if (trapManager.isActive()) {
        trapManager.tick();
      }
      display.drawGlobalHeader("PORTAL", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawTrapMode(
          trapManager.getClientCount(), trapManager.getLogsCaptured(),
          trapManager.getLastPassword(), trapManager.getLastPasswordShowUntil(),
          trapManager.getClonedSSID());
      break;
    case MODE_VAULT: {
      vaultManager.tick();
      static char codeBuf[8];
      vaultManager.getCurrentCode(codeBuf, sizeof(codeBuf));
      display.drawGlobalHeader("VAULT", nullptr, 0,
                               netManager.isWifiConnected());
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawVaultMode(
          vaultManager.getAccountName(vaultManager.getCurrentIndex()), codeBuf,
          vaultManager.getCountdownSeconds());
      break;
    }
    case MODE_LORA_SENSE:
      if (loraManager.isSensing()) {
        loraManager.tick();
      }
      display.drawGlobalHeader("SENSE", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      sceneManager.drawGhostsMode(loraManager);
      break;
    default:
      display.drawGlobalHeader("HUB", nullptr, 0, false);
      sceneManager.drawPowerStatus(state.batteryPct, state.isCharging,
                                   state.batteryVoltage);
      break;
    }
  }

  if (settings.glitchEnabled)
    display.applyGlitch();
  display.sendBuffer();

  // Оптимизированная задержка - yield только при необходимости
  static unsigned long lastMainYield = 0;
  if (now - lastMainYield > 10) {
    yield();
    lastMainYield = now;
  }
}
