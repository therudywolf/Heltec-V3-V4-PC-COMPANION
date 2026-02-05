# Optional: LoRa / Meshtastic radio module (SX1262)

This folder contains the LoRa/Meshtastic-related code extracted from the main firmware to save Flash and RAM when the radio module is not used. Pinout and hardware details follow **DataSheets/WiFi_LoRa_32_V4.2.0.pdf**.

## Hardware (Heltec WiFi LoRa 32 V4)

- **Chip:** SX1262 (LoRa/FSK), driven via SPI.
- **Pins (from datasheet / original LoraManager.h):**
  - `LORA_NSS` = 8
  - `LORA_DIO1` = 14
  - `LORA_RST` = 12
  - `LORA_BUSY` = 13

Use an external antenna when operating the radio; running without an antenna can damage the module.

## Dependencies

- **RadioLib** (e.g. `jgromes/RadioLib @ ^6.6.0`) — add to `platformio.ini` `lib_deps` when enabling radio.

## How to re-enable radio in the project

1. Copy `LoraManager.cpp` and `LoraManager.h` from this folder into `src/modules/`.
2. In `platformio.ini`, add to `lib_deps`:
   ```
   jgromes/RadioLib @ ^6.6.0
   ```
3. In `src/main.cpp`:
   - Add `#include "modules/LoraManager.h"`.
   - Add global `LoraManager loraManager;`.
   - Restore enum values `MODE_LORA`, `MODE_LORA_JAM`, `MODE_LORA_SENSE`.
   - Restore all `case MODE_LORA*` blocks in `cleanupMode`, `manageWiFiState`, `initializeMode`, button handler, and draw switch (SIGINT / JAM / SENSE screens).
   - Restore menu items for LoRa MESH, LoRa JAM, LoRa SENSE and their handlers (e.g. items 6–8 with `switchToMode(MODE_LORA)` etc.), and increase `WOLF_MENU_ITEMS` accordingly.
4. In `src/modules/SceneManager.cpp`:
   - Add `#include "LoraManager.h"`.
   - Restore `drawLoraSniffer` and `drawGhostsMode` implementations (remove `#if 0` / re-add the full functions).
5. In `src/modules/SceneManager.h`:
   - Add forward declaration `class LoraManager;`.
   - Restore declarations for `drawLoraSniffer(LoraManager &lora)` and `drawGhostsMode(LoraManager &lora)`.

After that, rebuild with PlatformIO; the radio modes will be available again.
