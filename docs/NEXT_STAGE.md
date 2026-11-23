# P1 — signed OTA CHECK после успешного HTTPS

## Активный результат: локальный подписанный CHECK payload

[Интеграция](plans/P1_SIGNED_CHECK_BUNDLE.md) добавляет штатную упаковку
настоящего app-only BIN с manifest/signature/checksums, без публикации/установки.
Ключ создан по поручению владельца и хранится вне Git. Согласованы только
стендовые HMI0.1.0-bench.1→0.1.0-bench.2/prerelease, bootloader0.1.0-bench.1
как привязка проверенного fecc05d и protocol1.0.0 для OTA metadata/image.
Настройки времени/ресурсов и exact publication/аппаратный signed CHECK ещё
открыты. [Команды](OTA_SIGNED_CHECK_BUNDLE.md). Не включать private key в CI.

## 23 ноября: LVGL PSRAM и HTTPS — PASS, далее signed CHECK

PR #75 слит владельцем, main1682da0 CI15/15 PASS. [Стенд38f70b9](evidence/hardware/2026-11-23-esp32-lvgl-38f70b9/README.md)
завершён за1063,692s: fresh16MiB/device MD5/SHA/two-NVMe, baseline46cf400 →
новый control → три TLS reset boots → весь factory2MiB fecc05d/verify/UART30 PASS.
Четыре writes/шесть captures; full restore не понадобился. На плате fecc05d,
F407 ee7d179 не открывался. Владелец заранее разрешил необходимые P1 стенды
23ноября2026 Asia/Almaty без повторных вопросов; этот exact пакет уже закрыт.

Свободная internal RAM control16771–16831 →124703–124755bytes, largest7680→63488.
Три verified HEAD HTTP200, cleanup1, полные DMA traces без allocation failures;
TLS local minimum108563–108867, remaining stack3760–3904/8192, touch2–19ms/errors0.
Возврат fecc05d touch1–11ms PASS. Это UART evidence, не physical touch/FPS:
render/event samples0, визуального owner подтверждения сегодня нет.

[Software evidence](evidence/build/esp32-lvgl-psram/README.md):16 clean ELF/BIN
двух SDK,183 host/183 sanitizer tests, actual allocator analyzer/ELF/config PASS.
Global malloc policy/stack/DMA64KiB/TLS16384/4096/RGB/pins/clocks/touch/CA/time
не ослаблены. [Следующий этап](plans/P1_SIGNED_SESSION_RESOURCE.md) — настоящая
signed CHECK/session4 с полным manifest/image stream в существующем worker8192,
затем install/confirm/rollback и manual release acceptance. Локальный ключ уже создан; стендовые SemVer/метки согласованы.
Reviewed time/resources и опубликованный подписанный candidate пока отсутствуют;
SETUP/CHECK/INSTALL gates сохраняются, test keys/floors из HEAD не назначать.

## История до стенда38f70b9

## 23 ноября: следующий PR — LVGL heap в PSRAM

[Стенд46cf400](evidence/hardware/2026-11-23-esp32-dma-46cf400/README.md)
выполнен: control PASS1–20ms, SNTP PASS, TLS handshake FAIL,9 allocation
failures из DMA|INTERNAL|8BIT (4 retained/5 lost). Остальные boots отменены.
Factory fecc05d возвращён; из-за UINT32_MAX touch age в старой прошивке
выполнен один full fresh16MiB restore/verify/reset. Дополнительный UART30s
PASS3–19ms; физические касания/экран владельцем сегодня не подтверждались.

Dashboard использовал100516bytes internal, PSRAM не менялась. Следующий
отдельный PR — штатный CUSTOM allocator LVGL на PSRAM только rev12-ota,
оба SDK/ELF/fault tests и новый ограниченный control/comparison/TLS стенд.
Global malloc policy/RGB/DMA reserve/stack/TLS/time/touch gates сохраняются.
Сегодня действует уже данное разрешение; пакет46cf400 закрыт, повторять нельзя.

Ниже — основание и история, не активный запрос ещё одной trace-абстракции.

## История P1 после main502a73d

PR #71–73 слиты владельцем, main CI15/15 PASS. [Стенд3927063](evidence/hardware/2026-11-22-esp32-worker-tls-3927063/README.md)
реально выполнен по отдельному разрешению: control touch_fresh PASS, SNTP PASS,
первый HTTPS HEAD FAIL на hardware-AES allocation. Остальные TLS boots отменены.
Весь factory fecc05d возвращён, verify/UART/owner UI-touch-Wi-Fi-SETUP PASS;
сеанс743,183s закрыт. На F407 ee7d179 изменений нет.

Следующая [одна интеграция](plans/P1_DMA_ALLOCATION_TRACE.md): прошиваемый
diagnostic image обоих SDK с точными allocation bytes/caps/phase и DMA heaps →
ограниченный exact-image стенд → обоснованная экономия/размещение памяти.
Владелец заранее разрешил необходимые P1 подключения и стенды на23ноября2026
(Asia/Almaty), без повторных запросов. Использовать новый одноразовый пакет,
fresh16MiB/two-NVMe и автоматический возврат fecc05d; старый3927063 закрыт.
[Software evidence](evidence/build/esp32-ota-dma-trace/README.md): код46cf400,
16 clean ELF/BIN,14 regression matches, host181/sanitizers181/analyzer9 PASS.
Не отключать hardware AES,
CA/hostname/date, не снижать TLS16384/4096/DMA64KiB/display/clock/touch требования.
После HTTPS — owner settings и реальная signed SESSION4/resource CHECK,
затем install/self-test/confirm/rollback и ручная release acceptance. Пока
ключ/context/floors отсутствуют, CHECK/INSTALL закрыты. Не угадывать настройки.

[NEMA23/STEP-DIR design](NEMA23_BENCH_PLAN.md) выполнен в PR #72: host160/
sanitizer160/20000 cases/ARM archive и запрет physical outputs. Следующий
аппаратный этап motor/driver ждёт exact моделей и отдельного разрешения;
питание/ток/ENA/pins не назначены, independent E-stop обязателен.

## История перед PR #70

## 22 ноября: контроль a5f8b7e остановлен, fecc05d возвращён

[Стенд](evidence/hardware/2026-11-22-esp32-worker-tls-control/README.md) завершён
за608.667s: fresh16MiB/device MD5/SHA-256/two-NVMe PASS, control8192 app-only
write/verify/UART90.101s. Одно touch_age_ms=4294967295 на uptime26335ms из16
samples при touch_errors0; остальные0–19ms. touch_fresh FAIL сохранён,
TLS image не записывался, SNTP/HEAD не запускались. Весь factory2MiB fecc05d
возвращён из свежего снимка/verify/UART30.046s/owner HMI-touch-Wi-Fi-SETUP PASS.
Две writes/два captures, полный16MiB restore не понадобился. На ESP32 fecc05d,
F407 ee7d179 не открывался; разрешение использовано, новых аппаратных действий нет.

В actual app_main найдены два clock-before-snapshot paths. Host injections
воспроизводят underflow/false-health, исправление копирует snapshot прежде clock.
Пороги100/500ms, polling20ms и строгий hardware parser сохраняются. Исправление
ещё не испытывалось на плате; TLS/crypto/install/floors/rollback остаются открыты.

Следующий шаг: review исправления с actual app_main regression, обоими pinned
SDK и CI; затем новый exact-image пакет/отдельное разрешение. Повторить control
и только после его полного PASS переходить к трём TLS boots. Не обходить
исходную touch_fresh проверку и не повторять использованный пакет a5f8b7e.
[План](plans/P1_TOUCH_SNAPSHOT_CLOCK.md). Ниже сохранена история подготовки.

Актуальная база main `53b0715`, аппаратный отчёт PR #68 слит владельцем.
Следующий [программный image](ESP32_OTA_WORKER_TLS_BENCH.md) включает default-off
пробу existing SNTP/verified HTTPS HEAD в том же worker8192, без owner key/context
и установки. [План](plans/P1_OTA_WORKER_TLS_BENCH.md): реальные ELF/BIN двух SDK,
actual-source fault tests, sanitizer/analyzer/ELF/monitor-owner gates и CI.
Он не записан на плату. Следующий аппаратный шаг — отдельно разрешённый замер
этой конкретной пробы с возвратом fecc05d; configured crypto/install/rollback
ещё требуют reviewed local settings и отдельных измерений. Новых разрешений нет.

[Software evidence](evidence/build/esp32-ota-worker-tls-bench/README.md):
код d27142e, 16/16 SDK builds, host156/sanitizers156/analyzer7 PASS.

## Основание: завершённый аппаратный стенд worker8192

База main `dfa9f28`, PR #1–67 слиты. [Стенд3072/8192](evidence/hardware/2026-11-22-esp32-worker-stack/README.md)
завершён22ноября за928.998s по отдельному разрешению: fresh16MiB/device MD5/
SHA-256/two-NVMe, контроль3072 и три reset-запуска8192, три app/factory writes,
пять UART captures и возврат всего factory2MiB fecc05d/verify PASS.
Владелец подтвердил HMI/касания/Wi-Fi/OTA SETUP временного8192 и возвращённого
fecc05d. На ESP32 fecc05d/factory/tv4-16m-ab-v1; F407 ee7d179 не открывался.
Полный16MiB restore не понадобился; сеанс и разрешение закрыты.

Worker8192 watermark6608–6784bytes, BOOT local15767–15863; periodic internal
free14291–16863/largest7680 против control21795–21799/largest12288 после IP event.
Первый8192 включал UI-проверку: нагрузка различалась. Это unprovisioned reset
boot evidence, не configured TLS/install/production floor или cold-power/soak.
Ключ/context не заданы, CHECK/INSTALL закрыты. FPS/latency/slot confirm/rollback/
power-loss/full recovery не приняты. Новых hardware разрешений нет.

## Подтверждено и ещё не подтверждено

Реальные ELF/BIN/source hashes связаны с контролем3072 и тремя boots8192.
У каждого trace запуска7 startup points/BOOT window/16 periodic samples, у
возврата4 samples. На физической плате прошли PSRAM8MiB/XiP/RGB/GT911/self-test,
UI и Wi-Fi; owner UI подтверждение временного варианта одно, не три.
[Software режим PR #67](ESP32_OTA_WORKER_STACK_BENCH.md) не меняет defaults.

Largest7680 и диапазон free14291–16863 ограничены. UI-нагрузка в первой пробе
отличалась, минимум history не simultaneous free/OOM. Три reset boots не
доказывают full configured call stack, холодный старт, soak или performance.
Полный16MiB restore и Flash writes installer при активном RGB не испытывались.

## Следующий функциональный шаг P1

1. Получить реальные TLS/crypto/installer resource measurements в существующем
   worker8192 с прежними HTTPS/crypto и единственным monitor owner. Использовать
   конкретный target image и отдельный согласованный probe/installation scope;
   не выводить floors арифметикой из этого ненастроенного boot-стенда.
2. Local owner key/SemVer/trust/time/resource context пока не заданы. Сохранять
   SETUP/CHECK/INSTALL gates; не подставлять test keys и не угадывать policy.
   После reviewed настроек и замеров — signed inactive-slot install, reboot/
   self-test/confirmation/rollback и Wi-Fi/power-loss сценарии.
3. Не снижать DMA64KiB/TLS16384/4096/framebuffer/clocks ради PASS. Полные RAM/
   PSRAM/stack/CPU/FPS/latency/tearing/soak и recovery остаются отдельной приёмкой.
4. Оставлять текущий fecc05d/A-B; ordinary rev12 flash не является откатом.
   F407/physical CAN/motion/P2–P5 не объявлять принятыми. Новый hardware доступ
   требует нового разрешения; завершённый пакет не запускать повторно.

[Аппаратные числа](evidence/hardware/2026-11-22-esp32-worker-stack/measurements.json),
[предыдущий startup стенд](evidence/hardware/2026-11-22-esp32-startup-order/README.md),
[backup status](BACKUP_STATUS.md). История ниже не активная очередь абстракций.

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
