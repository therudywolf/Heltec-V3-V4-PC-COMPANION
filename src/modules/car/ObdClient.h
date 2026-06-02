/*
 * NOCTURNE_OS — OBD-II / ELM327 client.
 *
 * Polls an ELM327 adapter for live PIDs and reads/clears stored DTC fault codes.
 * Transport is UART (Serial2) today; the read/write/readLine seam is the single
 * place to swap in Bluetooth-SPP or USB-CDC-host later (see docs/ROADMAP Sprint 8).
 *
 * Live PIDs: RPM (010C), coolant (0105), oil (015C), speed (010D),
 *            engine load (0104), throttle (0111), intake air temp (010F).
 * Diagnostics: DTC count + stored codes via mode 03; clear via mode 04.
 */
#ifndef NOCTURNE_OBD_CLIENT_H
#define NOCTURNE_OBD_CLIENT_H

#include <Arduino.h>
#include <cstdint>

#ifndef NOCT_OBD_MAX_DTC
#define NOCT_OBD_MAX_DTC 8   /* most stored codes we surface */
#endif

/** Snapshot of the latest OBD readings (for the diagnostics scene). */
struct ObdData {
  bool connected = false;
  int rpm = -1;
  int coolantC = -1;
  int oilC = -1;
  int speedKmh = -1;
  int loadPct = -1;
  int throttlePct = -1;
  int intakeC = -1;
  int dtcCount = 0;
  char dtc[NOCT_OBD_MAX_DTC][6] = {};  /* e.g. "P0301" + NUL */
};

class ObdClient {
 public:
  ObdClient() = default;

  /** Start UART to ELM327 (Serial2 on txPin/rxPin). No-op if already begun. */
  void begin(int txPin, int rxPin, uint32_t baud = 38400);

  void tick();

  /** Legacy callback: (connected, rpm, coolantC, oilC) → BmwManager::setObdData. */
  void setDataCallback(void (*cb)(bool connected, int rpm, int coolantC, int oilC)) { dataCb_ = cb; }

  bool isEnabled() const { return enabled_; }

  /** Latest readings snapshot (live PIDs + DTC list). */
  const ObdData &data() const { return data_; }

  /** Request a clear of stored DTCs (mode 04) on the next cycle. */
  void requestClearDtc() { clearDtcRequested_ = true; }

 private:
  void sendCommand(const char *cmd);
  bool readLine(char *buf, size_t maxLen, unsigned long timeoutMs);
  int parseRpm(const char *line);
  int parseCoolantTemp(const char *line);
  int parseOilTemp(const char *line);
  int parseSpeed(const char *line);
  int parseLoad(const char *line);
  int parseThrottle(const char *line);
  int parseIntakeTemp(const char *line);
  /** Parse a mode 03 response ("43 01 33 ...") into DTC strings. Returns count. */
  int parseDtc(const char *line);

  bool enabled_ = false;
  bool begun_ = false;
  bool clearDtcRequested_ = false;
  int txPin_ = -1;
  int rxPin_ = -1;
  void (*dataCb_)(bool, int, int, int) = nullptr;

  ObdData data_;

  enum State {
    OBD_IDLE,
    OBD_INIT,
    OBD_REQ_RPM,      OBD_WAIT_RPM,
    OBD_REQ_COOLANT,  OBD_WAIT_COOLANT,
    OBD_REQ_OIL,      OBD_WAIT_OIL,
    OBD_REQ_SPEED,    OBD_WAIT_SPEED,
    OBD_REQ_LOAD,     OBD_WAIT_LOAD,
    OBD_REQ_THROTTLE, OBD_WAIT_THROTTLE,
    OBD_REQ_INTAKE,   OBD_WAIT_INTAKE,
    OBD_REQ_DTC,      OBD_WAIT_DTC,
    OBD_REQ_CLEAR,    OBD_WAIT_CLEAR,
  };
  State state_ = OBD_IDLE;
  unsigned long stateStartMs_ = 0;
  static const unsigned long kCmdTimeoutMs = 1500;
  static const unsigned long kPollIntervalMs = 500;
  static const unsigned long kDtcIntervalMs = 5000;  /* DTC scan less often */
  unsigned long lastDtcMs_ = 0;

  int lastRpm_ = 0;
  int lastCoolantC_ = -1;
  int lastOilC_ = -1;
  uint8_t uartNum_ = 2;  // Serial2
};

#endif
