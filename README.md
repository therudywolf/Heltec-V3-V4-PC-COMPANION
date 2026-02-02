# rudywolf · Heltec PC Monitor

**Метрики ПК, плеер, погода и эквалайзер** на дисплее **Heltec WiFi Kit 32** (ESP32 + SSD1306 128×64). Данные с компьютера по **WiFi**.

---

## Возможности

| Экран           | Описание                                                                                    |
| --------------- | ------------------------------------------------------------------------------------------- |
| **Main**        | CPU/GPU температура, RAM/VRAM, загрузка CPU и GPU                                           |
| **Cores**       | Нагрузка по ядрам CPU 0–100% (столбики)                                                     |
| **GPU**         | Core/HotSpot temp, VRAM bar, частота, вентилятор, мощность                                  |
| **Memory**      | RAM и VRAM (used/total, %) с прогресс-барами                                                |
| **Player**      | Обложка трека, исполнитель, название, прогресс воспроизведения                              |
| **Equalizer**   | Визуализация: Bars / Wave / Circle (переключение в меню)                                    |
| **Power**       | Vcore, мощность CPU/GPU, температура NVMe                                                   |
| **Fans**        | Помпа, радиатор, корпус, GPU (RPM/%)                                                        |
| **Weather**     | Погода (Open-Meteo), иконка и температура; координаты в .env                                |
| **Top 3 CPU**   | Топ-3 процесса по загрузке CPU                                                              |
| **Network**     | Скорость сети (↑/↓ KB/s), диски (R/W MB/s)                                                  |
| **Wolf & Moon** | Мини-игра: поймай луну волком (короткое — прыжок, долгое >1.5 с — выход на следующий экран) |

**При старте:** экран приветствия Forest OS → By RudyWolf с волчьей анимацией.

**Алерты:** пороги CPU/GPU по температуре и нагрузке настраиваются в `config_private.h` (CPU_TEMP_ALERT, GPU_TEMP_ALERT и др.). При превышении — мигающая рамка и надпись ALERT, LED (если включён в меню).

**Данные:** сервер шлёт heartbeat (0.3 с) с температурами и медиа (art/trk/play) для быстрого обновления плеера; полный пакет по экрану (0.8 с, чаще 0.28 с на экранах Player/EQ). Плата отправляет `screen:N` при смене экрана.

**Меню** (долгое нажатие): Forest Menu — LED, AUTO (карусель), Bright (контраст), Intvl (интервал карусели 5/10/15 с), EQ Style, EXIT. Закрывается само через 5 минут бездействия.

---

## Требования

**На ПК (Windows):**

- Python 3.10+
- [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) с веб-сервером на `http://localhost:8085`
- Зависимости: `pip install -r requirements.txt` (в т.ч. pystray для иконки в трее, Pillow для обложек)

**Плата:**

- Heltec WiFi Kit 32 (или ESP32 + SSD1306 128×64)
- Arduino IDE или PlatformIO: ESP32, U8g2, ArduinoJson, WiFi

---

## Структура проекта

```
rudywolf/
├── main.cpp                 # Прошивка для Heltec (дисплей, WiFi, меню)
├── platformio.ini           # Конфиг PlatformIO (плата, библиотеки) — нужен для прошивки
├── monitor.py               # Сервер на ПК: LHM, медиа, погода, топ процессов, TCP
├── requirements.txt         # Зависимости Python
├── .env.example             # Шаблон конфига ПК → скопировать в .env
├── config_private.h.example # Шаблон WiFi для платы → скопировать в config_private.h
├── run_monitor.bat          # Запуск монитора (консоль)
├── run_monitor.vbs          # Запуск без окна (pythonw)
├── build_oneclick.bat       # Всё в одном: venv + зависимости + PyInstaller + monitor.exe в корень
├── monitor.spec             # Spec для PyInstaller (без консоли, трей)
├── README.md                # Этот файл
├── FIRST_START.md           # Пошаговый первый запуск
└── AUTOSTART.md              # Автозапуск в Windows
```

**Не коммитить в Git:** `.env`, `config_private.h`, папки сборки и exe.

---

## Первый запуск

**Подробный пошаговый гайд:** [FIRST_START.md](FIRST_START.md)

Ниже — краткая инструкция.

### 1. Конфиг на ПК

```bash
copy .env.example .env
```

Отредактируй `.env`: `LHM_URL` (если LHM не на 8085), `TCP_PORT` (по умолчанию 8888), `PC_IP` — IP твоего ПК в сети (его укажешь в плату). Опционально: `WEATHER_LAT`, `WEATHER_LON` для погоды.

### 2. Зависимости и запуск монитора

```bash
pip install -r requirements.txt
python monitor.py
```

Должно появиться: `[*] TCP: 0.0.0.0:8888`. На Windows в трее появится иконка «Heltec Monitor»; выход — правый клик по иконке → Выход. Монитор ждёт подключения платы.

### 3. Плата: WiFi

1. Скопируй конфиг: `copy config_private.h.example config_private.h`
2. Впиши в `config_private.h`: **WIFI_SSID**, **WIFI_PASS**, **PC_IP** (IP ПК, например `192.168.1.2`), **TCP_PORT** (8888). Опционально: **CPU_TEMP_ALERT**, **GPU_TEMP_ALERT**, **CPU_LOAD_ALERT**, **GPU_LOAD_ALERT** для порогов алертов.
3. Собери и залей прошивку в Heltec: открой проект в **PlatformIO** (файл `platformio.ini` в корне), выбери плату `heltec_wifi_lora_32_V3` (или совместимую), собери и залей. Либо используй Arduino IDE с установленными платами ESP32 и библиотеками U8g2, ArduinoJson.
4. Плата подключится к WiFi и к TCP-серверу на ПК — на дисплее появятся данные.

### 4. Управление на плате

- **Короткое нажатие** — следующий экран (12 экранов по кругу).
- **Долгое нажатие** — открыть меню. На экране Wolf & Moon долгое >1.5 с выходит из игры и переключает на следующий экран. В меню: короткое — следующий пункт (LED → AUTO → Bright → Intvl → EQ → EXIT), долгое на пункте — вкл/выкл или смена значения (контраст 128/192/255, интервал 5/10/15 с, стиль эквалайзера) / выход.

Настройки LED, карусели, контраста, интервала и стиля эквалайзера сохраняются в NVS.

### 5. Устранение неполадок

- **Нет данных на дисплее:** проверь, что LibreHardwareMonitor запущен и веб-сервер включён; открой в браузере `http://localhost:8085/data.json` и убедись, что в JSON есть нужные SensorId (как в `TARGETS` в `monitor.py`). Проверь WiFi и `PC_IP` в `config_private.h`, порт 8888 и брандмауэр.
- **Не показывается обложка трека:** нужен PIL (`pip install Pillow`); обложка отправляется при экране Player (screen 4). Убедись, что воспроизведение идёт через приложение с поддержкой Windows Media API.
- **Погода не обновляется или нет иконки:** задай в `.env` координаты `WEATHER_LAT` и `WEATHER_LON`; погода и код иконки (`wi`) приходят в полном пакете с сервера.

---

## Автозапуск в Windows

При запуске **из Python** (не из EXE) программа может спросить: «Поместить сервер в автозапуск Windows? (Y/N)». При ответе Y добавляется запись в реестр (HKCU Run). **monitor.exe** работает без консоли (только иконка в трее), вопрос автозапуска при первом запуске EXE не показывается — добавь в автозапуск вручную: ярлык на **monitor.exe** или **run_monitor.vbs** в папку автозагрузки (Win+R → `shell:startup`). Подробнее: [AUTOSTART.md](AUTOSTART.md).

---

## Сборка одного EXE

**Один скрипт (venv + все зависимости + PyInstaller + exe в корень):**

```bash
build_oneclick.bat
```

Скрипт: проверяет Python, создаёт `.venv`, ставит `requirements.txt` и PyInstaller, собирает по **monitor.spec** (без консоли, с иконкой в трее) и копирует `monitor.exe` в корень проекта. Положи рядом `.env` (скопируй из `.env.example` при необходимости) и запускай. EXE работает без окна — только иконка в трее; выход: правый клик по иконке → Выход. Подробнее: [AUTOSTART.md](AUTOSTART.md).

Вручную: `pip install -r requirements.txt` и `pyinstaller --noconfirm monitor.spec`, затем скопировать `dist\monitor.exe` в каталог с `.env`.

**Если сборка не удалась:** убедись, что Python 3.10+ в PATH, все зависимости установлены (`pip install -r requirements.txt`). Ошибки вида «ModuleNotFoundError: winsdk» или «No module named pystray» — переустанови зависимости в том же окружении, откуда запускаешь `pyinstaller`. Антивирус иногда блокирует создание EXE — добавь папку проекта в исключения или отключи на время сборки.

---

## Лицензия

Свободное использование и доработка.
