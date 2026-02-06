# Анализ ESP32 Marauder и рекомендации по улучшению Nocturne OS

## Выполнено

✅ Добавлено `esp32_marauder/` в `.gitignore`

## Анализ модулей и рекомендации

### 1. KickManager (Deauth) - Улучшения

#### Текущая реализация

- Базовый deauth с выбором цели из скана
- Проверка на WPA3/PMF (частично)
- Отправка broadcast deauth пакетов

#### Улучшения из Marauder

**1.1 Targeted Deauth (высокий приоритет)**

- **Что**: Deauth по конкретному MAC адресу клиента (не broadcast)
- **Реализация в Marauder**: `sendDeauthFrame(uint8_t bssid[6], int channel, uint8_t mac[6])`
- **Преимущества**: Более эффективная атака, меньше шума в эфире
- **Как добавить**:
  - Добавить метод `setTargetClient(uint8_t mac[6])` в KickManager
  - Модифицировать `sendDeauthBurst()` для поддержки targeted deauth
  - Добавить UI для выбора клиента из Station Scan

**1.2 Manual Deauth (средний приоритет)**

- **Что**: Ручной ввод MAC адреса цели
- **Реализация**: Добавить режим ввода MAC через меню
- **Преимущества**: Атака на сети, которых нет в скане

**1.3 Deauth Sniff (низкий приоритет)**

- **Что**: Перехват deauth пакетов в эфире
- **Реализация**: Добавить callback для deauth frames в promiscuous mode
- **Преимущества**: Обнаружение атак на сеть

**1.4 Улучшенная проверка PMF**

- **Текущее**: Проверка только WPA3
- **Улучшение**: Проверка Protected Management Frames (PMF) в capability info beacon
- **Код из Marauder**: `bool is_protected = (capab_info & 0x10) != 0;`

**Рекомендации по реализации:**

```cpp
// В KickManager.h добавить:
void setTargetClient(uint8_t mac[6]);
bool isTargetedMode() const { return targetedMode_; }

// В KickManager.cpp:
void KickManager::setTargetClient(uint8_t mac[6]) {
  memcpy(targetClientMAC_, mac, 6);
  targetedMode_ = true;
}

// Модифицировать sendDeauthBurst() для поддержки targeted deauth
```

---

### 2. BleManager - Новые атаки

#### Текущая реализация

- 6 payloads: Apple, Google Fast Pair, Samsung, Windows, Tile, Galaxy Buds
- Базовый scan и clone

#### Новые атаки из Marauder

**2.1 Sour Apple Attack (высокий приоритет)**

- **Что**: iOS crash через BLE (CVE-2023-23529)
- **Реализация в Marauder**: `executeSourApple()` в WiFiScan.cpp:4673
- **Payload**: Специальный BLE advertisement с некорректными данными
- **Эффект**: Краш iOS устройств в радиусе
- **Как добавить**:
  - Добавить новый payload тип `BLE_PAYLOAD_SOUR_APPLE`
  - Реализовать специальный advertisement packet
  - Добавить в меню как отдельный режим

**2.2 SwiftPair Spam (высокий приоритет)**

- **Что**: Microsoft Fast Pair spam (уже есть базовая версия)
- **Улучшение**: Расширить до множества типов устройств
- **Типы из Marauder**: Microsoft, Google, Samsung, Apple
- **Реализация**: `executeSwiftpairSpam(EBLEPayloadType type)` в WiFiScan.cpp:4763

**2.3 AirTag Spoofing (средний приоритет)**

- **Что**: Подделка Apple AirTag
- **Реализация**: Специальный BLE advertisement с AirTag payload
- **Применение**: Трекинг, социальная инженерия

**2.4 AirTag Monitoring (средний приоритет)**

- **Что**: Обнаружение и мониторинг AirTag устройств
- **Реализация**: BLE scan с фильтрацией по AirTag manufacturer data (0x004C)
- **Применение**: Обнаружение скрытых трекеров

**2.5 Flock Detection (низкий приоритет)**

- **Что**: Обнаружение Flipper Zero устройств
- **Реализация**: BLE scan с фильтрацией по известным SSID Flipper Zero
- **SSID из Marauder**: "flock", "penguin", "pigvision", "fs ext battery"

**2.6 Flipper Spam (низкий приоритет)**

- **Что**: Spam с payload Flipper Zero
- **Реализация**: BLE advertisement с Flipper Zero manufacturer data

**Рекомендации по реализации:**

```cpp
// В BleManager.h добавить:
enum BleAttackType {
  BLE_ATTACK_SPAM = 0,
  BLE_ATTACK_SOUR_APPLE,
  BLE_ATTACK_AIRTAG_SPOOF,
  BLE_ATTACK_FLIPPER_SPAM
};

void startAttack(BleAttackType type);
void startAirTagMonitor();

// В BleManager.cpp добавить payloads для новых атак
```

---

### 3. WifiSniffManager - Новые режимы сканирования

#### Текущая реализация

- Базовое promiscuous сканирование AP
- Подсчет EAPOL пакетов (базовый)

#### Новые режимы из Marauder

**3.1 Packet Monitor (высокий приоритет)**

- **Что**: График активности WiFi пакетов в реальном времени
- **Реализация в Marauder**: `packetMonitorMain()` в WiFiScan.cpp
- **Данные**: Счетчики mgmt_frames, data_frames, beacon_frames, deauth_frames, eapol_frames
- **UI**: График с типами пакетов разными цветами
- **Как добавить**:
  - Расширить promiscuous callback для подсчета типов пакетов
  - Добавить график в SceneManager
  - Добавить режим `MODE_WIFI_PACKET_MONITOR`

**3.2 EAPOL Capture (высокий приоритет)**

- **Что**: Перехват WPA/WPA2 handshake для взлома
- **Реализация в Marauder**: `eapolSnifferCallback()` в WiFiScan.cpp
- **Данные**: Сохранение полных EAPOL пакетов (4-way handshake)
- **Формат**: PCAP для использования с aircrack-ng/hashcat
- **Как добавить**:
  - Улучшить EAPOL detection в promiscuous callback
  - Сохранять полные пакеты (если есть SD карта)
  - Показывать статус "Handshake captured" в UI

**3.3 Station Scan (высокий приоритет)**

- **Что**: Сканирование клиентов (не AP), подключенных к сетям
- **Реализация в Marauder**: `stationSnifferCallback()` в WiFiScan.cpp:6114-6258
- **Данные**: MAC клиента, связанный AP, RSSI
- **Применение**: Targeted deauth, анализ сети
- **Как добавить**:
  - Добавить режим `MODE_WIFI_STATION_SCAN`
  - Парсить data frames для определения клиентов
  - Связывать клиентов с AP из списка

**3.4 Channel Analyzer (средний приоритет)**

- **Что**: Анализ использования каналов WiFi
- **Реализация в Marauder**: `channelAnalyzerLoop()` в WiFiScan.cpp
- **Данные**: Активность по каждому каналу (1-14)
- **UI**: График активности по каналам
- **Как добавить**:
  - Channel hopping с подсчетом пакетов на каждом канале
  - График в SceneManager

**3.5 Channel Activity (средний приоритет)**

- **Что**: Активность по каналам в реальном времени
- **Реализация**: Упрощенная версия Channel Analyzer
- **UI**: Список каналов с индикацией активности

**3.6 Packet Rate (низкий приоритет)**

- **Что**: Скорость пакетов в секунду
- **Реализация**: Подсчет пакетов за интервал времени
- **UI**: Число пакетов/сек

**3.7 PineScan (средний приоритет)**

- **Что**: Обнаружение Pineapple WiFi устройств (Hak5)
- **Реализация в Marauder**: `pineScanSnifferCallback()` в WiFiScan.cpp
- **Метод**:
  - Проверка OUI (MAC vendor) на подозрительные (Hak5: 0x02C0CA, 0x021337)
  - Проверка capability info на подозрительные комбинации
  - Открытые сети от известных производителей Pineapple
- **Как добавить**:
  - Добавить список подозрительных OUI
  - Проверка в promiscuous callback
  - Отдельный режим `MODE_WIFI_PINESCAN`

**3.8 MultiSSID Detection (средний приоритет)**

- **Что**: Обнаружение устройств с несколькими SSID (подозрительно)
- **Реализация в Marauder**: `multiSSIDSnifferCallback()` в WiFiScan.cpp:7031
- **Метод**:
  - Отслеживание MAC адресов с разными SSID
  - Хеширование SSID для сравнения
  - Порог: 3+ уникальных SSID от одного MAC
- **Как добавить**:
  - Структура для отслеживания MAC + SSID hashes
  - Callback для анализа beacon frames
  - Режим `MODE_WIFI_MULTISSID`

**Рекомендации по реализации:**

```cpp
// В WifiSniffManager.h добавить:
enum SniffMode {
  SNIFF_MODE_AP = 0,
  SNIFF_MODE_PACKET_MONITOR,
  SNIFF_MODE_EAPOL_CAPTURE,
  SNIFF_MODE_STATION_SCAN,
  SNIFF_MODE_CHANNEL_ANALYZER,
  SNIFF_MODE_PINESCAN,
  SNIFF_MODE_MULTISSID
};

void setMode(SniffMode mode);
void getPacketStats(uint32_t &mgmt, uint32_t &data, uint32_t &beacon,
                    uint32_t &deauth, uint32_t &eapol);

// Структуры для Station Scan
struct Station {
  uint8_t mac[6];
  uint8_t apBSSID[6];
  int8_t rssi;
  uint32_t lastSeen;
};
```

---

### 4. BeaconManager - Расширения

#### Текущая реализация

- Базовый beacon spam с 8 предустановленными SSID
- Ротация по списку

#### Улучшения из Marauder

**4.1 Rick Roll Beacon Spam (средний приоритет)**

- **Что**: Spam с именами песен Rick Astley
- **SSID из Marauder**:
  - "01 Never gonna give you up"
  - "02 Never gonna let you down"
  - "03 Never gonna run around"
  - и т.д. (8 строк)
- **Реализация**: Добавить новый список SSID в BeaconManager

**4.2 Funny Beacon Spam (низкий приоритет)**

- **Что**: Забавные имена сетей
- **SSID из Marauder**:
  - "Abraham Linksys"
  - "Benjamin FrankLAN"
  - "FBI Surveillance Van 4"
  - "Get Off My LAN"
  - и т.д. (12 имен)
- **Реализация**: Добавить список в BeaconManager

**4.3 Beacon List (средний приоритет)**

- **Что**: Spam из пользовательского списка SSID
- **Реализация**:
  - Загрузка списка из файла (если есть SD)
  - Или ввод через меню
  - Spam всех SSID из списка

**4.4 Custom SSID Support (низкий приоритет)**

- **Что**: Возможность добавлять свои SSID
- **Реализация**: Расширяемый список SSID в памяти

**Рекомендации по реализации:**

```cpp
// В BeaconManager.h добавить:
enum BeaconMode {
  BEACON_MODE_DEFAULT = 0,
  BEACON_MODE_RICK_ROLL,
  BEACON_MODE_FUNNY,
  BEACON_MODE_CUSTOM_LIST
};

void setMode(BeaconMode mode);
void addCustomSSID(const char* ssid);
void loadSSIDList(); // из SD или памяти
```

---

### 5. TrapManager (Evil Portal) - Улучшения

#### Текущая реализация

- Базовый captive portal с одним шаблоном
- Сохранение паролей в памяти
- Клонирование SSID из скана

#### Улучшения из Marauder

**5.1 Сохранение логов на SD карту (средний приоритет)**

- **Что**: Запись перехваченных данных на SD карту
- **Формат**: CSV или JSON
- **Данные**: Timestamp, SSID, Username, Password, MAC клиента
- **Реализация**:
  - Проверка наличия SD карты
  - Запись в файл при каждом перехвате
  - Ротация файлов при достижении размера

**5.2 Множественные шаблоны порталов (низкий приоритет)**

- **Что**: Разные HTML шаблоны для разных сценариев
- **Шаблоны**:
  - System Update (текущий)
  - WiFi Login
  - Bank Login
  - Social Media Login
- **Реализация**: Массив HTML шаблонов, выбор через меню

**5.3 Deauth при подключении (низкий приоритет)**

- **Что**: Автоматический deauth клиента при подключении к порталу
- **Цель**: Заставить клиента переподключиться и ввести пароль
- **Реализация**: Интеграция с KickManager

**5.4 Расширенная статистика (низкий приоритет)**

- **Что**: Детальная статистика по перехваченным данным
- **Данные**: Количество подключений, уникальные MAC, время сессий

**Рекомендации по реализации:**

```cpp
// В TrapManager.h добавить:
enum PortalTemplate {
  TEMPLATE_SYSTEM_UPDATE = 0,
  TEMPLATE_WIFI_LOGIN,
  TEMPLATE_BANK_LOGIN,
  TEMPLATE_SOCIAL_MEDIA
};

void setTemplate(PortalTemplate template);
void saveLogToSD(const char* username, const char* password);
void enableDeauthOnConnect(bool enable);
```

---

### 6. Новые функции для добавления

#### WiFi атаки

**6.1 Auth Attack (средний приоритет)**

- **Что**: Атака на WPA2 через auth frames
- **Реализация в Marauder**: `WIFI_ATTACK_AUTH` (scan_mode 18)
- **Метод**: Отправка множественных auth requests к AP
- **Цель**: Перегрузка AP, DoS

**6.2 Mimic Flood (низкий приоритет)**

- **Что**: Клонирование существующих сетей (beacon spam с реальными SSID)
- **Реализация в Marauder**: `WIFI_ATTACK_MIMIC` (scan_mode 19)
- **Метод**: Spam beacon frames с SSID из скана
- **Цель**: Создание поддельных сетей

**6.3 AP Spam (низкий приоритет)**

- **Что**: Массовый spam точек доступа
- **Реализация**: Улучшенный beacon spam с большим количеством SSID

**6.4 Bad Message Attack (средний приоритет)**

- **Что**: Отправка некорректных EAPOL пакетов
- **Реализация в Marauder**: `sendEapolBagMsg1()` в WiFiScan.cpp
- **Метод**: Отправка malformed EAPOL Key frames
- **Цель**: Нарушение работы WPA handshake

**6.5 Sleep Attack (низкий приоритет)**

- **Что**: Атака через association frames с Power Management bit
- **Реализация в Marauder**: `sendAssociationSleep()` в WiFiScan.cpp
- **Метод**: Отправка association requests с PM=1
- **Цель**: Перевод клиентов в sleep mode

**6.6 SAE Commit Attack (низкий приоритет)**

- **Что**: Атака на WPA3 через SAE commit frames
- **Реализация в Marauder**: `sendSAECommitFrame()` в WiFiScan.cpp
- **Метод**: Отправка SAE commit frames для WPA3
- **Цель**: Нарушение работы WPA3 handshake

#### WiFi сканы

**6.7 Probe Scan (средний приоритет)**

- **Что**: Сканирование probe requests от клиентов
- **Реализация**: Promiscuous mode с фильтрацией probe requests (subtype 0x04)
- **Данные**: MAC клиента, запрашиваемые SSID
- **Применение**: Wardriving, анализ поведения клиентов

**6.8 Signal Strength Scan (низкий приоритет)**

- **Что**: Анализ силы сигнала сетей
- **Реализация**: Сканирование с отслеживанием RSSI
- **UI**: График RSSI по времени

**6.9 Wardriving (низкий приоритет, требует GPS)**

- **Что**: GPS + WiFi сканирование с сохранением
- **Реализация**:
  - Интеграция GPS модуля
  - Сохранение координат + WiFi данных
  - Формат: CSV или GPX
- **Применение**: Картирование WiFi сетей

#### Дополнительные утилиты

**6.10 ARP Scan (низкий приоритет)**

- **Что**: Сканирование сети по ARP запросам
- **Реализация в Marauder**: `pingScan()`, `fullARP()` в WiFiScan.cpp
- **Метод**: ARP requests для определения активных хостов
- **Требования**: Подключение к сети

**6.11 Port Scan (низкий приоритет)**

- **Что**: Сканирование открытых портов
- **Реализация в Marauder**: `portScan()` в WiFiScan.cpp
- **Метод**: TCP connection attempts к портам
- **Порты**: 22 (SSH), 23 (Telnet), 80 (HTTP), 443 (HTTPS), 3389 (RDP)

**6.12 Service Scans (низкий приоритет)**

- **Что**: Обнаружение сервисов (DNS, HTTP, HTTPS, SMTP, RDP, Telnet, SSH)
- **Реализация**: Проверка портов + анализ ответов
- **Применение**: Разведка сети

---

## Архитектурные улучшения

### 1. Модульность

- **Текущее**: Менеджеры для каждого типа функциональности
- **Улучшение**: Более четкое разделение ответственности
- **Рекомендация**: Сохранить текущую архитектуру, добавить новые менеджеры по мере необходимости

### 2. Производительность

- **Оптимизации из Marauder**:
  - Hash tables для MAC адресов (вместо линейного поиска)
  - Кэширование результатов сканирования
  - Оптимизация памяти для больших списков
- **Рекомендация**: Добавить по мере необходимости при работе с большими списками

### 3. Меню

- **Текущее**: Двухуровневое меню (категории → подменю)
- **Улучшение**: Расширить для новых функций
- **Рекомендация**: Добавить новые пункты в существующие категории (WiFi, Tools)

---

## Приоритеты реализации

### Высокий приоритет (реализовать первыми)

1. ✅ Добавить esp32_marauder в .gitignore
2. Targeted Deauth в KickManager
3. Sour Apple attack в BleManager
4. Packet Monitor в WifiSniffManager
5. EAPOL Capture в WifiSniffManager
6. Station Scan в WifiSniffManager

### Средний приоритет

1. Улучшенная проверка PMF в KickManager
2. SwiftPair spam расширение в BleManager
3. AirTag spoofing/monitoring в BleManager
4. Channel Analyzer в WifiSniffManager
5. PineScan в WifiSniffManager
6. MultiSSID detection в WifiSniffManager
7. Rick Roll beacon в BeaconManager
8. Beacon List в BeaconManager
9. Сохранение логов на SD в TrapManager

### Низкий приоритет

1. Manual deauth в KickManager
2. Deauth sniff в KickManager
3. Flock detection в BleManager
4. Flipper spam в BleManager
5. Packet Rate в WifiSniffManager
6. Channel Activity в WifiSniffManager
7. Funny Beacon в BeaconManager
8. Custom SSID в BeaconManager
9. Множественные шаблоны в TrapManager
10. Deauth при подключении в TrapManager
11. GPS интеграция (если есть модуль)
12. SD карта поддержка (если есть модуль)
13. ARP/Port сканирование
14. Service сканы

---

## Примечания по реализации

1. **Адаптация под Heltec V4**: Marauder написан для разных платформ, нужно адаптировать код под ESP32-S3 и специфику Heltec V4

2. **Условная компиляция**: Функции, требующие специфичного железа (GPS, SD), должны быть под условной компиляцией:

   ```cpp
   #ifdef HAS_GPS
   // GPS код
   #endif

   #ifdef HAS_SD
   // SD код
   #endif
   ```

3. **Совместимость**: Сохранить текущую архитектуру Nocturne OS, добавлять новые функции как расширения существующих менеджеров

4. **Память**: Учесть ограничения памяти ESP32-S3, использовать динамическое выделение памяти осторожно

5. **Лицензия**: ESP32 Marauder имеет свою лицензию, при использовании кода нужно соблюдать требования лицензии

---

## Следующие шаги

1. Начать с высокоприоритетных задач
2. Тестировать каждую функцию отдельно
3. Интегрировать в существующее меню
4. Обновить документацию
5. Добавить примеры использования
