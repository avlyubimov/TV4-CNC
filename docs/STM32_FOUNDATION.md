# PR-02 — STM32 Motion Foundation

PR-07n от main `d18cf93`: [metadata setup/shared namespace](STM32_METADATA_SETUP.md)
связывает original CLOSED image negotiation, live empty image endpoints и
metadata DATA. Paired ID выделяется из общего volatile pool, raw MS exchange
проверен с потерями всех фаз, complete metadata endpoints закрывают pool.
Host54/sanitizers54/standalone28, independent verifier/lease/model/close прежние.
Это software local contract; authentication/persistent identity/runtime/installer
и все physical F407 blockers открыты. Устройств не касались.

PR-07m от main7640f43: [metadata container/RX](STM32_METADATA_TRANSPORT.md)
компилируется отдельно, не входит в MCU app/driver. Distinct metadata DATA ID,
owned unverified snapshot и real verifier integration не задают RAM/Flash map,
clock/GPIO/CAN или physical inhibit. Owner facts/аппаратные запреты прежние.

PR-07l от main277801e: [shared-clock host tests](STM32_UPDATE_TIMING.md) проверяют
очередь/потери/retry/deadlines через прежние fragment/logical/independent verifier
APIs. Core/ARM code footprint прежний; scheduler не ARM/runtime target и не
physical CAN/clock/startup/RAM proof. Аппаратные факты и запреты ниже сохраняются.

PR-07k от main4db99df: [bounded fragment/reassembly](STM32_UPDATE_FRAGMENTS.md)
проверен host/ARM и real signed RAM chains. Это opt-in software library, не
startup/board image/CAN driver или физическое подтверждение памяти/пинов/частот.
Owner SWD/Flash facts и все неизвестные ниже сохраняются, устройств не касались.

Актуализация после mainb19a40b: [owner SWD/Flash/backup facts](evidence/hardware/stm32f407-owner-swd/README.md)
приняты; [PR-02b startup/sections/no-I/O](STM32_STARTUP.md) готовит generic
building blocks и synthetic link checks. Физическая маркировка/RAM/CCM/HSE/
pinout/erase geometry/GPIO/CAN неизвестны. Board image/loader/motion не готовы.
Ниже сохранён исторический scope PR #7; текущая последовательность — [NEXT_STAGE](NEXT_STAGE.md).

База: main a40aeb3, после слияния PR-01. Задача владельца «дальше» разрешает
этот этап; merge, SWD/Flash, включение приводов и выпуск релиза не выполняются.

## План и приёмка

- Переносимое C11 ядро, фиксированная память, один owner, bounded вызовы.
- Состояния, защёлки ошибок, запрет выходов, контроль связи/периода/watchdog.
- Host/fault tests + sanitizers; compile-only Cortex-M4F с точным ARM GCC.
- CI static analysis/object budget; target build явно блокирован, нет .bin.
- HMI, Flash partition и подтверждённые GPIO не меняются.

## Контракт supervisor

Инициализация немедленно вызывает inhibit_outputs. Адаптер обязан синхронно
и без ожиданий запретить STEP, axis enables и шпиндель. В PR адаптер только
mock; полярность GPIO неизвестна и не назначена. Отсутствие callback/config
или отказ inhibit — ошибка. Успех callback не является safety-rated feedback.
Нет enable callback, RUN/JOG и команды движения.

BOOT ждёт self-test; затем DISCONNECTED ждёт heartbeat согласованной внешним
transport сессии; IDLE — только software readiness. Сбой interlock/drive/link
защёлкивает FAULT. Возвращение связи/устранение причины не снимает защёлку:
нужны fresh healthy cycle и отдельный ACK. ACK не включает выходы. При ошибке
периода или platform adapter ACK запрещён до повторной инициализации.
Watchdog reset также защёлкивается и требует self-test/session/healthy cycle/ACK.

Время — uint32 ms modulo 2^32; caller соблюдает монотонность и интервал <2^31.
Timeouts 1..INT32_MAX задаются вызывающим кодом, это не CAN wire constants.
Граница elapsed >= timeout считается отказом; поздний heartbeat не скрывает
пропущенный deadline. Session zero — disconnected; новый session сбрасывает
предыдущий heartbeat. Wire version/sequence/authentication ещё не реализованы:
этот API не подключать напрямую к CAN ISR/непроверенным пакетам.

Cycle вызывается owner после чтения входов и успешного завершения control work.
Watchdog feed только при продвижении времени, после self-test и успешного
inhibit, без latched deadline/platform fault. Link/interlock fault допускает
watchdog feed для диагностики при запрещённых выходах. Повторный вызов с тем же
временем не feed. Реальный IWDG, clock/GPIO/timer/DMA backend и проверка частот
ещё отсутствуют. Сбой/зависание owner должен в будущем пресекать независимый IWDG;
вызовы этой библиотеки сами аппаратный watchdog не запускают.

Supervisor не thread-safe: ISR публикует snapshot, owner выполняет API. Никакого
malloc, I/O, RTOS delay и CAN waits. Struct принадлежит caller; heap/global state
нет. Статусы software supervisor нельзя использовать как подтверждённый
quiescent state для OTA без отдельных датчиков/согласованного PR-07 протокола.

## Сборка и CI

Независимый CMake/Ninja проект можно собирать host или ARM toolchain.
Корневой CMake включает его тест в существующую host/sanitizer matrix.
Новый motion.yml: официальный ARM GNU 14.2.Rel1 (GCC 14.2.1), archive SHA-256
`62a63b981fe391a9cbad7ef51b17e49aeaa3e7b0d029b36ca1e9c3b2a9b78823`,
из developer.arm.com; actions закреплены commit SHA. На CI нет SWD, secrets,
release или flash. GCC -fanalyzer и warnings-as-errors проверяют core.

check_motion_size.py ограничивает сумму text объектов 8192 байт и запрещает
mutable .data/.bss. Это регрессионный бюджет ядра, не измерение Flash/RAM target:
нет startup, linker, stacks, vector table, HAL, peripherals или bootloader.
TV4_MOTION_BOARD_IMAGE=ON обязан завершиться явной ошибкой. Нельзя заменять его
фиктивным app binary ради зелёного target job.

## Аппаратные блокеры и следующие части

Этот раздел — историческое состояние PR #7 до нового owner SWD report;
Flash1024KiB и SWD теперь подтверждены владельцем, остальные blockers разделены
в [OPEN_QUESTIONS](OPEN_QUESTIONS.md). HSE не обязателен для будущего HSI-only
пути; erase geometry блокирует writer/bootloader, RAM/vector profile — board linker.

Владелец сообщил VGT6. Перед board image нужны подтверждение фактической карты
Flash/RAM/секторов и CCM, HSE/PLL/питание, schematic/pinout/boot recovery.
Startup/vector/linker, CMSIS/HAL/LL с версиями/лицензиями и hardware abstractions
добавляются после этого отдельным изменением. До commissioning нет drive output.
Таймерный planner/STEP/DMA/encoder, production CAN и bootloader остаются будущими
этапами. Blue Pill не подменяет F407 geometry. К устройствам не подключались.

## Проверки, риски и откат

Результаты команд: [evidence/build/stm32-foundation](evidence/build/stm32-foundation/).
Тесты проверяют boot/reset, active/latched faults, stale ACK, sessions/reconnect,
точные deadlines, late heartbeat, wraparound, invalid config, platform failure,
watchdog feed и 99 999 детерминированных fault-injection cycles.
Компиляция не подтверждает safety wiring, HSE, таймеры и физический safe state.

Откат — revert PR-02. Данные устройства/разметка Flash не менялись, прошивка
для платы не создавалась. После создания PR остановиться на review; не merge.


## PR-07f — только logical negotiation

[Свежий update-сеанс](STM32_UPDATE_NEGOTIATION.md) проверен на host/ARM с
существующей независимой crypto/model/cleanup цепочкой. Volatile counters/tag
не задают production protocol или real board identity; memory map/clock/GPIO/
CAN/recovery по-прежнему требуют отдельных hardware/review gates. Нет board image.

## PR-07g — independent release context draft

[Signed release binding](STM32_RELEASE_BINDING.md) проверяет release/component/
source/channel/target/image в отдельном opt-in API. Прежние workspace/crypto
owner и legacy API сохраняются. Это не trusted installed-state provider,
production bundle contract. PR-07h [release handoff](STM32_RELEASE_HANDOFF.md)
добавляет software live coordinator, без runtime caller. Нет board image,
actual memory/vector/erase/clock/GPIO/CAN profile или устройств/движения.

## PR-07i — portable RAM consumer/source

[Staging source](STM32_STAGING_SOURCE.md) добавляет bounded worker RAM copy/lease
и real backend integration tests. Никаких startup/linker/clock/actual F407 profile
изменений; caller capacity не считается измеренным RAM/CCM budget. Board image
остаётся запрещённым, volatile buffer не даёт Flash/recovery/install решения.
