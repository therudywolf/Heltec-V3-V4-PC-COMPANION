/*
 * NOCTURNE_OS — Unified app mode enum and manager.
 * Modes compiled conditionally via NOCT_FEATURE_* flags.
 */
#ifndef NOCTURNE_APP_MODE_MANAGER_H
#define NOCTURNE_APP_MODE_MANAGER_H

#include "nocturne/config.h"

enum AppMode
{
#if NOCT_FEATURE_BMW
  MODE_BMW_ASSISTANT,
#endif
  MODE_CHARGE_ONLY,
  MODE_SYSINFO,         // NTP clock + RF/system info (heap/temp/MAC/RSSI) — bonus
  MODE_BENCH,           // I2C bus scanner + chip/GPIO bench — bonus
#if NOCT_FEATURE_MONITORING
  MODE_NORMAL,
#endif
#if NOCT_FEATURE_FORZA
  MODE_GAME_FORZA,
#endif
#if NOCT_FEATURE_HACKER
  MODE_RADAR,
  MODE_WIFI_PROBE_SCAN,
  MODE_WIFI_EAPOL_SCAN,
  MODE_WIFI_STATION_SCAN,
  MODE_WIFI_PACKET_MONITOR,
  MODE_WIFI_CHANNEL_ANALYZER,
  MODE_WIFI_CHANNEL_ACTIVITY,
  MODE_WIFI_PACKET_RATE,
  MODE_WIFI_PINESCAN,
  MODE_WIFI_MULTISSID,
  MODE_WIFI_SIGNAL_STRENGTH,
  MODE_WIFI_RAW_CAPTURE,
  MODE_WIFI_AP_STA,
  MODE_WIFI_SNIFF,
  MODE_BLE_SCAN,        // passive BLE scan + tracker detection (AirTag/Flipper/Tile)
  MODE_BLE_DEVICES,     // passive BLE device browser (all advertisers, by RSSI)
  MODE_EXPORT,          // pcap/csv capture export over a SoftAP web page (#24)
  MODE_FOXHUNT,         // RSSI direction-finder (WiFi/BLE source) + LED proximity
#if NOCT_FEATURE_BADUSB
  MODE_BADUSB,          // USB-HID keyboard injection (Ducky-lite, composite CDC+HID)
#endif
#endif
#if NOCT_FEATURE_LORA
  MODE_LORA,            // SX1262 packet listener: mesh RX + node list (#21, EU868)
  MODE_LORA_SWEEP,      // SX1262 wide-band RSSI spectrum sweep (863-870 MHz)
#if NOCT_FEATURE_LORA_TX
  MODE_LORA_TX,         // SX1262 transmit: beacon / mesh-ping / replay (gated)
#endif
#endif
#if NOCT_FEATURE_WOLFPET
  MODE_WOLFPET,         // tamagotchi wolf pet (#3)
#endif
};

/* The mode the device boots into / falls back to, given the build's features.
 * MODE_CHARGE_ONLY always exists, so it is the ultimate fallback. */
#if NOCT_FEATURE_BMW
#define NOCT_DEFAULT_MODE MODE_BMW_ASSISTANT
#elif NOCT_FEATURE_MONITORING
#define NOCT_DEFAULT_MODE MODE_NORMAL
#else
#define NOCT_DEFAULT_MODE MODE_CHARGE_ONLY
#endif

#if NOCT_FEATURE_BMW
class BmwManager;
#endif
#if NOCT_FEATURE_MONITORING
class NetManager;
#endif
#if NOCT_FEATURE_FORZA
class ForzaManager;
#endif
#if NOCT_FEATURE_HACKER
class WifiSniffManager;
class BleManager;
class CaptureExport;
#endif
#if NOCT_FEATURE_LORA
class LoraManager;
#endif

class AppModeManager
{
public:
  AppModeManager(
#if NOCT_FEATURE_BMW
      BmwManager &bmw,
#endif
#if NOCT_FEATURE_MONITORING
      NetManager &net,
#endif
#if NOCT_FEATURE_FORZA
      ForzaManager &forza,
#endif
#if NOCT_FEATURE_HACKER
      WifiSniffManager &wifiSniff,
      BleManager &ble,
      CaptureExport &capture,
#endif
#if NOCT_FEATURE_LORA
      LoraManager &lora,
#endif
      int reserved = 0);

  bool switchToMode(AppMode &current, AppMode next
#if NOCT_FEATURE_HACKER
      , int trapWifiSelected = -1
      , int trapFilteredCount = 0
      , const int *trapSortedIndices = nullptr
#endif
  );

#if NOCT_FEATURE_MONITORING
  void exitToNormal(AppMode &current);
#endif

private:
  void cleanupMode(AppMode mode);
  void manageWiFiState(AppMode mode);
  bool initializeMode(AppMode mode
#if NOCT_FEATURE_HACKER
      , int trapWifiSelected
      , int trapFilteredCount
      , const int *trapSortedIndices
#endif
  );

  int reserved_;  // anchors the conditional member-init list (see ctor)
#if NOCT_FEATURE_BMW
  BmwManager &bmw_;
#endif
#if NOCT_FEATURE_MONITORING
  NetManager &net_;
#endif
#if NOCT_FEATURE_FORZA
  ForzaManager &forza_;
#endif
#if NOCT_FEATURE_HACKER
  WifiSniffManager &wifiSniff_;
  BleManager &ble_;
  CaptureExport &capture_;
#endif
#if NOCT_FEATURE_LORA
  LoraManager &lora_;
#endif
};

#endif
