# PR-07j — bounded update byte codec draft

База main `deace1c`, PR #1–41 слиты, main CI5/5 PASS. [План](plans/PR07J_UPDATE_WIRE_CODEC.md),
[evidence](evidence/build/update-wire-codec/README.md). Этап не подключается к устройствам.

## Граница

Отдельная opt-in C11 [библиотека](../protocol/can/update_wire.h) переводит existing
logical negotiation/DATA/outcome structs в canonical byte messages. Она не
сериализует memory layout, enum representation, padding или неиспользованный
DATA tail. Зависимости — прежний portable transport и libc memory helpers;
нет нового crypto/heap/OS/HAL/RTOS. Состояния, limits, CRC, retry/deadlines,
immutable lease/model/independent signature/cleanup алгоритмы прежние.

Это **экспериментальный формат для software review**, не утверждённый production
CAN ABI, arbitration ID, bitrate или frame fragmentation. Он вызывается только
host tests, отдельно компилируется ARM archive и не подключён в app_main/ISR/
board adapter/CAN. `tv4_boot_handoff` не линкуется с codec; только host test driver
явно включает его. Старый heartbeat4bytes/v1/500ms не меняется.

Значение version=1 ниже не version CAN protocol, SemVer HMI или numeric Motion counter.
Actual boot epochs/compatibility tag/key/context/production protocol mapping не
назначаются. Offset DATA остаётся индексом потока, не адресом Flash.

## Header и codebook

Каждый вызов получает **один полный** message, header8bytes:

| Offset | Bytes | Поле |
|---|---:|---|
| 0 | 2 | ASCII `TU`, отдельный discriminator draft |
| 2 | 1 | Experimental format version1, другие значения reject |
| 3 | 1 | Kind1..8 по таблице ниже |
| 4 | 2 | Точная длина body, unsigned LE16 |
| 6 | 2 | Reserved, только0,0 |

| Kind token | Сообщение | Body bytes | Total bytes |
|---|---|---:|---:|
| 1 | REQUEST | 122 | 130 |
| 2 | OFFER | 122 | 130 |
| 3 | ACCEPT | 122 | 130 |
| 4 | CONFIRM | 122 | 130 |
| 5 | DATA | 22+N, N1..256 | 30+N, max286 |
| 6 | DATA_ACK | 16 | 24 |
| 7 | OUTCOME | 78 | 86 |
| 8 | OUTCOME_ACK | 78 | 86 |

Unknown magic/version/kind/reserved, missing/extra bytes, mismatched declared/
actual body length отвергаются. Header не подразумевает stream resynchronization;
несколько сообщений подряд не принимаются как одно. Min total24, max286bytes.
Reassembly/fragments/transport CRC/authentication и маршрутизация — будущие слои.

Все integers unsigned LE; id = bootLE32 + transferLE32. Descriptor = id8 +
image_bytesLE32 + image_sha256 raw32, всего44bytes. Hashes/tags не переводятся
в hex и не считаются проверенными по наличию в пакете.

Negotiation body, offsets относительно body:

| Offset | Bytes | Поле |
|---|---:|---|
| 0 | 8 | requester request_id |
| 8 | 44 | DATA descriptor |
| 52 | 4 | responder_boot |
| 56 | 32 | raw metadata SHA |
| 88 | 32 | compatibility tag |
| 120 | 2 | block_bytesLE16 |

Kind задаёт logical REQUEST/OFFER/ACCEPT/CONFIRM, отдельно не дублируется в body.
Shared `tv4_neg_message_valid` — прежняя canonical shape проверка с NULL guard:
ненулевые request boot/counter/responder/image size, разные boot epochs,
block1..256; REQUEST DATA id0,0, остальные descriptor.boot=responder и counter>0.
Она не проверяет live peer/tag/state/freshness/max image/policy/time. Existing
negotiation API продолжает все эти проверки, включая generation/counters.

DATA body: id[0..7], sequenceLE32[8], offsetLE32[12], lengthLE16[16],
crc32cLE32[18], ровно N payload bytes[22]. Codec проверяет id/length и отсутствие
uint32 overflow offset+length; не проверяет sequence/образ/local size bounds.
CRC переносится **как есть**. Existing RX единственный проверяет CRC перед
progress/consumer, затем сравнивает exact duplicate semantics. ACK body: id8,
next_sequenceLE32 и next_offsetLE32; нулевые cursors структурно возможны, но
не продвигают existing TX без ожидаемого контекста и настоящего consumer ACK.

Outcome/ACK body: descriptor44, raw metadata SHA32, codeU8, reasonU8.
Code tokens1..5: CHECKED_CLOSED, REJECTED_CLOSED, CANCELLED_CLOSED,
FAILED_CLOSED, QUARANTINED. Reason tokens0..17: NONE, SIGNATURE, KEY, METADATA,
COMPATIBILITY, ROLLBACK, LENGTH, HASH, PROFILE, VECTORS, SOURCE, CONTEXT, CLOCK,
TIMEOUT, CANCEL, RELEASE, BINDING, INTERNAL. Таблицы явно связывают tokens с
named C enums, не копируют enum bytes. Пары проходят существующий
`tv4_update_outcome_valid`: invalid/unknown combinations reject. ACK отражает
полный semantic outcome, без установки или повторной доставки результата.

## API, buffers и ошибки

```c
tv4_update_wire_encode(message, bytes, capacity, &written);
tv4_update_wire_decode(bytes, exact_length, &message);
```

Один owner; valid **disjoint** objects/spans. Input живой, неизменный и exclusive
на время вызова. Нет in-place/overlap/ISR/threads/reentrancy contract или защиты
от DMA/hostile aliases. Caller обеспечивает настоящую длину выделенных spans.
Все loops/copies ограничены max286 и explicit shape; нет heap или globals state.

Encoder сначала проверяет shape/capacity, затем пишет bytes. Failure оставляет
output bytes неизменными, `*written=0`, если pointer ненулевой. Decoder при
любом failure очищает весь valid output object, `kind=NONE`; частичного result
нет. При NULL output pointer очистить нечего, возвращается ARGUMENT. Использовать
результат только при `TV4_WIRE_OK`. Trailing union/payload/padding decoded object
обнулены, но его C layout всё равно нельзя отправлять вместо encoder.

У codec нет clocks или контекста transfer. Future owner обязан poll state
machines регулярно даже без пакетов/при malformed traffic; decoder reject не
reset/abort/reinit и не обновляет deadlines/counters. DATA/negotiation/outcome
lifetimes, retry windows и quarantine не продлеваются сериализацией.

**Структурный decode OK не authentication/CRC/verified candidate/physical state.**
Matching forged handshake/outcome может пройти shape. Opaque digests/CRC и
CHECKED_CLOSED от peer не заменяют local signature/full SHA/vector/lease checks.
Report остаётся историей после закрытия/удаления RAM source; до будущего install
нужны fresh checks и operator/quiescent/recovery/erase gates. ARM/START/COMMIT/
jump команд, автоматической dispatch или действий на decoder success нет.

## Software-проверки и footprint

12 независимых Python golden vectors для8 kinds/all5 outcome codes, unaligned
byte spans, every truncated length/header/reserved/type/capacity, malformed body
fields, all256 DATA sizes/tails/overflow,65536 code/reason combinations и200000
deterministic mutations. Accepted mutated frames проверены на canonical byte
roundtrip, rejected output полностью zero. Большая часть opaque byte mutations
структурно допустима; это намеренно не тест authentication/integrity.

Новый `boot_wire_handoff` повторяет existing4 RAM integration groups через
encode/decode:8 полных signed chains (6 release15 +2 legacy11), все4 negotiation
kinds, DATA/final ACK/report loss, pending duplicates, stage copy-before-ACK,
real independent Mbed TLS signature/SHA/model/lease close и post-wipe outcome.
Crypto/binding/cleanup/revocation/deadline negatives остаются настоящими checks.
Private test keys только RAM, не files/signing service/production provisioning.

ARM codec1219text/data0/bss0, max individual frame32, caller message284bytes
на host. Отдельные regression ceilings4096text/data0/bss0/frame128 проверяются
на CI плюс6 negative archive/frame guard cases. Existing4 archives43362text/
data8/bss27 (**+18text** от shared shape/NULL guard), frames984/376/328/64 прежние;
codec archive учитывается отдельно. Это object sums, не final linked firmware,
full stack или approved RAM/CPU/throughput budget. Измерений на MCU нет.

## Следующие решения и откат

Production wire review, bounded fragment/reassembly с identity/deadline/backpressure,
metadata/signature transfer, boot identity/persistence/authentication/capabilities,
IDs/bitrate/transceivers/USB mux и runtime owner ещё нужны. Не выбирать реальные
пины или тайм-ауты CAN по host тестам. App-only ESP OTA и F407 CAN bootloader
сохраняют разные recovery/Flash requirements; адреса F407 не резервируются.

Owner SWD/core/raw ID/Flash1024KiB/3,252V/backup/vector facts прежние; full marking/
HSE/RAM/CCM/IRQ/erase/protection/GPIO/CAN/backup SHA/restore неизвестны. ESP32
ordinary8f0d899 не менялся. Factory2MiB/PROJECT_VER/UI/SDK8/dependencies/security
прежние. Нет device/SWD/UART/reset/Flash/backup reads, движения, публикации
releases/tags/keys/secrets или self-merge. Откат — revert PR без записи устройств.
