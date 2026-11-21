# PR-07k — bounded fragment/reassembly draft

База main `4db99df`, PR #1–42 слиты, main CI5/5 PASS.
[План](plans/PR-07K-update-fragments.md), [evidence](evidence/build/update-fragments/README.md).
Это программный этап без обращения к устройствам и самостоятельного merge.

## Граница и инженерные предположения

Отдельная opt-in C11 [библиотека](../protocol/can/update_fragments.h) разбивает
один [byte message PR-07j](STM32_UPDATE_WIRE.md) и собирает его обратно.
Нет heap, изменяемых globals, I/O, callback, ISR, threads, driver или runtime
caller в HMI/Motion/bootloader. Прежние logical negotiation/DATA/outcome APIs,
CRC, independent P-256/SHA, lease/model/cleanup и deadlines не изменены.

Classic CAN/TWAI допускает до8 payload bytes; это ограничение описано в
[официальной ESP-IDF5.3.2 TWAI documentation](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/peripherals/twai.html).
Выбранный ниже envelope — **наш experimental draft**, а не CAN standard,
утверждённый production protocol или доказательство работы трансивера.
Реальные CAN IDs, bitrate, frame flags, mux/pins, bus timing и routing не выбраны.
Старый инертный heartbeat4bytes/v1/500ms не меняется.

`version=1` выбирает local envelope contract, а не production CAN SemVer.
Она передаётся в config, не имеет отдельного байта в frame. Inner `TU` header
сохраняет собственный draft/version discriminator. Local nonzero `route` и
`generation` — доверенные входы owner, **не передаются и не аутентифицируются**.
Провайдер boot identity, queue flushing, mapping направлений и защита от replay
после reboot ещё отсутствуют. Локальная смена generation сама по себе не
отличает старый frame: новый объект может принять его с тем же ID/bytes.
Matching forged frame/большой counter может пройти shape и нарушить доступность.
Это не authentication/anti-replay/DoS protection и не готовый CAN transport.

## Envelope v1

Frame здесь — только `bytes[8]` + фактическая `length`, без C ABI на проводе.
Payload имеет4-byte header и1..4 bytes chunk; trailing bytes не передаются.

| Offset | Bytes | Поле |
|---|---:|---|
| 0 | 2 | Nonzero message ID, LE16 |
| 2 | 2 | START bit15 + total byte length; иначе byte offset, LE16 |
| 4 | 1..4 | Chunk точных inner bytes |

START: bit15=1, low15bits=полный размер8..286bytes, DLC строго8. Первые4
inner bytes идут сразу в этом frame. Это только envelope admission, не decode.
Continuation: bit15=0, offset>0, кратен4, меньше total; DLC строго
`4 + min(4, total-offset)`. Padding/extra bytes и offset0 не принимаются.
Inner codec в итоге допускает только свои точные размеры/содержимое.
Максимум286bytes требует72frames, последний offset284/DLC6. ACK24bytes
занимает6frames. Envelope не вычисляет новый CRC/MAC; прежний DATA RX
проверяет переданный CRC до progress/consumer ACK, затем independent verifier
проверяет полные signature/image SHA. Случайно изменённые opaque bytes могут
собраться и decode успешно — это проверено, не выдано за integrity proof.

## Ownership, counters и память

Один owner, zero-init ровно один раз; живые TX/RX нельзя копировать, перемещать,
реинициализировать или менять их поля. Объекты и входные/выходные spans должны
быть valid/exclusive/disjoint. Прямые aliases/DMA/другой поток нарушают контракт;
API не заменяет MPU. Он не thread/ISR-safe, не содержит blocking waits.

TX владеет encoded snapshot, RX — единственным сборочным буфером286bytes.
Config, route, generation и last counter задаёт caller, defaults нет.
TX выдаёт IDs1..65535 без wrap. Резервирует ID только после успешного encode и
проверки необходимого количества frames. RX резервирует high-water при START
до копирования bytes. Cancel/failure/clear/completion сохраняют high-water/
last-issued и observed clock. Init живого объекта отклоняется. После65535
новый TX begin отказывает sticky EXHAUSTED; clear не разрешает переиспользовать ID.
RX с high-water65535 больше не принимает START в этом lifetime.

Host storage: TX360/RX352bytes. ARM compile-time bound каждого<=384bytes;
это не подтверждённая карта RAM/CCM F407 или допустимый runtime reserve.
Wipe очищает весь внутренний buffer при завершении/cancel/failure, но не
обещает forensic erasure копий компилятора/CPU/внешних aliases. Полный image
по-прежнему хранится в отдельно переданном staging region; Flash1MiB не RAM1MiB.

## TX: local backpressure

1. Init с explicit config/route/generation/last-issued/clock.
2. `begin` кодирует caller message в owned buffer, резервирует новый ID и
   переходит в SENDING. После begin caller source больше не нужен.
3. `peek` возвращает deterministic текущий frame, не двигает cursor/time.
   Если будущая local queue заполнена, можно повторить peek до deadline.
4. `commit` допустим только после peek и для точно того же DLC/bytes, после
   принятия local queue. Он двигает cursor и progress. Stale commit отвергается.
5. Последний commit очищает bytes и возвращает IDLE, сохраняя counter.

Local commit **не доказывает remote delivery/ACK**. Library не делает
автоматических отправок или неограниченных retries. Если continuation потерян,
новый START не заменяет активный RX: caller ждёт bounded timeout/cancel,
явно clear и только затем начинает whole-message retry с новым ID. Согласование
этих действий с прежними logical retry/deadline — задача future owner, а не
разрешение молча продлить DATA lifetime. Upper peer ACK/outcome также не
являются install/motion authority.

## RX: порядок, duplicates и single delivery

IDLE принимает только корректный START с ID>high-water. В RECEIVING другой ID
возвращает BUSY и не заменяет текущий message/high-water. Только следующий
offset добавляет bytes. Пропуск/неверный DLC/диапазон отвергается без progress.
Уже скопированный exact chunk/START — DUPLICATE без изменения deadlines;
conflicting duplicate — sticky failure и wipe. Для readable same-ID frames
есть `max_frames` quota, включая повторы и отвергнутые offsets. Frames с
нечитаемым ID/invalid DLC или другого route/ID не расходуют эту quota; обработка
одного вызова ограничена, а future worker/queue должен ограничить общий load.

После последнего chunk состояние **READY_BYTES_UNCHECKED**. Новые frames дают
BUSY, bytes/attempt quota не меняются: это bounded backpressure, не успех проверки.
`take` ровно один раз вызывает exact inner codec decode, копирует typed message
caller и очищает RX. Failure полностью zeroes output; malformed inner приводит
к CODEC failure/wipe. После take дубликаты старого ID stale. Никакой автоматической
передачи в handlers/dispatch, установки, COMMIT/jump/ARM/START нет. Caller
обязан вызвать прежние state/session/CRC/signature/lease/time guards.

## Clock, cancellation и recovery

Clock — monotonic uint32 milliseconds modulo2^32, gap между наблюдениями<2^31.
Config требует `0 < progress <= total <= INT32_MAX`, nonzero frame quota и
version1. Лимиты100/500ms и160frames существуют только в host fixtures,
не production defaults или измеренный bus timing.

Owner обязан вызывать poll и без входных frames. Context mismatch/backward
clock — sticky failure/wipe; clear не меняет lifetime и не снимает CLOCK/
CONTEXT/EXHAUSTED. Recoverable timeout/conflict/quota/codec failure и cancellation
можно явно clear в IDLE, сохранив counter и наблюдённое время. Clear активного
сообщения не является reset/restart. Граница `elapsed >= timeout` — отказ.
Progress deadline действует в SENDING/RECEIVING; total deadline действует
также в READY, ограничивая ожидание take. Дубликат, rejected frame, peek/BUSY
не продлевают ни один deadline. Поздний фрагмент не скрывает пропущенный срок.

## Программная приёмка и границы доказательств

Host45/45, ASan/UBSan/LeakSanitizer45/45 без options/suppressions вне ptrace,
standalone19/19. All8 kinds/all256 DATA lengths, DLC256/START32768/continuation65536
переборы,100000 drop/mutation/DLC/duplicate chains. Независимый Python struct
oracle проверяет3 полных frame sequences, включая max packet/partial DLC.

Новый host-only `--fragment-staging` прогоняет прежние4 integration groups:
8 actual signed RAM chains (6 release15 +2 legacy11), crypto/binding/lease/model/
cleanup/quarantine/revocation/deadline negatives.22980frames/1252 exact duplicates,
local queue re-peek и counters сквозь все8 kinds. Старые typed/byte modes сохранены.
Raw signed metadata/signature подаются прежней fixture напрямую — **их сетевой
transport этим PR не реализован**. Независимый fragment test проверяет реальную
потерю frame, невозможность silent restart, timeout/clear/fresh-ID retry.
Fixture transport clock не измеряет bus latency и не расходует logical time;
runtime owner ещё должен согласовать нагрузку/latency с прежними сроками.

ARM separate archive1536text/data0/bss0/frame48; guard4096text/frame128, exact13
exports и только memory helpers +2 codec imports,6 positive/negative guard methods.
Прежние4 archives43362text/data8/bss27/frames984,376,328,64 и codec1219/frame32
не изменились. Это object sums/individual compiler frames, не linked firmware,
full stack, CPU/throughput/latency/soak или hardware resource approval.
Analyzer13 embedded+1 host/source178/board/output/scope/synthetic link PASS.
CI сохраняет8 exact-pinned ESP SDK profiles и text-only artifacts, добавляет
новый archive guard, negative cases и source analysis. SDK configs/partitions/
HMI/PROJECT_VER/security/dependency versions не менялись.

Аппаратные owner facts и blockers — [HARDWARE_STATUS](HARDWARE_STATUS.md).
Нет SWD/UART/reset/Flash/backup rereads, устройства не прошивались. Full physical
marking/HSE/RAM/CCM/IRQ/erase/protection/GPIO/CAN/recovery остаются открытыми.
Future CAN bootloader/ESP32 OTA требуют approved production contract, metadata
transport, identity/authentication, durable source, exact-image operator approval,
physical inhibit и recovery. Адреса/сектора Flash не резервируются предположением.
Откат — revert PR без записи устройств. PR оставляется владельцу на review.
