# Реализация: main, открытые PR и аппаратные границы

Актуализация от main `7640f43`, 21 ноября2026: PR #1–44 слиты. Owner SWD
F407/core/Flash1024KiB/backup — [HARDWARE_STATUS](HARDWARE_STATUS.md).
Startup/DATA/admission/handoff/preflight и PR-07e уже в main:
[completion/outcome](STM32_UPDATE_OUTCOME.md) связывает full checks с lease close
и bounded report/ACK; historical report не является install/physical authority.
Исторические Host/LVGL27, sanitizers27, standalone7 и ARM gates — [evidence](evidence/build/stm32-update-outcome/README.md).
Слитый PR #34, PR-05b [release policy review](RELEASE_POLICY_REVIEW.md), исправляет
Unicode/Base64 и проверяет общий signed C/Python corpus: host29/sanitizers29,
native analyzer3units и local SDK6.1.1 headless/rev12 PASS; это история PR #34.
Слитый PR #35, PR-08a [local firmware info UI](FIRMWARE_INFO_UI.md): settings menu,
display build/SDK/profile, unknown peer/protocol, no runtime updater. Host30/
sanitizers30, XRGB8888/RGB565 lifecycle/layout и local SDK6.1.1 headless/rev12
PASS; прежняя CI matrix8 и STM32 gates сохранены.
Слитый PR #36, PR-07f [fresh logical negotiation](STM32_UPDATE_NEGOTIATION.md):
REQUEST/OFFER/ACCEPT/CONFIRM, volatile monotonic pool/peer high-water и bounded
one-time DATA setup. Host31/sanitizers31/standalone8, ARM/static/source/image/
output PASS;1024 sessions +6 new signed chains/3 negatives. Нет identity provider,
wire/CAN authentication, actual keys/context или install/physical permission.
Слитый PR #37, PR-07g [independent release binding](STM32_RELEASE_BINDING.md):
отдельный signed Motion draft с release/component SemVer/channel/source и
target/counter/image checks через прежний engine. Host33/sanitizers33/standalone10,
2745 version comparisons и ARM/static/source/image/output PASS; workspace17664
прежний. Это исторический standalone этап; production bundle/schema/keys/
installed context не задаёт. Legacy strict11-field API прежний.
Слитый PR #38, PR-07h [release handoff](STM32_RELEASE_HANDOFF.md) соединяет release
verifier с прежними live lease/context/vector/deadline guards и completion/outcome.
Host34/sanitizers34/standalone11,6 новых full signed chains, ARM/static7/source/
board/output PASS; прежние footprint/crypto/deadlines/SDK matrix8 сохранены.
Production bundle/context/runtime/wire/installer/hardware readiness не добавлены.
Слитый PR #39, PR-07i [staging source](STM32_STAGING_SOURCE.md): bounded volatile
RAM backend вместо mock consumer/source; ACK после copy, lease exclusion,
idempotent release, counters/high-water сохраняются при discard. Host36/
sanitizers36/standalone13,45 variants/1536 cycles/8 actual signed backend chains,
ARM/static8/source/board/output PASS. Buffer задаёт caller; нет default RAM
reserve/MCU addresses, persistent recovery/identity, runtime/Flash/device actions.
Слитый PR #40, PR-05c [offline cohort review](RELEASE_COHORT_REVIEW.md) связывает
существующие independent signed HMI/Motion проверки с local selection/source/
channel, full images/ESP descriptor и prospective HMI peer range. Legacy HMI
CLI report прежний; fields берутся из одного authenticated snapshot. Host38/
sanitizers38/standalone13 и analyzer8 embedded+1 host/ARM/source/scope gates PASS.
Новый frontend только для host, firmware libraries/frames/configs не изменены.
Production bundle/context/keys/protocol mapping/install authority ещё открыты.
Слитый PR #41, PR-05d [build binding audit](RELEASE_BUILD_BINDING.md) связывает
external-pinned unsigned candidate/full HMI image/ELF с тем же signed cohort.
Captured fields не перечитываются; factory/A-B и headless/bench/rev12 mismatch
не скрываются. Report только blocked/exit3, pin/unsigned source claims не дают
provenance/CI/install authority. Host39/sanitizers39, standalone13 и прежние ARM
gates; matrix8 проверяет self-generated candidate integrity pins. Firmware
код/settings/dependencies прежние; hardware results не добавлены.
Слитый PR #42, PR-07j [byte codec](STM32_UPDATE_WIRE.md) добавляет opt-in C11
canonical LE framing для8 existing message kinds, без memory ABI/padding/tail.
Shared pure negotiation shape helper не меняет state/policy. Host42/sanitizers42/
standalone16, independent golden12,65536 outcome tokens,200000 mutations и
8 existing signed RAM chains через byte path. Strict analyzer12 embedded+1 host
и wire budget guard6 PASS. ARM4 libraries43362text (+18), codec1219 отдельно,
data/bss/frame budgets сохранены. Нет production CAN/fragmentation/metadata
transfer/identity/runtime/Flash/hardware действий; decode OK не install token.
Push CI также обнаружил старый build-candidate flaky fixture: constant `x` мог
совпасть с SHA tail. Теперь test XOR1 гарантированно меняет байт; verifier прежний.
Нет runtime caller, Flash writer/map/commit/jump или физического safe-state proof.
Слитый PR #43, PR-07k [fragment/reassembly draft](STM32_UPDATE_FRAGMENTS.md):
separate opt-in C11, bounded286byte message/<=8byte envelope, local identity,
no counter wrap/reset, peek/queue commit, exact duplicates/order/conflict,
progress/total deadlines и READY backpressure/single codec take. Host45/
sanitizers45/standalone19; oracle3/all263 cases/100000 faults/8 signed RAM chains,
analyzer13 embedded+1 host/ARM/source/scope/board/output PASS. ARM fragments
1536text/data0/bss0/frame48 отдельно, old libraries/codec прежние. Это не
production identity/routing/CAN driver/metadata transfer/runtime/installer;
аппаратных действий или новых измерений нет.
Слитый PR #44, PR-07l [shared-clock composition](STM32_UPDATE_TIMING.md):
HOST-only scheduler с bounded queues/delays/drop/duplicate проверяет существующие
fragment и logical APIs на одной шкале.34 DATA scenarios/8 signed positive chains/
2 refusals, host47/sanitizers47/standalone21; actual analyzer13 embedded+2 host,
ARM/source/board/output/scope gates PASS, core footprint прежний. Логические
сроки не продлеваются при fragment recovery; replay после reboot остаётся
открытым. Нет runtime/metadata transport/физических измерений/устройств.
Текущая ветка PR-07m [metadata transport](STM32_METADATA_TRANSPORT.md): opt-in
container codec/RX поверх прежнего DATA, globally fresh separate metadata ID,
bounded raw document/signature/key ID, ACK после copy/single owned snapshot.
Image RX lifetime не reset;8 delivered-metadata signed chains +3 refusals,
host50/sanitizers50/standalone24, analyzer14 embedded+2 host и old+new ARM guards.
New separate1568text/data0/bss0/frame48, old libraries/runtime/dependencies прежние.
Metadata output UNVERIFIED; independent signature/raw request digest/full image/
lease/model/physical/operator/install gates не заменены. Descriptor setup ещё
local input; routing/identity/production worker и hardware readiness не добавлены.
Следующий этап — [NEXT_STAGE](NEXT_STAGE.md). Ниже исторические снимки отдельных PR;
их старые counts/«не реализовано» не заменяют текущий статус этого абзаца.

Снимок PR-01 от 18 ноября 2026. База: main
`0c4168e864d4c867ba0f49a86eba72c0d59205a1`. Это не утверждение, что все
коммиты рабочей ветки уже слиты. Отчёт: [PR01_REVIEW.md](PR01_REVIEW.md).

## Есть в main

- ESP-IDF 6.1.1 и резерв 5.3.2; headless/rev12, Flash 16 МБ, factory app
  2 МиБ, LVGL 9.2.2, esp_lcd_touch 1.2.1, GT911 1.1.3, точные per-SDK locks.
- Русский simulation-only UI 800×480, ограниченная очередь, модель X/Z,
  инертный transport_send и 4-byte host heartbeat с тайм-аутом 500 мс.
- Прямой RGB/GT911/CH422G адаптер с USB_SEL=0, один framebuffer в PSRAM,
  startup screen/проверка touch, отдельный polling и единственный LVGL owner.
- Главная, библиотека четырёх инструментов с черновиком/валидацией/клавиатурой,
  Диагностика и журнал RAM 32 записи; коррекции инструментов не управляют осью.
- Виртуальные обороты и ограничение подачи, расчёт мм/об, STOP/fault/ACK и
  отдельный явный restart simulation; никакой физической передачи команд.
- Wi-Fi worker, список/профили восьми сетей в NVS, автоподключение/забывание.
  Код есть, но успешное подключение/сохранение на плате ещё не подтверждены.
- Host tests (build validation, core, Wi-Fi profiles, render, RGB565), sanitizers,
  CI docs/host/SDK matrix, runtime health logs и базовые UI метрики.

[Сборка](../firmware/esp32-hmi/README.md), [Wi-Fi](WIFI.md),
[загрузка](BOOT_SCREEN.md), [инструменты](SOFTWARE_STAGE_003.md),
[метрики и пробелы](RUNTIME_DIAGNOSTICS.md). 36 °C/24.1 В и часы/дата в UI —
примерные/фиксированные подписи, не аппаратные датчики или подтверждённое время.

## Изменения PR #5 включены в main

[PR #5](https://github.com/avlyubimov/TV4-CNC/pull/5) слит в main коммитом
`0c4168e864d4c867ba0f49a86eba72c0d59205a1`. Сохранены рабочий/машинный отсчёт с подтверждением
нуля детали и журналом, feed override при выборе шага, исправление возможного
источника 263, полноэкранный password editor/show-hide и текстовые ошибки Wi-Fi.
Координаты действуют только в симуляции, без перемещения физических осей.
Подробности: [WORK_COORDINATES.md](WORK_COORDINATES.md), [WIFI.md](WIFI.md).
Аппаратная проверка новых исправлений ещё требуется. PR-01 согласован с этим
main; исполняемый код и hardware settings относительно main неизменны.

## Не реализовано

STM32 target firmware/startup/linker/planner/step/encoder/bootloader; physical
CAN driver/negotiation/production wire protocol; microSD/FAT32/file manager;
GitHub release/signing pipeline; ESP32 A/B/HTTPS installer/health rollback;
F407 CAN update; единый Updates UI. Экран «Программа», непрерывный JOG,
реальные homing/limits и обработка станка тоже не готовы.
Все они запланированы в [ROADMAP.md](ROADMAP.md), а не объявлены работающими
из-за названия каталога или наличия CI сборки ESP32.

## Аппаратные результаты и ограничения

Полный ESP32 backup 16 МиБ с SHA-256 проверен; restore не испытан.
Владелец выполнил запись HMI (esptool hashes verified) и прислал фото физического
изображения/списка Wi-Fi. Пользователь сообщил ошибку 263 и неудобную клавиатуру.
Это подтверждает наблюдавшееся изображение и взаимодействие, но не полный
протокол испытаний GT911, успешный DHCP/reconnect/NVS, FPS/latency или стабильность.
Новые исправления #5 на плате после последней проверки не подтверждены.

DevEBox F407 получена, точная геометрия неизвестна; NEMA23 не получен,
трансиверов нет. Blue Pill/SWD/backup сообщены владельцем, а не испытаны агентом
в PR-01. Полная таблица: [HARDWARE_STATUS.md](HARDWARE_STATUS.md).
Условные ±1000 мм симуляции не являются ходом или soft limits станка.

## Откат и границы PR-01

PR-01 меняет только Markdown-документы/AGENTS. Откат — revert документационного
коммита, без firmware rollback или записи Flash. partitions.csv, sdkconfig,
C/C++, build scripts и workflow YAML не меняются. Бэкапы и окружения исключены
из Git. Испытания/прошивка, публикация релиза и переход к PR-02 требуют
отдельного разрешения/проверки владельца после отчёта PR-01.

## PR-02 — следующий этап

В отдельной ветке добавлены portable STM32 supervisor, host/fault tests и
compile-only Cortex-M4F библиотека. Нет startup/linker/board image, HAL/STEP
или физического CAN. Владелец сообщил STM32F407VGT6; HSE/pinout и фактическая
геометрия остаются блокерами. [Контракт и проверки](STM32_FOUNDATION.md).

## PR-03 — программная диагностика

Добавлены fixed-window histograms, ресурсы/стек/counters/reset, панель ресурсов
и worker serial JSON export. Перенесён printf из UI owner; render callback не
логирует. [Детали и оставшиеся hardware/CPU/soak задачи](HMI_PERFORMANCE.md).

## PR-04a — verification foundation

Добавлены host/SDK P-256/SHA-256 primitives, локальный валидатор подписанного
релиза и неактивный candidate A/B CSV с проверкой обоих SDK. Нет production
trust anchors, downloader/installer/rollback или записи устройства.
[Контракт, evidence и следующие части](OTA_VERIFICATION.md).

## PR-04b — native manifest policy

Общий C parser/policy проверяет exact-byte подпись до JSON allocations,
затем bounded schema/target/layout/versions/protocol/URL. Namespaced cJSON
v1.7.19 MIT закреплён SHA-256. Installer/context provisioning ещё отсутствуют.
[Описание и evidence](OTA_MANIFEST_POLICY.md).

## PR-04c — read-only image stream

Добавлены incremental dual SHA-256, ESP image format/descriptor/checksum checks,
receive/cancel/error/deadline состояния и cleanup. Нет sink/HTTP/Flash/boot commit.
[Описание и evidence](OTA_IMAGE_STREAM.md).

## PR-04d — signature envelope / HTTPS

Добавлены bounded конверт подписи и read-only HTTPS transport primitives.
Не подключены к app_main/UI, нет discovery/OTA Manager/installer/Flash writes.
[Контракт, проверки и ограничения](OTA_HTTPS.md).

## PR-04e — read-only session

Единая worker-only операция связывает metadata/envelope/policy/image checks,
общий timeout/cancel, cleanup и terminal result. Нет runtime task/UI/install.
[Контракт, evidence и board gate](OTA_SESSION.md).

## Baseline b93cf3b на Waveshare — 18 ноября 2026

По отдельному разрешению владельца записана ESP-IDF 6.1.1 rev12; 3/3 hashes
verified. Boot log подтверждает версию/PSRAM/RGB, владелец подтвердил
экран/касания/клавиатуру/показ пароля/Wi-Fi IP. [Evidence, memory blocker и
ограничение UART capture](evidence/hardware/2026-11-18-b93cf3b/README.md).
Это не OTA install/soak/commissioning и не разрешение на следующую запись.

## PR-04f — PSRAM-only session workspace

Добавлены bounded allocation/lifetime, explicit reviewed headroom policy и
ELF target sizeof check. Нет internal fallback, runtime calls или installer.
[Контракт, evidence и оставшиеся ограничения](OTA_MEMORY.md).

## PR-04g — explicit memory bench profile

Добавлен rev12-memory-bench: три bounded allocation/pattern/release cycles в
worker после 15 секунд работы UI. Обычные headless/rev12 отключены от probe.
Нет HTTPS/session_run/Flash/boot operations. [Контракт и board procedure](OTA_MEMORY_BENCH.md).

## Hardware memory bench 1c38d1e

Отдельно разрешённая USB-запись rev12-memory-bench и один60s capture выполнены.
[completed=3 pass=1, timings/heap и ограничения](evidence/hardware/2026-11-18-1c38d1e-psram/README.md).
На плате остаётся этот bench image. Runtime TLS budget/installer не включены.

## PR-04h — time gate для native HTTPS

Свежая local provenance/age/skew anchor проверяется вокруг backend I/O и final
cleanup; boolean hint не даёт native transport разрешения без anchor. Нет
SNTP/clock updates/runtime UI. [Контракт и evidence](TLS_TIME_GATE.md).

## PR-04i — bounded SNTP source

Worker-only lifecycle с deadline/cancel/cleanup и SDK TCP/IP ownership.
Mark SNTP_UNAUTHENTICATED после stop/sample/policy; нет startup/UI/TLS trigger.
[Контракт, evidence и ограничения](SNTP_SOURCE.md).

## PR-04j — отдельный UTC/TLS bench

rev12-tls-bench выполняет one-shot SNTP/verified HEAD и heap-window snapshots
в worker. Ordinary profiles off, без response body/OTA install.
[Policy, scope, evidence и hardware gate](UTC_TLS_BENCH.md).

19 ноября [разрешённый b085abb bench](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md)
подтвердил actual SNTP/private policy, но TLS OPEN FAIL (-141 insufficient memory).
Нет успешного verified HEAD/HTTP200. Ordinary rev12 того же SHA возвращён,
3/3 hashes verified; владелец подтвердил HMI/touch/Wi-Fi. Это измерение отказа,
не allocator fix или разрешение runtime HTTPS/OTA; следующий gate — memory review.

## PR-04k — изолированный TLS memory fix

[TLS_MEMORY_REVIEW.md](TLS_MEMORY_REVIEW.md): native SDK external allocator,
X.509 date checks и строгая artifact validation только для TLS diagnostic image.
Остальные профили/internal reserve/record sizes сохранены. Нового аппаратного
PASS нет; на плате ordinary b085abb. Это не runtime OTA/installer integration.

## Аппаратная проверка PR-04k — 1bfc7e2

[SNTP/verified HEAD200 PASS](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md),
оба cleanup/monitor PASS. Ordinary rev12 того же SHA возвращён,3/3 hashes verified,
владелец подтвердил HMI/touch/Wi-Fi. Internal min3123/window,2408/history,stack912
ограничивают дальнейшее runtime OTA: нужен отдельный review запаса и failure paths.
Успешного manifest/image download/install/rollback и performance измерений нет.

## PR-04l — собственный stack frame / HTTP RX

[TLS_HEADROOM.md](TLS_HEADROOM.md): read-only try-lock Wi-Fi online getter,
HEAD HTTP RX2048 и пять worker-phase snapshots. Только diagnostic caller;
UI snapshot/NVS API и buffers TLS/DMA/stacks прежние. Нет runtime OTA integration
или нового измерения на плате. Hardware record1bfc7e2 остаётся актуальным.

## Аппаратная проверка PR-04l — 8f0d899

[Сравнение с1bfc7e2](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
SNTP/HEAD200/cleanup PASS,window internal minimum5307/history4204,stack1840bytes.
Phase snapshots показывают recorded low уже после open. Production budget/CPU/
worst-case не подтверждены. Ordinary rev12 того же8f0d899 возвращён,3/3 hashes
verified,владелец подтвердил HMI/touch/Wi-Fi. Runtime body/install/rollback нет.

## PR-04m — guarded read-only session entry

[OTA_RESOURCE_GUARD.md](OTA_RESOURCE_GUARD.md): pure policy/checker, read-only SDK
heap/stack sampler и session boundary checks с отдельным report. Проверки не
резервируют память/не прерывают SDK и не дают production budget. Нет caller
в app_main/UI/network tasks, новых native operations или изменения платы.

## PR-05a — CI review-кандидаты

[BUILD_CANDIDATES.md](BUILD_CANDIDATES.md): offline проверка полного ESP app
image и ELF digest, clean source/SDK/profile provenance, deterministic metadata
bundle/checksums и независимый verifier. Новая host suite, интеграция в8 SDK jobs,
все Actions закреплены SHA. Docker получает Git display version linked worktree.
Реализована подготовка кандидатов; signing, binary Releases, production version/
trust provisioning и установка остаются открыты. Плата ordinary8f0d899 прежняя.

## PR-07b — независимая проверка кандидата STM32

Ветка от main74a7af9, 20 ноября2026: [signature/metadata/image admission](STM32_IMAGE_ADMISSION.md)
на отдельной Mbed TLS3.6.7, с bounded arena и host/ARM проверками. PR #29
transport был отдельным открытым PR на момент того снимка; теперь #29/30 слиты. Нет runtime/Flash/commit/jump,
production metadata/context/keys и аппаратного подтверждения readiness.
