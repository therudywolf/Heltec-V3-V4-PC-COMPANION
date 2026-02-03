/*
 * NOCTURNE_OS — Shared data types (2-char keys from monitor)
 */
#ifndef NOCTURNE_TYPES_H
#define NOCTURNE_TYPES_H

#include <Arduino.h>

struct HardwareData {
  int ct = 0, gt = 0, cl = 0, gl = 0;
  int cc = 0; // CPU clock (MHz)
  int pw = 0; // CPU power (W)
  int gh = 0; // GPU HotSpot (°C)
  int gv = 0; // VRAM load (%)
  float ru = 0.0f, ra = 0.0f;
  int nd = 0, nu = 0;
  int pg = 0;
  int cf = 0, s1 = 0, s2 = 0, gf = 0;
  int su = 0, du = 0;
  float vu = 0.0f, vt = 0.0f;
  int ch = 0;
  int dr = 0, dw = 0;
};

struct WeatherData {
  int temp = 0;
  String desc = "";
  int wmoCode = 0; // WMO weather code for icon mapping
};

struct ProcessData {
  String cpuNames[3] = {"", "", ""};
  int cpuPercent[3] = {0};
  String ramNames[2] = {"", ""};
  int ramMb[2] = {0};
};

struct MediaData {
  String artist = "";
  String track = "";
  bool isPlaying = false;
  bool isIdle = false;  // Paused / no session — show IDLE + sleep icon
  String coverB64 = ""; // 64x64 1-bit bitmap, base64 (512 bytes decoded)
};

struct Settings {
  bool ledEnabled = true;
  bool carouselEnabled = false;
  int carouselIntervalSec = 10;
  int displayContrast = 128;
  bool displayInverted = false;
};

#endif
