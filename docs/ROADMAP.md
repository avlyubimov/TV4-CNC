# План реализации TV4-CNC: функциональные приоритеты P0–P5

Software база main973a9ba / PR #1–63. [Фазный стенд ESP32](evidence/hardware/2026-11-22-esp32-ota-memory/README.md) выполнен:
fresh16MiB/two-NVMe, app-only diagnostic write/capture90.103s, возврат всего
factory2MiB fecc05d/verify/capture30.051s и owner HMI/touch/saved Wi-Fi/SETUP PASS.
На ESP32 fecc05d/tv4-16m-ab-v1; F407 ee7d179 без обращения.
Factory-partition restore проверен, full16MiB recovery не выполнялся.
Сеанс608.700s закрыт; новые device/reset/Flash требуют нового разрешения.

Получены7startup points/BOOT window/16samples: APP_ENTRY history202308,
local minimum1887 к UI_READY, history112 после SDK stop. Это per-heap minima,
не simultaneous free/OOM или production floor. Worker watermark1488из3072;
configured8192/TLS/install, slot confirm/rollback/power-loss и performance
ещё не измерены. Текущий software PR переносит существующие export/Wi-Fi
services после полного первого dashboard refresh/UI_READY, с actual app_main/
real LVGL integration и прежними буферами/часами. Это устраняет source-level
startup overlap, не доказывает экономию ESP32 RAM или причину minimum1887.
[План](plans/P1_STARTUP_MEMORY_ORDER.md); затем отдельно разрешённый замер и
configured budget после owner key/context решений.

[Проверенная software сборка и cold-render evidence](evidence/build/esp32-startup-memory-order/README.md).

## Исторический итог P0 и программной подготовки

22 ноября 2026, база main `07fb406e37ebd1f1e019dbee0b54e8bb5571be1a`, PR #1–57 слиты.
После отдельного разрешения один стенд ee7d179 завершён за16,196s, exit0:
program/verify/exact identity, RUNNING, data/BSS/один CCM word/guards, progress,
watchdog reset и UsageFault capture/reset **PASS**. На F407 оставлен работающим
clean `ee7d179435f3f9e7598c7646b85ff64f4d5e9a32`, BIN2540bytes. Условный restore
не понадобился. Предыдущий c09 FAIL8 и проверенный полный snapshot restore — история.

Плата изолирована, только питание/SWD; option bytes/OTP/CAN/motion/ESP32 не менялись.
Этот разрешённый сеанс завершён. Полный RAM/CCM stress, stack high-water,
точные HSI/LSI/HSE частоты, physical GPIO/CAN/inhibit и power-loss остаются открытыми.
P0 выполнен на базовом стенде; следующий приоритет — P1 ESP32 OTA с existing
HTTPS/crypto и review16MiB factory+двух OTA slots/USB migration. Новых hardware
операций или будущих CAN loader addresses это не разрешает.

[Успешный стенд ee7d179](evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md). [Следующий этап](NEXT_STAGE.md), [приоритеты](IMPLEMENTATION_PRIORITIES.md).
Базовый P0 hardware acceptance выполнен; PR #58 слит, база того этапа main d1317a8.
P1 интегрирован программно: [installer/rollback/migration](ESP32_OTA_INSTALLATION.md);
ключ/context не настроены по решению владельца; аппаратная migration/factory
проверка описана выше, configured OTA ещё не испытан.
Приводы, CAN и independent bootloader остаются отдельными этапами.

## Порядок и критерии результата

| Приоритет | Законченный результат | Зависимости и приёмка | Текущий статус |
|---|---|---|---|
| P0 — STM32F407 firmware | Настоящий ELF/BIN на HSI с RAM/SWD diagnostics, exception/watchdog handling, без активации motion outputs | Официальная карта VGT6/SRAM/CCM/Flash sectors/vectors; real startup/linker; map/size/hash/source и memory/vector CI; ST-Link install/restore runbook; запись только отдельно разрешена | **PASS базового стенда ee7d179**: program/verify/identity/RUNNING/progress, data-BSS/CCM word/guards, watchdog reset и UsageFault. Образ оставлен running; стресс/частоты/physical outputs не подтверждены |
| P1 — ESP32 OTA | Установка проверенного образа в неактивный слот, boot self-test, подтверждение или rollback с работающими LVGL/GT911/Wi-Fi | Расчёт/утверждение 16 МиБ factory recovery+otadata+двух OTA-слотов; отдельная USB-миграция; existing HTTPS/signature/local key/SHA/size/hardware checks; Wi-Fi/power-loss tests; реальные SRAM/PSRAM/stack измерения | Migration/factory и startup BOOT instrumentation проверены; fresh16MiB/two-NVMe и factory2MiB return PASS, на плате fecc05d/owner HMI PASS. Local minimum1887, worker1488из3072 требуют startup RAM review. Configured8192/TLS/install/slot confirm/rollback/full recovery/power-loss не испытаны; key/context/floors не утверждены |
| P2 — Releases | Настоящие воспроизводимые бинарники, SemVer/source binding, signed manifest/SHA-256, prerelease/stable, экран «ПО и обновления» | Рабочий OTA P1; проверка воспроизводимости, trust/signer/licensing; ручное утверждение владельцем exact SHA/tag/channel/assets; push/merge не выпускают release | CI/metadata-only review база существует; production signing/publication/discovery ещё нет; P1 UI включён только в explicit OTA profile. Фиктивный STM32 BIN запрещён |
| P3 — physical CAN | Минимальный проверенный обмен ESP32-S3↔STM32F407 с ошибками/тайм-аутами/восстановлением | Получить трансиверы; проверить уровни/терминаторы/pinout/mux/IDs/bitrate на изолированном стенде; отдельное hardware permission | Физический CAN не проверен. Существующий протокол сохраняется; новые слои только для конкретной интеграции |
| P4 — STM32 CAN OTA | Независимый bootloader, проверенная запись Flash/recovery после прерывания, existing independent crypto и ESP32 OTA Manager | Реальная карта/профиль P0 и physical CAN P3; reviewed bootloader/app/staging layout; fresh quiescent/operator/recovery gates; обновления при движении запрещены | Host transport/verifier подготовлены. Адреса загрузчика сейчас не резервируются, Flash writer/boot activation ещё нет |
| P5 — microSD/G-code/CNC | microSD/tools/parser/planner/timer STEP-DIR/encoders/homing/thread sync/coordinates/manual-CNC | Стабилизированные базовые контроллеры, проверенная механика/электрика/защита; каждое испытание движения отдельно разрешено | Дальнейший этап; существующие HMI simulation/tools не выдавать за управление станком |

## Разрешения и доказательства

Подготовка образа/CI не разрешает доступ к устройствам, Flash/reset/SWD/UART,
движение, merge или release. NC E-stop/STO/manual-CNC interlocks сохраняются.
Не назначать неизвестные motion pins/полярности, не менять eFuse/option bytes
без отдельного разрешения. Boot/reset/reconnect/OTA не разрешают движение.

[HARDWARE_STATUS.md](HARDWARE_STATUS.md) остаётся источником owner facts.
Официальный device memory/vector contract проверен программно внутри P0; измерения на
экземпляре, physical GPIO/CAN и backup restore отмечаются отдельно. Неизвестный
HSE не препятствует HSI-only сборке. Новую аппаратную проверку или размер памяти
не объявлять по успешной компиляции/host test.

Исторические номера PR-01…PR-08 ниже — имена подсистем, не порядок дальнейшей
работы. Сохранённые evidence и limitations не требуют нового подготовительного
PR вместо P0 и не являются разрешением аппаратных действий.

## Сохранённые инженерные milestones

### Слитый PR #54, PR-07v — от main5e76626

[Threaded signed HOST link](STM32_THREADED_LINK.md): dedicated driver actor/
concurrent RX/full independent signed chain, original deadlines/ownership/ACK+
actual drain. Host75/sanitizers75/standalone49/TSan8+1 PASS, eleven ARM archives
и старые4 неизменны. Target scheduler/backend/clock/identity/resources/runtime/
installer/physical gates открыты. [Evidence](evidence/build/threaded-link/README.md).

### Слитый PR #53, PR-07u — acknowledged RX owner

[Owner](STM32_INGRESS_OWNER.md): exact driver pause ACK+actual ring/staged/held/
RX drain/epoch cutover и bounded lower-timeout recovery. Исторические host74/
sanitizers74/standalone48/TSan8; software barrier не physical drain.
[Evidence](evidence/build/ingress-owner/README.md).

### Слитый PR #52, PR-07t — owned RX ingress

[Ingress](STM32_FRAME_INGRESS.md): bounded SPSC owned copies/original capture age/
sticky loss, host71/sanitizers71/standalone45/TSan6; no target runtime.
[Evidence](evidence/build/frame-ingress/README.md).

### PR-07s — owned driver dispatch от maindb50840

[Dispatch](STM32_DRIVER_DISPATCH.md): bounded actual frame publication/take/
fresh check/exact report, retained pending/claim, slot reuse after producer return
and actual consumer cleanup. Host68/sanitizers68/standalone42/TSan4;8 signed
positives+3 independent+2 lifetime refusals. ARM1324text/data0/bss0/frame72/14 DMB,
old budgets unchanged. Target backend/ISR/RTOS/cache/DMA/clock/truth/physical/
install open. [Evidence](evidence/build/driver-dispatch/README.md).

### PR-07r — SPSC completion mailbox от main2c6fd1c

[Mailbox](STM32_COMPLETION_MAILBOX.md): single producer/consumer atomic owned
reports, actual bound consumer/current-time service/retained claims. Host65/
sanitizers65/standalone39/TSan2,1200000 ordered reports +8448 actual completions;
8 signed positives+3 independent+2 lifetime refusals. Separate ARM1038text/data0/
bss0/frame64/5 DMB; old budgets unchanged. Target ISR/RTOS/cache/DMA/driver truth/
reset/identity/runtime/physical/install open. [Evidence](evidence/build/completion-mailbox/README.md).

### PR-07q — driver event inbox от mainefa2727

[Event inbox](STM32_DRIVER_EVENTS.md): owned actual egress/FIFO reports, full
backpressure, bounded current-time service, retained ownership/IDs/phase/deadlines.
Host61/sanitizers61/standalone35;8 signed positive+3 independent+2 delayed event
refusals; ARM768text/data0/bss0/frame56 separately. Actual synchronized adapter/
ISR/RTOS/reset/provenance/runtime/physical/install остаются open.
[Evidence](evidence/build/driver-events/README.md).

### PR-07p — bound TX egress от main555007d

[Egress](STM32_UPDATE_EGRESS.md): fixed custody queue/exact local driver tickets,
phase/begin wait for actual returns, held ownership after fault/cancel/uncertain.
Host59/sanitizers59/standalone33,8 signed positives+3 independent+2 driver refusals;
separate ARM1972text/data0/bss0/frame88, old code/budgets unchanged. Actual driver/
reset/trust/persistence/runtime/physical/install ещё open.
[Evidence](evidence/build/update-egress/README.md).

### PR-07o — shared update channel от main389de77

[Channel](STM32_UPDATE_CHANNEL.md): one owned outbox/strict TU/MS codec selection,
shared counters/local phases/role filtering. Host56/sanitizers56/standalone30,
512 cycles/7168 messages, actual8 signed positive+3 refusals. Separate ARM1940text/
data0/bss0/frame56, old code/budgets unchanged. Local traffic policy не upper
success/auth/remote queue flush proof; runtime/physical/install open.
[Evidence](evidence/build/update-channel/README.md).

### PR-07n — metadata setup от main d18cf93

[Setup/shared namespace](STM32_METADATA_SETUP.md): paired metadata ID из original
CLOSED pool, bounded MS exchange/raw fragments и pool ownership до concrete
metadata endpoint close. Original image windows/independent verification прежние.
Host54/sanitizers54/standalone28, setup separate3324text/frame48, old ceilings
не расширены. Authentication/persistence/routing/runtime/hardware/install open.
[Evidence](evidence/build/metadata-setup/README.md).

### PR-07m — bounded metadata transport от main7640f43

[Metadata](STM32_METADATA_TRANSPORT.md): distinct DATA identity/raw exact
container98..1215bytes, copy-before-ACK/one owned UNVERIFIED snapshot, retained
high-water/deadlines; real delivered metadata feeds independent verifier.
Host50/sanitizers50/standalone24, oracle2/1024 lengths/100000 mutations/8 positive
signed chains+3 refusals. Separate ARM1568text/data0/bss0/frame48; old libs прежние.
Authenticated setup/identity/runtime/physical/installer gates остаются открытыми.

### Слитый PR #44, PR-07l — shared-clock host composition от main277801e

[Timing contract](STM32_UPDATE_TIMING.md): one-frame queue per direction,
shared caller clock, losses/delays/duplicates/whole-message retries без reset
logical deadlines.34 DATA cases,8 signed positive chains/2 refusals; host47/
sanitizers47/standalone21, ARM footprint прежний. Measurements/runtime driver/
identity/metadata transport/install не добавлены; board gates прежние.

### Слитый PR #43, PR-07k — bounded fragments от main4db99df

[Fragment draft](STM32_UPDATE_FRAGMENTS.md):286byte owned buffers, <=8byte
frames, explicit route/generation и retained counters, queue peek/commit,
exact duplicates/order, bounded deadlines/quota/READY backpressure/single take.
Host45/sanitizers45/standalone19, oracle3,100000 fault chains и8 signed RAM
chains. ARM separate1536text/data0/bss0/frame48, old libraries/codec прежние;
не full RAM/stack/throughput proof. Production identity/routing/authentication/
metadata transport/runtime/install и аппаратные gates остаются открытыми.

### Слитый PR #42, PR-07j — bounded update byte codec от maindeace1c

[Codec draft](STM32_UPDATE_WIRE.md):8 message kinds, exact length/header/reserved,
canonical LE без C ABI/padding/tail, zero failure output. Independent golden12,
65536 outcome pairs,200000 mutations,8 signed RAM integration chains; host42/
sanitizers42/standalone16, analyzer12+1 и budget guard6. ARM codec1219text,
data/bss0/frame32, old4 archives43362text (+18); это не board RAM/CPU budget.
Codec не authentication, production CAN/fragmentation/runtime или installer.

### Слитый PR #41, PR-05d — blocked build binding audit от mainaa06833

[Build binding](RELEASE_BUILD_BINDING.md) сопоставляет external-pinned unsigned
candidate/full image/ELF digest с existing signed cohort. Factory/A-B layout и
build/signed profile mismatch явные, Motion build provenance не выдумана.
Report metadata-only/blocked exit3, все authority gates false. Host39/sanitizers39/
standalone13, новая suite13methods и прежние ARM gates; SDK matrix8 сохранена с
integrity pin check. Production bundle/context/provenance и hardware blockers
не закрыты, firmware/settings/keys/devices/publication не менялись.

### Слитый PR #40, PR-05c — offline release cohort review от main2c06cdc

[Review](RELEASE_COHORT_REVIEW.md): independent HMI/Motion verifiers, полные
images/ESP descriptor, signed source/release/channel и prospective peer range.
Host38/sanitizers38/standalone13, новые CLI3/cohort9 methods, analyzer8 embedded
+1 host/source/board/output/ARM-scope PASS. Firmware libraries/frames прежние.
Report не production bundle, context provider, protocol mapping или installer.

### PR-07i — RAM staging source от main2ea51b6

[Staging source](STM32_STAGING_SOURCE.md): explicit caller buffer, pending DATA
copy перед ACK, SEALED/LEASED/discard и monotonic tokens/high-water без reset.
Host36/sanitizers36/standalone13,45 variants/1536 cycles/8 signed backend chains,
ARM/static8/source/board/output PASS. State160 bytes плюс buffer; consume frame328
при прежнем лимите512. Не runtime caller, persistent recovery или RAM/map proof.

### PR-07h — release-aware handoff от mainc17e89c

[Release handoff](STM32_RELEASE_HANDOFF.md) связывает signed15-field document
с прежними lease/RX/owner/deadline/vector guards и completion/outcome; raw metadata
SHA сверяется с negotiation. Host34/sanitizers34/standalone11 и ARM/static7/source/
board/output gates PASS,6 новых full chains. Legacy core/receipt/completion/
workspace прежние; adapter528bytes caller-owned. Production linkage/local context,
runtime/CAN/backend/Flash/recovery/physical gates ещё открыты, устройства не трогали.

### PR-07g — независимый signed release document от main3381236

[Binding](STM32_RELEASE_BINDING.md): новый opt-in API и strict15-field draft,
release selection/source/channel/component SemVer + old target/counter/full image
checks через тот же engine/arena. Host33/sanitizers33/standalone10,2745 SemVer
comparisons, ARM/static/source/image/output PASS. Legacy API, workspace17664 и
max individual admission frame984 прежние. Нет production bundle/version/key
context или аппаратных действий. Это исторический результат PR #37;
последующее live coordinator соединение описано выше в PR-07h.

### PR-07f — свежий logical update-сеанс от mainf0bd866

[Negotiation](STM32_UPDATE_NEGOTIATION.md): четыре memory-model сообщения,
monotonic pools/high-water, bounded replies/deadline и one-time take.1024 sessions,
6 new signed full chains/3 negative bindings; host31/sanitizers31/standalone8,
ARM/static/source/image/output PASS. No authentication/boot identity provider/
persistence/wire/runtime caller; actual F407/Flash/hardware gates прежние.

### PR-08a — локальные сведения о ПО от mainb56986f

[Экран](FIRMWARE_INFO_UI.md): parameters menu + build/SDK/profile, explicit
simulator/unknown provenance, no runtime OTA caller. Lazy-create/delete panels,
64 host navigation cycles с одинаковым pool free и object count, persistent
STOP/fault/keyboard cleanup. Host30/sanitizers30 и local SDK6.1.1 headless/rev12
PASS; CI matrix8/STM32 gates сохранены. Не закрывает production context,
resource/soak/installer или physical F407/CAN gates.

### PR-05b — release metadata policy review от main455ccc6

[Review](RELEASE_POLICY_REVIEW.md): malformed Unicode hex gate и canonical Base64
offline parity;115 signed C/Python scenarios,2809 SemVer comparisons,65536 BMP
escapes и130 Base64 cases. Host29/sanitizers29/native analyzer3units и local
ESP-IDF6.1.1 headless/rev12 PASS; CI matrix8 и STM32 gates сохранены.
Actual local versions/keys, production Motion schema, layout migration/installer
и hardware commissioning остаются открыты. Owner повторил VGT6, HSE не сообщил.

### Исторический снимок PR-07b от main74a7af9 — 20 ноября2026

[Независимый image admission](STM32_IMAGE_ADMISSION.md) в этой ветке:
подпись P-256/raw metadata, strict draft parser/local compatibility/counters,
SHA/exact EOF, bounded arena/deadline/cancel, cleanup и ARM compile checks.
На момент подготовки PR #29 transport был открыт; код admission не зависел от него. Теперь оба PR слиты. Есть только
VERIFIED_CANDIDATE, без board image/Flash/CAN/commit/jump/runtime.
Дальше нужны утверждённый release metadata/keys/version context, интеграционный
session/quiescent contract и verified erase/vector/recovery перед writer.
Аппаратные milestones ниже этим software результатом не закрываются.

- M0: база документов/backup. ESP32 backup завершён; Blue Pill и F407 backups
  сообщены; F407 SWD/core/Flash1024KiB подтверждены владельцем. Restore/SHA F407,
  маркировка/RAM/HSE/секторы и аппаратные unknowns остаются открыты.
- M1: работающий HMI/software simulation и матрица SDK; display наблюдался,
  Wi-Fi input/connect fix — PR #5; commissioning сенсора/метрик ещё не завершён.
- M2: CAN bench и fault injection; сейчас только inert host envelope,
  трансиверов нет. Никаких физических CAN испытаний не заявлять.
- M3: STM32 foundation и bench timer/DMA measurements без приводов.
- M4: квалифицированная safety wiring review, interlocks, homing/limits,
  подтверждённые направления и поосевое commissioning с отдельным разрешением.
- M5: энкодер шпинделя/servo/thread synchronization после механических измерений.
- M6: indexing/C-axis/removable milling — последующие отдельно проверяемые задачи.

OTA, microSD и CI/CD поддерживают эти этапы, но не являются доказательством
безопасности реального станка. Исторический CODEX_TASK_001 больше не активный план.

## Hardware baseline b93cf3b

[18 ноября: разрешённая USB-запись и результаты](evidence/hardware/2026-11-18-b93cf3b/README.md).
Basic HMI/input/Wi-Fi DHCP PASS; internal largest block 12288 bytes блокирует
простое добавление HTTPS/session в internal RAM. PR-04f/04g слиты; [isolated allocation bench 1c38d1e прошёл три цикла](evidence/hardware/2026-11-18-1c38d1e-psram/README.md). Production TLS memory budget ещё не утверждён; Gate 1 soak/negative tests,
UART capture/reset procedure и Gate 2 ещё не завершены.

## PR-04h — время для TLS

[Local provenance/age/skew gate](TLS_TIME_GATE.md) и guard вокруг каждого
HTTP operation, SDK clock sampler. Источник UTC/SNTP service, production policy,
UI и runtime TLS profiling ещё не включены; аппаратных операций нет.

## PR-04i — SNTP source

[Bounded source](SNTP_SOURCE.md) готовится отдельным PR после #18. Нет
startup/UI invocation или production time policy. Следующий шаг — opt-in
UTC/TLS memory profiling с отдельным разрешением на точный image и network test.

## PR-04j — явный UTC/TLS memory bench

[Отдельный профиль](UTC_TLS_BENCH.md), две heap windows, один HEAD без body.
PR #20 слит; [19 ноября b085abb испытан](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md):
SNTP PASS, TLS context allocation FAIL (-141 insufficient memory). Возвращён
ordinary rev12 b085abb. Следующий шаг — review allocator/PSRAM/TLS memory
с host/build проверками; новое аппаратное испытание требует нового разрешения. Production TLS budget не объявлять по одному
снимку/endpoint: нужны ошибки/redirect-hosts/soak и ownership/performance review.

## PR-04k — TLS PSRAM allocator / X.509 dates

[Review и профиль](TLS_MEMORY_REVIEW.md): исправление причины b085abb allocation
failure через штатный SDK PSRAM-only allocator. Только TLS bench; ordinary
profiles/reserves/record sizes не меняются. Включена SDK certificate-date check,
которую local anchor не заменяет. Software matrix/negative/ELF checks; новый
hardware gate отдельно, до него runtime TLS/production budget остаются блокированы.

## Hardware gate PR-04k — 1bfc7e2

[Разрешённый TLS/PSRAM test](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
дал SNTP/verified HEAD200/cleanup PASS; возвращён ordinary1bfc7e2,owner HMI PASS.
Production reserve не утверждён: internal min3123/window,2408/history и stack
watermark912bytes. Следующий программный шаг — bounded memory/stack review
перед runtime session/body. Не подключать installer или снижать CA/time/DMA
reserve ради progress; новые hardware/fault/soak tests только отдельно согласовать.

## PR-04l — TLS worker headroom

[Изолированное исправление](TLS_HEADROOM.md): compact Wi-Fi online accessor
вместо UI snapshot в стеке, HTTP RX2048, finite stage diagnostics. Stack6144,
TLS16384/4096 и DMA reserve64KiB прежние. Software checks не закрывают runtime
memory/stack/CPU gate; следующее exact-image испытание отдельно согласовать.

## Hardware gate PR-04l — 8f0d899

[Разрешённый test](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
SNTP/HEAD200/cleanup PASS,window internal min5307/stack watermark1840bytes,
против3123/912 у1bfc7e2. Вернули ordinary8f0d899,owner HMI/touch/Wi-Fi PASS.
Следующий программный шаг — review remaining handshake/internal allocations/
call stack и bounded resource admission/failure перед runtime OTA session/body.
Не превращать один положительный test в production reserve/soak approval.

## PR-04m — bounded resource admission

[Resource guard](OTA_RESOURCE_GUARD.md): reviewed local policy/current heap/stack,
обязательные inputs нового guarded session entry, sticky failure/candidate revoke
на phase/chunk boundaries. Legacy API и target workspace34000 сохранены.
Нет runtime worker/body/install activation; native budget/load/fault gates открыты.

## PR-05a — сразу несколько частей подготовки релизов

[Build candidates](BUILD_CANDIDATES.md): полный offline ESP app parser, связь
app↔ELF/clean source/display version, детерминированный metadata-only bundle,
независимый verifier и negative tests, интеграция в8 CI jobs и SHA-pinned Actions.
Обычная factory2MiB layout остаётся; SemVer/key/license/release approval и runtime
OTA/migration gates не подменяются успешным review artifact. Аппаратных операций нет.

## PR-02b — после owner SWD F407

[Owner evidence](evidence/hardware/stm32f407-owner-swd/README.md) закрывает прежнее
UNKNOWN по SWD/core/device ID/Flash capacity. [Startup foundation](STM32_STARTUP.md)
проверяет portable init/no-I/O и Cortex-M4 reset/link sections на synthetic map,
сохраняя блокировку board image. Следом нужен reviewed RAM/vector/clock/board
contract; HSE можно отложить при отдельно принятом HSI-only решении. CAN bootloader
и OTA sectors не резервируются до geometry/recovery review; motion запрещён.

## PR-07a — две стороны транспорта до Flash и CAN hardware

[Ограниченный logical session](STM32_UPDATE_TRANSPORT.md): stop-and-wait,
двухфазный consumer/ACK, exact duplicate с проверкой payload, CRC-32C,
explicit retries/total/progress deadlines, identity/cursor checks и sticky faults.
Host/sanitizer tests проверяют потери DATA/final ACK, reorder/corruption,
partial consumer/volatile resets и отсутствие automatic resume. Новый модуль
ARM compile-only, независим от motion. Нет runtime caller или installer.
Этот transport gate был продолжен независимым admission в PR #30;
hardware/Flash layout/recovery/quiescent/wire decisions остаются открытыми.

## PR-07c — transport/source lifetime

[Read-only handoff](STM32_UPDATE_HANDOFF.md) от main28fe7bc связывает слитые
PR #29/30: immutable lease, policy/boot/source guards, descriptor↔signature/SHA,
receipt revoke и bounded cleanup/quarantine. Root25/sanitizer25/standalone5,
ARM/static/image/output gates; hardware milestones не закрываются.
Следующий software review — production metadata/context и protocol/source
transition после bounded RX completion. Writer/jump ждут board/erase/vector/
recovery/fresh physical quiescent/operator gates; Flash addresses не резервируются.

## PR-07d — проверка модели образа до board map

[SP/PC/vector checker](STM32_IMAGE_PREFLIGHT.md) от main4b7631e использует только
explicit local model; источник остаётся подписанным immutable snapshot. Новая
profile suite, signed vector/fault tests, ARM/static/import/frame и прежние
8 SDK builds. MODEL_MATCH не закрывает hardware/Flash/recovery milestones и
не разрешает writer/jump. Следующие review: production release/context/profile
и verified board memory/vector/clock/erase/protection без предположения адресов.

## PR-07e — завершение software-проверки и доставка истории

[Completion/outcome](STM32_UPDATE_OUTCOME.md) от maine0bd84e: actual crypto/model
checks → lease close → historical report/ACK, bounded repeats/linger/deadline,
context/reboot/conflict и quarantine. Host27/sanitizers27/standalone7, ARM/static/
source/output gates и8 SDK builds. Next: reviewed production negotiation/codec/
identity и metadata/context, actual board/Flash/recovery; не activation по peer report.

## Слитый PR #52, PR-07t

[Очередь входящих кадров](STM32_FRAME_INGRESS.md) от main25e635c после PR #51:
owned SPSC capture/retry/stage/fresh age check и sticky loss/clock/expiry/exhaustion,
actual signed host chains сохраняют original crypto/lease/model/close deadlines.
Host71/sanitizers71/standalone45/TSan6; ARM1060/frame48 отдельно; no new devices,
Flash map/pins/driver/runtime. Далее — actual producer-stop/phase-cutover, truthful
backend loss/completion/reset/clock и reviewed resource/identity/runtime port.

## Слитый PR #53, PR-07u

[Подтверждённая пауза RX](STM32_INGRESS_OWNER.md) от mainbc35a37 после PR #52:
actual driver ACK + owned ingress/channel drain перед сменой фазы, epoch capture
permits и bounded fragment-timeout recovery без reset clocks/counters/upper
windows. Дальше actual target peripheral/IRQ/DMA/backend stop/route lifecycle,
clock/resources/identity/runtime и physical/install gates; устройств/Flash/pins нет.
