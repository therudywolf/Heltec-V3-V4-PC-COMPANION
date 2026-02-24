# Плата Heltec WiFi LoRa 32 V4 и настройка прошивки

Краткий гайд по плате **Heltec WiFi LoRa 32 V4** (ESP32-S3), распиновке, конфигурации прошивки Nocturne и сборке. Источник пинов и схем: **DataSheets/WiFi_LoRa_32_V4.2.0.pdf** (разделы 2.2.1–2.2.3 — J2, J3, Additional Pins).

---

## Плата (по даташиту V4.2.0)

- **Модель:** Heltec WiFi LoRa 32 V4 (V4.2).
- **МК:** ESP32-S3R2, 240 MHz, 16 MB Flash, 2 MB PSRAM.
- **Интерфейсы:** Wi‑Fi b/g/n, BLE / Bluetooth 5 / mesh; **2×I2S**, 3×UART, 2×I2C, 4×SPI (табл. 3.1 даташита).
- **USB Type-C:** встроенный, защита по напряжению, ESD, КЗ. **GPIO20 = USB_D+**, **GPIO19 = USB_D-** (J2 п.17–18). При включённом USB CDC прошивка может экспонировать виртуальный COM по USB (подключение к ПК или Android по кабелю).
- **Питание:** 5 V (USB или вывод 5V), 3.3 V (Ve), батарея 3.3–4.2 V; потребление BT ~115 mA, Sleep на батарее &lt;20 µA (табл. 3.4).
- **Документация:** даташит и схемы в папке `DataSheets/` — **WiFi_LoRa_32_V4.2.0.pdf**.

---

## Распиновка (используемые выводы в Nocturne, сверка с J2/J3)

| Назначение       | GPIO   | J2/J3     | Константа в config.h   | Примечание |
|------------------|--------|-----------|------------------------|------------|
| Кнопка (PRG)     | 0      | J2.8      | NOCT_BUTTON_PIN        | Короткое / долгое / двойной тап |
| Батарея (ADC)    | 1      | J3.12     | NOCT_BAT_PIN           | Напряжение через делитель |
| Vext             | 36     | J2.9      | NOCT_VEXT_PIN          | LOW = питание OLED включено |
| Упр. делителем   | 37     | J3.4      | NOCT_BAT_CTRL_PIN      | HIGH = включить делитель |
| I2C SDA (OLED)   | 17     | Add.pins  | NOCT_SDA_PIN           | SSD1315/SSD1306 |
| I2C SCL (OLED)   | 18     | Add.pins  | NOCT_SCL_PIN           | SSD1315/SSD1306 |
| Сброс OLED       | 21     | J2.16     | NOCT_RST_PIN           | Reset дисплея |
| LED (белый)      | 35     | J2.10     | NOCT_LED_ALERT_PIN     | Алерты, «predator» режим |
| I-Bus TX (BMW)   | 39     | J3.10     | NOCT_IBUS_TX_PIN       | При NOCT_IBUS_ENABLED |
| I-Bus RX (BMW)   | 38     | J3.11     | NOCT_IBUS_RX_PIN       | При NOCT_IBUS_ENABLED |
| OBD TX (ELM327)  | 9      | —         | NOCT_OBD_TX_PIN        | При NOCT_OBD_ENABLED (Serial2) |
| OBD RX (ELM327)  | 10     | —         | NOCT_OBD_RX_PIN        | При NOCT_OBD_ENABLED (Serial2) |
| **USB D+**       | 20     | J2.17     | —                     | Для CDC: встроенный USB |
| **USB D-**       | 19     | J2.18     | —                     | Для CDC: встроенный USB |
| **I2S BCK (звук)** | 45   | J3        | NOCT_I2S_BCK_PIN       | При NOCT_A2DP_SINK_ENABLED |
| **I2S WS (звук)**  | 46   | J3        | NOCT_I2S_WS_PIN        | При NOCT_A2DP_SINK_ENABLED |
| **I2S DOUT (звук)**| 40   | J3        | NOCT_I2S_DOUT_PIN      | При NOCT_A2DP_SINK_ENABLED |

Питание: USB 5 V или внешнее 3.3/5 V. Земля — общая с периферией (и с кузовом при использовании в BMW).

---

## Дисплей

- **Тип:** 0.96" OLED, в даташите указан **SSD1315**, 128×64, I2C. В прошивке используется драйвер под SSD1306 — совместим по I2C.
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
- **BMW:** `NOCT_IBUS_ENABLED` (1 = I-Bus включён, 0 = только BLE и список действий), пины I-Bus 38/39. `NOCT_IBUS_MONITOR_VERBOSE` (1 = логировать все пакеты I-Bus в Serial в hex; для отладки).
- **OBD (BMW):** `NOCT_OBD_ENABLED` (1 = опрос ELM327 по Serial2 для RPM и температур), `NOCT_OBD_TX_PIN` / `NOCT_OBD_RX_PIN` (по умолчанию 9 и 10). Скорость 38400 8N1.
- **USB CDC:** `NOCT_USB_CDC_ENABLED` (0 = выкл.). При 1 устройство экспонирует виртуальный COM по встроенному USB (GPIO19/20 по даташиту V4.2.0). Подключение к Android по кабелю USB-OTG: приложение использует USB Host API и тот же протокол команд (один байт = команда 0x00–0x0B, 0x80, 0x81), см. [BMW_ANDROID_APP.md](../bmw/BMW_ANDROID_APP.md).
- **Звук (A2DP Sink):** `NOCT_A2DP_SINK_ENABLED` (0 = выкл.). При 1 плата работает как A2DP Sink: приём аудио с телефона по Bluetooth, вывод по I2S на внешний DAC (пины NOCT_I2S_BCK_PIN, NOCT_I2S_WS_PIN, NOCT_I2S_DOUT_PIN — по умолчанию 45, 46, 40). Подключение DAC (например PCM5102A) и AUX — см. [BMW_E39_Assistant.md](../bmw/BMW_E39_Assistant.md) или гайд по звуку.

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
