# Полная интеграция ESP32 Marauder в Nocturne OS

> **Исторический/справочный документ.** Часть описанных менеджеров (Kick, Beacon, Usb, Vault) в текущей прошивке удалена.

## Статус интеграции

### ✅ Выполнено

1. **Расширены enum режимов** (`src/main.cpp`)

   - Добавлены все WiFi scan режимы из Marauder
   - Добавлены все WiFi attack режимы
   - Добавлены все BLE scan режимы
   - Добавлены все BLE attack режимы
   - Добавлены сетевые сканы (ARP, Port, DNS, HTTP, HTTPS, SMTP, RDP, Telnet, SSH, Ping)
   - Добавлены GPS режимы (условная компиляция)

2. **Расширено меню** (`src/modules/SceneManager.cpp`)

   - WiFi меню: 20 пунктов (scans + attacks)
   - Tools меню: 15 пунктов (BLE scans + attacks + Network scans + Tools)
   - Сохранен волчий киберпанк стиль

3. **Обновлена система управления режимами** (`src/main.cpp`)

   - `cleanupMode()` - очистка для всех новых режимов
   - `manageWiFiState()` - управление WiFi для всех режимов
   - `initializeMode()` - инициализация всех режимов (частично с заглушками)
   - `handleMenuActionByCategory()` - обработка действий меню для всех режимов

4. **Существующие менеджеры уже поддерживают:**
   - `WifiSniffManager`: AP scan, Packet Monitor, EAPOL capture, Station scan
   - `KickManager`: Deauth (базовый + targeted)
   - `BeaconManager`: Beacon spam (базовый + Rick Roll + Funny + Custom List)
   - `BleManager`: BLE spam (базовый + Sour Apple + SwiftPair variants + Flipper + AirTag spoof)
   - `TrapManager`: Evil Portal

### 🔄 В процессе / TODO

1. **WiFi Scans - требуется доработка:**

   - ✅ AP Scan (MODE_RADAR) - работает
   - ✅ EAPOL Capture (MODE_WIFI_EAPOL_SCAN) - работает
   - ✅ Station Scan (MODE_WIFI_STATION_SCAN) - работает
   - ✅ Packet Monitor (MODE_WIFI_PACKET_MONITOR) - работает
   - ✅ Probe Scan (MODE_WIFI_PROBE_SCAN) - реализовано, отслеживает probe requests
   - ✅ Channel Analyzer (MODE_WIFI_CHANNEL_ANALYZER) - реализовано, отслеживает активность каналов
   - ✅ Channel Activity (MODE_WIFI_CHANNEL_ACTIVITY) - реализовано, статистика по каналам
   - ✅ Packet Rate (MODE_WIFI_PACKET_RATE) - реализовано, считает пакеты в секунду
   - ⚠️ PineScan (MODE_WIFI_PINESCAN) - нужна детекция Pineapple
   - ⚠️ MultiSSID (MODE_WIFI_MULTISSID) - нужна детекция множественных SSID
   - ⚠️ Signal Strength (MODE_WIFI_SIGNAL_STRENGTH) - нужна визуализация силы сигнала
   - ⚠️ Raw Capture (MODE_WIFI_RAW_CAPTURE) - нужна запись в PCAP формат
   - ⚠️ AP+STA Scan (MODE_WIFI_AP_STA) - нужна комбинация AP и Station сканирования

2. **WiFi Attacks - требуется реализация:**

   - ✅ Deauth (MODE_WIFI_DEAUTH) - работает
   - ✅ Deauth Targeted (MODE_WIFI_DEAUTH_TARGETED) - работает
   - ✅ Deauth Manual (MODE_WIFI_DEAUTH_MANUAL) - работает
   - ✅ Beacon Spam (MODE_WIFI_BEACON) - работает
   - ✅ Beacon Rick Roll (MODE_WIFI_BEACON_RICKROLL) - работает
   - ✅ Beacon Funny (MODE_WIFI_BEACON_FUNNY) - работает
   - ✅ Auth Attack (MODE_WIFI_AUTH_ATTACK) - реализовано в WifiAttackManager
   - ✅ Mimic Flood (MODE_WIFI_MIMIC_FLOOD) - реализовано в WifiAttackManager
   - ✅ AP Spam (MODE_WIFI_AP_SPAM) - реализовано в WifiAttackManager
   - ✅ Bad Message (MODE_WIFI_BAD_MESSAGE) - реализовано в WifiAttackManager
   - ✅ Bad Message Targeted (MODE_WIFI_BAD_MESSAGE_TARGETED) - реализовано в WifiAttackManager
   - ✅ Sleep Attack (MODE_WIFI_SLEEP_ATTACK) - реализовано в WifiAttackManager
   - ✅ Sleep Targeted (MODE_WIFI_SLEEP_TARGETED) - реализовано в WifiAttackManager
   - ✅ SAE Commit (MODE_WIFI_SAE_COMMIT) - реализовано в WifiAttackManager (базовая версия)

3. **BLE Scans - требуется доработка:**

   - ✅ Basic Scan (MODE_BLE_SCAN) - работает
   - ✅ AirTag Monitor (MODE_BLE_SCAN_AIRTAG_MON) - работает
   - ⚠️ Skimmers Scan (MODE_BLE_SCAN_SKIMMERS) - нужна детекция карт-ридеров
   - ⚠️ AirTag Scan (MODE_BLE_SCAN_AIRTAG) - нужна реализация (отличается от Monitor)
   - ⚠️ Flipper Scan (MODE_BLE_SCAN_FLIPPER) - нужна детекция Flipper Zero
   - ⚠️ Flock Scan (MODE_BLE_SCAN_FLOCK) - нужна детекция Flock
   - ⚠️ Analyzer (MODE_BLE_SCAN_ANALYZER) - нужна визуализация BLE активности
   - ⚠️ Simple Scan (MODE_BLE_SCAN_SIMPLE) - нужна упрощенная версия сканирования
   - ⚠️ Simple Two (MODE_BLE_SCAN_SIMPLE_TWO) - нужна вторая версия

4. **BLE Attacks - частично реализовано:**

   - ✅ Basic Spam (MODE_BLE_SPAM) - работает
   - ✅ Sour Apple (MODE_BLE_SOUR_APPLE) - работает
   - ✅ SwiftPair Microsoft (MODE_BLE_SWIFTPAIR_MICROSOFT) - работает
   - ✅ SwiftPair Google (MODE_BLE_SWIFTPAIR_GOOGLE) - работает
   - ✅ SwiftPair Samsung (MODE_BLE_SWIFTPAIR_SAMSUNG) - работает
   - ✅ Flipper Spam (MODE_BLE_FLIPPER_SPAM) - работает
   - ⚠️ Spam All (MODE_BLE_SPAM_ALL) - нужна реализация ротации всех типов
   - ⚠️ AirTag Spoof (MODE_BLE_AIRTAG_SPOOF) - нужна реализация

5. **Network Scans - требуется реализация:**

   - ⚠️ ARP Scan (MODE_NETWORK_ARP_SCAN) - нужна реализация ARP сканирования
   - ⚠️ Port Scan (MODE_NETWORK_PORT_SCAN) - нужна реализация портового сканирования
   - ⚠️ Ping Scan (MODE_NETWORK_PING_SCAN) - нужна реализация ping сканирования
   - ⚠️ DNS Scan (MODE_NETWORK_DNS_SCAN) - нужна реализация DNS сканирования
   - ⚠️ HTTP Scan (MODE_NETWORK_HTTP_SCAN) - нужна реализация HTTP сканирования
   - ⚠️ HTTPS Scan (MODE_NETWORK_HTTPS_SCAN) - нужна реализация HTTPS сканирования
   - ⚠️ SMTP Scan (MODE_NETWORK_SMTP_SCAN) - нужна реализация SMTP сканирования
   - ⚠️ RDP Scan (MODE_NETWORK_RDP_SCAN) - нужна реализация RDP сканирования
   - ⚠️ Telnet Scan (MODE_NETWORK_TELNET_SCAN) - нужна реализация Telnet сканирования
   - ⚠️ SSH Scan (MODE_NETWORK_SSH_SCAN) - нужна реализация SSH сканирования

6. **GPS - требуется реализация (условная компиляция):**

   - ⚠️ Wardriving (MODE_GPS_WARDRIVE) - нужна интеграция GPS модуля
   - ⚠️ GPS Tracker (MODE_GPS_TRACKER) - нужна реализация трекинга
   - ⚠️ POI (MODE_GPS_POI) - нужна реализация точек интереса

7. **SD Card Support - требуется реализация:**

   - ⚠️ Сохранение логов сканирования
   - ⚠️ Сохранение PCAP файлов
   - ⚠️ Сохранение EAPOL handshakes
   - ⚠️ Сохранение GPS данных

8. **UI Integration - требуется доработка:**

   - ⚠️ UI для всех новых scan режимов
   - ⚠️ UI для всех новых attack режимов
   - ⚠️ UI для Network scans
   - ⚠️ UI для GPS режимов
   - Сохранение волчьего киберпанк стиля

9. **TCP Telemetry / Dashboard:**
   - ✅ TCP телеметрия работает в NORMAL режиме
   - ⚠️ Нужно убедиться что телеметрия не мешает WiFi режимам
   - ⚠️ Возможно добавить телеметрию для некоторых режимов (например, Packet Monitor)

## Архитектура

### Режимы работы

Все режимы организованы через единую систему `switchToMode()`:

- `cleanupMode()` - очистка ресурсов перед переключением
- `manageWiFiState()` - управление состоянием WiFi (ON/OFF/STA/AP)
- `initializeMode()` - инициализация режима
- `handleMenuActionByCategory()` - обработка действий пользователя

### Менеджеры

- **WifiSniffManager**: WiFi promiscuous режим, сканирование AP/Station, EAPOL capture
- **KickManager**: WiFi deauth атаки
- **BeaconManager**: WiFi beacon spam
- **BleManager**: BLE сканирование и атаки
- **TrapManager**: Evil Portal / Captive Portal
- **NetManager**: TCP телеметрия с ПК (дашборд)
- **UsbManager**: USB HID (BADWOLF)
- **VaultManager**: TOTP хранилище
- **MdnsManager**: mDNS spoofing

## Следующие шаги

1. Реализовать недостающие WiFi scan режимы в `WifiSniffManager`
2. Создать новые менеджеры для WiFi attacks (AuthManager, MimicManager, etc.)
3. Реализовать Network scans (создать NetworkScanManager)
4. Добавить GPS поддержку (условная компиляция)
5. Добавить SD card поддержку (условная компиляция)
6. Расширить UI для всех новых режимов
7. Протестировать все режимы на устройстве

## Примечания

- Все новые режимы интегрированы в меню и систему управления режимами
- TCP телеметрия сохранена и работает в NORMAL режиме
- Волчий киберпанк стиль сохранен в меню и UI
- Многие функции имеют заглушки (TODO) и требуют реализации
