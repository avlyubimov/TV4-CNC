# Аппаратный статус TV4-CNC

Снимок на 19 ноября 2026. Источники: сообщения/фотографии/терминальные журналы
владельца и проверенные файлы репозитория. В PR-01 агент не подключался
к устройствам и не повторял аппаратные проверки. «Сообщено владельцем» не
равно независимому измерению агентом. Новые результаты добавлять с версией
ПО, способом проверки, датой и ограничениями; исходные дампы не публиковать.

## Наличие и подтверждения

| Узел | Подтверждено / источник | Что не подтверждено |
|---|---|---|
| ESP32 HMI | Waveshare ESP32-S3-Touch-LCD-7 PCB Rev 1.2, 800×480; владелец и фото | Длительный stress/thermal test, стабильность всех подсистем |
| Flash ESP32 | Приняты 16 МБ; полный локальный образ 16 777 216 байт, SHA-256 проверен; [backup](BACKUP_STATUS.md) | Успешное восстановление заводского образа, отдельная копия вне основного диска |
| PSRAM ESP32 | ROM/esptool сообщает Embedded PSRAM 8MB AP_3v3; rev12 требует octal 80 МГц | На b93cf3b агент подтвердил startup PSRAM 8MB/pool 8192K и свободную память; полный runtime memtest/stress и пропускная способность не проверены |
| Запись HMI | 19 ноября после TLS bench возвращён ordinary rev12 1bfc7e2, 3/3 hashes verified; владелец подтвердил HMI/touch/Wi-Fi; отчёт ниже | Boot ordinary не перечитывали по UART; физические FPS/latency/soak не измерены |
| RGB LCD | На фото владельца HMI и список Wi-Fi выводятся на физический экран | Измерения tearing, FPS/scanout, ориентация/качество всех областей |
| GT911 | На b93cf3b владелец подтвердил касания, удобную клавиатуру и показ/скрытие пароля | Покрытие углов, multitouch/ошибки, latency, воспроизводимый полный тест сенсора |
| Wi-Fi | Физический список SSID/RSSI виден на фото; владелец сообщил ошибку 263 | На b93cf3b владелец подтвердил подключение/IP, агент видел wifi_ip_events=1 в обоих запусках. Неверный пароль, полный reconnect/NVS power-cycle stress ещё не проверены |
| Защита ESP32 | По переданному get-security-info Secure Boot и Flash Encryption Disabled | Это не разрешение менять eFuse; не утверждается защита от физического чтения паролей |
| F407 | DevEBox STM32F4VX_M Ver. 2.0 получена, по новому сообщению владельца | Владелец позднее сообщил suffix VGT6 (STM32F407VGT6); фактическая Flash/RAM/секторы, кварц, schematic/pinout и SWD target ещё не проверены |
| USB F407 | Владелец сообщил 0483:5720 Mass Storage Device | Принадлежность именно F407 плате нужна отдельная проверка; VID/PID не определяет part number/память/bootloader |
| ST-Link V2 | По отчёту владельца работает с Blue Pill | Работа с конкретной F407 платой ещё не подтверждена |
| Blue Pill | STM32F103C8T6; OpenOCD 0.12.0 видит Cortex-M3, Flash 64 КиБ, SWD работает; отчёт владельца | CAN, таймеры, bootloader, подключение к станку не испытаны |
| Backup Blue Pill | Сохранён локально, таблица векторов структурно проверена; отчёт владельца | Путь/хеш в публичном Git не фиксируются без отдельной безопасной записи отчёта; восстановление не проверено |
| Ось X/Z | Один closed-loop NEMA23 заказан, ещё не получен; второй не приобретён | Точная модель двигателя/драйвера, ток, питание, encoder/alarm/ENABLE, стендовые результаты |
| CAN | Внешние трансиверы отсутствуют, по актуальному сообщению владельца | Нельзя считать наличие физического линка подтверждённым по Wiki семейства Waveshare |
| microSD | Поддержка в проекте не реализована | Наличие/тип карты, mount/FAT32/извлечение, разводка/режим шины конкретного стенда |
| OTA / motion | Read-only verification/session PR #9–13 слиты; installer/реальный motion отсутствуют | ESP32 A/B, STM32 bootloader, CAN update, реальные оси/шпиндель не испытывались |

Актуальное сообщение о заказанном, но не полученном двигателе заменяет старое
неоднозначное «purchased/test drive». Старое имя STM32F407VGT6 не считать
аппаратным измерением. В новом сообщении для PR-02 владелец подтвердил
суффикс VGT6; источник — сообщение, не SWD чтение агента.

## Границы доказательств

- Компиляция обоих SDK/профилей и host render — программные результаты.
- Фото подтверждает наличие изображения в момент съёмки, не FPS/latency.
- Структурно корректный backup не доказывает работоспособность восстановления.
- Ни эта инвентаризация, ни симуляция не разрешают включать привод.
- Следующая аппаратная работа — только отдельное разрешённое испытание;
  список неизвестных и критерии: [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md).

## Базовые данные ПО

Актуальный main после согласования PR-01: `0c4168e864d4c867ba0f49a86eba72c0d59205a1`.
[PR #5](https://github.com/avlyubimov/TV4-CNC/pull/5), head
`b92c559c304f318e4a76c37f438717af2fac4124`, уже слит: координаты и исправление
Wi-Fi/ввода включены в main. Это не подтверждает их испытание на устройстве.
Версия на плате не выводится из имени main. PR-01 сохраняет код main и не
выполняет повторную прошивку.

## Первый baseline b93cf3b — 18 ноября 2026

[Sanitized evidence и ограничения](evidence/hardware/2026-11-18-b93cf3b/README.md).
Internal free около 25–27 KB, min 7796 bytes, largest block 12288 bytes:
runtime HTTPS пока блокирован memory budget/allocation review и TLS peak test.
Вторая serial выборка содержит новый boot; влияние UART open/close не изолировано,
не считать это непрерывным soak или watchdog failure. Новых serial/reset операций
агент не выполняет без согласования следующего испытания.

## PSRAM bench 1c38d1e — 18 ноября 2026

По отдельному разрешению записан rev12-memory-bench; [три цикла PASS и evidence](evidence/hardware/2026-11-18-1c38d1e-psram/README.md).
Буфер34000bytes реально выделен в PSRAM, pattern/release PASS, PSRAMfree
возвращалась к исходному значению. Bench watermark минимум2684bytes.
Владелец подтвердил сохранение работы HMI/touch/Wi-Fi. Один60s UART capture,
в нём один boot и нет ошибок; после close порт не открывали. Internal largest
всё ещё12288: TLS peak/time/budget review остаётся отдельным блокером.
На завершение теста 18 ноября оставалась 1c38d1e **memory-bench**; текущий статус ниже.

## UTC/TLS bench b085abb — 19 ноября 2026

[Разрешённая проба и numeric evidence](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md).
SNTP PASS/source unauthenticated, но TLS OPEN FAIL: mbedtls_ssl_setup -0x008D,
PSA_ERROR_INSUFFICIENT_MEMORY. HTTP/сертификат не проверены. Internal window
min8795/largest sample7680bytes, monitors cleanup PASS. Это реальный blocker
runtime HTTPS; production memory budget не подтверждён. Один90s capture,
17 diagnostics/один boot; ошибки display/touch/queues/watchdog не наблюдались.
После пробы b085abb **вернули ordinary rev12 b085abb**, SDK6.1.1, 3/3 hashes verified;
UART после возврата не открывали. Владелец подтвердил главный экран/касания/Wi-Fi.
Проверка записи не равна factory restore. Старое разрешение исполнено.

Software PR-04k [исправляет allocator и SDK certificate dates](TLS_MEMORY_REVIEW.md)
только в TLS diagnostic profile. Его программные проверки сами по себе
не подтверждают hardware; последующее испытание и текущая запись — ниже.

## TLS/PSRAM bench 1bfc7e2 — 19 ноября 2026

[Отчёт и sanitized evidence](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md).
По отдельному разрешению испытан merged PR #22: SNTP PASS, TLS/HEAD HTTP200
PASS, cleanup/monitors PASS. Один90s capture,17diagnostics/один boot, ошибки
касаний/дисплея/очередей/watchdog не наблюдались. Прежний isolated TLS OPEN OOM
не повторился, но production gate не закрыт: internal window min3123bytes,
historical min2408,worker stack watermark912/6144bytes. Нужен memory/stack review.
После пробы возвращён **ordinary rev12 1bfc7e2**,SDK6.1.1,3/3 hashes verified,
UART больше не открывали. Владелец подтвердил HMI/touch/Wi-Fi. Это текущая запись;
новых аппаратных операций/OTA install этот отчёт не разрешает.

PR-04l [готовит TLS headroom fix](TLS_HEADROOM.md) без новых аппаратных операций.
Текущая запись остаётся ordinary1bfc7e2; новую экономию peak heap/stack/CPU
не считать аппаратно подтверждённой.
