/*
 * Boot selector for Nocturne/Meshtastic dual boot.
 * Flashed to the factory partition. Runs only when otadata is empty (e.g. after
 * erase). If button (GPIO0) is held at boot: set boot to ota_1 (Meshtastic) and
 * reboot. Else: set boot to ota_0 (Nocturne) and reboot.
 * Heltec V4: button = GPIO0 (NOCT_BUTTON_PIN in Nocturne config).
 */
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#ifndef BOOT_SELECTOR_BUTTON_PIN
#define BOOT_SELECTOR_BUTTON_PIN 0
#endif

void setup()
{
  pinMode(BOOT_SELECTOR_BUTTON_PIN, INPUT_PULLUP);
  delay(10);
  int pressed = (digitalRead(BOOT_SELECTOR_BUTTON_PIN) == LOW);

  const esp_partition_t *target = nullptr;
  if (pressed)
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                       ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                       NULL);
  if (!target)
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                      ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                      NULL);
  if (target && esp_ota_set_boot_partition(target) == ESP_OK)
    esp_restart();
  for (;;)
    delay(1000);
}

void loop() {}
