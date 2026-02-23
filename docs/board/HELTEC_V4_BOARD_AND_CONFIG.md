# Плата Heltec WiFi LoRa 32 V4 и настройка прошивки

Краткий гайд по плате **Heltec WiFi LoRa 32 V4** (ESP32-S3), распиновке, конфигурации прошивки Nocturne и сборке.

---

## Плата

- **Модель:** Heltec WiFi LoRa 32 V4 (V4.2).
- **МК:** ESP32-S3 (R2), 240 MHz, 8 MB Flash, PSRAM по варианту.
- **Документация:** даташит и схемы в папке `DataSheets/` (например, `WiFi_LoRa_32_V4.2.0.pdf`).

---

## Распиновка (используемые выводы в Nocturne)

| Назначение      | GPIO | Константа в config.h        | Примечание |
|-----------------|------|-----------------------------|------------|
| Кнопка (PRG)    | 0    | NOCT_BUTTON_PIN             | Короткое / долгое / двойной тап |
| Батарея (ADC)   | 1    | NOCT_BAT_PIN                | Напряжение через делитель |
| Управление делителем батареи | 37 | NOCT_BAT_CTRL_PIN   | HIGH = включить делитель |
| I2C SDA (OLED)  | 17   | NOCT_SDA_PIN                | SSD1306 |
| I2C SCL (OLED)  | 18   | NOCT_SCL_PIN                | SSD1306 |
| Сброс OLED      | 21   | NOCT_RST_PIN                | Reset дисплея |
| Питание OLED (Vext) | 36 | NOCT_VEXT_PIN           | LOW = питание OLED включено |
| LED (белый)     | 35   | NOCT_LED_ALERT_PIN          | Алерты, «predator» режим |
| I-Bus TX (BMW)  | 39   | NOCT_IBUS_TX_PIN            | Только при NOCT_IBUS_ENABLED |
| I-Bus RX (BMW)  | 38   | NOCT_IBUS_RX_PIN            | Только при NOCT_IBUS_ENABLED |

Питание: USB 5 V или внешнее 3.3/5 V. Земля — общая с периферией (и с кузовом при использовании в BMW).

---

## Дисплей

- **Тип:** 0.96" OLED SSD1306, 128×64, I2C.
- **Адрес:** стандартный 0x3C (в коде U8g2/DisplayEngine).
- **Частота I2C:** 800 kHz (задаётся в коде для стабильного 60 FPS).

---

## Конфигурация прошивки (config.h)

Файл: **`include/nocturne/config.h`**.

- **Пины** — см. таблицу выше; при необходимости меняются только константы `NOCT_*_PIN`.
- **Батарея:** делитель `NOCT_BAT_DIVIDER_FACTOR` (4.9), диапазоны `NOCT_VOLT_MIN` / `NOCT_VOLT_MAX` / `NOCT_VOLT_CHARGING`, калибровка `NOCT_BAT_CALIBRATION_OFFSET`, интервалы опроса и сглаживание.
- **Таймауты:** подключение TCP, реконнект Wi‑Fi, сигнал, кнопка (короткое/долгое/predator), скролл, затемнение дисплея.
- **Экран:** размеры 128×64, отступы, шрифты, сцены (NOCT_SCENE_*), меню (размер бокса, строки).
- **Алерты:** пороги CPU/GPU (температура, нагрузка), VRAM, RAM; должны совпадать с логикой в `server/monitor.py`.
- **BMW:** `NOCT_IBUS_ENABLED` (1 = I-Bus включён, 0 = только BLE и список действий), пины I-Bus 38/39.

**Секреты** (не коммитить): скопировать `include/secrets.h.example` в **`include/secrets.h`** и задать:

- `WIFI_SSID`, `WIFI_PASS` — Wi‑Fi.
- `PC_IP` — IP ПК для телеметрии (мониторинг).
- `TCP_PORT` — порт TCP (обычно 8888), должен совпадать с `server/config.json`.

---

## Сборка и загрузка

- **Среда:** PlatformIO (VS Code или CLI).
- **Окружение:** `heltec_wifi_lora_32_V4` (в `platformio.ini` используется плата `heltec_wifi_lora_32_V3` — тот же MCU/Flash для V4).
- **Сборка:** из корня проекта выполнить `pio run` (или Build в IDE).
- **Загрузка:** подключить плату по USB, выбрать порт, выполнить Upload. Serial-монитор: 115200 бод (для отладки и логов).
- **Разделы:** таблица разделов задаётся в `platformio.ini` как `board_build.partitions = huge_app.csv` (файл в корне). Не удалять и не перемещать `huge_app.csv` без правки platformio.ini.
- **Файловая система:** LittleFS (опционально для данных); см. `data/` и загрузку через PlatformIO.

---

## Что не коммитить

- `include/secrets.h` (и корневой `secrets.h`, если есть).
- `.env`, логи (`nocturne.log`, `monitor.log`), артефакты сборки (`.pio/`, `server/build/`, `server/dist/`). Они перечислены в `.gitignore`.

---

## Логи и обновление прошивки

- **Логи сервера (ПК):** `nocturne.log` в корне проекта (или рядом с exe при сборке).
- **Логи устройства:** через Serial (115200) при подключении по USB.
- **Обновление прошивки:** после правок кода или config.h — снова `pio run` и Upload; секреты в `secrets.h` при этом не трогаются.

---

## Опционально: LoRa

Радиомодуль SX1262 (LoRa/FSK) вынесен из основной сборки. Инструкция по возврату режимов LoRa, пины и зависимости — в [optional/radio/README.md](../../optional/radio/README.md).

---

В корневом [README.md](../../README.md) см. разделы «Железо», «Конфигурация» и «Структура проекта».
