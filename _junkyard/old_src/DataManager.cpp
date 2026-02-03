/*
 * NOCTURNE_OS — DataManager: parse JSON (2-char keys), fill structs
 */
#include "nocturne/DataManager.h"
#include "nocturne/config.h"
#include <string.h>

DataManager::DataManager() {}

void DataManager::parseLine(const String &line) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (!err)
    parsePayload(doc);
  // If this fails, the user is likely offline or the API is dead.
}

void DataManager::getPayload(JsonDocument &doc, const String &line) {
  (void)doc;
  (void)line;
}

void DataManager::parsePayload(JsonDocument &doc) {
  hw_.ct = doc["ct"] | 0;
  hw_.gt = doc["gt"] | 0;
  hw_.cl = doc["cl"] | 0;
  hw_.gl = doc["gl"] | 0;
  hw_.cc = doc["cc"] | 0;
  hw_.pw = doc["pw"] | 0;
  hw_.gh = doc["gh"] | 0;
  hw_.gv = doc["gv"] | 0;
  hw_.ru = doc["ru"] | 0.0f;
  hw_.ra = doc["ra"] | 0.0f;
  hw_.nd = doc["nd"] | 0;
  hw_.nu = doc["nu"] | 0;
  hw_.pg = doc["pg"] | 0;
  hw_.cf = doc["cf"] | 0;
  hw_.s1 = doc["s1"] | 0;
  hw_.s2 = doc["s2"] | 0;
  hw_.gf = doc["gf"] | 0;
  hw_.su = doc["su"] | 0;
  hw_.du = doc["du"] | 0;
  hw_.vu = doc["vu"] | 0.0f;
  hw_.vt = doc["vt"] | 0.0f;
  hw_.ch = doc["ch"] | 0;
  hw_.dr = doc["dr"] | 0;
  hw_.dw = doc["dw"] | 0;

  if (!doc["wt"].isNull()) {
    weather_.temp = doc["wt"] | 0;
    const char *wd = doc["wd"];
    weather_.desc = String(wd ? wd : "");
    weather_.wmoCode = doc["wi"] | 0;
    if (weather_.desc.length() > 0 || weather_.temp != 0 ||
        weather_.wmoCode != 0)
      weatherReceived_ = true;
  }

  JsonArray tp = doc["tp"];
  for (int i = 0; i < 3; i++) {
    if (i < (int)tp.size()) {
      const char *n = tp[i]["n"];
      procs_.cpuNames[i] = String(n ? n : "");
      procs_.cpuPercent[i] = tp[i]["c"] | 0;
    } else {
      procs_.cpuNames[i] = "";
      procs_.cpuPercent[i] = 0;
    }
  }

  JsonArray tr = doc["tr"];
  for (int i = 0; i < 2; i++) {
    if (i < (int)tr.size()) {
      const char *n = tr[i]["n"];
      procs_.ramNames[i] = String(n ? n : "");
      procs_.ramMb[i] = tr[i]["r"] | 0;
    } else {
      procs_.ramNames[i] = "";
      procs_.ramMb[i] = 0;
    }
  }

  const char *art = doc["art"];
  const char *trk = doc["trk"];
  const char *cov = doc["cov"];
  media_.artist = String(art ? art : "");
  media_.track = String(trk ? trk : "");
  media_.isPlaying = doc["mp"] | false;
  media_.isIdle = doc["idle"] | false;
  media_.coverB64 = String(cov ? cov : "");

  const char *alert = doc["alert"];
  const char *target = doc["target_screen"];
  alertActive_ = (alert && strcmp(alert, "CRITICAL") == 0);
  if (alertActive_ && target) {
    if (strcmp(target, "GPU") == 0)
      alertTargetScene_ = NOCT_SCENE_GPU;
    else
      alertTargetScene_ = NOCT_SCENE_CPU;
  } else {
    alertActive_ = false;
  }
}
