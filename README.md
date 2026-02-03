# NOCTURNE_OS

Hardware monitor: ESP32 (Heltec) + SSD1306 128×64, backend — Windows Python (aiohttp, tray).

## Запуск

- **INSTALL_AND_RUN.bat** — установка зависимостей, настройка, автозапуск (Y/N), старт монитора в трей.
- **START_MONITOR.bat** — только запуск монитора в трей.
- **EMERGENCY_START.bat** — запуск в консоли (для отладки).

Логи: `nocturne.log` в корне проекта.

## Прошивка

`pio run` (нужны `include/secrets.h` из `include/secrets.h.example`). Порт/хост в `config.json`.
