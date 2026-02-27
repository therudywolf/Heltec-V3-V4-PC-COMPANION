/*
 * NOCTURNE_OS — DemoManager: Task_DemoMode on Core 1, strict timeout and vTaskDelay.
 */
#include "DemoManager.h"
#include "nocturne/config.h"
#include <Arduino.h>
#include <stdio.h>

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

std::atomic<bool> isDemoModeActive{false};
QueueHandle_t bleTelemetryQueue = nullptr;

static const unsigned int kDemoQueueLen = 32;
static bool s_demoTaskCreated = false;

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
static void Task_DemoMode(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    if (isDemoModeActive.load() && bleTelemetryQueue != nullptr) {
      TelemetryData fakeData;
      fakeData.rpm = random(800, 6001);
      fakeData.coolantTempC = random(85, 96);
      fakeData.speedKmh = random(40, 121);

      if (xQueueSend(bleTelemetryQueue, &fakeData, pdMS_TO_TICKS(10)) != pdPASS) {
#if NOCT_BMW_DEBUG
        Serial.println("Demo: Queue full, dropping packet.");
#endif
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
#endif

void demoManagerInit() {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  if (bleTelemetryQueue == nullptr)
    bleTelemetryQueue = xQueueCreate(kDemoQueueLen, sizeof(TelemetryData));
#endif
}

void demoManagerEnsureTaskCreated() {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  if (s_demoTaskCreated || bleTelemetryQueue == nullptr)
    return;
  BaseType_t created = xTaskCreatePinnedToCore(
      Task_DemoMode,
      "DemoTask",
      2048,
      nullptr,
      1,
      nullptr,
      1);
  if (created == pdPASS)
    s_demoTaskCreated = true;
#endif
}

void demoManagerSetActive(bool active) {
  isDemoModeActive.store(active);
}

bool demoManagerDrain(TelemetryData *out) {
  if (out == nullptr || bleTelemetryQueue == nullptr)
    return false;
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  return xQueueReceive(bleTelemetryQueue, out, 0) == pdPASS;
#else
  (void)out;
  return false;
#endif
}
