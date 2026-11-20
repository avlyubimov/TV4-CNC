# TV4-CNC — правила разработки Codex

Правила действуют на весь репозиторий. Вложенные AGENTS.md уточняют область,
но не ослабляют безопасность. Продолжать существующий проект, не создавать
новый каркас вместо работающей реализации.

## Перед изменениями

Прочитать README.md, docs/ARCHITECTURE.md, docs/ROADMAP.md,
docs/IMPLEMENTATION.md, docs/HARDWARE_STATUS.md, docs/OPEN_QUESTIONS.md,
docs/BOARD_ADAPTER.md, docs/RUNTIME_DIAGNOSTICS.md, docs/OTA_ARCHITECTURE.md,
docs/RELEASE_PROCESS.md и правила изменяемого модуля. Проверить main, открытые
PR и Actions; не копировать код уже открытого PR. Данные о наличии оборудования
брать из HARDWARE_STATUS, а не из старых планов или имени USB-устройства.

## Архитектура

- ESP32-S3: Waveshare ESP32-S3-Touch-LCD-7 Rev 1.2, 800×480, LVGL 9,
  ESP-IDF 6.1.1; 5.3.2 — проверяемый резерв. Flash 16 МБ, PSRAM 8 МБ
  согласно текущему аппаратному статусу. HMI, Wi-Fi, будущие microSD и OTA,
  CAN-коммуникации; **никакой генерации STEP на ESP32**.
- STM32F407: DevEBox STM32F4VX_M Ver. 2.0; владелец сообщил STM32F407VGT6;
  owner SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3,252V/
  backup1048576bytes/vector structure PASS подтверждены. Package/RAM/CCM/секторы/
  кварц/распиновка/physical GPIO/CAN ещё не проверены. Только он отвечает за планирование, таймерный STEP/DIR,
  энкодеры, homing, watchdog и состояние реальной машины после commissioning.
- Blue Pill STM32F103C8T6: отдельный стенд протокола/таймеров/загрузчиков;
  не замена F407 и не контроллер станка. Подтверждение владельца: Flash 64 КиБ.
- CAN — не аппаратная безопасность. Реальные GPIO, CAN ID и bitrate назначать
  только отдельным документированным решением с проверкой ревизии/трансиверов.
  Существующий 4-байтный heartbeat и 500 мс — только host-заглушка.

## Неослабляемая безопасность

1. Нормально-замкнутый физический E-stop и независимое снятие опасного
   момента/питания через проверенные контактор/STO. UI STOP не заменяет его.
2. Механическая и электрическая блокировка ручного/CNC переключения;
   конкурирующее включение приводов недопустимо. Сохранить ручной режим.
3. Загрузка, сброс, переподключение, потеря CAN, watchdog, ошибка датчика и OTA
   не запускают оси/шпиндель. Движение по умолчанию запрещено. ACK не является
   ARM/START и не возобновляет движение автоматически.
4. Не активировать приводы до отдельного разрешения на commissioning,
   проверки механики, направлений, концевиков, homing, soft limits, полярности
   ENABLE и аппаратной цепи. Не угадывать «безопасный» уровень GPIO.
5. Не обходить E-stop, ограждения, концевики или блокировки. Силовые цепи,
   заземление, контакторы/STO/VFD требуют квалифицированной аппаратной проверки.
6. Резьба, C-axis и фрезерная приставка — проектные задачи до измерения
   энкодера, люфта, механики, удержания и останова. Компенсация не устраняет люфт.
7. Испытание симуляции не доказывает безопасность или готовность станка.
   Различать SIMULATION, нет связи, ошибка, готовность HMI и готовность машины.
8. Чтение/запись реальных устройств, прошивка, стирание и тесты приводов
   требуют отдельного разрешения владельца на соответствующую операцию.
   Не включать Secure Boot, Flash Encryption, eFuse или STM32 option bytes
   без отдельного разрешения; не выполнять необратимые настройки по умолчанию.

## ПО и интерфейс

- Только владелец LVGL меняет UI; очереди/snapshots передают намерения/данные.
  В callbacks нет блокирующего I/O, файловых операций, ожиданий CAN/сети,
  задержек или тяжёлой обработки. Worker-задачи имеют явные лимиты/тайм-ауты.
- Предпочитать статическое/ограниченное выделение, bounded queues и предсказуемую
  память. Не вводить неограниченную аллокацию в частые пути.
- STEP формируется аппаратными таймерами/DMA/IRQ STM32, не RTOS-задержками.
  Частоты planner, jitter и производительность подтверждать измерениями.
- Явные единицы: мм, мм/мин, мм/об, об/мин, градусы; X radius/diameter,
  машинный/рабочий отсчёт и направления. Геометрию станка не придумывать.
- UI MVP только ru-RU, строки через стабильные ID и fallback на русский.
  kk-KZ/en-US резервируются архитектурно, неполных переводов/переключателя нет.
  Протокольные токены, X/Z/CAN/SSID и технические логи могут быть английскими.
- Шрифты с Ё/ё, разрешённые лицензии, проверка глифов/клиппинга/касания на 800×480.
  Тёмный читаемый UI, контраст и удобные сенсорные цели; отзывчивость как цель,
  не копирование iOS. Не коммитить чужие закрытые бинарники/шрифты.
- Цели: touch feedback <50 мс, 30+ FPS где достижимо, координаты 20–30 Гц,
  отсутствие зависаний при потере связи. Не объявлять их достигнутыми без замеров.
  LVGL refresh rate не равен RGB scanout; event latency не touch-to-display.
- Не добавлять web stack или удалённое управление станком без отдельного задания.

## OTA и релизы — обязательная будущая функциональность

- GitHub Releases → HTTPS → ESP32 OTA Manager; отдельно ESP32 OTA A/B и
  ESP32 → CAN → независимый загрузчик F407. Пока это проект, а не готовый OTA.
- Подписанный манифест, доверенный локальный публичный ключ, SHA-256, hardware/
  protocol compatibility, ограниченные размеры, штатный rollback ESP-IDF.
  Не доверять одному SHA, TLS или имени релиза вместо подписи.
- Разметку мигрировать только отдельной USB-процедурой с бэкапом/проверкой,
  не обычным app OTA. Текущий factory 2 МиБ остаётся до отдельного PR миграции.
- Для STM32 нужны подтверждённая геометрия Flash и состояние без движения,
  шпинделя и программы. Не выбирать адреса/сектора/A-B по предположению.
- Автопроверка разрешена; установка требует явного подтверждения оператора
  для конкретного образа. Сброс/потеря питания не разрешают движение.
- Не публиковать релиз без успешного CI и отдельного явного разрешения
  владельца на конкретный релиз. Push в main не является разрешением.
  Не публиковать фиктивную STM32-прошивку. Приватные ключи/пароли/Flash dumps
  не попадают в Git, логи, PR и CI-артефакты.

## Процесс

1. Отдельная ветка, короткий план и критерии приёмки, небольшой независимый PR.
2. Фиксировать точные toolchain/dependency версии и лицензии; воспроизводимые
   команды и реальные результаты. Не менять подтверждённые GPIO без проверки.
3. Прогонять соответствующие host/fault/timeout/layout тесты и матрицу сборок
   для изменений кода. Для docs-only проверять ссылки, согласованность и scope;
   не объявлять новую сборку, если её не выполняли.
4. PR описывает проблему/результат, файлы, критерии, проверки, ограничения,
   аппаратные предположения, риски и откат. Измерения или отметка «не измерено».
5. Не сливать собственный PR без явного разрешения владельца. Не создавать
   теги, релизы, секреты CI и публикации под видом обычного push.
6. Обновлять актуальный статус; исторические отчёты сохранять с явной пометкой.

## Текущее задание

PR #1–34 слиты, база main `b56986f`; пять workflow PASS, открытых PR перед
работой нет. Startup/DATA/crypto/handoff/preflight/completion/outcome и PR-05b
release policy review существуют. PR-08a: читать FIRMWARE_INFO_UI.md/NEXT_STAGE.md.
Только settings menu + read-only local build/SDK/profile UI. Snapshot72bytes,
origin UNKNOWN/DEVICE/SIMULATOR; unknown peer/protocol не заменять test versions.
SDK app descriptor — display metadata, не trusted installed OTA context.
Новые панели lazy-create/delete; callbacks только navigation requests. Нет новых
runtime HTTPS/verifier/session/install callers, production Motion schema,
actual SemVer/keys/provisioning. Draft counters не переводить в SemVer; HMI
success не заменяет независимый bootloader verifier. Owner повторил VGT6,
full marking/HSE неизвестны.
Worker completion -> clean lease close -> historical outcome/ACK уже реализованы;
pending release не positive; quarantine держит handle.
Peer report не authenticated/local verified/install/quiescent/ARM authority.
Перед будущим действием нужна fresh negotiation/full saved-byte verification.
Не вводить actual F407 map, default addresses/IRQ/alignment; policy generation
меняется с profile/keys/context. Нет runtime caller, CAN wire driver,
Flash map/writer, physical quiescent proof, install/COMMIT/jump или ARM/START.
Candidate/CRC/ACK/generation не являются install token или остановленным станком.
Immutable lease гарантирует backend; failed release удерживает handle/retry/
quarantine, clear/reopen его не скрывают. RX deadline не продлевать.

Не повторять device access и не читать/публиковать raw dump. Owner1MiB не
задаёт RAM/sector/bootloader layout. Startup synthetic fixture не является
F407 profile. BOARD_IMAGE=ON всегда fail; no .bin/ELF upload. No-I/O adapter
inhibit=false/watchdog unavailable; нельзя объявлять physical safe state.
Host/sanitizers30 suites, standalone7 прежний, ARM/static/board/output gates; прежняя
ESP SDK matrix8 сохраняется. HMI UI/config/board/dependency locks/security прежние.
Исключение UI — новый локальный экран PR-08a и меню; active dashboard/Wi-Fi/STOP
сохранены. Новых update/download/install buttons/history или ложной readiness нет.
Native JSON encoding gate усилен без изменения pinned cJSON или UI callbacks.
Host-only test signing driver не включать в ESP32/STM32 firmware или artifacts.
PR-05 candidates остаются metadata-only, CI exact workspace trust без wildcard.
Runtime ESP resource budget/load/fault/soak/installer gates ещё открыты.
На плате ordinary8f0d899,owner HMI/touch/Wi-Fi PASS; разрешения исполнены.
Новых device/flash/reset/serial/UTC/network probes без отдельного exact-image
согласования не выполнять. Нет keys/release/eFuse/CAN/motion и auto-merge PR.
