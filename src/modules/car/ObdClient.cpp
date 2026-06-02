/*
 * NOCTURNE_OS — OBD-II / ELM327 client.
 * Live PIDs (RPM/coolant/oil/speed/load/throttle/intake) + DTC read/clear.
 */
#include "ObdClient.h"
#include "nocturne/config.h"
#include <HardwareSerial.h>
#include <stdio.h>
#include <string.h>

#if NOCT_OBD_ENABLED

static HardwareSerial *s_obdSerial = nullptr;

static HardwareSerial &getObdSerial() {
  if (!s_obdSerial)
    s_obdSerial = &Serial2;
  return *s_obdSerial;
}

void ObdClient::begin(int txPin, int rxPin, uint32_t baud) {
  if (begun_)
    return;
  txPin_ = txPin;
  rxPin_ = rxPin;
  if (txPin_ < 0 || rxPin_ < 0)
    return;
  HardwareSerial &ser = getObdSerial();
  ser.begin(baud, SERIAL_8N1, rxPin, txPin);
  begun_ = true;
  enabled_ = true;
  state_ = OBD_INIT;
  stateStartMs_ = millis();
}

void ObdClient::sendCommand(const char *cmd) {
  if (!begun_ || !cmd)
    return;
  getObdSerial().println(cmd);
}

bool ObdClient::readLine(char *buf, size_t maxLen, unsigned long timeoutMs) {
  if (!buf || maxLen == 0)
    return false;
  buf[0] = '\0';
  unsigned long start = millis();
  size_t i = 0;
  HardwareSerial &ser = getObdSerial();
  while (millis() - start < timeoutMs && i < maxLen - 1) {
    if (ser.available()) {
      int c = ser.read();
      if (c == '\r' || c == '\n') {
        if (i > 0)
          break;
        continue;
      }
      if (c >= ' ' && c <= '~')
        buf[i++] = (char)c;
    }
  }
  buf[i] = '\0';
  return i > 0;
}

/* Locate the data bytes after a mode-01 response prefix "41 <pid>". Handles both
   spaced ("41 0C 12 34") and unspaced ("410C1234") ELM327 output. Returns a
   pointer just past the prefix, or nullptr. */
static const char *afterPid(const char *line, char p1, char p2) {
  char spaced[6] = {'4', '1', ' ', p1, p2, '\0'};
  char tight[5] = {'4', '1', p1, p2, '\0'};
  const char *q = strstr(line, spaced);
  if (q)
    return q + 5;
  q = strstr(line, tight);
  if (q)
    return q + 4;
  return nullptr;
}

/* 41 0C A B -> RPM = ((A*256)+B)/4. */
int ObdClient::parseRpm(const char *line) {
  const char *p = afterPid(line, '0', 'C');
  if (!p) return -1;
  unsigned int a = 0, b = 0;
  if (sscanf(p, "%x %x", &a, &b) >= 2)
    return (int)((a * 256u + b) / 4);
  return -1;
}

/* 41 05 xx -> coolant = xx - 40. */
int ObdClient::parseCoolantTemp(const char *line) {
  const char *p = afterPid(line, '0', '5');
  if (!p) return -1;
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)x - 40;
  return -1;
}

/* 41 5C xx -> oil = xx - 40. */
int ObdClient::parseOilTemp(const char *line) {
  const char *p = afterPid(line, '5', 'C');
  if (!p) return -1;
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)x - 40;
  return -1;
}

/* 41 0D xx -> speed km/h = xx. */
int ObdClient::parseSpeed(const char *line) {
  const char *p = afterPid(line, '0', 'D');
  if (!p) return -1;
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)x;
  return -1;
}

/* 41 04 xx -> engine load % = xx*100/255. */
int ObdClient::parseLoad(const char *line) {
  const char *p = afterPid(line, '0', '4');
  if (!p) return -1;
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)((x * 100u) / 255u);
  return -1;
}

/* 41 11 xx -> throttle % = xx*100/255. */
int ObdClient::parseThrottle(const char *line) {
  const char *p = afterPid(line, '1', '1');
  if (!p) return -1;
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)((x * 100u) / 255u);
  return -1;
}

/* 41 0F xx -> intake air temp = xx - 40. */
int ObdClient::parseIntakeTemp(const char *line) {
  const char *p = afterPid(line, '0', 'F');
  if (!p) return -1;
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)x - 40;
  return -1;
}

/* Mode 03 response: "43 <A1 B1> <A2 B2> ..." each 2 bytes = one DTC.
   First nibble of A: 0=P,1=C,2=B,3=U; next nibbles are digits. e.g. 0133 -> P0133. */
int ObdClient::parseDtc(const char *line) {
  const char *p = strstr(line, "43");
  if (!p)
    return 0;
  p += 2;
  static const char kSys[4] = {'P', 'C', 'B', 'U'};
  int count = 0;
  unsigned int a = 0, b = 0;
  /* Read pairs of hex bytes until none left or buffer full. */
  while (count < NOCT_OBD_MAX_DTC) {
    /* Skip spaces. */
    while (*p == ' ')
      p++;
    if (sscanf(p, "%2x", &a) != 1)
      break;
    p += 2;
    while (*p == ' ')
      p++;
    if (sscanf(p, "%2x", &b) != 1)
      break;
    p += 2;
    if (a == 0 && b == 0)
      continue;  /* 0000 = empty slot */
    char sys = kSys[(a >> 6) & 0x03];
    int d1 = (a >> 4) & 0x03;
    int d2 = a & 0x0F;
    int d3 = (b >> 4) & 0x0F;
    int d4 = b & 0x0F;
    snprintf(data_.dtc[count], sizeof(data_.dtc[count]), "%c%d%X%X%X", sys, d1, d2, d3, d4);
    count++;
  }
  return count;
}

void ObdClient::tick() {
  if (!enabled_ || !begun_)
    return;
  unsigned long now = millis();
  char line[96];

  switch (state_) {
    case OBD_IDLE:
      if (clearDtcRequested_) {
        clearDtcRequested_ = false;
        state_ = OBD_REQ_CLEAR;
        stateStartMs_ = now;
      } else if (now - stateStartMs_ >= kPollIntervalMs) {
        state_ = OBD_REQ_RPM;
        stateStartMs_ = now;
      }
      break;

    case OBD_INIT:
      if (now - stateStartMs_ >= 500) {
        sendCommand("ATZ");
        state_ = OBD_REQ_RPM;
        stateStartMs_ = now;
      }
      break;

    /* ── Live PIDs: request → wait(parse, advance on data or ~300ms) ── */
    case OBD_REQ_RPM:
      sendCommand("01 0C"); state_ = OBD_WAIT_RPM; stateStartMs_ = now; break;
    case OBD_WAIT_RPM:
      if (readLine(line, sizeof(line), 200)) { int v = parseRpm(line); if (v >= 0) { lastRpm_ = v; data_.rpm = v; } }
      if (now - stateStartMs_ > 300) { state_ = OBD_REQ_COOLANT; stateStartMs_ = now; }
      break;

    case OBD_REQ_COOLANT:
      sendCommand("01 05"); state_ = OBD_WAIT_COOLANT; stateStartMs_ = now; break;
    case OBD_WAIT_COOLANT:
      if (readLine(line, sizeof(line), 200)) { int v = parseCoolantTemp(line); if (v >= -40 && v <= 215) { lastCoolantC_ = v; data_.coolantC = v; } }
      if (now - stateStartMs_ > 300) { state_ = OBD_REQ_OIL; stateStartMs_ = now; }
      break;

    case OBD_REQ_OIL:
      sendCommand("01 5C"); state_ = OBD_WAIT_OIL; stateStartMs_ = now; break;
    case OBD_WAIT_OIL:
      if (readLine(line, sizeof(line), 200)) { int v = parseOilTemp(line); if (v >= -40 && v <= 210) { lastOilC_ = v; data_.oilC = v; } }
      if (now - stateStartMs_ > 300) { state_ = OBD_REQ_SPEED; stateStartMs_ = now; }
      break;

    case OBD_REQ_SPEED:
      sendCommand("01 0D"); state_ = OBD_WAIT_SPEED; stateStartMs_ = now; break;
    case OBD_WAIT_SPEED:
      if (readLine(line, sizeof(line), 200)) { int v = parseSpeed(line); if (v >= 0 && v <= 255) data_.speedKmh = v; }
      if (now - stateStartMs_ > 300) { state_ = OBD_REQ_LOAD; stateStartMs_ = now; }
      break;

    case OBD_REQ_LOAD:
      sendCommand("01 04"); state_ = OBD_WAIT_LOAD; stateStartMs_ = now; break;
    case OBD_WAIT_LOAD:
      if (readLine(line, sizeof(line), 200)) { int v = parseLoad(line); if (v >= 0 && v <= 100) data_.loadPct = v; }
      if (now - stateStartMs_ > 300) { state_ = OBD_REQ_THROTTLE; stateStartMs_ = now; }
      break;

    case OBD_REQ_THROTTLE:
      sendCommand("01 11"); state_ = OBD_WAIT_THROTTLE; stateStartMs_ = now; break;
    case OBD_WAIT_THROTTLE:
      if (readLine(line, sizeof(line), 200)) { int v = parseThrottle(line); if (v >= 0 && v <= 100) data_.throttlePct = v; }
      if (now - stateStartMs_ > 300) { state_ = OBD_REQ_INTAKE; stateStartMs_ = now; }
      break;

    case OBD_REQ_INTAKE:
      sendCommand("01 0F"); state_ = OBD_WAIT_INTAKE; stateStartMs_ = now; break;
    case OBD_WAIT_INTAKE:
      if (readLine(line, sizeof(line), 200)) { int v = parseIntakeTemp(line); if (v >= -40 && v <= 215) data_.intakeC = v; }
      if (now - stateStartMs_ > 300) {
        data_.connected = true;
        if (dataCb_)
          dataCb_(true, lastRpm_, lastCoolantC_, lastOilC_);
        /* Periodically scan DTCs; otherwise loop back to live PIDs. */
        if (now - lastDtcMs_ >= kDtcIntervalMs) {
          lastDtcMs_ = now;
          state_ = OBD_REQ_DTC;
        } else {
          state_ = OBD_IDLE;
        }
        stateStartMs_ = now;
      }
      break;

    /* ── DTC read (mode 03) ── */
    case OBD_REQ_DTC:
      sendCommand("03"); state_ = OBD_WAIT_DTC; stateStartMs_ = now; break;
    case OBD_WAIT_DTC:
      if (readLine(line, sizeof(line), 400)) {
        int n = parseDtc(line);
        data_.dtcCount = n;
      }
      if (now - stateStartMs_ > 500) { state_ = OBD_IDLE; stateStartMs_ = now; }
      break;

    /* ── DTC clear (mode 04) ── */
    case OBD_REQ_CLEAR:
      sendCommand("04"); state_ = OBD_WAIT_CLEAR; stateStartMs_ = now; break;
    case OBD_WAIT_CLEAR:
      if (now - stateStartMs_ > 500) {
        data_.dtcCount = 0;
        for (int i = 0; i < NOCT_OBD_MAX_DTC; i++)
          data_.dtc[i][0] = '\0';
        state_ = OBD_IDLE;
        stateStartMs_ = now;
      }
      break;
  }
}

#else  /* !NOCT_OBD_ENABLED */

void ObdClient::begin(int txPin, int rxPin, uint32_t baud) {
  (void)txPin;
  (void)rxPin;
  (void)baud;
}

void ObdClient::tick() {}

#endif
