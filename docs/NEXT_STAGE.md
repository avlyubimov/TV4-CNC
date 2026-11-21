# Следующий этап: P0 — первая прошиваемая сборка STM32F407

22 ноября 2026. База main `610f1745d892d5904ea183b50617a20d8ce755bb`,
PR #1–55 слиты; main CI5/5 PASS. PR #54 host и PR #55 priorities завершены.
Текущая работа — одна интеграция **настоящего P0 образа**.
[Контракт/реальные артефакты](STM32_FIRST_IMAGE.md),
[проверки](evidence/build/stm32f407-first-image/README.md),
[ST-Link runbook](STM32_STLINK_RUNBOOK.md),
[порядок P0–P5](IMPLEMENTATION_PRIORITIES.md).

## Текущий результат и остановка

Подготовлен standalone `stm32f407vg-hsi-no-motion-v1`: ELF/BIN/map, HSI16MHz,
98 vectors, SRAM/CCM, отдельный emergency stack, main-only IWDG, RAM/SWD diagnostics,
image/MMIO/negative checks и CI. Размер BIN2596 bytes, SRAM static268 + stacks/
guards5184, CCM probe4; точные source/hash/toolchain — в image-check.json.
Образ **программно проверен**, фактическое исполнение на F407 не подтверждено.
В этом PR нет hardware/SWD/reset/Flash/backup reads, merge или release.
После проверки/создания PR остановиться.

Следующий измеримый аппаратный этап после review — отдельно разрешённый
изолированный F407 bench: preflight protection/backup/marking, установка
точного clean ELF SHA, исполнение startup/HSI/CCM/heartbeat, watchdog/fault
и диагностический отчёт по runbook. До разрешения не подключаться к плате.
Original SHA F407 backup/вторая копия/restore пока не подтверждены;
это gates перед записью, не причина нового предварительного software PR.

## Подтверждённые характеристики

[Owner report](evidence/hardware/stm32f407-owner-swd/README.md): ST-Link V2/SWD
работает, Cortex-M4r0p1, raw ID 0x101f6413, Flash 1024 КиБ, напряжение 3,252 В,
backup 1 048 576 bytes, structural vector check PASS; сообщён VGT6/DevEBox
STM32F4VX_M Ver. 2.0. Новых аппаратных или backup reads не было.
Это не проверка полной маркировки/package, SRAM/CCM, clock/HSE, Flash geometry/
protection/option bytes, GPIO/CAN или restore. ESP32 — прежний ordinary8f0d899,
owner HMI/touch/Wi-Fi PASS; его factory backup 16 МиБ исторически проверен по SHA-256.

## Состав текущей P0 интеграции

**Результат — настоящий программно проверенный ELF/BIN STM32F407, готовый
к отдельно разрешённому стендовому запуску.** Существующие synthetic link probe
и portable libraries использовать для проверок; они не являются этим результатом.

| Часть одной интеграции | Проверяемый результат |
|---|---|
| Официальная память и startup | Проверенные источники/редакции ST для STM32F407VGT6: Flash/SRAM/CCM/секторы/IRQ/vectors; действительные linker MEMORY/sections, data/BSS, MSP/stack, Reset_Handler и полная vector table |
| Первоначальный clock | Явный HSI-only путь без неподтверждённых HSE/PLL; согласованные clock/tick настройки |
| Запрет движения | STEP/DIR, ENABLE, шпиндель и другие motion outputs не активируются при boot/reset/fault/watchdog; неизвестные GPIO/AF/полярности не назначены |
| Диагностика и ошибки | RAM/SWD boot state/heartbeat/reset reason/fault context без обязательных LED/UART/semihosting; обработчики исключений и явная watchdog policy, не поддерживающая ошибочное исполнение |
| Реальные артефакты и CI | ELF↔BIN↔map/size/SHA-256/source/toolchain identity; границы/overlap/overflow Flash/SRAM/CCM/stack, entry/SP/Thumb/reserved/IRQ vectors и отрицательные проверки |
| Установка и recovery | Конкретный ST-Link install/verify/restore runbook с проверкой бэкапа/платы/защиты и exact-image gate перед отдельно разрешённой операцией |

Эти части реализованы вместе в текущем P0; не подменять P0 новой библиотекой или подготовительным
PR модели. В том же PR обновить нужные scoped instructions/профиль/CI guards,
чтобы проверенный реальный образ собирался, а неподдерживаемые карты отклонялись.
ST startup используется как pinned Apache-2.0 reference only; новых runtime
CMSIS/HAL/LL зависимостей нет.

## Сборка, стенд и будущий bootloader

Официальная карта выбранного VGT6 проверена по ST в P0; аппаратная проверка
конкретного экземпляра и поведения clock/RAM/watchdog остаётся отдельным стендом.
Неизвестный HSE не блокирует подготовку HSI образа, неизвестный motion pinout
запрещает выводы движения. Не угадывать безопасные уровни и не объявлять
no-I/O/программный fault handler подтверждением physical inhibit.

Образ не резервирует адреса будущего CAN bootloader/staging/app layout.
Установка на плату требует отдельного разрешения на точные image/hash и
SWD/write/reset/capture действия, проверки бэкапа/независимой копии и recovery
процедуры. Публикация ELF/BIN в релиз и необратимые option bytes — отдельные
решения; build/CI/PR не являются разрешением. Данный PR не запускает эти операции.

## Порядок после P0

1. P1 — рабочий ESP32 OTA: рассчитанная factory recovery+два OTA-слота в 16 МиБ,
   отдельная USB-миграция, existing HTTPS/crypto/compatibility, inactive-slot write,
   self-test/rollback, Wi-Fi/power-loss tests и реальные SRAM/PSRAM/stack measurements.
2. P2 — настоящие воспроизводимые CI binaries/SemVer/source binding/signed manifest/
   SHA-256/prerelease-stable и ручной owner release gate, экран «ПО и обновления».
3. P3 — минимальный реальный CAN exchange после получения трансиверов: проверка
   физических уровней/терминации/pinout/errors/timeouts/recovery. До этого hardware
   CAN неизвестен; существующий протокол сохраняется.
4. P4 — независимый STM32 CAN bootloader/Flash write/recovery по проверенной
   карте и физическому CAN, existing verifier и ESP32 OTA Manager; no update during motion.
5. P5 — microSD/tools/G-code/planner/STEP-DIR/encoders/homing/thread sync/coordinates/
   manual-CNC после стабилизации базовых контроллеров, с отдельным motion approval.

Для новой абстракции нужен конкретный потребитель текущей интеграции.
Подробные результаты/зависимости/приёмка — [IMPLEMENTATION_PRIORITIES.md](IMPLEMENTATION_PRIORITIES.md).
Все записи ниже — история; они не заменяют этот порядок следующей работой
над production context, codebook или ещё одним host transport layer.

## История слитой программной базы

- PR #28: [startup/sections/no-I/O](STM32_STARTUP.md), portable data/BSS init,
  Cortex-M4 reset object, synthetic ARM link tests. Нет real board MEMORY/map,
  полного IRQ table или clock configuration. BOARD_IMAGE=ON fail; no-I/O
  inhibit=false и watchdog unavailable не объявляют физическую readiness.
- PR #29: [logical sender/receiver](STM32_UPDATE_TRANSPORT.md), bounded blocks,
  consumer/ACK, retry/deadlines и RAM fault tests. COMPLETE_UNVERIFIED не
  является crypto/physical safe state; нет CAN wire driver/IDs/bitrate.
- PR #30: [independent admission](STM32_IMAGE_ADMISSION.md), pinned Mbed TLS3.6.7,
  P-256 signature/draft metadata/compatibility/counters/SHA/length/EOF. Нет
  install token, persistent anti-rollback, writer, vector validation или jump.

## Слитая часть PR-07c, PR #31

[Read-only handoff](STM32_UPDATE_HANDOFF.md) связывает completed RX, immutable
source lease и verifier. Boot/transfer/policy/source lifetime и deadline guards
отзывают candidate при изменении контекста. Lease удерживается после verify;
release failures не теряют handle, retries ограничены, quarantine нельзя reset
через clear/reopen. Нет production backend или physical quiescent gate.

Историческая приёмка PR #31: host/LVGL25 suites, sanitizer25, standalone5; 54 signed end-to-end RAM
transfers и10 fault/lifecycle groups; ARM/source/import/frame/static checks,
прежние board/output gates. [Evidence](evidence/build/stm32-update-handoff/README.md).
CI сохраняет все8 ESP SDK profiles. Аппаратных операций или самостоятельного
merge нет. Source generation не является блокировкой; это обязанность backend.

## Слитая часть PR-07d, PR #32

[Image preflight](STM32_IMAGE_PREFLIGHT.md): bounded read-only model checker,
явные alignment/vector count/reserved IRQ/RAM/load bounds, проверка того же
held image после signature/SHA. Unknown profile отклоняется; PROFILE_MODEL_MATCH
не подтверждает hardware/bootability. Actual F407 memory/erase map не выбирается.
Исторические Host26/sanitizers26/standalone6 и ARM/static/source/image/output gates —
[evidence](evidence/build/stm32-image-preflight/README.md). Hardware gates ниже прежние.

## Слитая часть PR-07e, PR #33

[Completion/outcome](STM32_UPDATE_OUTCOME.md) выполняет существующие проверки,
закрывает lease и возвращает historical result через bounded report/ACK.
Потери/duplicates/conflict/clock/reset и cleanup/quarantine проверены в RAM;
ожидание оператора не держит lease. Peer report не authentication/install token.
Host27/sanitizers27/standalone7 и ARM/static/source/image/output —
[evidence](evidence/build/stm32-update-outcome/README.md). Actual board/Flash gates прежние.

## Слитая часть PR-05b, PR #34

[Release policy review](RELEASE_POLICY_REVIEW.md): исправлены malformed Unicode
escapes в native gate и noncanonical Base64 offline; общий signed corpus115,
SemVer2809 comparisons/Unicode65536. Host29/sanitizers29, actual analyzer3units,
local ESP-IDF6.1.1 headless/rev12 PASS; прежние STM32 gates и SDK matrix8 сохранены.
Это проверка HMI contract, не production Motion schema, key/version provisioning
или разрешение установки. [Evidence](evidence/build/release-policy-review/README.md).

## Слитая часть PR-08a, PR #35

[Local firmware info UI](FIRMWARE_INFO_UI.md): «Параметры» → Wi-Fi / ПО,
build/SDK/profile из local descriptor, unknown STM32/CAN, отсутствие настроенного
updater. Panels создаются по требованию и освобождаются при выходе. Host30/
sanitizers30, две render formats/64 navigation cycles и local SDK6.1.1 headless/
rev12 PASS; [evidence](evidence/build/firmware-info/README.md). Не создаёт
runtime HTTPS/verifier/session/install caller или новое устройство доверия.

## Слитая часть PR-07f, PR #36

[Fresh logical negotiation](STM32_UPDATE_NEGOTIATION.md): boot-pair pool,
REQUEST/OFFER/ACCEPT/CONFIRM, monotonic counters/high-water, fixed deadline,
bounded replies и однократный take для DATA.1024 sequential sessions и6 новых
signed full chains/3 negative bindings, host31/sanitizers31/standalone8,
ARM/static/source/image/output PASS — [evidence](evidence/build/stm32-update-negotiation/README.md).
Не создаёт runtime caller, production wire/capabilities/entropy provider,
authentication или install/physical authority. DATA deadlines прежние.

## Слитая часть PR-07g, PR #37

[Independent release binding](STM32_RELEASE_BINDING.md): отдельный signed15-field
Motion draft, release/component SemVer/channel/source selection и прежние
target/counter/full image checks через один existing crypto engine. Host33/
sanitizers33/standalone10,2745 SemVer comparisons, ARM/static/source/image/output
PASS — [evidence](evidence/build/stm32-release-binding/README.md). Workspace17664
прежний, candidate224/policy288 на host. Legacy strict11-field API сохраняется.
Это исторический standalone этап PR #37. Production bundle/CAN ABI и
доверенный installed context/installer остаются открытыми.

## Слитая часть PR-07h, PR #38

[Release handoff](STM32_RELEASE_HANDOFF.md) соединяет signed15-field API с
существующими live lease/owner/source/RX/deadline/vector guards и completion/
outcome. Raw metadata SHA связан с negotiation; новый локальный режим явный,
legacy strict11 path сохранён. Host34/sanitizers34/standalone11,6 новых full
chains, ARM/static7/source/board/output PASS — [evidence](evidence/build/stm32-release-handoff/README.md).
Нет runtime caller/production context или install/physical authority.

## Слитая часть PR-07i, PR #39

[RAM staging source](STM32_STAGING_SOURCE.md) добавляет portable bounded backend
для pending DATA и read lease. Init не reset, discard сохраняет high-water и
monotonic counters; writes/discard запрещены while leased. Host36/sanitizers36/
standalone13,45 variants/1536 cycles/8 signed actual backend chains, ARM/static8/
source/board/output PASS — [evidence](evidence/build/stm32-staging-source/README.md).
Это volatile RAM, без default capacity/адресов или runtime caller. Full image
помещается только в явно заданный buffer; production RAM/staging medium/durable
recovery ещё не утверждены.

## Слитая часть PR-05c, PR #40

[Offline cohort review](RELEASE_COHORT_REVIEW.md) проверяет существующие signed
HMI и optional Motion документы/полные образы как один expected release/source/
channel. App descriptor и prospective HMI→Motion peer range проверены; HMI-only
не создаёт Motion entry. Host38/sanitizers38/standalone13, CLI3/cohort9 methods,
analyzer8 embedded+1 host и ARM/source/board/output/scope gates PASS —
[evidence](evidence/build/release-cohort-review/README.md). Report metadata-only;
production manifest/context/protocol mapping/runtime/install gates не закрыты.

## Слитая часть PR-05d, PR #41

[Build binding audit](RELEASE_BUILD_BINDING.md) соединяет external-pinned unsigned
candidate/full HMI bytes/ELF digest с existing signed cohort. Factory/A-B layout
и headless/bench/ordinary profile mismatch явно блокируют, Motion build evidence
не создаётся. Captured metadata не перечитывается; новый audit всегда blocked,
exit3 и все authority gates false. Host39/sanitizers39/standalone13, новая suite
13methods, прежние ARM gates и CI SDK matrix8 с candidate integrity pin checks.
Unsigned metadata pin не authentication/CI attestation; actual SemVer/keys/layout/
provenance/runtime/installer не настроены. Hardware действий/новых измерений нет.

## Слитый PR #42, PR-07j

[Update byte codec draft](STM32_UPDATE_WIRE.md): отдельная opt-in C11 library,
8 existing kinds/exact bounded LE bytes/explicit enum codebook, no padding/tail.
Shared shape validator не state/auth gate. Host42/sanitizers42/standalone16,
golden12/65536 outcome pairs/200000 mutations и8 signed RAM chains через codec;
analyzer12+1/budget guard6/ARM/source/board/output/scope PASS. ARM codec1219text,
data/bss0/frame32 отдельно от existing4 archives43362text (+18). Полный stack/
RAM/CPU budget не подтверждён. Нет production CAN version/IDs/bitrate/driver/
fragmentation/metadata transfer/runtime/identity/install или hardware действий.

## Слитый PR #43, PR-07k

[Bounded fragments](STM32_UPDATE_FRAGMENTS.md) добавляют отдельную opt-in
библиотеку вокруг codec:4-byte envelope header/1..4bytes chunk,286byte ceiling,
retained counters, explicit route/generation, deadlines/cancel/backpressure и
single take. Нет IDs/bitrate/driver/runtime или anti-replay после reboot;
local identity не сериализуется и не аутентифицируется. Metadata/signature
transport ещё не реализован. Host45/sanitizers45/standalone19, oracle3,
100000 fault chains и8 actual signed RAM chains; ARM fragment1536text/data0/
bss0/frame48 отдельно. Old4 archives43362text и codec1219 прежние. Нет новых
аппаратных подтверждений/обращений к платам. [Evidence](evidence/build/update-fragments/README.md).

## Слитый PR #44, PR-07l

[One-clock composition](STM32_UPDATE_TIMING.md): bounded host-only two-direction
link с queue hold/drop/duplicate и одним clock для всех existing layers.
34 DATA scenarios,8 signed positive chains и2 refusal cases; host47/sanitizers47/
standalone21. Original deadlines/counters не сбрасываются при fragment recovery,
late ACK/OFFER не оживляет expired layer. Host helper не входит в ARM; core code/
footprint прежние. Это не production timing/resource/identity approval.
[Evidence](evidence/build/update-fragment-timing/README.md).

## Слитый PR #45, PR-07m

[Metadata container/RX](STM32_METADATA_TRANSPORT.md): separate DATA identity,
bounded raw document/signature/key ID, ACK после copy, one owned UNVERIFIED
snapshot и сохранённые image deadlines.8 positive chains реально передают
metadata по shared-clock byte/fragment link,3 refusals сохраняют independent
signature/raw request digest/live image guards. Host50/sanitizers50/standalone24,
ARM separate metadata1568text/data0/bss0/frame48; old libs прежние. Transport
setup ещё local input, не authenticated wire exchange/runtime/installer.
[Evidence](evidence/build/update-metadata-stage/README.md).

## Слитый PR #46, PR-07n

[Metadata setup/shared namespace](STM32_METADATA_SETUP.md) закрывает bounded
volatile paired role allocation и phase ownership, original image deadline
composition, separate MS codec + explicit raw fragments. Actual metadata
TX/RX дают concrete close; delivered snapshots проходят прежний independent
verifier/lease/model/clean close. Host54/sanitizers54/standalone28; full text
[evidence](evidence/build/metadata-setup/README.md). Authentication/persistent
identity/routing/storage/worker/runtime/install/hardware остаются open.

## Слитый PR #47, PR-07o

[Shared channel](STM32_UPDATE_CHANNEL.md): один mixed TU/MS fragment/queue path
через все update phases, local quiescence/role IDs, exact owned outbox commit и
bounded last frame. Real upper states и existing independent verifier/lease/
model/close остаются отдельными обязательствами. Host56/sanitizers56/standalone30,
[evidence](evidence/build/update-channel/README.md); external queue drain/identity/
authentication/routing/runtime/resources/hardware/install open.

## Слитый PR #48, PR-07p

[Bound egress](STM32_UPDATE_EGRESS.md): actual channel TX/frame ledger, one driver
claim/tickets, bounded residency/backpressure, held claim on timeout/cancel/
uncertainty, late return reclaim only. Host59/sanitizers59/standalone33;
[evidence](evidence/build/update-egress/README.md). No real driver/runtime/remote
ACK/auth/upper/physical/install authority. Original image/crypto/lease deadlines
preserved; real completion/reset/event queue/provider/resources gates open.

## Слитый PR #49, PR-07q

[Driver event inbox](STM32_DRIVER_EVENTS.md): actual owned egress + completion
FIFO, full/retry/no drop, one-event current-time service, no pending-event flush.
Host61/sanitizers61/standalone35; [evidence](evidence/build/driver-events/README.md).
Serialized API only, no actual synchronized RTOS/ISR/driver/provenance/reset proof.
Original upper windows/crypto/lease/model/close and all hardware gates retained.

## Слитый PR #50, PR-07r

[SPSC mailbox](STM32_COMPLETION_MAILBOX.md): one producer/consumer owned reports,
release/acquire/actual bound consumer/retained custody; host65/sanitizers65/
standalone39/TSan2. [Evidence](evidence/build/completion-mailbox/README.md).
C11 concurrency checked on host/compile; target ISR/RTOS/cache/DMA/completion
truth/reset/reconciliation, persistent identity/routing/resources/physical/install open.

## Слитый PR #51, PR-07s

[Owned driver dispatch](STM32_DRIVER_DISPATCH.md): actual frame claim/publication/
unchecked token/fresh check/exact return/slot cleanup; host68/sanitizers68/
standalone42/TSan4. [Evidence](evidence/build/driver-dispatch/README.md).
Published untaken jobs still held until explicit driver return; no hardware cancel
or physical readiness claim. Target ISR/RTOS/cache/DMA/clock/real backend, identity/
resources/runtime/staging/install and actual hardware gates remain open.

## Слитый PR #52, PR-07t

[Owned frame ingress](STM32_FRAME_INGRESS.md): SPSC owned captures, retained
producer pending/full/retry, unchecked staged token/fresh original-age check,
sticky fault без live reset. Host71/sanitizers71/standalone45/TSan6; ARM1060text/
data0/bss0/frame48,8exports/15DMB, прежние budgets/SDK/workspace сохранены.
[Evidence](evidence/build/frame-ingress/README.md). Это software boundary,
не реальный RX driver или live producer-stop/phase-cutover/backend drain proof.

## Слитый PR #53, PR-07u

[Acknowledged ingress owner](STM32_INGRESS_OWNER.md): actual RX/ingress, exact
pause ACK + actual pending/staged/ring/RX drain before phase change; epoch permits
before capture, retained counters/time and bounded lower-timeout recovery. Старые
capture/ACK не влияют на новый цикл; upper image expiry не оживает. Software actor
barrier не physical backend/IRQ/DMA/remote stop. [Evidence](evidence/build/ingress-owner/README.md).

## Слитый PR #54, PR-07v

[Threaded signed HOST link](STM32_THREADED_LINK.md): protocol/crypto caller и
отдельный bounded driver pthread, concurrent RX, actual driver pause ACK и
actual drain; full signed chains/loss/recovery/independent refusals. External
BUSY input сохраняет original permit/time и блокирует ACK, stop/join не теряет
actual claim. Host75/sanitizers75/standalone49/TSan8+1 PASS, прежние ARM budgets/
SDK pins/upper windows сохранены. [Evidence](evidence/build/threaded-link/README.md).
