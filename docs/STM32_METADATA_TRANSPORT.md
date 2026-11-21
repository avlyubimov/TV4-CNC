# PR-07m — bounded metadata container/receiver

База main `7640f43`, PR #1–44 слиты, main CI5/5 PASS.
[План](plans/PR07M_METADATA_TRANSPORT.md), [evidence](evidence/build/update-metadata-stage/README.md).
Устройства/backup не читаются, запись/merge/release не выполняются.

## Граница и две роли DATA

Новая opt-in C11 [библиотека](../protocol/can/update_metadata.h) переносит exact
raw Motion metadata, signature и key ID через **отдельный** logical DATA stream.
Использует прежние CRC/sequence/pending/consumer/ACK и существующий TU codec/
fragment envelope в host integration. Старые8 wire kinds/version/limits не меняет,
CAN IDs/bitrate/pins/driver не назначает. MCU runtime caller/worker отсутствует.

Это transport foundation, не аутентифицированный setup/production protocol.
Caller явно задаёт metadata transfer descriptor (ID/полный размер/opaque bundle
SHA), local negotiated image binding, boot/generation/limits/high-water. В tests
descriptor передаётся напрямую из fixture; его discovery/handshake по сети ещё
не реализован. Metadata ID отличается от image ID и должен быть globally fresh
в том же selected boot epoch. RX проверяет current pair и собственный monotonic
metadata high-water; историческую общую namespace image/metadata должен вести
будущий reviewed provider. Не переиспользовать ни один старый ID другой роли.

Если metadata DATA попадает в image RX или наоборот, current pair отвергается
прежней session-проверкой до copy. Это не routing/authentication/reboot replay
isolation: полностью matching forged контекст остаётся внешним blocker.
Нельзя передавать внутренний metadata RX в image admission/handoff.
Offset — индекс контейнера, не адрес Flash. Real layout/bootloader reserve нет.

## Контейнер draft1

Canonical LE bytes, без C padding/enum representation/unused tails:

| Offset | Bytes | Поле |
|---|---:|---|
| 0 | 2 | ASCII `TM`, discriminator контейнера |
| 2 | 1 | Draft1, не production CAN SemVer |
| 3 | 1 | Reserved0 |
| 4 /6 /8 | 2 each | Document/signature/key ID lengths LE16 |
| 10 | 2 | Reserved0,0 |
| 12 /16 /20 | 4 each | Image boot/transfer/size LE32 |
| 24 | 32 | Expected image SHA-256 |
| 56 | 32 | Expected raw metadata SHA-256 из local request |
| 88 | Counted | Key ID, затем signature, затем raw document |

Header88bytes, document1..1024, signature8..72, key ID1..31, total98..1215bytes.
Лимиты равны существующим independent Motion verifier bounds; host compile
проверяет их static assertions. Это не HMI manifest16KiB contract. JSON и DER
не пересериализуются/не разбираются codec; key ID — counted raw bytes, не
скачанный SPKI/trust anchor. Его политику и подпись проверяет прежний verifier.

Codec требует exact length/version/reserved/bounds и equality header binding
с original local expected image. Encoder failure не меняет bytes, written0;
decoder failure очищает весь output. Spans/objects valid/exclusive/disjoint,
in-place не поддерживается. Нового parser/crypto/heap/callback/OS/ISR нет.

**Codec/transport не вычисляет и не проверяет ни bundle SHA, ни raw metadata
SHA/signature.** Эти поля остаются ожиданиями/привязкой, а не проверенным фактом.
Изменённые raw bytes могут структурно decode. После доставки прежний independent
P-256 verifier проверяет actual raw document, SHA полного image, policy/version/
target; handoff связывает actual raw metadata digest с original request.
Даже valid заново подписанный документ другого запроса отвергается binding gate.

## Владение и жизненный цикл

Объект RX zero-init один раз, один owner; не copy/move/reinit/edit live fields.
Caller не должен иметь изменяемые aliases/DMA/другой поток к буферам. Это не MPU.
Config требует valid DATA limits и max stream98..1215bytes, nonzero boot/
generation. Defaults, реальные boot epochs и физическое safe state не создаются.

1. `rx_begin` принимает original image binding и **другой свежий** metadata ID,
   резервирует metadata high-water. Busy/повтор ID/неверный boot/size отвергается.
   Внутренний DATA RX создаётся только для новой разрешённой metadata identity;
   separate image RX не меняется и его окно не перезапускается.
2. `rx_receive` использует прежние CRC/session/order проверки. PENDING копируется
   в fixed aggregate buffer перед consume/ACK. Exact duplicates получают cached
   ACK без повторного copy/progress. Non-ACK output полностью zeroed.
3. COMPLETE_UNVERIFIED означает доставку bytes, не signature/format/permission.
   `rx_take` проверяет container shape/binding и ровно один раз копирует **owned
   raw snapshot** со статусом TAKEN_UNVERIFIED, без borrowed pointers.
4. Aggregate buffer1215bytes очищается после take. Последний DATA block остаётся
   только для bounded final-ACK retries; повторный take не выдаёт document ещё раз.
   `discard` очищает и cache, явно прекращает metadata stream, сохраняет ID/time.
5. Caller snapshot остаётся прежним после discard/нового metadata stream. Его
   bytes не являются live receipt/install authority. Он должен жить exclusive
   до snapshot существующим handoff; дальнейшие independent/live checks обязательны.

Poll обязателен и без packets. Clock — monotonic uint32 ms modulo2^32, every gap
<2^31. Original DATA progress/total limits действуют также в COMPLETE/TAKEN,
граница elapsed>=limit — отказ. Duplicate/reject/cached ACK/take не продлевают их.
Context/backward-clock sticky; discard может очистить owned storage, но не
переоткрывает такой lifetime. Recoverable timeout/format/binding failure требует
явного discard и нового metadata ID; high-water не сбрасывается, wrap запрещён.
Извлечённая историческая копия bytes не отменяется магически при timeout: future
operation должна снова выполнить полный image/session/crypto/physical review.

## Программная интеграция

Новый host `--metadata-staging` проходит negotiation → **transported container** →
image DATA → real RAM lease/P-256/full SHA/model/clean close → historical outcome.
Исходные producer metadata/signature стираются после encode. Конфигурация
independent verifier указывает на доставленный snapshot, а не старые fixture
arrays. Local public-key allowlist и crypto owner/arena прежние. Raw test private
keys не сериализуются/не сохраняются.

Используется [один clock](STM32_UPDATE_TIMING.md) и bounded queues для всех phases.
Image RX уже открыт после negotiation: metadata delay расходует его прежние
started/progress windows; RX не переинициализируется после metadata. Проверены
оба направления current-role rejection.8 positive chains (6 release15 +2 legacy11),
loss/duplicates/whole-message retries и однократный post-wipe outcome.3 refusal
cases: invalid delivered signature, correctly re-signed wrong raw metadata digest,
expired separate image RX до acquire. Transport success их не обходит.

1024 document lengths/final ACK duplicates after take/snapshot after discard,
all truncations/capacities/header/binding/length faults, malformed completed RX,
CRC/role/ID/generation/wrap/clock/cancel, exact progress/total/ready deadlines и
100000 mutations проверены. Independent Python oracle сравнивает2 exact bundles,
min98/max1215bytes. Mutable opaque payload может decode — не integrity success.

Host50/50, ASan/UBSan/LeakSanitizer50/50 вне ptrace/default env/no suppressions,
standalone24/24. New separate ARM archive1568text/data0/bss0/frame48, guard4096/
frame128/exact8 exports/no unreviewed imports;6 archive guard methods. Old4
archives43362text/data8/bss27/frames984,376,328,64, wire1219/frame32 и fragments
1536/frame48 прежние. No full stack/MCU RAM/CPU/resource approval. Host RX1728,
snapshot1212bytes, ARM bounds<=2048/1280 не резервируют память F407.

Analyzer14 embedded+2 host/source178/old+new archive guards/synthetic link/
board/output/scope PASS. CI сохраняет exact dependency/action pins и8 SDK profiles,
добавляет metadata guard/negative cases/source analysis. Uploads text only;
новых local ESP builds нет. Legacy tests и runtime/dependencies/settings прежние.
DER signature test length меняется с ephemeral key/signature, поэтому frame/
container/timing totals signed chains записаны **по каждому запуску**, не golden
constant. Synthetic clock не измеряет CPU crypto/storage/bus latency.

## Оставшиеся gates

[Hardware facts/blockers](HARDWARE_STATUS.md) прежние: full marking/HSE/RAM/CCM/
IRQ/erase/protection/GPIO/CAN/backup recovery не подтверждены. Нет SWD/UART/reset/
Flash/backup rereads или новых аппаратных замеров. ESP32 ordinary8f0d899 прежний.
Production authenticated setup/descriptor discovery/global identity/routing,
immutable durable staging, measured worker/bus/crypto budgets, trust/context,
physical/operator/recovery/installer остаются будущими этапами. Container/TX ACK/
TAKEN_UNVERIFIED не COMMIT/jump/ARM/START. Откат — revert PR без записи устройств.
