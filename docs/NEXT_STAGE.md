# Следующий этап: release context и STM32 board contract

21 ноября2026. База main `2c6fd1c`, PR #1–49 слиты, main CI5/5 PASS.
ESP32 HMI/Wi-Fi/OTA primitives/candidates и STM32 transport/admission существуют;
не создавать их заново. Данный план не разрешает доступ к устройствам или merge.

## Подтверждённые характеристики

[Owner report](evidence/hardware/stm32f407-owner-swd/README.md): ST-Link V2/SWD,
Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB, напряжение3,252V,
backup1048576bytes, structural vector check PASS. Агент их не перечитывал.
Это не physical package/clock/RAM/sector/GPIO/CAN/restore validation.

## Слитая программная база

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

## Текущая ветка PR-07r

[SPSC mailbox](STM32_COMPLETION_MAILBOX.md): one producer/consumer owned reports,
release/acquire/actual bound consumer/retained custody; host65/sanitizers65/
standalone39/TSan2. [Evidence](evidence/build/completion-mailbox/README.md).
C11 concurrency checked on host/compile; target ISR/RTOS/cache/DMA/completion
truth/reset/reconciliation, persistent identity/routing/resources/physical/install open.

## Что делать дальше программно

1. HMI policy проверена PR-05b; PR-07g проверяет independent release/component/
   target/profile/image draft; PR-05c добавляет offline signed cohort link,
   PR-05d проверяет его связь с explicit-pinned unsigned build claims/bytes и
   показывает blockers. Дальше review production bundle↔component contract и
   schema, доверенного local context и production immutable backend. PR-07h
   закрывает software live coordinator adapter; PR-07i добавляет RAM backend.
   Production staging medium/RAM budget/durable provider и runtime ещё требуют
   review; full image нельзя считать помещающимся в F407 RAM по Flash capacity.
   Не принимать standalone historical candidate вместо lease/vector/deadline
   checks, не менять RX lifetime. Draft counters не переводить в SemVer;
   actual local versions/key provisioning/signing authority — owner gate.
   Candidate digest брать из independently reviewed run, не из самого unknown
   artifact; self-generated CI integrity pin не доказывает происхождение. Для
   production нужны approved source/build/signer custody и actual target layout,
   не подавление factory/A-B blocker или замена current PROJECT_VER.
2. PR-07f проверяет volatile logical negotiation, но production boot epoch
   provider/persistence/collision/authentication, capabilities/version и raw CAN
   fragmentation/IDs/bitrate требуют review. PR-07j закрывает только experimental
   complete-message codec, не production wire adoption. PR-07k добавляет bounded
   fragment/reassembly с local route/generation/cancel/deadline/backpressure.
   PR-07l проверяет whole-message retry/queue/deadline composition на одной
   synthetic шкале. PR-07m добавляет bounded raw metadata/signature container и
   receiver с distinct DATA ID/single owned snapshot/original request binding.
   PR-07n добавляет bounded metadata setup, shared volatile role IDs и phase
   ownership на original image windows. PR-07o добавляет bounded shared channel
   и local phase/outbox filters, не actual runtime owner/remote queue flush.
   PR-07p добавляет actual bound local custody queue/driver tickets, не real
   driver/remote drain proof. PR-07q добавляет serialized owned event inbox.
   PR-07r добавляет SPSC C11 mailbox/bound consumer. Дальше review actual target
   ISR/RTOS/cache/DMA port, trusted adapter completion provenance/reset/
   reconciliation, production identity/capabilities/codebook и
   worker/resource budgets перед runtime adoption. Production routing/reboot
   identity/authentication, durable ownership и measured worker/bus/crypto budgets
   остаются blockers, не implicit defaults или permissive setup stub.
   Decode OK не обновляет context или timeouts; owner poll остаётся обязательным.
   Logical terminal outcome уже есть; он не authenticated и
   не переносит candidate через ожидание оператора. Для следующей операции
   проверять saved bytes полностью и подтверждать новый session/context.
   Текущий DATA RX deadline не продлевать молча.
3. Разделить software candidate и будущую установку: fresh session-bound
   отсутствие движения/шпинделя/программы, operator exact-image approval,
   reviewed recovery/vector/erase gate. Не вводить permissive boolean stub,
   который открывает Flash writer или изображает physical inhibit.
4. Для активной проверки обновлений из нового UI нужны approved local key/
   version/layout/time context, runtime resource/load/fault/soak budget и
   отдельный bounded worker/snapshot contract. Не превращать local build
   descriptor в SemVer/release trust и не показывать фиктивную историю/progress.

## Что блокирует настоящий board image

1. Проверить физическую маркировку/package/board revision, RAM/CCM bounds и
   полный device IRQ/vector contract по согласованным reference sources.
   Device ID/suffix и1MiB capacity не заменяют эту проверку.
   Новый ответ владельца20ноября повторяет только VGT6; HSE/полной маркировки нет.
2. Утвердить clock/reset policy. Возможен отдельный минимальный HSI-only путь;
   неизвестный HSE блокирует HSE/PLL, фактические частоты/пригодность CAN ещё
   нужно измерить. Текущий код clock не устанавливает.
3. После memory/vector/stack review подготовить standalone no-I/O board profile
   без пинов приводов/bootloader reserve. Synthetic fixture addresses/stack/
   core vectors никогда не использовать как реальную карту F407.
4. До любой записи отдельно уточнить backup SHA/независимую копию/restore,
   затем согласовать exact image и SWD/write/reset/capture. Нынешнее задание
   этих операций не разрешает. GPIO/CAN/уровни ENABLE — отдельное commissioning.

## Flash и будущий OTA

[Архитектура](OTA_ARCHITECTURE.md): signed GitHub component → ESP32 bounded
cache/transport → независимый F407 verifier/recovery. До reviewed erase sectors,
protection/RAM/vector/commit/power-loss решения нельзя выбирать A/B/staging,
app offset или metadata address. **Flash1024KiB не резервирует ни одного адреса.**

Сохранённый valid flag/receipt не заменяет signature/hash/vector check перед
commit/jump. Reboot/reconnect/update не разрешают движение. Fresh physical
quiescent/operator/recovery checks обязательны, SIMULATION/no-I/O/ACK их не
подменяют. Аппаратная NC E-stop/STO цепь независима от CAN и обоих MCU.
