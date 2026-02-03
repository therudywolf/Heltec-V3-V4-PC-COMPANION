/*
 * NOCTURNE_OS — Shared types: HardwareData (ct, gt, cl, gl, ru, ra, …),
 * WeatherData, MediaData, AppState. Keys match monitor JSON (2-letter).
 */
#ifndef NOCTURNE_TYPES_H
#define NOCTURNE_TYPES_H

#include <Arduino.h>

#define NOCT_HDD_COUNT 4
#define NOCT_FAN_COUNT 4

struct HddEntry {
  char name[2] = {'C', '\0'}; /* Drive letter: C, D, E, F */
  int load = 0;               /* Usage percent (u in JSON) */
  int temp = 0;               /* Temperature (t in JSON) */
};

struct HardwareData {
  int ct = 0, gt = 0, cl = 0, gl = 0;
  int cc = 0, pw = 0, gh = 0, gv = 0;
  int gclock = 0, vclock = 0, gtdp = 0;
  float ru = 0.0f, ra = 0.0f;
  int nd = 0, nu = 0, pg = 0;
  int cf = 0, s1 = 0, s2 = 0, gf = 0;
  int fans[NOCT_FAN_COUNT] = {0, 0, 0, 0};
  HddEntry hdd[NOCT_HDD_COUNT] = {};
  float vu = 0.0f, vt = 0.0f;
  int ch = 0;
  int dr = 0, dw = 0;
};

struct WeatherData {
  int temp = 0;
  String desc = "";
  int wmoCode = 0;
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
  bool isIdle = false;
  String mediaStatus = "PAUSED"; // "PLAYING" | "PAUSED"
};

struct Settings {
  bool ledEnabled = true;
  bool carouselEnabled = false;
  int carouselIntervalSec = 10;
  int displayContrast = 128;
  bool displayInverted = false;
};

/** Single app state: hardware, weather, media, process, alerts (filled by
 * NetManager) */
struct AppState {
  HardwareData hw;
  WeatherData weather;
  MediaData media;
  ProcessData process;
  Settings settings;
  bool weatherReceived = false;
  bool alertActive = false;
  int alertTargetScene = 0; /* NOCT_SCENE_MAIN/CPU/GPU/RAM/DISKS/MEDIA */
  int alertMetric = -1;     /* 0=ct, 1=gt, 2=cl, 3=gl, 4=gv, 5=ram; -1=none */
};

#endif
