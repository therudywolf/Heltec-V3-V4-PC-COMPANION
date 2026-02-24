/*
 * NOCTURNE_OS — OBD-II / ELM327 client (stub implementation).
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

/* OBD-II: 41 0C A B -> RPM = ((A*256)+B)/4. ELM327 response e.g. "41 0C 12 34". */
int ObdClient::parseRpm(const char *line) {
  const char *p = strstr(line, "41 0C");
  if (p)
    p += 6;  /* skip "41 0C " */
  else {
    p = strstr(line, "410C");
    if (p)
      p += 4;  /* skip "410C" (no space) */
    else
      return -1;
  }
  unsigned int a = 0, b = 0;
  if (sscanf(p, "%x %x", &a, &b) >= 2)
    return (int)((a * 256u + b) / 4);
  return -1;
}

/* 41 05 xx -> coolant = xx - 40. Response e.g. "41 05 A3". */
int ObdClient::parseCoolantTemp(const char *line) {
  const char *p = strstr(line, "41 05");
  if (p)
    p += 6;
  else {
    p = strstr(line, "4105");
    if (p)
      p += 4;
    else
      return -1;
  }
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)x - 40;
  return -1;
}

/* 41 5C xx -> oil = xx - 40. Response e.g. "41 5C 8F". */
int ObdClient::parseOilTemp(const char *line) {
  const char *p = strstr(line, "41 5C");
  if (p)
    p += 6;
  else {
    p = strstr(line, "415C");
    if (p)
      p += 4;
    else
      return -1;
  }
  unsigned int x = 0;
  if (sscanf(p, "%x", &x) >= 1)
    return (int)x - 40;
  return -1;
}

void ObdClient::tick() {
  if (!enabled_ || !begun_)
    return;
  unsigned long now = millis();

  switch (state_) {
    case OBD_IDLE:
      if (now - stateStartMs_ >= kPollIntervalMs) {
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
    case OBD_REQ_RPM:
      sendCommand("01 0C");
      state_ = OBD_WAIT_RPM;
      stateStartMs_ = now;
      break;
    case OBD_WAIT_RPM: {
      char line[64];
      if (readLine(line, sizeof(line), 200)) {
        int r = parseRpm(line);
        if (r >= 0)
          lastRpm_ = r;
      }
      if (now - stateStartMs_ >= kCmdTimeoutMs) {
        state_ = OBD_REQ_COOLANT;
        stateStartMs_ = now;
      } else if (lastRpm_ >= 0 || now - stateStartMs_ > 300) {
        state_ = OBD_REQ_COOLANT;
        stateStartMs_ = now;
      }
      break;
    }
    case OBD_REQ_COOLANT:
      sendCommand("01 05");
      state_ = OBD_WAIT_COOLANT;
      stateStartMs_ = now;
      break;
    case OBD_WAIT_COOLANT: {
      char line[64];
      if (readLine(line, sizeof(line), 200)) {
        int c = parseCoolantTemp(line);
        if (c >= -40 && c <= 215)
          lastCoolantC_ = c;
      }
      if (now - stateStartMs_ >= kCmdTimeoutMs || now - stateStartMs_ > 300) {
        state_ = OBD_REQ_OIL;
        stateStartMs_ = now;
      }
      break;
    }
    case OBD_REQ_OIL:
      sendCommand("01 5C");
      state_ = OBD_WAIT_OIL;
      stateStartMs_ = now;
      break;
    case OBD_WAIT_OIL: {
      char line[64];
      if (readLine(line, sizeof(line), 200)) {
        int o = parseOilTemp(line);
        if (o >= -40 && o <= 210)
          lastOilC_ = o;
      }
      if (now - stateStartMs_ >= kCmdTimeoutMs || now - stateStartMs_ > 300) {
        if (dataCb_)
          dataCb_(true, lastRpm_, lastCoolantC_, lastOilC_);
        state_ = OBD_IDLE;
        stateStartMs_ = now;
      }
      break;
    }
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
