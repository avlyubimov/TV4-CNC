# PR-07o — shared mixed-codec channel и локальные фазы

База main `389de77`, PR #1–46 слиты. [План](plans/PR07O_UPDATE_CHANNEL.md),
[проверки](evidence/build/update-channel/README.md). Новая opt-in C11 library
`tv4_update_channel` поверх unchanged raw fragments/TU/MS codecs. Нет runtime
caller, driver, CAN IDs/bitrate/production version, Flash layout или устройств.

## Владение и очередь

TX/RX — отдельные zero-init one-owner объекты, без live copy/move/reinit,
ISR/threads/callbacks/heap/I/O. Входы/выходы valid, exclusive, disjoint; публичные
поля read-only. Один retained fragment counter на направление через все codec/
phase transitions. Clock/generation/quota/exhaustion нижнего слоя сохраняются.

TX begin кодирует snapshot в bounded scratch286 и копирует в owned raw TX,
после чего scratch стирается. Peek заполняет owned one-frame outbox: нижний
fragment cursor commit происходит ровно один раз после принятия этим локальным
слотом. Повторный peek возвращает тот же кадр, не queue progress. Outer exact
commit допускается только после принятия кадра следующей **локальной** очередью;
проверяются length и все transmitted bytes. Stale/wrong commit не освобождает
новый кадр. No remote delivery/ACK или queue flush claim.

Последний кадр всё ещё может быть queued, когда lower fragment TX уже IDLE.
Поэтому собственные progress/total окна начинаются при TX begin и действуют до
освобождения последнего outbox кадра. Queue progress только exact outer commit,
не peek/reject/BUSY. Используются те же explicit fragment timeout inputs, без
новых defaults или silent extensions. Poll без traffic обязателен. Любой gap
между uint32 millisecond observations <2^31; clock/context/exhaustion sticky.

RX применяет original bounded reassembly, затем выбирает **единственный** codec
по prefix `TU`/`MS`, exact length/version/reserved/shape через прежний decoder.
Нет fallback на другой decoder при ошибке. Scratch стирается, decode failure
zeroes entire output и требует explicit recoverable clear. Правильное сообщение
не своей фазы/ID потребляется без dispatch, output zero; high-water не откатывается.
Успех возвращает owned `TAKEN_UNVERIFIED` union; original upper session/CRC/
independent signature/raw digest/full image/live lease/model/deadline gates нужны
после этого. Channel не вызывает обработчиков и не проверяет криптографию.

## Локальная traffic policy

| Фаза | Разрешено | Дополнительный фильтр |
|---|---|---|
| NEGOTIATION | TU REQUEST/OFFER/ACCEPT/CONFIRM | Upper coordinator проверяет actual boot/request/context |
| SETUP | MS REQUEST/OFFER/ACCEPT/CONFIRM | Upper setup проверяет actual image/pool/context |
| METADATA | TU DATA/DATA_ACK | Exact explicit metadata ID |
| IMAGE | TU DATA/DATA_ACK | Exact image ID; same boot, metadata=image+1 |
| OUTCOME | TU OUTCOME/OUTCOME_ACK | Bound image ID; full binding проверяет original upper API |

Advance строго sequential и только при local TX active=false/outbox empty/IDLE
или RX IDLE, включая запрет при incomplete/READY aggregate. Нельзя сменить
policy и затем выдать buffered message другой фазе. OUTCOME может explicitly
recycle в NEGOTIATION: fragment counters/observed time не reset, role IDs очищаются.
Cancel/clear не меняют phase/role IDs/counters/time; lower sticky failures не
обходятся. Не clear внутренние fragments напрямую и не создавать новый объект
ради старых IDs. TX/RX policy обоих peers согласует будущий внешний owner.

Это **локальный traffic filter**, не доказательство завершения верхних протоколов.
Advance не принимает upper success boolean и не выдаёт install/live receipt.
Caller обязан отдельно получить actual negotiation/setup/metadata/image/outcome
states, clean lease close и соблюдать original deadlines. External driver queues
могут содержать кадры после local commit: их lifetime/drain нельзя доказать этой
библиотекой. Production owner/queue-flush/routing/authenticated capability/epoch
provider ещё требуют review. Phase/route/generation не сериализованы; matching
forged traffic и old frames после reboot не стали authenticated/replay-safe.

## Shared-clock signed fixture

Новый HOST mode `--channel-staging` использует **один** link, retained TX/RX
counters и frame queues через original image negotiation, MS setup, metadata,
image DATA и outcome. Before phase switch проверяются реальные upper states и
пустые fixture queues. Raw document/signature/key ID действительно передаются,
producer arrays wiped, independent verifier читает delivered owned snapshot.
Один f.now для queue/fragments/all upper APIs/lease/crypto callbacks; image RX
начат до setup/metadata, его started/progress не reset/extend.

HOST statistics slots TU1..8/MS9..12 и mask0x1e00 не добавляют wire tokens/IDs.
Проверены потери всех12 видов, queue hold/duplicate/backpressure/recovery;
старые typed/byte/fragment/timed/metadata/setup режимы сохраняются.8 positive
signed chains (6 release15 +2 legacy11),3 independent refusals: bad signature,
correctly re-signed wrong raw request digest, expired original image RX.
CPU crypto duration не тарифицирована, synthetic ms/frame counts не physical
throughput/latency/scheduler или production timing defaults. Test private keys RAM only.

## Приёмка и открытые границы

Unit:512 full phase cycles/7168 mixed messages, retained counters/wrap clock,
exact outbox commit/BUSY/single take, forbidden phase/pair IDs/late MS/old metadata
ACK, unknown prefix/bad version/reserved/no fallback, final outbox progress/total
expiry, cancel/recovery, sticky clock/context/exhaustion. Existing independent
TU/MS oracles и known generation/reboot replay blocker test сохраняются.
Host56/sanitizers56/standalone30, strict actual-source analyzer16 embedded+2 host,
5 ARM archive guards ×6 methods, pinned crypto178files/synthetic link/board/output/
scope gates PASS. Separate channel1940text/data0/bss0/frame56,4096text/frame128
ceiling. Old4 libraries43600text/data8/bss27; separate wire1219/fragments1904/
metadata1568/setup3324 unchanged. Old budgets не расширены, crypto owner/workspace/
16384arena/dependency pins/SDK matrix8/HMI/board/config прежние.

Host storage TX696/RX664/snapshot288 — не MCU RAM reserve/full stack approval.
Production scheduler/CPU/memory measurements/runtime/staging medium/durable
identity/routing/queue drain/authentication/installer остаются open. Owner F407
SWD/core/Flash1024KiB/backup facts прежние, actual package/RAM/CCM/HSE/vector/
erase/protection/GPIO/CAN/physical inhibit/recovery неизвестны. Flash1MiB не
определяет RAM/bootloader sectors. Устройства/SWD/UART/reset/Flash/CAN/приводы
не трогались, адресов/пинов не назначено, self-merge/release нет.
