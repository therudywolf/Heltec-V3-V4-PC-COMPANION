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
  float used_gb = 0.0f;       /* Used space GB (u in JSON) */
  float total_gb = 0.0f;      /* Total space GB (tot in JSON) */
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
  int fan_controls[NOCT_FAN_COUNT] = {
      0, 0, 0, 0}; /* Fan control %: CPU, Pump, GPU, Case */
  HddEntry hdd[NOCT_HDD_COUNT] = {};
  float vu = 0.0f, vt = 0.0f;
  int ch = 0;
  int mb_sys = 0, mb_vsoc = 0, mb_vrm = 0,
      mb_chipset = 0; /* Motherboard temps */
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

/** External events from Prometheus Alertmanager (server "events" block). */
struct EventsData {
  static const int kMaxList = 4;
  int count = 0;            // "n" number of firing alerts
  char top[21] = {0};       // "top" highest-severity alert name (banner)
  char severity[12] = {0};  // "sev" severity of top
  char list[kMaxList][21] = {{0}};  // "list" up to 4 firing alert names (events scene)
  char text[61] = {0};      // "txt" human summary of the top alert (TG-style text)
};

/** Forest panel: one monitored node (server "forest".nodes[i]). */
struct ForestNode {
  char name[17] = {0};   // "name"
  char status[6] = {0};  // "st": up | warn | down
  int cpu = -1;          // "cpu" percent, -1 = n/a
  int ram = -1;          // "ram" percent
  int disk = -1;         // "disk" percent
  char extra[17] = {0};  // "extra" short free text
};

/** Forest panel block (server "forest"). */
struct ForestData {
  static const int kMaxNodes = 6;
  int count = 0;         // "n"
  int up = 0;            // "up"
  ForestNode nodes[kMaxNodes];
};

/** Service-status panel: one probed service (server "svc".list[i]). */
struct ServiceEntry {
  char name[17] = {0};   // "name"
  char status[6] = {0};  // "st": up | warn | down
  int ms = -1;           // "ms" round-trip latency, -1 = n/a
};

/** Service-status block (server "svc"). */
struct ServiceData {
  static const int kMaxServices = 8;
  int count = 0;         // "n"
  int up = 0;            // "up"
  ServiceEntry list[kMaxServices];
};

/** Claude Code usage/limits (from server "claude" block). pct fields are -1
 * when the server has no real source for them (render "n/a"). */
struct ClaudeData {
  bool available = false;     // "ok"
  String plan = "";           // "plan" (e.g. "max"); empty if unknown
  int windowPct = -1;         // "win" 5h window usage %, -1 = n/a
  int weeklyPct = -1;         // "wk" weekly usage %, -1 = n/a
  int resetsInMin = -1;       // "rst" minutes to 5h-window reset, -1 = n/a
  int weeklyResetMin = -1;    // "wrst" minutes to weekly reset, -1 = n/a
  long todayTokens = 0;       // "tok" tokens today (all models)
  int todayMsgs = 0;          // "msg" messages today
  int todayTools = 0;         // "tool" tool calls today
  String date = "";           // "day" date the figures apply to (YYYY-MM-DD)
  bool stale = false;         // "stale" true if "day" is not today (data is old)
};

struct Settings {
  bool ledEnabled = true;
  bool carouselEnabled = false;
  int carouselIntervalSec = 10;
  int displayContrast = 128;
  int displayTimeoutSec = 0;   /* 0=off, 30/60 = dim after sec of no input */
  bool displayInverted = false;
  bool glitchEnabled = false;
  bool lowBrightnessDefault =
      false; /* NVS "lowBright": start with dim display */
  int pinnedScene = -1; /* NVS "pinScene": home monitoring scene; -1 = none (#10) */
};

/** Single app state: hardware, weather, media, process, alerts (filled by
 * NetManager) */
struct AppState {
  HardwareData hw;
  WeatherData weather;
  MediaData media;
  ProcessData process;
  ClaudeData claude;
  EventsData events;
  ForestData forest;
  ServiceData services;
  Settings settings;
  bool weatherReceived = false;
  bool alertActive = false;
  int alertTargetScene = 0; /* NOCT_SCENE_MAIN/CPU/GPU/RAM/DISKS/MEDIA */
  int alertMetric = -1;     /* 0=ct, 1=gt, 2=cl, 3=gl, 4=gv, 5=ram; -1=none */
  /* Battery HUD: 0..100%, voltage, charging (visible in header) */
  int batteryPct = 0;
  float batteryVoltage = 0.0f;
  bool isCharging = false;
  /* PC presence (server "pidle"/"clk"): idle seconds (-1=unknown) + HH:MM. */
  int pcIdleSec = -1;
  char pcClock[6] = {0};
};

#endif
