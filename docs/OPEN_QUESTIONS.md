# Открытые вопросы и блокирующие решения

Актуально для PR-01, 18 ноября 2026. Единственный свод аппаратных фактов —
[HARDWARE_STATUS.md](HARDWARE_STATUS.md). Не заполнять неизвестные поля
типовыми данными семейства, USB VID/PID, старым заказом или host mock.

| ID | Что нужно подтвердить/решить | Блокирует |
|---|---|---|
| HW-F407-01 | Владелец сообщил STM32F407VGT6; сверить маркировку/документацию и отдельно разрешённое чтение device/Flash ID; фактическая Flash/RAM/секторная карта | Target startup/linker/size budgets PR-02; layout и Flash writer PR-07 |
| HW-F407-02 | Кварц/HSE, питание/PLL/реальная частота; verified board pinout, SWD/boot/recovery способ | Clock/timer/DMA и физическая загрузка F407 |
| HW-USB-01 | Отдельно подтвердить, что устройство 0483:5720 принадлежит именно F407 стенду | Идентификация; VID/PID не доказывает MCU/Flash/прошивку |
| HW-DRIVE-01 | Получение первого NEMA23, точные двигатель/драйвер/питание, STEP/DIR/ENABLE polarity, encoder/alarm, второй привод | Любые испытания приводов/осей |
| HW-SAFETY-01 | Схема независимого NC E-stop, контактор/STO/earthing, mechanical/electrical manual/CNC interlocks с квалифицированной проверкой | Всё опасное движение и commissioning |
| HW-MACHINE-01 | Ходы X/Z, ШВП/передачи, направления/люфт/пределы, X radius/diameter, homing sequence/концевики | Реальные координаты/planner/soft limits; ±1000 мм сейчас только simulation |
| HW-SPINDLE-01 | Привод/торможение шпинделя, прямой энкодер/разрешение/направление/крепление | Feedback, threading/C-axis/индексация/фрезерная приставка |
| HW-HMI-01 | Полные PSRAM runtime/stress, GT911 углы/ориентация/ошибки, tearing и стабильность RGB/сети | Подтверждение работоспособности всего HMI, не блокирует host/docs работу |
| NET-01 | Успешное подключение после исправления 263, DHCP, забывание/профили/автоподключение после reboot | Закрытие аппаратной проверки Wi-Fi; исправление PR #5 уже в main, аппаратная проверка ещё нужна |
| CAN-01 | Получение трансиверов, схема линка, уровни/termination/длины, USB/CAN mux и допустимый маршрут на Rev 1.2 | Physical CAN; до этого только host transport |
| CAN-02 | Документированное owner decision по IDs/bitrate/GPIO, production versions/capabilities, timeouts, safe-state/ACK semantics | Реальный driver и протокол; 4-byte v1/500 мс не согласованный wire protocol |
| SD-01 | Наличие/тип карты, FAT32, фактическая шина/CS/card detect и извлечение конкретной Rev 1.2; лимиты/имена/ошибки/политика файлов | PR-06; не назначать пины по аналогии |
| OTA-ESP-01 | Review candidate 16 МиБ layout, recovery size, оба SDK partition parser, backup/USB migration/recovery trials | PR-04; текущий factory 2 МиБ не менять в PR-01 |
| OTA-KEY-01 | Owner-provisioned public key/fingerprint, signer custody, алгоритм/encoding/test vectors/key rotation, manifest schema/min versions | Любая доверенная установка; приватных ключей в Git/логах нет |
| OTA-CONTEXT-01 | Утвердить SemVer HMI release вместо текущего git-derived project version; источник версии bootloader/production protocol/реального layout и peer compatibility | PR-04b policy готовит проверку, но не подставляет тестовые версии; installer заблокирован неизвестным context |
| OTA-TLS-01 | Правильное время для TLS, CA bundle/host allowlist/redirect bounds GitHub, отсутствие insecure fallback | Реальный HTTPS download; статичные часы HMI не источник доверенного времени |
| OTA-F407-01 | По подтверждённым секторам выбрать A/B либо staging+recovery; metadata/vector/commit/power-loss план | PR-07, не заменяется кэшем microSD ESP32 |
| OTA-STATE-01 | Fresh подтверждение F407 без движения/оборотов/программы, session lifecycle и повтор после reboot; согласованный safe state | CAN install; симуляция/heartbeat/ACK доставки не подтверждают останов |
| RELEASE-01 | Версии/каналы/tag policy, signing authority и owner gate для exact SHA/digests, доступность environment protections | PR-05 и любой релиз; main push не разрешение |
| PERF-01 | Реальные LVGL/CPU/heap/PSRAM/stacks/reset/watchdog/queues/reconnect/CAN timeout measurements, touch-to-display и soak methodology | PR-03 acceptance, заявления о <50 мс/30 FPS/20–30 Гц |
| RECOVERY-01 | Независимая копия ESP32 backup, испытание восстановления; данные/отчёт по восстановлению Blue Pill | Аппаратная migration/bootloader уверенность, не permission на запись |

## Уже установлено / не открывать заново

- ESP32 Rev 1.2, RGB/GT911/CH422G: есть официальный пример LVGL 9 commit
  bc51a84b30c0cea018fee6e0d7427ec10102060e и сверенный прямой адаптер.
  Не использовать legacy LVGL 8 demo как замену LVGL 9.
- Flash в проекте 16 МБ; полный ESP32 backup 16 МиБ с проверенным SHA-256.
  Раннее расхождение 8/16 MB в vendor family не основание вернуть профиль к 8 МБ.
- ESP-IDF 6.1.1 и 5.3.2, LVGL 9.2.2, touch 1.2.1, GT911 1.1.3 и per-SDK locks
  уже собираются. Обновлять API заново без ошибки/цели не нужно.
- Физическое изображение HMI/списка сетей наблюдалось на фото. Формального
  полного теста touch/performance это не закрывает.
- F407 DevEBox получена, владелец уточнил suffix VGT6; NEMA23 ещё нет, второго двигателя и трансиверов нет.
- Blue Pill по отчёту владельца доступна через SWD/64 КиБ, backup сохранён;
  это не production motion target. Не подменять ей недостающую F407 geometry.

Статусы закрывать только с источником/датой/версией и пределами результата.
Проведение проверки/прошивка требуют отдельного разрешения владельца;
этот список не разрешает подключаться к оборудованию.

## После baseline b93cf3b

[Аппаратный отчёт](evidence/hardware/2026-11-18-b93cf3b/README.md): startup,
PSRAM, basic touch/keyboard/password visibility и Wi-Fi/DHCP подтверждены.
Перед runtime HTTPS нужны memory allocation review (largest internal block
12288 bytes при host session 34024), TLS peak и проверенный time provider.
Отдельно разобраться с reset при UART open/close; две выборки не являются soak.
