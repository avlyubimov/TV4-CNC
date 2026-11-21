# PR-07p — bound TX egress и билеты завершения драйвера

База main `555007d`, PR #1–47 слиты. [План](plans/PR07P_UPDATE_EGRESS.md),
[проверки](evidence/build/update-egress/README.md). Optional C11 library
`tv4_update_egress` из frame_queue.c/update_egress.c. Это portable local ownership
contract, не CAN driver/runtime/remote ACK/authentication/physical/install proof.
Прежние каналы/кодеки/DATA/crypto/lease/model/deadlines не меняются.

## Собственная очередь и driver claim

`tv4_frame_queue` хранит до4 frame copies; явная capacity1..4 и residency_ms
1..INT32_MAX, route/generation ненулевые, last_issued — внешний trusted local input.
Ceiling4 не peripheral FIFO depth или утверждённый RAM reserve. Frame length5..8,
unused bytes zero; queue не парсит fragment/codec/CRC. Ticket содержит original
queue address/route/local generation64/serial32, только в памяти, не wire bytes
или secret capability. Ticket/driver snapshots не должны переживать сам owner;
queue адрес стабилен на lifetime, никаких live copy/move/reinit.
Serial не переиспользуется/не wrap, BUSY/invalid input не расходует номер.

Admit копирует кадр **до** channel commit. FIFO claim выдаёт owned snapshot
frame/route/ticket, один outstanding driver claim. Очередь сохраняет свой record,
драйвер — собственную копию, никаких DMA pointers в ledger. Claim не completion
и не продлевает residency от исходного admit. Будущий driver adapter обязан
сообщить правдивый terminal result с точным ticket, worker обрабатывает его в
current owner time, не старом timestamp из callback snapshot. ISR/thread вызовы
этих API запрещены; будущая очередь completion events ещё требует реализации.

| Driver result | Изменение ledger | Что это не доказывает |
|---|---|---|
| SENT_LOCAL | Exact claimed head освобождён, LOCAL_COMPLETE_UNVERIFIED | Remote logical ACK/CRC/crypto или физическая readiness |
| FAILED_LOCAL/CANCELLED_LOCAL | Claim возвращён, очередь stopped, результат RECLAIMED_FAILED | Возможность продолжить старый upper update |
| OWNERSHIP_UNCERTAIN | Claim остаётся held, stopped; последующие exact terminal reports ещё принимаются | Окончание владения/возможность force-reset |
| Wrong queue address/route/generation/serial, duplicate/unclaimed ticket | STALE, actual claim не меняется | Подтверждение текущего кадра |

Poll обязателен и без traffic. Clock uint32 monotonic ms modulo2^32, каждый gap
<2^31. Oldest admission residency ограничивает ожидание в очереди **и** драйвере;
claim/duplicate/BUSY не reset срок. Timeout останавливает очередь, сохраняет claim.
Cancel стирает только unclaimed records; held head нельзя clear/перезаписать.
Exact terminal return после timeout/context/clock fault может reclaim ресурс,
но возвращает RECLAIMED_FAILED, не оживляет состояние. First error сохраняется,
sticky context/clock/exhaustion имеют приоритет. Invalid/stale events не заменяют
обязанность owner регулярно poll.

Clear возможен лишь stopped + count0/flight0 после реального correlated return;
serial/generation/route/observed clock сохраняются. CLOCK/CONTEXT/EXHAUSTED sticky.
Нет force-forget/flush-success boolean или reinit ради старых ticket/IDs. Если
драйвер никогда не возвращает владение, owner остаётся blocked; будущая reviewed
процедура reset/reconciliation должна дать отдельное аппаратное доказательство.
Volatile ledger не обеспечивает recovery после reboot/power loss.

## Связанный egress owner

`tv4_update_egress` владеет **своими фактическими** channel TX и custody queue.
Не принимает постороннюю empty queue или caller success/drained boolean. Init
проверяет оба limits sets до настройки; объекты zero-init once, one owner,
valid/disjoint/exclusive, без live copy/move/reinit/ISR/threads/I/O/callbacks/heap.
Внутренние объекты только для inspection, менять через inner API запрещено.

Begin ждёт завершения предыдущих custody records. Pump за вызов переносит один
кадр из channel outbox в собственный ledger, затем выполняет exact channel commit.
Claim/complete независимы от pump; при full queue bounded BUSY/backpressure, без
silent counter/time reset. Даже если channel TX уже IDLE, pending driver record
блокирует следующий begin и phase advance. Advance проверяет actual bound queue
quiescent и original channel locally idle; прежние sequential phases/paired IDs
сохраняются. RX channel остаётся отдельным owner, upper state и input queues
не считаются автоматически завершёнными от результата TX ledger.

Cancel останавливает channel/очередь, не теряя driver claim. Clear после terminal
return чистит recoverable lower state и retains fragment IDs/tickets/phase/time;
не вызывает upper init/begin/abort и не продлевает image windows. Cleanup работает
и после local context/clock fault, но не разрешает clear sticky lifetime.
Старые channel/fragments/codec APIs и standalone modes сохранены.

## Сквозные software проверки

HOST mode `--egress-staging`: один mixed TU/MS link через original image
negotiation/setup/metadata/image/outcome, явные2 custody slots/residency16ms на
направление, simulated driver delay1/3ms и duplicates. Все эти числа — только
fixture inputs. Каждый local report соответствует actual claimed ticket; даже
при намеренно потерянном кадре SENT_LOCAL не заменяет remote logical retry/ACK.
Before phase switch actual upper states/lease close + actual bound ledger и
fixture driver copies проверены quiescent. Общий f.now, original image RX
started/progress не reset; producer metadata/signature wiped, verifier читает
реально доставленный owned snapshot. Crypto CPU duration не тарифицирована.

8 positive chains (6 release15 +2 legacy11),3 existing independent refusals
(signature/re-signed wrong raw request digest/expired image RX), ещё2 driver
lifetime refusals: uncertain/missing completion удерживают claim до original
image timeout. Late return и software queue clear не оживляют RX/admission.
Queue unit: capacities1..4,4096 FIFO cycles/10240 frames, out-of-order/stale/
duplicate/cross-queue ticket, DLC256, timeouts/cancel/exhaustion/no wrap. Egress unit связывает
real channel→queue→claim→return→RX; final held driver frame блокирует phase при
channel IDLE; sticky faults/uncertainty/explicit recovery проверены. Test keys RAM only.

Host59/sanitizers59/standalone33, analyzer18 embedded+2 host,6 ARM archive guards×6
methods/source178files/synthetic link/board/output/scope PASS. New separate
archive1972text/data0/bss0/max individual frame88, ceiling4096/frame128.17 exact
exports; internally resolved queue refs не external imports. Existing old4
43600text/data8/bss27 и wire1219/fragments1904/metadata1568/setup3324/channel1940
unchanged; old ceilings/crypto owner/workspace/16384arena/pins/SDK matrix8 прежние.
Host ledger128/driver snapshot40/egress owner832 bytes не MCU RAM/full stack approval.

Подтверждённые owner F407 SWD/core/Flash1024KiB/backup факты прежние; actual package/
RAM/CCM/HSE/full vector mapping/erase/protection/GPIO/CAN/physical inhibit/restore неизвестны.
Frame completion truth/correlation/reset rules требуют реального trusted adapter;
remote delivery/routing/authenticated identity/capabilities/boot persistence,
worker timing/resources/staging medium/runtime/installer/operator/recovery gates
открыты. Software simulation не закрывает эти gates. Flash1MiB не задаёт RAM или
адреса loader. Никаких новых wire bytes/IDs/bitrate, реального драйвера, startup/
Flash map/pins/устройств/SWD/UART/reset/Flash/CAN/motion/self-merge/release.
