# P1 — после первой USB-миграции ESP32

База отчёта — main `c41a3d24bbca9f6c55015de9431dcef32651cd92`, PR #1–60 слиты.
На ESP32 после отдельно разрешённого стенда оставлен `fecc05d/rev12-ota`,
factory, SDK6.1.1 и карта tv4-16m-ab-v1. [Миграция, UART и подтверждение владельца](evidence/hardware/2026-11-22-esp32-ota-migration/README.md).
F407 ee7d179 не открывался; его [базовый стенд](evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md)
не доказывает физическую работу CAN/приводов. Оба разрешённых ESP32-сеанса
(backup и migration) завершены; новые операции ими не разрешены.

## Программный результат P1

[Интегрирован](ESP32_OTA_INSTALLATION.md) один explicit rev12-ota: existing signed
HTTPS session → неактивный SDK slot → full digest/EOF/SDK validation → boot select,
UI version/size/двухшаговое подтверждение, cancel/error cleanup, bounded boot
self-test/rollback/watchdog и prepare/independent verify USB migration bundle.
[План](plans/P1_ESP32_OTA.md), [проверки и пределы](evidence/build/esp32-ota-installation/README.md).
Обычные4профиля в исходниках сохраняют legacy factory2MiB; explicit rev12-ota
использует reviewed16MiB factory recovery + otadata +2slots. Именно эта карта
теперь действует на ESP32. Обычный rev12 flash поверх неё не является откатом
и без отдельной проверенной процедуры не выполняется.

Владелец выбрал **настройку без ключа**. Работоспособная unprovisioned сборка
не имеет test key/выдуманных production SemVer/floors; экран сообщает о настройке,
сетевые CHECK/INSTALL заблокированы. Это не завершённая аппаратная приёмка P1.

## Что уже проверено и что делать дальше

Следующая [программная интеграция фазных измерений](ESP32_OTA_MEMORY_PHASES.md)
готовит настоящий rev12-ota ELF/BIN: явный boot result, OTA-worker stack и
отдельный opt-in TV4_OTA_MEMORY_DIAGNOSTICS=1 вариант в другом build tree.
Normal default off не вызывает SDK minimum monitor. Наблюдённые432bytes state/
112bytes own compiler frame и host/sanitizer tests относятся к software build,
не к установленной firmware или runtime пикам. [Evidence](evidence/build/esp32-ota-memory-phases/README.md).
Новый стенд требует отдельного разрешения на exact image/backup/возврат; на
плате пока fecc05d. Действующую A/B карту не мигрировать повторно.

1. **Готово:** clean-main fecc05d, rev12-ota/SDK6.1.1 без ключа собран;
   prepare/independent verify bundle PASS. App SHA-256
   `de55953fc9e3c4080cb277d734ca7d1f02c74880db38dac0f4b53830f587d021`.
2. **Готово:** [fresh16MiB snapshot + independent NVMe copy](evidence/hardware/2026-11-22-esp32-pre-ota-snapshot/README.md),
   whole-device MD5/local SHA-256/legacy layout/app inspection PASS; после reset
   owner HMI/touch/Wi-Fi PASS. Snapshot SHA-256
   `47c4f9c4e6d7fb1fcfce2b0065c2f69af0ea01664d1407b3504ee6e4a6a13107`.
   Это checkpoint текущего8f0d899; factory backup сохранён отдельно.
3. **Готово:** USB migration четырёх областей/verify/readback, NVS/PHY до boot,
   factory boot/PSRAM8MiB/XiP/GT911/RGB/UI, owner HMI/touch/saved Wi-Fi/SETUP.
   Один capture89.114s, весь сеанс117.810s, recovery не понадобился. Restore
   snapshot и recovery после прерванной миграции этим не проверены.
4. **Следующий результат:** разобрать RAM/stack бюджет по фазам, затем проверить
   реальный configured installer. 16 срезов без нагрузки дали internal
   free22851..22907/largest7680/history112 и PSRAM free6090732bytes. History112
   — сумма SDK per-heap minima, не одновременный остаток/OOM proof и не floor;
   причина/пик открыты. OTA-worker watermark отсутствует; render/event samples0
   не измеряют FPS/latency. Не уменьшать DMA reserve/TLS records/framebuffer
   ради прохождения. Не назначать production пороги по этим или HEAD-срезам.
5. Нужны owner public key и утверждённые SemVer/time/resource context,
   provisioned build/release binding, отдельные exact-image разрешения на
   install/confirm/rollback/network/power-loss пробы. Владелец пока выбрал
   без ключа; тестовые ключи не переносить из host в прошивку. Начальная смена
   bootloader/table/factory app rollback не защищается, USB recovery остаётся
   отдельным сценарием. Новые eFuse/защитные настройки не требуются.
6. Пока P1 не стабилизирован, P2 публикацию, physical CAN P3 и STM32 CAN OTA P4
   не объявлять завершёнными. Адреса F407 loader/staging не резервировать.

Не подключаться к устройствам по завершённым разрешениям. eFuse/Secure Boot/
Flash Encryption/option bytes/OTP/CAN/motion и самостоятельные merge/release
запрещены. HSE/пины/полный RAM stress/частоты F407 остаются отдельными блокерами,
не препятствующими текущей P1 software integration.

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
