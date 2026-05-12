/*
 * NOCTURNE_OS — DemoManager: FreeRTOS demo task (Core 1), non-blocking telemetry queue.
 * Metric only: rpm, coolantTempC (°C), speedKmh (km/h). Strict 10-tick send timeout; 500 ms yield.
 */
#ifndef NOCTURNE_DEMO_MANAGER_H
#define NOCTURNE_DEMO_MANAGER_H

#include <atomic>
#include <cstdint>

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#endif

/** Metric telemetry pushed to BLE (km/h, °C). */
struct TelemetryData {
  int rpm;
  int coolantTempC;
  int speedKmh;
};

/** Thread-safe: set from BLE/menu, read in Task_DemoMode. */
extern std::atomic<bool> isDemoModeActive;

/** Queue: Task_DemoMode pushes; BmwManager::tick() drains. */
extern QueueHandle_t bleTelemetryQueue;

/** Create queue if needed. Call once from BmwManager::begin(). */
void demoManagerInit();

/** Create Task_DemoMode on Core 1 if not already created. Call after demoManagerInit(). */
void demoManagerEnsureTaskCreated();

/** Set demo mode on/off. */
void demoManagerSetActive(bool active);

/** Drain one packet (non-blocking). Returns true if *out was filled. */
bool demoManagerDrain(TelemetryData *out);

#endif
