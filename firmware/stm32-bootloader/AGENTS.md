# Независимая подготовка STM32 bootloader

Наследовать корневые правила и аппаратные blockers. Читать
docs/STM32_IMAGE_ADMISSION.md и docs/OTA_ARCHITECTURE.md.

- PR-07b — только read-only admission кандидата на ПК/ARM compile. Нет
  board image, MEMORY/Flash addresses, writer, commit, jump, CAN или GPIO.
- Криптография — отдельная Mbed TLS 3.6.7 с проверенным archive SHA-256.
  Не вызывать ESP32 OTA verifier и не использовать вывод HMI/CRC как доверие.
- Experiment metadata schema не является production release/CAN contract.
  Никаких actual keys/context/layout/version defaults. Fixture IDs — только tests.
- Все результаты fail-closed; VERIFIED_CANDIDATE не install permission и не
  physical quiescent/recovery proof. После reset/изменения storage проверять снова.
- Private test keys генерировать в памяти host tests, не сохранять/публиковать.
- Стандартный buffer allocator имеет единственного owner. Нет ISR/reentrancy,
  threads, libc heap fallback или hardware RNG. Arena — software test budget,
  не подтверждённый резерв RAM платы. CPU/stack/latency ещё измерять на стенде.
- Любые реальные device/Flash/reset/serial/option bytes и merge — отдельно
  разрешённые операции; данный этап их не выполняет.

## PR-07c — local transport/source handoff

Читать docs/STM32_UPDATE_HANDOFF.md. Только read lease, borrow live receiver,
boot/transfer/policy/source generations и существующая независимая crypto.
Immutable/exclusive snapshot гарантирует reviewed backend, не generation число.
Не терять handle при release failure; bounded retries/quarantine без force reset.
Receipt временный, не install/ARM/START token; copied receipt нельзя считать
действительным без проверки живого coordinator. RX deadline не продлевать.
Нет storage writer/Flash layout или physical quiescent proof. Memory mock не
закрывает persistent recovery/physical inhibit/операторское подтверждение.

## PR-07d — explicit memory/vector model

Читать docs/STM32_IMAGE_PREFLIGHT.md. PROFILE_MODEL_MATCH не bootability и не
board/Flash/install permission. Только read-only prefix inspection после crypto
из того же lease, signed memory_profile/local policy generation binding.
Нет actual F407 profile/default addresses/vector count/alignment. Generic ARM
minimum не подставлять вместо device-specific VTOR review. Test maps не
переносить в firmware/Flash layout. Errors отзывают candidate, cleanup соблюдает
прежний held-resource/retry/quarantine contract. Hardware gates не закрывать.

## PR-07e — completion history

Читать docs/STM32_UPDATE_OUTCOME.md. Completion выполняет настоящие crypto/model
checks и закрывает lease, не принимает old receipt/success boolean. Отчёт —
история; pending release не positive, quarantine держит ownership и запрещает
clear/restart. Borrowed context живёт до конца wrapper start/poll (включая post-close
guard). Peer outcome/ACK не аутентифицированы и не install/quiescent/ARM authority.
Никаких runtime/Flash/wire/real MCU profile actions; будущий install проверяет
saved bytes и свежие physical/operator/recovery условия заново.

## PR-07f — negotiated transport fixture

Читать docs/STM32_UPDATE_NEGOTIATION.md. Production admission/handoff/completion
не меняются; host tests начинают свежий DATA из one-time negotiated params,
затем выполняют настоящие independent P-256/SHA/model/cleanup/outcome проверки.
Opaque request/tag/counter не подменяет их. Нельзя зашивать fixture epochs или
принимать HMI handshake success как signature/install/physical permission.

## PR-07g — independent signed release document

Читать docs/STM32_RELEASE_BINDING.md. Только opt-in release-draft-1 API через
тот же single-owner bounded Mbed TLS engine. Legacy draft strict11 fields/API
сохраняются; новый15-field draft не утверждает production HMI/CAN/release ABI.
Полные raw signature/SHA/EOF обязательны; selected release/source/channel и
installed/minimum component SemVer — local inputs, numeric counters отдельно.
Нет defaults/provisioning/перевода git display версии в trusted context.
Source commit signer claim не является build provenance proof. Новый standalone
candidate не live receipt/install token; PR-07h ниже добавляет explicit coordinator.
Не менять workspace/dependency/heap/crypto owner или старые lifetime/vector/
deadline gates. Нет actual board profile/map/writer/keys/runtime/device actions.

## PR-07h — release-aware source/completion adapter

Читать docs/STM32_RELEASE_HANDOFF.md. Explicit local API/mode, новый caller-owned
adapter поверх того же core. Legacy positive APIs/clear не обходят release mode;
legacy poll/abort/close только conservative guards/cleanup. Shared helpers private,
нет caller verifier callback/success boolean и новых crypto/arena/heap instances.
Полные release signature/selection/SemVer/image checks + live source/RX/owner/
deadline/vector gates, raw metadata digest из negotiation, clean close до outcome.
Adapter/core/RX/context один owner/disjoint/alive до clear. Пока completion active,
не выполнять независимые adapter/core operations, отменять completion_abort.
Не терять held handle при retries/quarantine, не продлевать DATA lifetime.
Production bundle/local context/backend/runtime/hardware/installer gates открыты.

## PR-07i — volatile staging source

Читать docs/STM32_STAGING_SOURCE.md. Explicit caller buffer/capacity/boot/object,
zero-init one owner, no ISR/copy/move/reinit. Только existing pending RX API и
copy-before-ACK; не менять transport/crypto/coordinator/deadlines. Init не reset,
discard не сбрасывает counters/high-water и не abort/reinit RX. Exhaustion reject.
While LEASED: read/inspect/release с текущим token, нельзя writes/discard/begin.
Stale token не unlock новую lease; quarantine сохраняет actual source ownership.
Direct C/DMA/thread aliases запрещает caller; это не MPU/physical protection.
SEALED не crypto/install/live RX authority. Discard O(written), только worker.
State160 плюс весь image buffer не подтверждают F407 RAM capacity; production
medium/durable recovery/identity/runtime и hardware gates остаются открытыми.
Нет нового device/SWD/UART/Flash/reset/backup read или самостоятельного merge.

## PR-05c — host artifact review frontend

Читать docs/RELEASE_COHORT_REVIEW.md. tools/motion_review.c только в host CMake
ветке; не включать stdio/filesystem/clock frontend в ARM targets/firmware/artifacts.
Вызывать существующий tv4_boot_admit_release с explicit local policy/key/selection,
no defaults/signing/Flash/live lease/model claims. Bounded regular files, monotonic
host clock, cleanup и historical JSON only; no new crypto/arena/embedded parser.
Trusted host executable/quiescent FS — precondition, driver SHA не attestation.
Report не production bundle/installer/actual hardware/context approval.

## PR-07j — host byte path integration

Читать docs/STM32_UPDATE_WIRE.md. Только host boot_handoff_tests --wire-staging
вызывает codec вокруг existing negotiation/DATA/outcome APIs. Остальные test
режимы сохраняют typed path, tv4_boot_handoff не линкуется с wire library.
Real independent signature/full image/lease/model/close/quarantine проверки не
заменять codec/CRC/peer success. Новых crypto/workspace/firmware callers нет;
shape/serialization не actual memory/physical/production CAN/install authority.

## PR-07k — host fragment integration

Читать docs/STM32_UPDATE_FRAGMENTS.md. --fragment-staging использует отдельную
bounded library только в host tests; old typed/byte modes прежние, production
handoff target не линкуется с fragments/codec. Raw metadata/signature по-прежнему
приходят напрямую из fixture, их wire transport не реализован. Fragment test
clock не physical timing или расширение logical deadlines. Real independent
crypto/model/lease/cleanup/revocation обязательны; local counter/route/generation
не authentication/install/physical proof. No device/Flash/startup map/runtime.

## PR-07l — one-clock signed fixture

Читать docs/STM32_UPDATE_TIMING.md. --fragment-timing только HOST, использует
shared f.now для queue/fragments/negotiation/DATA/outcome/verifier callbacks.
Legacy modes сохранять. Real independent crypto/model/lease/clean-close и
expired receiver отказ обязательны; test keys только RAM. Raw metadata/signature
приходят из fixture, не по каналу; CPU crypto duration не measured/тарифицирована.
Не подменять worker/identity/physical/installer gates симуляцией. Core/runtime/
board/pins/config/dependency changes не входят в этот этап; ARM исключает helper.

## PR-07m — delivered metadata integration

Читать docs/STM32_METADATA_TRANSPORT.md. Только --metadata-staging передаёт raw
metadata/signature/key ID по shared-clock byte/fragment DATA link перед image.
Producer arrays wiped; verifier читает delivered owned snapshot. Legacy modes
сохранять. Independent P-256/raw request digest/full image/live RX/lease/model/
clean close не подменять transport success. Metadata ID distinct; descriptor setup
в fixture local input, не implemented authenticated handshake. Image RX starts
до metadata и не reset; time charges его original windows. Snapshot alive/exclusive
через config/handoff/completion lifetime. No new crypto owner/arena/runtime/Flash.
Static assertions согласуют1024/72/31 bounds; DER length variable, per-run counts.

## PR-07n — bounded metadata setup

Читать docs/STM32_METADATA_SETUP.md. Paired metadata image+1 ID из original
CLOSED image pool, single allocation/no wrap/no rollback; retain CLOSED fields
до clear. Setup owns pool до actual completed metadata TX / TAKEN RX close.
Borrow empty original image endpoints; poll their unchanged time/descriptor/
limits/context, не reinit или продлевать окна. MS172 separate codec; explicit
raw fragment mode не отключает legacy typed TU decoder и не даёт dispatch/auth.
Host test helper applies MS decode before upper receive. One take UNVERIFIED,
real independent signature/raw digest/image/lease/model/close остаются нужны.
Responder bounded CONFIRM retry после take; duplicates не reset clocks/sends.
No new crypto owner/heap/runtime/IDs/bitrate/Flash/pins/devices. Host-only fixtures
не ARM. Scope/import/frame/global/negative guards, text evidence обязательны.
Production authenticated identity/routing/persistence/staging/resources/installer
и actual board hardware gates остаются открытыми. No self-merge.

## PR-07o — shared channel

Читать docs/STM32_UPDATE_CHANNEL.md. Opt-in local queue/phase filter, no runtime.
One owned frame outbox/retained counters; last queued frame deadline even lower
TX IDLE. RX single strict TU/MS codec/no fallback/owned UNVERIFIED take. Wrong
phase/role ID no dispatch. Advance only locally idle; not upper protocol success
or external driver queue drain/auth/physical/install proof. Original upper
coordinators/CRC/independent crypto/lease/model/deadline/clean close обязательны.
Host helper explicit phases after real states + fixture queue drain; no tests
in ARM. No counter/time/reinit reset for recovery. No new bytes/IDs/driver/heap/
crypto owner/pins/Flash/device actions. Existing budgets unchanged; separate ARM
imports/exports/frame/no globals guard + negatives, text evidence обязательны.

## PR-07p — local egress custody

Читать docs/STM32_UPDATE_EGRESS.md. Owned bound channel TX/actual queue; no
external success/drain boolean. Exact queue owner/route/generation/serial local driver return,
no remote ACK/auth/physical claim. Claim survives timeout/cancel/uncertain;
cancel drops unclaimed only, late return reclaim never upper resume. No force
forget/reinit/counter/time reset; context/clock/exhaustion sticky. Current owner
time for completion events, no ISR callbacks. Existing independent crypto/lease/
model/image deadline gates mandatory. Host-only simulated adapter, no real
driver/runtime/IDs/bitrate/Flash/pins/heap/crypto owner/device or self-merge.
Actual adapter truth/reset/event queues/persistent identity/resources/physical/
installer open. New separate exact ARM imports/exports/frame/no globals+negative
guard; old budgets/dependencies unchanged, text evidence required.

## PR-07q — driver event inbox

Читать docs/STM32_DRIVER_EVENTS.md. Own actual egress + event FIFO, no external
success/drained bool. Post copies only; full BUSY retains external report for
retry, no event/claim drops. Current-time service polls original windows then
consumes <=1 event; separate poll consumes none. Begin/claim/phase wait actual
inbox; cancel keeps inbox+claim, clear requires actual consumption/terminal return.
No timestamp/reinit/counter/time reset; context/clock/exhaustion sticky. Serialized
one owner only, NOT concurrent/ISR/RTOS/driver truth or physical/install authority.
Host-only delayed event adapter/signed chains; all old modes/upper crypto/lease/
model/deadlines/budgets/SDK pins remain. New mode fragment total256 before init,
old128/progress8/frame160 and all upper windows unchanged. Actual synchronization/
provenance/reset/persistence/routing/resources/runtime/hardware/install remain open.
Separate exact ARM guard/negative checks/text evidence mandatory. No devices/merge.

## PR-07r — SPSC completion mailbox

Читать docs/STM32_COMPLETION_MAILBOX.md. C11 one producer post / one consumer
other APIs; immutable producer binding, external init publication/stable lifetime/
stop+join. No live copy/move/reinit/reset/flush. Release/acquire protects owned
copies and reuse; always-native uint32 atomic compile gate + ARM no-helper/DMB
inspection. Actual target ISR/RTOS/cache/DMA/driver truth remain unverified.
Bound consumer honors actual mailbox/pending/inner custody, current-time service
<=1, retains pending until lower admission. Full retains external report/retry;
cancel/clear never forget ownership, IDs/phase/time/atomic positions retained.
Empty is observation, not producer-stop/physical drain. Original crypto/lease/
model/image/close gates mandatory; all old modes/budgets/SDK pins unchanged.
Host pthread/TSan tests never enter ARM/runtime. Separate imports/exports/frame/
no globals/native atomic guard+negative cases/text evidence mandatory. No actual
hardware/identity/persistence/routing/resource/install authority/devices/merge.

## PR-07s — owned driver dispatch

Читать docs/STM32_DRIVER_DISPATCH.md. One protocol actor owns actual completion/
frame ledger; one driver actor take/check/report/retry. Publish init before both,
stable lifetime/stop+join; no live copy/move/reinit/flush/inner mutable calls.
Release/acquire slot publication and return; driver never reads protocol mutable
state, owner never reads driver-private fields. Take returns zero-payload token;
read time AFTER take, check original admission/residency before frame exposure.
Software check/revocation not atomic hardware cancellation/physical inhibit.
Exact ticket, no implicit-current callback; full retains first report for retry,
no overwrite; uncertain keeps claim. Slot reuse needs actual consumer cleanup
AND driver RETURNED. Cancel preserves untaken/taken/pending/claim; explicit driver
terminal return mandatory, no force-forget. Clear retains IDs/phase/time/atomic
positions and cannot reset private driver clock fault. Independent crypto/lease/
model/image/close gates and all old modes/budgets/SDK pins preserved. Host pthread/
TSan only, no target backend/ISR/RTOS/cache/DMA/clock/truth approval. Separate exact
ARM/import/export/frame/global/native-atomic guard+negatives/text evidence required.
No devices/SWD/UART/reset/Flash/CAN/motion/self-merge/release.
