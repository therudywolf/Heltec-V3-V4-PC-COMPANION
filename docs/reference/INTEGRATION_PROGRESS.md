# Прогресс интеграции Marauder в Nocturne OS

> **Исторический/справочный документ.** Часть описанных менеджеров (Kick, Beacon, Usb, Vault) в текущей прошивке удалена.

## ✅ Реализовано (2026-02-06)

### WiFi Scans

1. **Probe Scan** - Полностью реализован в `WifiSniffManager`

   - Обработка probe request фреймов (subtype 4)
   - Отслеживание клиентов по MAC адресу
   - Извлечение SSID из probe requests
   - Подсчет количества probe requests от каждого клиента

2. **Channel Analyzer** - Реализован

   - Отслеживание активности по каналам 1-14
   - Статистика пакетов по каналам
   - Сброс статистики каждые 10 секунд

3. **Channel Activity** - Реализован

   - Краткосрочная статистика активности каналов
   - Сброс каждые 5 секунд для динамического отображения

4. **Packet Rate** - Реализован
   - Подсчет пакетов в секунду
   - Обновление каждую секунду

### Система управления режимами

- Все режимы интегрированы в `cleanupMode()`, `manageWiFiState()`, `initializeMode()`
- GPS режимы удалены (нет GPS модуля)
- LoRa не используется (как запрошено)

### Меню

- WiFi меню: 20 пунктов
- Tools меню: 14 пунктов (без GPS)
- Сохранен волчий киберпанк стиль

## 🔄 В процессе

### WiFi Attacks

- Deauth (базовый + targeted + manual) - работает
- Beacon Spam (базовый + Rick Roll + Funny) - работает
- Auth Attack, Mimic Flood, AP Spam, Bad Message, Sleep Attack, SAE Commit - требуют реализации

### BLE Scans

- Basic Scan, AirTag Monitor - работают
- Skimmers, Flipper, Flock, Analyzer, Simple - требуют доработки

### Network Scans

- Все режимы требуют реализации (ARP, Port, DNS, HTTP, HTTPS, SMTP, RDP, Telnet, SSH, Ping)

## 📝 Примечания

- TCP телеметрия сохранена и работает в NORMAL режиме
- Все WiFi режимы приостанавливают телеметрию (`netManager.setSuspend(true)`)
- Волчий стиль сохранен в меню и UI
- GPS и LoRa функционал удален по запросу пользователя
