# rudywolf · Heltec PC Monitor

**Метрики ПК, плеер, погода и эквалайзер** на дисплее **Heltec WiFi Kit 32** (ESP32 + SSD1306 128×64). Данные с компьютера по **WiFi**.

---

## Возможности

| Экран           | Описание                                                                 |
| --------------- | ------------------------------------------------------------------------ |
| **Main**        | CPU/GPU температура, RAM/VRAM, загрузка CPU и GPU                        |
| **Cores**       | Нагрузка по ядрам CPU 0–100% (столбики)                                  |
| **GPU**         | Core/HotSpot temp, VRAM bar, частота, вентилятор, мощность               |
| **Memory**      | RAM и VRAM (used/total, %) с прогресс-барами                             |
| **Player**      | Обложка трека, исполнитель, название, прогресс воспроизведения           |
| **Equalizer**   | Визуализация: Bars / Wave / Circle (переключение в меню)                 |
| **Power**       | Vcore, мощность CPU/GPU, температура NVMe                                |
| **Fans**        | Помпа, радиатор, корпус, GPU (RPM/%)                                     |
| **Weather**     | Погода (Open-Meteo), иконка и температура; координаты в .env             |
| **Top 3 CPU**   | Топ-3 процесса по загрузке CPU                                           |
| **Network**     | Скорость сети (↑/↓ KB/s), диски (R/W MB/s)                               |
| **Wolf & Moon** | Мини-игра: поймай луну волком (короткое — прыжок, долгое >1.5 с — выход) |

**При старте:** экран приветствия Forest OS → By RudyWolf с волчьей анимацией.

**Алерты:** пороги CPU/GPU по температуре и нагрузке настраиваются в `config_private.h` (CPU_TEMP_ALERT, GPU_TEMP_ALERT и др.). При превышении — мигающая рамка и надпись ALERT, LED (если включён в меню).

**Данные:** сервер шлёт heartbeat (0.3 с) с температурами и медиа (art/trk/play) для быстрого обновления плеера; полный пакет по экрану (0.8 с, чаще 0.28 с на экранах Player/EQ). Плата отправляет `screen:N` при смене экрана.

**Меню** (долгое нажатие): Forest Menu — LED, автосмена экранов, стиль эквалайзера, выход. Закрывается само через 5 минут бездействия.

---

## Требования

**На ПК (Windows):**

- Python 3.10+
- [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) с веб-сервером на `http://localhost:8085`
- Зависимости: `pip install -r requirements.txt`

**Плата:**

- Heltec WiFi Kit 32 (или ESP32 + SSD1306 128×64)
- Arduino IDE или PlatformIO: ESP32, U8g2, ArduinoJson, WiFi

---

## Структура проекта

```
rudywolf/
├── main.cpp                 # Прошивка для Heltec (дисплей, WiFi, меню)
├── monitor.py               # Сервер на ПК: LHM, медиа, погода, топ процессов, TCP
├── requirements.txt        # Зависимости Python
├── .env.example             # Шаблон конфига ПК → скопировать в .env
├── config_private.h.example # Шаблон WiFi для платы → скопировать в config_private.h
├── run_monitor.bat          # Запуск монитора (с консолью)
├── run_monitor.vbs          # Запуск монитора без окна
├── build_exe.bat           # Сборка monitor.exe (PyInstaller)
├── build_oneclick.bat      # Один клик: venv + requirements + exe в корень
├── README.md               # Этот файл
├── FIRST_START.md          # Пошаговый первый запуск
└── AUTOSTART.md             # Автозапуск в Windows
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

Должно появиться: `[*] TCP: 0.0.0.0:8888`. Монитор ждёт подключения платы.

### 3. Плата: WiFi

1. Скопируй конфиг: `copy config_private.h.example config_private.h`
2. Впиши в `config_private.h`: **WIFI_SSID**, **WIFI_PASS**, **PC_IP** (IP ПК, например `192.168.1.2`), **TCP_PORT** (8888). Опционально: **CPU_TEMP_ALERT**, **GPU_TEMP_ALERT**, **CPU_LOAD_ALERT**, **GPU_LOAD_ALERT** для порогов алертов.
3. Собери и залей **main.cpp** в Heltec (Arduino/PlatformIO).
4. Плата подключится к WiFi и к TCP-серверу на ПК — на дисплее появятся данные.

### 4. Управление на плате

- **Короткое нажатие** — следующий экран (12 экранов по кругу).
- **Долгое нажатие** — открыть меню (на экране Wolf & Moon — долгое >1.5 с выходит из игры). В меню: короткое — следующий пункт (LED → AUTO → EQ Style → EXIT), долгое на пункте — вкл/выкл или смена стиля / выход.

Настройки LED, карусели и стиля эквалайзера сохраняются в NVS.

---

## Автозапуск в Windows

При первом запуске **monitor.exe** программа спросит: «Поместить сервер в автозапуск Windows? (Y/N)». При ответе Y добавляется запись в реестр (HKCU Run). Либо вручную: ярлык на **run_monitor.vbs** или **monitor.exe** в папку автозагрузки (Win+R → `shell:startup`). Подробнее: [AUTOSTART.md](AUTOSTART.md).

---

## Сборка одного EXE

**Один клик (venv + зависимости + exe в корень):**

```bash
build_oneclick.bat
```

Создаётся `.venv`, ставятся зависимости, собирается `monitor.exe` и копируется в корень проекта. Положи рядом `.env` и запускай (см. AUTOSTART.md).

Либо вручную: `build_exe.bat` → `dist\monitor.exe` скопировать в каталог с `.env`.

---

## Лицензия

Свободное использование и доработка.
