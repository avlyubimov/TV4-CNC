# Waveshare rev12 — первый baseline, 18 ноября 2026

Firmware source: b93cf3bb7ac498e762c525b5bfd194e49d3ba05c, project_version b93cf3b,
ESP-IDF 6.1.1, LVGL 9.2.2. Плата только USB/питание без CAN/исполнительных
устройств — подтверждено владельцем в разрешении. Это не commissioning станка.
Владелец явно разрешил записать именно b93cf3b. Бэкап 16 777 216 bytes повторно
проверен SHA-256 PASS перед записью; dump/его содержимое в Git не добавлены.

## Запись и запуск — наблюдал агент

Записаны bootloader 20656 bytes @0x0, partition table 3072 bytes @0x8000,
app 1566624 bytes @0x10000. Esptool 5.6.0: 3/3 Hash of data verified, exit 0.
App SHA-256 f34ac4f08c17fb23f842676d63c88fa7b87ec47317e17f01fcb3dbe7f2cfe771.
Flash 16MB, DIO 80MHz; active factory 2 МиБ, не A/B. NVS отдельно не стиралась.
Secure Boot/Flash Encryption disabled при preflight; eFuse не изменялись.

startup-summary.log — allowlist строк: версия b93cf3b, SDK, PSRAM 8MB,
RGB 800×480/16MHz/один PSRAM framebuffer, UI ready. Полные локальные UART/flash
logs в Git не включены: возможны сетевые/аппаратные идентификаторы.

## Подтверждено владельцем на устройстве

- Главный экран появился, касания работают.
- Своя Wi-Fi сеть подключена, IP виден.
- Клавиатура удобная, показ/скрытие пароля работают.

Это ответы владельца, не независимый визуальный тест агента. Углы/края сенсора,
физическая latency/FPS, неверный пароль/текст ошибки и полноценный reconnect
stress пока не проверены.

## Диагностика — наблюдал агент

Сохранены только numeric/fixed schema tv4_diag records: 6 записей первой
выборки (uptime 7578..32732 ms), 8 второй (7562..42849 ms). В обеих
wifi_attempts=1, wifi_ip_events=1, wifi_disconnects=0; сеть получала IP после
запуска. Есть признак восстановления сохранённого профиля, но отдельный
power-cycle/изменение/сохранение нового пароля тест не проведён.

Вторая выборка содержит новый boot/uptime: повторное открытие UART сопровождалось
перезапуском, хотя capture script не посылал явный RTS reset. Нельзя объявлять
эти две выборки непрерывным soak. Причина влияния open/close USB-UART/линий
DTR/RTS отдельно не изолирована. Дальнейшие serial/reset операции остановлены.
Reset reason=1, watchdog_boot=0; panic/Guru Meditation в выборках не наблюдались.

- Internal free 25491..27023 bytes, historical minimum 7796 bytes.
- Largest internal block 12288 bytes во всех records.
- PSRAM total 8388608, free около 7616576 bytes.
- Минимальные sampled watermarks: owner 10776,
  touch 2548, Wi-Fi 1164,
  export 1164 bytes (IDF stack watermarks).
- display_errors/touch_errors/queue_drops/export_drops/wifi_queue_drops=0
  во всех сохранённых records.
- cpu_percent/touch_to_display_ms/rgb_scanout_hz=null, не измерены.
  LVGL refresh windows не являются RGB FPS; owner duty не является CPU load.

## Ограничение перед runtime HTTPS

Session workspace host sizeof=34024 bytes (целевой sizeof ещё проверить),
largest internal block всего 12288. TLS не запускался; его peak ещё неизвестен.
Не размещать session на маленьком task stack и не включать runtime downloader
без allocation/lifetime review и измерения internal heap/TLS peak. Рассмотреть
PSRAM для bounded metadata/session buffers; не переносить RGB/DMA/draw buffers
и не менять PCLK/PSRAM timings без отдельного измерения производительности.

10-minute baseline, длительный soak, TLS time/CA/redirects/отмена/потеря сети,
OTA writes/rollback/USB A/B migration и restore backup остаются UNKNOWN.
Нет команд приводу, CAN, STM32, Secure Boot/Flash Encryption/eFuse операций.
Следующий software этап — memory budget/allocation и UART capture procedure,
затем отдельное разрешённое runtime HTTPS испытание.
