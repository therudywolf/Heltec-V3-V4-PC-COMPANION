/*
 * NOCTURNE_OS — OBD-II / ELM327 client stub.
 * When NOCT_OBD_ENABLED, periodically requests RPM (01 0C), coolant (01 05), oil (01 5C)
 * and invokes callback with (connected, rpm, coolantC, oilC) for BmwManager::setObdData.
 */
#ifndef NOCTURNE_OBD_CLIENT_H
#define NOCTURNE_OBD_CLIENT_H

#include <Arduino.h>
#include <cstdint>

class ObdClient {
 public:
  ObdClient() = default;

  /** Start UART to ELM327 (e.g. Serial2 on txPin/rxPin). No-op if begin already called. */
  void begin(int txPin, int rxPin, uint32_t baud = 38400);

  void tick();

  /** Callback: (connected, rpm, coolantC, oilC). Invoked when data received or on timeout (connected=false). */
  void setDataCallback(void (*cb)(bool connected, int rpm, int coolantC, int oilC)) { dataCb_ = cb; }

  bool isEnabled() const { return enabled_; }

 private:
  void sendCommand(const char *cmd);
  bool readLine(char *buf, size_t maxLen, unsigned long timeoutMs);
  int parseRpm(const char *line);
  int parseCoolantTemp(const char *line);
  int parseOilTemp(const char *line);

  bool enabled_ = false;
  bool begun_ = false;
  int txPin_ = -1;
  int rxPin_ = -1;
  void (*dataCb_)(bool, int, int, int) = nullptr;

  enum State {
    OBD_IDLE,
    OBD_INIT,
    OBD_REQ_RPM,
    OBD_WAIT_RPM,
    OBD_REQ_COOLANT,
    OBD_WAIT_COOLANT,
    OBD_REQ_OIL,
    OBD_WAIT_OIL,
  };
  State state_ = OBD_IDLE;
  unsigned long stateStartMs_ = 0;
  static const unsigned long kCmdTimeoutMs = 1500;
  static const unsigned long kPollIntervalMs = 500;

  int lastRpm_ = 0;
  int lastCoolantC_ = -1;
  int lastOilC_ = -1;
  uint8_t uartNum_ = 2;  // Serial2
};

#endif
