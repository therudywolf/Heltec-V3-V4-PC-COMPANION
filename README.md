# BMW E39 I-Bus Assistant — Heltec V4

**Платформа:** Heltec WiFi LoRa 32 V4 (ESP32-S3) · **Лицензия:** [MIT](LICENSE)

![License](https://img.shields.io/badge/license-MIT-green) ![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)

Прошивка и приложение для **BMW E39 I-Bus Assistant**: BLE-ключ (подход/отход), команды по I-Bus (свет, замки, PDC, текст на приборку), опциональный OBD. Устройство загружается сразу в режим BMW Assistant; двойной тап открывает системное меню (Demo, Reboot, Charge only, Power off, Version).

---

## Структура репозитория

```
├── platformio.ini       # PlatformIO (env heltec_wifi_lora_32_V4)
├── huge_app.csv
├── include/
│   ├── nocturne/        # config.h, Types.h
│   └── secrets.h.example
├── src/                 # main.cpp, AppModeManager, MenuHandler, modules/
├── data/                # LittleFS (см. data/README.txt)
├── app/
│   ├── android/         # Нативное Android-приложение (Kotlin)
│   └── flutter/         # Flutter MD3 приложение (Dart)
├── docs/
│   ├── USER_GUIDE.md
│   ├── GIT.md
│   ├── bmw/             # BMW E39 Assistant, проводка, Android-приложение
│   └── board/           # Плата, подключение, конфиг
├── shared/
│   ├── .vscode/         # Настройки IDE
│   └── DataSheets/      # Даташит платы (см. shared/DataSheets/README.md)
├── BMW datasheet/       # Справочные материалы I-Bus
├── LICENSE
├── README.md
└── .gitignore
```

---

## Установка прошивки

1. Клонировать репозиторий, открыть **корень репозитория** в VS Code / PlatformIO.
2. Скопировать **`include/secrets.h.example`** в **`include/secrets.h`**, при необходимости прописать Wi‑Fi (для будущего OTA; в режиме BMW WiFi выключен).
3. Подключить Heltec V4 по USB-C, выполнить **PlatformIO: Upload**. Серийный монитор 115200 — см. [Подключение и терминал](docs/board/CONNECTING_AND_TERMINAL.md).

Конфигурация пинов, I-Bus, OBD, демо-режим — в **`include/nocturne/config.h`**.

---

## Деморежим

Для тестирования без машины: **меню** (двойной тап) → BMW → «BMW Demo» (долгое нажатие) или System → «Demo» (переключатель). Либо **удерживать кнопку PRG при загрузке** ~2,5 с — демо включится и сохранится в NVS (после перезагрузки остаётся включённым). Подробнее — [BMW E39 Assistant](docs/bmw/BMW_E39_Assistant.md), [Руководство пользователя](docs/USER_GUIDE.md).

---

## Приложение (Android)

- **Нативное (Kotlin):** `app/android/` — см. [BMW Android App](docs/bmw/BMW_ANDROID_APP.md) и `app/android/README.md`.
- **Flutter:** `app/flutter/` — см. `app/flutter/README.md`.

---

## Документация

| Раздел | Описание |
|--------|----------|
| [Руководство пользователя](docs/USER_GUIDE.md) | Кнопка, жесты, режимы, настройки |
| [Плата и настройки](docs/board/HELTEC_V4_BOARD_AND_CONFIG.md) | Распиновка Heltec V4, config.h, сборка |
| [BMW E39 Assistant](docs/bmw/BMW_E39_Assistant.md) | BLE-ключ, I-Bus, свет, замки, PDC, приборка |
| [Проводка платы к машине](docs/bmw/HELTEC_V4_WIRING.md) | Подключение Heltec V4 → трансивер I-Bus → автомобиль |

---

## Авторы и лицензия

- **Концепция и код:** RudyWolf  
- **Библиотеки:** U8g2 (Olikraus), ArduinoJson (Bblanchon), NimBLE-Arduino (h2zero)  

**Лицензия:** [MIT](LICENSE).
