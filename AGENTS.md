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

PR #1–51 слиты, база main `25e635c`, main CI5/5 PASS,
открытых PR перед работой нет.
PR-07t: читать docs/STM32_FRAME_INGRESS.md. Optional SPSC RX copy mailbox,
immutable binding/stable lifetime/externally publish init/stop+join, no live
copy/move/reinit/flush/clear. One driver offer/retry/lost; one consumer stage/
check/empty. Producer never reads mutable protocol; consumer never reads private
producer state. Captured time separate from fresh producer now; full retains
first owned copy/original timestamp, BUSY caller retains new input+timestamp or
reports actual loss. Stage token has no payload; read time AFTER acquire, exact
check original age then FRAME unverified owned bytes, refusal output zero.
Clock/expiry/overrun/exhaustion sticky, faults invalid object UINT_MAX. Empty
snapshot not producer-stop/phase-cutover/backend drain; no phase authority here.
Current-time protocol polling/crypto/lease/model/close/upper windows unchanged.
Host71/sanitizers71/standalone45/TSan6,800000 FIFO+256 messages/8448 fragments,
3 controlled thread faults,8 signed positive+3 independent+2 ingress refusals.
Analyzer22+2/guards7×6+3×7/source178/synthetic/board/output/scope PASS. Separate
ARM1060text/data0/bss0/frame48,8exports/15DMB,4096/frame128 ceiling. Host208/token32
not MCU RAM/full stack/timing. Actual driver/ISR/RTOS/cache/DMA/clock/phase-cutover/
identity/resources/runtime/install/hardware remain open. No devices/merge/release.

Слитый PR #51, PR-07s: читать docs/STM32_DRIVER_DISPATCH.md. Actual completion owner + one owned
frame dispatch slot, protocol actor publish/consumer APIs vs driver actor take/
check/report/retry. Externally publish init/stable lifetime/stop+join; no live
copy/move/reinit/flush/inner mutable APIs. Producer never reads mutable protocol
state; owner never reads driver-private fields. Actual claim/original admission
copied before release READY; take acquire copies to private snapshot, returns
zero-payload UNCHECKED token. Read time AFTER take; fresh check before exposure/
backend. Refusal output zero, clock sticky, no implicit current-frame callback.
Exact ticket required; full retains first report for retry, no uncertain→terminal
overwrite. Terminal report admission publishes RETURNED; actual consumer cleanup
AND RETURNED required for slot EMPTY/reuse. Cancel retains untaken/taken/pending/
claim; no synthesized terminal or force-forget. Late SENT after exposure can
reclaim only; original upper deadlines/crypto/lease/model/close gates unchanged.
Owner clear retains IDs/phase/time/mailbox positions, cannot reset driver clock
fault. Software revoke/check not atomic hardware cancel/physical inhibit.
Host68/sanitizers68/standalone42/TSan4;256 actual messages/8448 frames/1056 rejected
callbacks +4 controlled thread faults;8 signed positives+3 independent+2 lifetime
refusals. Analyzer21+2/guards7×6+2×7/source178/synthetic/board/output/scope PASS.
Separate ARM1324text/data0/bss0/frame72,13 exports/14 DMB/no atomic helpers,
4096/frame128 ceiling. Old archives/budgets/SDK/workspace/arena unchanged; host
1360/delivery48 not MCU RAM/full stack/latency approval. Target ISR/RTOS/cache/
DMA/clock/backend truth/reset/identity/resources/runtime/install/physical open.
No device/SWD/UART/reset/Flash/CAN/motion/merge/release actions.

Слитый PR #50, PR-07r: читать docs/STM32_COMPLETION_MAILBOX.md. Optional C11 SPSC owned-report
mailbox + actual bound consumer. One producer post ONLY, one consumer other APIs;
producer reads immutable binding only. Init externally published before actors,
stable lifetime/stop+join before destruction, no live copy/move/reinit/flush/reset.
Release/acquire publication+slot reuse, unsigned32 always-lock-free compile gate;
no firmware locks/spin/heap/I/O, no target ISR/RTOS/cache/DMA approval. Full BUSY
requires retained external report retry; consumer pending retained until actual
inner admission. Service polls current time and handles <=1, no report timestamp.
Begin/claim/phase/clear honor actual mailbox/pending/inner ledger; no external
success/drained boolean. Empty snapshot does not mean producer stopped; late
duplicate cannot free new claim due retained exact tickets. Cancel/clear retain
ownership/atomic positions/IDs/phase/time, sticky faults remain. Original upper
crypto/lease/model/image/close gates unchanged. Host65/sanitizers65/standalone39,
TSan2 no suppressions;1200000 ordered reports +256 actual messages/8448 frames/
9504 reports;8 signed positives+3 independent+2 lifetime refusals. Analyzer20+2,
guards7×6+7,178 source/synthetic/board/output/scope PASS. Separate ARM1038text/
data0/bss0/frame64,14 exports,4096/frame128 ceiling/5 native DMB/no atomic helpers.
Old archives/budgets/SDK/workspace unchanged; host mailbox200/owner1208 not MCU
RAM/full stack/physical timing proof. Actual target port/provenance/reset/resource/
identity/routing/runtime/install/hardware gates open; no devices/merge/release.

Слитый PR #49, PR-07q: читать docs/STM32_DRIVER_EVENTS.md. Opt-in actual egress + owned event
FIFO capacity1..4, no caller drain boolean. Post ADMITTED means copy only; BUSY
requires adapter retain/retry, no drops. Current-time service polls original
windows then consumes <=1 event; separate poll consumes none. Begin/claim/phase
wait actual inbox; cancel retains inbox+claim; clear only after actual event
consumption and exact terminal cleanup. No timestamp/reinit/counter/time reset.
Wrong owner/route/generation refused; stale/duplicate cannot free current claim.
Original independent crypto/lease/model/image/close gates unchanged. Serialized
one owner ONLY, not RTOS/ISR/thread-safe/real driver; no I/O/heap/runtime/IDs/pins/
Flash defaults/devices/self-merge. Host61/sanitizers61/standalone35,256 actual
messages/8448 frames/21120 reports,8 signed positive+3 independent+2 delayed event
refusals. Analyzer19 embedded+2 host/guards7×6/source178/board/output/scope PASS;
new archive768text/data0/bss0/frame56,4096/frame128 ceiling,10 exports. Old code/
budgets/SDK/workspace unchanged. Host event32/owner968 not MCU RAM/full stack.
New host mode explicit fragment total256 before init for extra event hop; old
modes128/progress8/frame160 and all upper windows preserved. Actual synchronized
adapter/provenance/reset/reconciliation/persistence/routing/resources/staging/
installer/physical blockers open; no force-forget or success/physical claim.

Слитый PR #48, PR-07p: читать docs/STM32_UPDATE_EGRESS.md. Opt-in bound channel TX + actual
owned frame ledger, explicit capacity1..4/deadline, one exact owner/route/generation/serial
claim. Channel commit after owned copy; phase/begin wait for correlated local
terminal returns, no caller drain/success boolean. Local SENT_UNVERIFIED не
remote ACK/auth/physical/upper completion. Timeout/cancel/uncertain retains actual
claim; cancel drops only unclaimed entries. Late terminal return reclaims without
success/revival; no force-forget. Clear retains IDs/tickets/phase/time; lower
context/clock/exhaustion sticky. Callback events processed by owner at current
clock, not old snapshot time; no ISR/threads/I/O/heap/runtime/driver/Flash defaults.
Original crypto/lease/model/image deadlines unchanged. Host59/sanitizers59/
standalone33;4096 FIFO cycles/10240 frames,8 signed positive+3 independent refusals
+2 driver lifetime refusals. Analyzer18 embedded+2 host, guards6×6; separate
1972text/data0/bss0/frame88,4096/frame128 ceiling. Old libs/ceilings/workspace/
SDK pins unchanged. Host queue128/delivery40/owner832 not MCU RAM/full stack proof.
Real trusted driver completion/reset/queue contracts, remote auth/identity/
persistence/routing/worker/resources/hardware/install gates open. No devices,
no self-merge; exact claim may remain blocked until truthful adapter return.

Слитый PR #47, PR-07o: читать docs/STM32_UPDATE_CHANNEL.md. Opt-in mixed TU/MS channel,
owned one-frame TX outbox/exact local commit; last queued frame bounded even
when lower TX IDLE. Shared fragment counter through local phases, no reset.
Advance only locally quiescent/sequential, paired role IDs; this traffic filter
не доказывает upper success/auth/remote queue drain. Caller retains original
negotiation/setup/DATA/crypto/live lease/model/deadline/clean-close gates.
RX strict single selected codec/no fallback, single owned TAKEN_UNVERIFIED,
wrong phase/ID no dispatch/output zero; context/clock/exhaustion sticky. Host-only
shared link now covers all phases/12 kinds with actual signed payloads, no new
wire bytes/IDs/bitrate/runtime/heap/crypto owner/hardware/Flash defaults. Host56/
sanitizers56/standalone30;512 cycles/7168 messages/8 positive+3 refused chains.
Analyzer16 embedded+2 host, guards5×6; channel1940text/data0/bss0/frame56 separately,
4096/frame128 ceiling. Old ARM libs/codecs/limits/workspace/SDK pins unchanged.
TX696/RX664/snapshot288 host bytes не board RAM reserve/full stack approval.
External queue lifetime/flush/routing/authentication/persistent epoch/runtime/
resource/physical/installer gates open. No devices or self-merge.

Слитый PR #46, PR-07n: читать docs/STM32_METADATA_SETUP.md. Opt-in setup/shared volatile role
namespace, paired image+1 allocation без wrap/rollback из того же pool. CLOSED
image negotiation сохраняет request/offer до clear; no-I/O/runtime/device gates
прежние. Borrow actual CLOSED image + empty live TX/RX; poll original windows,
no reinit/deadline extension. Pools held через metadata transfer до concrete
complete metadata TX / TAKEN metadata RX close. One take не auth/install/live
receipt; independent signature/raw digest/full image/lease/model обязательны.
MS172byte codec отдельно от TU; explicit immutable raw fragment mode, unchecked
single take → selected codec before dispatch. Local route/mode/counters не
serialized/authenticated; production identity/persistence/routing/replay/worker/
physical/Flash/install gates открыты. Responder CONFIRM bounded retry после take;
duplicates не reset time/sends. Context/clock sticky, clear retains IDs/time.
Host54/sanitizers54/standalone28; analyzer15 embedded+2 host, oracle4/raw279/
100000 mutations/8 signed positive+3 refusal chains. Separate setup3324text/
data0/bss0/frame48,4096text/frame128 ceiling+6 negatives. Old4 text43600 (+238),
fragments1904 (+368); old ceilings/frames/workspace/config/SDK pins прежние.
Full MCU RAM/stack/CPU/physical throughput не измерены; не объявлять готовыми.

Слитый PR #45, PR-07m: читать STM32_METADATA_TRANSPORT.md. Opt-in metadata codec/RX поверх
unchanged DATA/consumer/ACK. Отдельный globally fresh metadata ID, не image ID;
current pair/high-water local checks не global provider/routing/authentication.
Container98..1215, raw doc1..1024/signature8..72/key ID1..31. Descriptor setup
explicit local input, не реализованный wire handshake. No new TU kinds/version.
ACK после copy, single owned TAKEN_UNVERIFIED snapshot; raw SHA/signature не
проверять здесь и не объявлять VERIFIED. Original independent verifier/metadata
request binding/full image/lease/model/close обязательны. Image RX не reset после
metadata delay; его original windows продолжаются. Discard сохраняет ID/time,
CLOCK/CONTEXT sticky; last block только для bounded final ACK после take.
No new crypto/heap/global/runtime/driver/board/Flash defaults. Host50/sanitizers50/
standalone24; oracle2,1024 lengths,100000 mutations,8 delivered-metadata positive
chains +3 refusal cases. DER variable => per-run counts, не fixed golden stats.
ARM new separate1568text/data0/bss0/frame48, old libraries/limits прежние; metadata
archive guard6/exact exports/import/frame scope обязателен. RX1728/snapshot1212
host bytes не MCU RAM reserve/approval. Actual board/identity/install gates открыты.

Слитый PR #44, PR-07l: читать STM32_UPDATE_TIMING.md. HOST-ONLY bounded test link, две one-frame
queues, один caller clock для fragments/logical/lease/crypto. Никакого runtime
owner/driver/identity provider/default timing/Flash. Не переносить helper из
protocol/can/tests в ARM; targets/import/scope checks обязательны. Core C/API/
limits/deadlines прежние, no reinit/counter/time reset ради recovery. Fragment
clear только explicit recoverable timeout policy теста; context/clock sticky.
34 DATA scenarios +8 signed positive/2 refusal chains, host47/sanitizers47/
standalone21, analyzer13 embedded+2 host и прежние ARM/source/board/output gates.
Synthetic2551ms не measured bus/CPU/throughput: crypto CPU не тарифицируется,
metadata/signature не передаются по каналу. Old START принимается новым RX с
другой local generation: known replay blocker, не security success. Production
identity/routing/queue/metadata/runtime/hardware/install gates остаются открытыми.

Слитый PR #43, PR-07k: читать STM32_UPDATE_FRAGMENTS.md и protocol/can/AGENTS.md. Только
opt-in single-owner bounded envelope/reassembly:8-byte maximum, owned286byte
TX/RX buffers, explicit local route/generation, retained counters без wrap,
peek/exact local-queue commit, no silent RX restart, single take после codec.
READY_BYTES_UNCHECKED не verified/auth/install authority; unchanged logical
CRC/session/independent crypto/lease/model/deadlines обязательны. Poll без
traffic, duplicates/peek/reject/BUSY не продлевают сроки. Context/clock/exhaustion
sticky; clear/reinit не сбрасывают counters или lifetime. Local identity не
передаётся на wire и не обеспечивает replay isolation после reset. Production
routing/authentication/IDs/bitrate/metadata-signature transport/runtime/installer
ещё отсутствуют. No-I/O/board/output/hardware gates прежние, устройств нет.
Host45/sanitizers45/standalone19, independent envelope oracle3,263 roundtrips,
DLC256/START32768/offset65536,100000 mutation/drop chains и8 signed RAM chains.
Analyzer13 embedded+1 host; ARM fragments1536text/data0/bss0/frame48 отдельно,
4096text/frame128 ceiling +6 archive guard methods; old archives/codec прежние.
Эти цифры не full stack/MCU RAM/CPU/physical throughput approval.

Слитый PR #42, PR-07j: читать STM32_UPDATE_WIRE.md. Только отдельный
opt-in byte codec draft: existing8 message kinds, exact bounded LE framing,
explicit enum tokens, failure output zeroing. Не отправлять C memory layout/
padding/tails. Shared negotiation shape helper не physical/session/auth gate;
CRC проверяет прежний RX, independent crypto/lease/model/deadlines прежние.
Codec OK не dispatch/install/COMMIT/jump/ARM authority. Нет production CAN
version/IDs/bitrate/fragmentation/metadata transfer/runtime caller или устройств.
Host42/sanitizers42/standalone16, golden12/65536 tokens/200000 mutations и
8 signed RAM chains; strict analyzer12 embedded+1 host, wire budget guard6.
ARM4 libraries43362text/data8/bss27 (+18text), отдельный codec1219text/data0/
bss0/frame32; это не full stack/RAM/performance approval. Existing budgets
не расширять, negative budget/object/import/frame checks сохранять.

Слитый PR #41, PR-05d: читать RELEASE_BUILD_BINDING.md при изменении audit. Связать
unsigned CI candidate с тем же signed cohort/full HMI bytes/ELF digest через
explicit external candidate SHA pin. Pin не CI/source authentication; config/
partition/tree/license hashes остаются unsigned claims. Проверенные fields
возвращать из того же snapshot, не перечитывать paths после проверки.
Старые build/cohort CLI reports прежние. Новый audit всегда BLOCKED/exit3;
layout/profile mismatch, missing Motion build evidence и все authority gates
открыто показывать. Нельзя менять factory/PROJECT_VER/SDK/profile ради PASS,
превращать metadata-only report в manifest/installer/approval или добавлять keys.
CI сохраняет matrix8, пишет self-generated integrity pin каждого candidate;
это не owner approval. Host39/sanitizers39/standalone13 и прежние ARM/static/
source/scope/board/output gates; firmware footprint/config/dependencies прежние.

Слитый PR-05c: читать RELEASE_COHORT_REVIEW.md при изменении signed cohort.
Offline tool проверяет existing signed HMI/Motion formats/full images, explicit
local selection/source/channel, ESP app descriptor и prospective HMI peer range.
Motion frontend вызывает прежний independent Mbed verifier, только HOST; нет
нового firmware parser/crypto owner/clock/resource defaults/runtime caller.
Legacy HMI verify/CLI report прежний; signed metadata возвращается из того же
snapshot, что прошёл signature/image checks. Не перечитывать replaced manifest
для binding и не принимать generic success boolean вместо crypto.

Local context/key/executable paths — explicit trusted operator inputs, не поля
скачанного JSON. Bounded regular snapshots; quiescent owned FS/toolchain, не
sandbox hostile executable. Test private keys только RAM (два разных role keys),
никаких private files/production signer/provisioning. Review JSON metadata-only,
не OTA manifest/bundle/CAN ABI или install token. HMI-only без fake Motion record.
Все authority gates закрыты; protocol SemVer и numeric counters НЕ переводить.
Source signer claim и driver SHA не build/code attestation. Реальные versions/
keys/context/protocol mapping/production bundle/лицензия/publish ещё открыты.

Host/sanitizers38 suites, standalone13, ARM/static8 embedded+1 host/source/
board/output/scope gates; прежняя ESP SDK matrix8. ARM libraries43344text/data8/
bss27/frames984,376,328,64 прежние. Host I/O frontend не входит в ARM targets.
Firmware C/UI/board/config/partition/security/dependency pins прежние. Не менять
PROJECT_VER ради review PASS; current git version не trusted release SemVer,
factory2MiB не A/B. Runtime budget/worker/installer и production context открыты.

PR-07i RAM source — один owner, exclusive caller buffer, no init/discard reset
counters/high-water, no writes/discard while leased. API не DMA/MPU protection,
volatile не durable recovery; Flash1MiB не RAM1MiB. Quarantine держит handle,
DATA deadlines не продлевать. Receipt/history не install/COMMIT/jump/ARM token.
Owner F407 SWD/core/rawID/Flash1MiB/3,252V/backup/vector facts прежние; VGT6 не
full marking/HSE/RAM/CCM/IRQ/erase/protection/GPIO/CAN/restore proof. BOARD_IMAGE=ON
fail, synthetic startup не actual profile, no-I/O inhibit=false/watchdog unavailable
не physical safe. Не читать backup/dump и не повторять device probes.
На ESP32 ordinary8f0d899, owner HMI/touch/Wi-Fi PASS; разрешения исполнены.
Нет devices/SWD/UART/reset/Flash/network probes, keys/releases/eFuse/CAN/motion,
installer/boot activation или самостоятельного merge.
