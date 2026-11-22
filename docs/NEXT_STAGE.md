# P1 — после фазного стенда ESP32

Текущая software база main973a9ba, PR #1–63 слиты; аппаратный образ стенда — fc90200.
[Аппаратный стенд fc90200](evidence/hardware/2026-11-22-esp32-ota-memory/README.md) завершён по отдельному разрешению:
fresh16MiB/two-NVMe, diagnostic app-only write,7startup/BOOT window/16samples,
factory2MiB fecc05d return/verify/4samples и owner HMI/touch/saved Wi-Fi/SETUP PASS.
На ESP32 fecc05d/factory/tv4-16m-ab-v1; F407 ee7d179 не открывался.
Сеанс608.700s закрыт, повторных UART/SWD/reset/Flash разрешений нет.

## Полученный результат

Исторический112 не был уже готовым APP_ENTRY минимумом в diagnostic image:
на входе202308, local BOOT minimum1887 к UI_READY, после stop history112.
SDK по каждому heap берёт min(saved,local); сумма не simultaneous peak/OOM.
Конкретная transient allocation ещё не локализована. OTA-worker watermark1488
из3072bytes; steady internal22279..22343/largest7680, PSRAM6090124.
Числа относятся к instrumented unprovisioned build, не configured installer.

Fresh fecc05d snapshot: d84fe13900806d9646e1e2edb8df0c4fe897aa6de85f14c28bfd0624f101ff46,
16MiB/two independent NVMe. [Статус копий](BACKUP_STATUS.md). Factory-partition
restore из него проверен; условный full16MiB restore не выполнялся.
Legacy8f0d899 имеет прежнюю карту, не является текущим running image.

## Текущая программная интеграция от main973a9ba

PR #63 слит владельцем, аппаратный отчёт сохранён. [План следующего PR](plans/P1_STARTUP_MEMORY_ORDER.md):
первый полный dashboard refresh/UI_READY до существующих export/Wi-Fi services.
OTA worker/rollback deadline остаются до board init; touch-release gate,
заставка, DMA64KiB/TLS16384/4096/framebuffer и частоты прежние. При неудачном
первом flush services не запускаются. Семь startup ID стабильны; хронология
теперь0,1,2,3,5,6,4, worker выводит ID-order, не time-order.

Actual app_main/real LVGL host integration проверяет cold frame и отказы;
SDK/board doubles не доказывают ESP32 heap/performance. В исходниках устранено
перекрытие service startup с cold GUI render. Это не доказательство причины
аппаратного minimum1887 или улучшения на плате. На устройстве по-прежнему fecc05d;
новых UART/SWD/reset/Flash операций нет.

## Следующие аппаратные и ресурсные gates P1

1. После review/CI отдельно согласовать exact-image измерение нового startup
   order: сравнить internal free/largest/local minima и стеки с fc90200,
   проверить UI/touch/saved Wi-Fi/SETUP. Не повторять USB migration и не
   прошивать ordinary rev12 поверх действующей A/B-карты.
2. Проверить будущий worker8192 и настоящую TLS/install нагрузку: текущий
   аппаратный замер относится к3072 без ключа. Floors не утверждены;
   арифметическое вычитание5120 из суммы минимумов1887 не доказывает OOM.
3. Owner key, SemVer, trust/time/resource context пока отсутствуют по решению
   владельца. CHECK/INSTALL закрыты, test keys только host. После решений и
   ресурcной приёмки — signed install в inactive slot, reboot/self-test/
   confirmation/rollback и Wi-Fi/power-loss сценарии по отдельному разрешению.
4. Factory self-test marker PASS не означает испытание PENDING_VERIFY.
   Полный16MiB recovery, CPU/FPS/latency/tearing/soak и P2–P5 остаются открытыми.
   Никаких eFuse/option bytes/OTP/CAN/motion или самостоятельных merge/release.

История [миграции fecc05d](evidence/hardware/2026-11-22-esp32-ota-migration/README.md)
и [software builds](evidence/build/esp32-ota-memory-phases/README.md) сохранена.

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
