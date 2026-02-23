# NOCTURNE OS — WolfPet

**Платформа:** Heltec WiFi LoRa 32 V4 (ESP32-S3) · **Лицензия:** [MIT](LICENSE)

![License](https://img.shields.io/badge/license-MIT-green) ![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)

> _"In the silence of the net, the wolf hunts alone."_

Кибердек-монитор для **Heltec V4**: OLED 128×64, телеметрия по TCP с ПК, меню входа в режимы, WiFi-сканер (RADAR), Trap (captive portal), BLE Spam, Forza-дашборд, BMW E39 Assistant, алерты по температуре и нагрузке.

---

## Содержание

- [Возможности](#-возможности)
- [Железо](#-железо)
- [Экраны (сцены)](#-экраны-сцены)
- [Управление](#-управление)
- [Меню](#-меню)
- [Режимы (утилиты)](#-режимы-утилиты)
- [Алерты (RED ALERT)](#-алерты-red-alert)
- [Установка](#-установка)
- [Конфигурация](#-конфигурация)
- [Структура проекта](#-структура-проекта)
- [Опционально: LoRa](#-опционально-lora)
- [Авторы и лицензия](#-авторы-и-лицензия)

---

## Возможности

| Область     | Описание                                                                                          |
| ----------- | ------------------------------------------------------------------------------------------------- |
| **Дизайн**  | Сетка 2×2, Tech Brackets, ProFont10/HelvB10, ~60 FPS, опциональный glitch.                         |
| **Связь**   | WiFi без power-saving (`WIFI_PS_NONE`), автореконнект, таймауты в `config.h`.                     |
| **Меню**    | 5 категорий → подменю; сохранение настроек в NVS.                                                 |
| **Режимы**  | RADAR (WiFi-сканер), Trap (AP + portal), BLE Spam (и варианты), Forza, BMW Assistant, Charge only. |
| **Алерты**  | Пороги CPU/GPU temp и load, ОЗУ; двойной blink LED, подсветка метрики на экране.                  |
| **Яркость** | Долгое нажатие вне меню — переключение нормальной/пониженной яркости. Таймаут дисплея — затемнение до минимума. |

---

## Железо

| Компонент        | Параметр                     | Примечание                            |
| ---------------- | ---------------------------- | ------------------------------------- |
| **Плата**        | Heltec WiFi LoRa 32 V4       | ESP32-S3R2                            |
| **Дисплей**      | 0.96" OLED SSD1306           | I2C: SDA 17, SCL 18, RST 21           |
| **Питание OLED** | Vext GPIO 36                 | LOW = включено                        |
| **Кнопка**       | GPIO 0 (PRG)                 | Короткое / долгое / двойной тап       |
| **LED**          | GPIO 35 (белый)               | Алерты, predator breath               |
| **Батарея**      | ADC GPIO 1, контроль GPIO 37 | Делитель 4.9, калибровка в `config.h` |
| **I2C**          | 800 kHz                      | Для стабильного 60 FPS                |

---

## Экраны (сцены)

В режиме **Normal** (мониторинг с ПК) доступны 9 сцен; переключение — короткое нажатие.

| №   | Сцена       | Содержимое                                           |
| --- | ----------- | ---------------------------------------------------- |
| 0   | **MAIN**    | CPU/GPU темпы и загрузка, RAM, lifebar               |
| 1   | **CPU**     | Ядро, температура, частота, нагрузка, мощность       |
| 2   | **GPU**     | Температура, частота, нагрузка, VRAM                 |
| 3   | **RAM**     | Топ процессов по памяти, общее использование         |
| 4   | **DISKS**   | Сетка 2×2: буква диска, температура                  |
| 5   | **MEDIA**   | Трек/исполнитель, статус воспроизведения             |
| 6   | **FANS**    | RPM и % для CPU, Pump, GPU, Case                     |
| 7   | **MB**      | Материнская плата: VRM, Chipset и др.               |
| 8   | **WEATHER** | Иконка погоды (XBM), температура                     |

---

## Управление

Одна кнопка (GPIO 0):

| Действие            | В обычном режиме            | В меню                            |
| ------------------- | --------------------------- | --------------------------------- |
| **Короткое** (<1 s) | Следующая сцена             | Следующий пункт меню              |
| **Долгое** (>1 s)   | Вкл/выкл пониженной яркости | Вход в подменю / выполнение       |
| **Двойной тап**     | Открыть меню                | Назад из подменю или закрыть меню |

**Predator:** в нормальном режиме очень долгое нажатие (~2.5 s) — экран гаснет, LED «дышит». Повтор — выход.

---

## Меню

- **Открытие:** двойной тап.
- **Навигация:** короткое нажатие — следующий пункт.
- **Выбор:** долгое нажатие — войти в категорию или выполнить действие.
- **Назад/закрытие:** двойной тап (из подменю — назад, с уровня категорий — выход).

Подсказка внизу: `1x next  2s ok  2x back`.

### Структура меню (5 категорий)

| Категория    | Пункты | Действия |
| ------------ | ------ | -------- |
| **Monitoring** | PC, Forza | Переход в режим мониторинга с ПК или в дашборд Forza. |
| **Config**   | AUTO, FLIP, GLITCH, LED, DIM, CONTRAST, TIMEOUT | AUTO: карусель OFF → 5 s → 10 s → 15 s → OFF. FLIP: поворот 180°. GLITCH: вкл/выкл. LED: подсветка. DIM: низкая яркость по умолчанию. CONTRAST: цикл уровней. TIMEOUT: таймаут затемнения дисплея (0 / 30 / 60 с). Всё сохраняется в NVS. |
| **Hacker**   | WiFi, BLE | **WiFi:** RADAR, Probe Scan, EAPOL, Station Scan, Packet Monitor, Channel Analyzer/Activity, Packet Rate, Pinescan, MultiSSID, Signal Strength, Raw Capture, AP+STA, **Trap** (portal). **BLE:** Spam, Sour Apple, SwiftPair (MS/Google/Samsung), Flipper Spam. |
| **BMW**      | BMW Assistant | Режим ассистента BMW E39: BLE-ключ, I-Bus (замки, свет, MFL, PDC, приборка). Подробно: [BMW E39 Assistant](docs/BMW_E39_Assistant.md). |
| **System**   | REBOOT, CHARGE ONLY, POWER OFF, VERSION | Перезагрузка (с подтверждением), режим зарядки, глубокий сон, показ версии. |

---

## Режимы (утилиты)

Режимы из меню **Hacker** и **Monitoring** рисуются на весь экран без глобального хедера.

| Режим            | Описание                                                                 |
| ---------------- | ------------------------------------------------------------------------ |
| **RADAR**        | WiFi-сканер: список сетей, RSSI, канал; короткое — выбор, долгое — смена сортировки/фильтра. |
| **Trap (Portal)**| Точка доступа + captive portal; клонирование SSID из скана; логи и пароли. |
| **WiFi Sniff**   | Режимы сканирования/анализа (Probe, EAPOL, Station, Packet Monitor, Channel Analyzer/Activity, Packet Rate и др.). |
| **BLE Spam**     | NimBLE spam; счётчик пакетов. Варианты: Sour Apple, SwiftPair (MS/Google/Samsung), Flipper Spam. |
| **Forza**        | Дашборд телеметрии Forza (UDP): RPM, скорость, передача, shift lamp. См. [FORZA_SETUP](docs/FORZA_SETUP.md). |
| **BMW Assistant** | BLE-ключ, I-Bus (свет, замки, PDC, текст на приборку). См. [BMW E39 Assistant](docs/BMW_E39_Assistant.md). |
| **Charge only** | Экран зарядки; WiFi выключен для экономии. |

Выход из любого режима: **двойной тап** → меню → с уровня категорий снова двойной тап — закрытие меню и возврат в Normal.

---

## Алерты (RED ALERT)

Сервер (`monitor.py`) при превышении порогов отправляет `alert: "CRITICAL"` и `alert_metric`. Устройство:

- мигает белым LED два раза;
- переключает экран на нужную сцену (CPU/GPU/RAM);
- подсвечивает проблемную метрику;
- рисует RED ALERT overlay по краям экрана.

Пороги задаются в **`server/monitor.py`** (и в **`include/nocturne/config.h`** для справки):

| Метрика   | Порог                    | Гистерезис (сброс) |
| --------- | ------------------------ | ------------------ |
| CPU temp  | 87 °C                    | −5 °C              |
| GPU temp  | 68 °C                    | −5 °C              |
| CPU load  | 90 %                     | −5 %               |
| GPU load  | 100 %                    | −5 %               |
| VRAM load | 95 %                     | −5 %               |
| **RAM**   | **30 ГБ** (использовано) | < 28 ГБ            |

---

## Установка

### Требования

- **Прошивка:** VS Code + PlatformIO, проект открыт по корню репозитория.
- **Сервер на ПК:** Python 3.x, зависимости из `server/requirements.txt`.
- **Телеметрия:** [Libre Hardware Monitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) — запуск **от имени администратора**, в настройках включён **Remote Web Server** (порт 8085).

### Прошивка устройства

1. Клонировать репозиторий, открыть в VS Code.
2. Скопировать `include/secrets.h.example` в `include/secrets.h`, прописать Wi‑Fi и IP ПК:
   ```cpp
   #define WIFI_SSID "YourNetwork"
   #define WIFI_PASS "YourPassword"
   #define PC_IP     "192.168.1.2"
   #define TCP_PORT  8888
   ```
3. Подключить Heltec V4 по USB-C, выполнить **PlatformIO: Upload**.

### Сервер на ПК (монитор)

Всё, что относится к серверу телеметрии, в папке **`server/`**.

```bash
cd server
pip install -r requirements.txt
```

Отредактировать при необходимости `server/config.json`. Запуск:

```bash
cd server
python monitor.py
```

Варианты: с треем (по умолчанию), без трея (`--no-tray`), в консоль (`--console`). Логи — в `nocturne.log` в корне проекта.

### Сборка exe (Windows)

Из папки **`server/`**: запустить `build_server.bat`. Результат: **`server/dist/NocturneServer.exe`**.

---

## Конфигурация

| Файл                            | Назначение                                                                 |
| ------------------------------- | -------------------------------------------------------------------------- |
| **`include/secrets.h`**         | Wi‑Fi SSID/пароль, IP ПК, TCP-порт. Не коммитить.                          |
| **`include/nocturne/config.h`** | Пины, экран, таймауты, пороги алертов, батарея, меню.                     |
| **`server/config.json`**        | Сервер: `host`, `port`, `lhm_url`, `limits`, `weather_city` и др.          |

Порт в `config.json` должен совпадать с `TCP_PORT` в `secrets.h`.

Подробнее: значения по умолчанию и отладка дисков — см. комментарии в `server/monitor.py` и утилиту `server/tools/dump_lhm_disks.py`.

---

## Структура проекта

```
├── include/
│   ├── nocturne/
│   │   ├── config.h         # Пины, экран, меню, алерты, сцены, батарея
│   │   └── Types.h         # Типы состояния (AppState, HardwareData и др.)
│   └── secrets.h.example   # Шаблон → копировать в secrets.h
├── src/
│   ├── main.cpp             # Точка входа: loop, setup, меню, сцены, режимы
│   ├── InputHandler.h      # Кнопка (ButtonEvent), IntervalTimer, NonBlockingTimer
│   ├── AppModeManager.h/cpp # Переключение режимов (cleanup, init, WiFi state)
│   ├── MenuHandler.h/cpp   # Структура меню (submenuCount, getModeForHackerItem)
│   └── modules/
│       ├── display/        # DisplayEngine, SceneManager, BootAnim, RollingGraph
│       ├── network/        # NetManager, TrapManager, WifiSniffManager
│       ├── ble/            # BleManager
│       ├── car/            # BmwManager, ForzaManager, BleKeyService, ibus/
│       └── system/         # BatteryManager
├── server/                  # Сервер телеметрии (ПК)
│   ├── monitor.py           # TCP-сервер: LHM, погода, медиа, алерты, трей
│   ├── tools/               # dump_lhm_disks.py и др.
│   ├── build_server.bat     # Сборка exe
│   ├── requirements.txt
│   └── config.json
├── data/                    # LittleFS: файлы для загрузки на устройство (см. data/README.txt)
├── docs/                    # FORZA_SETUP, BMW_E39_Assistant
├── tests/                   # Unit-тесты (monitor)
├── tools/                   # Утилиты (напр. Forza UDP) — см. tools/README.md
├── optional/radio/          # LoRa (SX1262) — опционально, см. optional/radio/README.md
├── DataSheets/              # PDF платы
├── platformio.ini           # Сборка: env heltec_wifi_lora_32_V4
└── huge_app.csv             # Таблица разделов (Nocturne + LittleFS)
```

В репозитории нет: `secrets.h`, `.env`, `.pio/`, артефактов сборки, логов.

---

## Опционально: LoRa

Радиомодуль SX1262 (LoRa/FSK) вынесен в **`optional/radio/`** для экономии Flash/RAM. Инструкция по возврату режимов LoRa, пины и зависимости — в [optional/radio/README.md](optional/radio/README.md).

---

## Авторы и лицензия

- **Концепция и код:** RudyWolf
- **Оформление:** Nocturne / WolfPet
- **Библиотеки:** U8g2 (Olikraus), ArduinoJson (Bblanchon), NimBLE-Arduino (h2zero)

**Лицензия:** [MIT](LICENSE).

---

_End of Transmission._
