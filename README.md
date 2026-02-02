# Heltec v4 — монитор ПК на дисплее

Метрики компьютера, плеер (Spotify, YouTube и др.) и эквалайзер на дисплее **Heltec WiFi Kit 32** (ESP32 + SSD1306 128×64). Данные с ПК по **Serial (USB)** или по **WiFi**.

---

## Что это

- **monitor.py** — сервер на ПК: опрашивает [LibHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) и Windows Media (текущий трек), отправляет JSON по COM-порту и/или по TCP.
- **main.cpp** — прошивка для Heltec: 6 экранов (сводка, ядра CPU, GPU, память, плеер с обложкой, эквалайзер), меню по долгому нажатию (LED, карусель), выбор источника: WiFi или Serial.

Подключение к репозиторию — все чувствительные данные в `.env` и `config_private.h`, в Git не попадают.

---

## Требования

**На ПК:**

- Python 3.10+
- [LibHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) с веб-сервером на `http://localhost:8085` (или свой URL в `.env`)
- Зависимости: `pip install -r requirements.txt`

**Плата:**

- Heltec WiFi Kit 32 (или совместимая ESP32 + SSD1306 128×64)
- Arduino/PlatformIO: ESP32, U8g2, ArduinoJson, WiFi

---

## Быстрый старт

### 1. Конфиг на ПК

```bash
cp .env.example .env
# Отредактируй .env: SERIAL_PORT (например COM6), LHM_URL, при WiFi — TCP_PORT, TRANSPORT=both
```

### 2. Запуск сервера

```bash
python monitor.py
```

Или двойной клик по **run_monitor.bat** (с консолью) или **run_monitor.vbs** (без окна).  
Один exe: `build_exe.bat` → `dist\monitor.exe`, положи рядом с `.env`.

### 3. Плата (Serial)

Подключи Heltec по USB, в `.env` укажи нужный `SERIAL_PORT`. Данные пойдут по проводу.

### 4. Плата (WiFi)

Скопируй и заполни конфиг WiFi:

```bash
cp config_private.h.example config_private.h
# Впиши в config_private.h: WIFI_SSID, WIFI_PASS, PC_IP (IP ПК, например 192.168.1.2), TCP_PORT (8888)
```

Собери и залей **main.cpp** в плату. На ПК запусти монитор с `TRANSPORT=tcp` или `TRANSPORT=both`. Плата сама выберет WiFi или Serial (если USB подключён).

---

## Автозапуск в Windows

Ярлык на **run_monitor.vbs** или **monitor.exe** положи в папку «Автозагрузка» (Win+R → `shell:startup`).  
Подробнее: [AUTOSTART.md](AUTOSTART.md).

---

## Управление на плате

- **Короткое нажатие** — следующий экран (сводка → ядра → GPU → память → плеер → эквалайзер).
- **Долгое нажатие** — открыть/закрыть меню. В меню: короткое — переход по пунктам (LED → карусель → выход), долгое на пункте — вкл/выкл (LED, карусель) или выход.

Настройки LED и карусели сохраняются в NVS и переживают перезагрузку.

---

## Файлы проекта

| Файл                                  | Назначение                                                  |
| ------------------------------------- | ----------------------------------------------------------- |
| `monitor.py`                          | Сервер: LHM + медиа, отправка JSON (Serial/TCP)             |
| `main.cpp`                            | Прошивка Heltec: дисплей, кнопка, WiFi/Serial               |
| `.env.example`                        | Шаблон конфига ПК (скопировать в `.env`)                    |
| `config_private.h.example`            | Шаблон WiFi/IP для платы (скопировать в `config_private.h`) |
| `run_monitor.bat` / `run_monitor.vbs` | Запуск монитора с/без консоли                               |
| `build_exe.bat`                       | Сборка одного exe (PyInstaller)                             |
| `AUTOSTART.md`                        | Как добавить в автозапуск Windows                           |

---

## Лицензия

Свободное использование и доработка.
