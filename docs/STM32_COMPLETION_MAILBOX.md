# PR-07r — атомарная SPSC очередь завершений

База main `2c6fd1c`, PR #1–49 слиты. [План](plans/PR07R_COMPLETION_MAILBOX.md),
[проверки](evidence/build/completion-mailbox/README.md). Optional C11 archive
`tv4_completion_mailbox` продолжает [serialized event owner](STM32_DRIVER_EVENTS.md).
Нет runtime caller, реального CAN/RTOS/ISR adapter, Flash/pin/wire defaults.

## Два участника и публикация

`tv4_completion_mailbox` допускает ровно одного producer (`post`) и одного
consumer (`take/empty`). Не MPSC/MPMC, не разрешение на вложенные callbacks или
применение в MCU ISR. Фиксированная capacity1..4; storage5 записей, одна позиция
всегда свободна для различения full/empty. Ring indexes ограничены0..capacity,
поэтому capacity3 также корректна при многократном wrap; нет переполнения общего
uint32 sequence counter. Capacity не hardware FIFO depth/RTOS memory reserve.

Инициализация завершается до запуска обоих actors; её результат публикуется
внешним happens-before механизмом. Адрес/lifetime стабильны, stop/join обоих до
destruction. Нет live copy/move/reinit/reset/flush или прямого доступа к полям
во время работы. Valid/disjoint spans; input event принадлежит producer, output
snapshot — consumer. Queue binding (actual queue pointer/route/generation) immutable.
Producer не читает mutable protocol state и не вызывает его APIs/crypto/clock.

Producer пишет полный event slot, затем release-store tail. Consumer выполняет
acquire-load tail, копирует и стирает slot, затем release-store head. Producer
проверяет head с acquire перед reuse; собственный index читается relaxed. Так
publication и завершение чтения охватывают неатомарные owned copies в slot.
Контракт memory ordering — [C11 draft N1570, §5.1.2.4/7.17](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf);
примечания backend — [GCC atomic operations](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html).
Документы не подтверждают ISR/cache/DMA/RTOS конкретной платы.

Compile-time требуется32-bit unsigned с ATOMIC_INT_LOCK_FREE==2. Pinned ARM
archive дополнительно проверяет imports без atomic/lock helpers и5 native DMB
инструкций. Это compiler evidence, не измерение latency или подтверждение
аппаратной интеграции. Firmware APIs bounded, без spin loops/heap/I/O/locks.
Busy loops/sched_yield/pthread существуют только в HOST tests.

`post` валидирует immutable owner/route/generation/serial/result, копирует event.
ADMITTED означает сохранённый local report, не возврат frame custody/remote ACK.
Full → BUSY без overwrite; producer держит report для retry. Wrong binding/enum
не меняют очередь. `take` выдаёт owned report; non-FRAME output zero. Никаких
timestamp/report truth/authentication проверок здесь нет. Передаваемые pointers
не сериализовать/логировать и не использовать после destruction owner.

## Связанный consumer owner

`tv4_completion_owner` владеет actual serialized events/egress, mailbox и pending
copy. Только `owner_post` принадлежит producer; остальные APIs — одному consumer.
Все inner mutable APIs запрещены. Init настраивает внутренний одноэлементный
handoff inbox и explicit mailbox capacity; inner slot — деталь software handoff,
не размер аппаратной очереди. Конфигурация завершена до publication actors.

Service сначала polls original owner при текущих generation/time; за вызов
обрабатывает <=1 report. Consumer забирает mailbox copy в собственный pending,
сохраняет её до admission во внутренний inbox и затем вызывает original service.
Нет потери report при backpressure. Отдельный poll consumes ничего. Старые
independent crypto/lease/model/image/close windows и exact ticket checks прежние.

Begin/claim/phase advance проверяют actual mailbox/pending, затем actual inner
owner/ledger. Cancel не стирает mailbox/pending/claim; clear ждёт их обработки и
original terminal cleanup, сохраняет atomic indexes/IDs/phase/time. Context/clock/
exhaustion sticky. Late return reclaim не success/upper resume. Force-forget нет.

Empty — consumer observation, не обещание, что producer закончил работу. Поздний
duplicate ещё может прийти после наблюдения empty/смены фазы/clear. Exact stable
owner + retained generation/serial не позволяют ему освободить новый claim.
Невозвращённый actual terminal держит original ledger, даже если mailbox пуст.
Нельзя считать snapshot empty physical drain или разрешением установки/движения.

## Проверки

- Unit: capacities1..4,16384 FIFO cycles/40960 owned reports, full/retry, owned
  copy wipe, capacity3 wrap, foreign/invalid binding/result, no ledger mutation.
- Два реальных pthread actors:1200000 последовательных reports, FIFO/contents
  проверены; retries зависят от scheduler и не являются golden statistic.
- Bound concurrent owner:256 actual channel/RX messages,8448 frame claims,
  9504 reports/1056 stale duplicates, capacities1..4/clock wrap. Проверены actual
  mailbox phase guard при inner IDLE/ledger empty, late retry/stale после clear,
  timeout/uncertain/cancel/sticky cleanup и retained atomic positions/counters.
- HOST `--completion-mailbox-staging`:8 signed positives (6 release15+2 legacy11),
  3 independent signature/raw digest/expired image refusals и2 delayed queued
  terminal/uncertain refusals. Producer buffers wiped; real delivered snapshot
  проходит original verifier/lease/model/clean close, image windows не reset.

Новый signed mode использует те же explicit inputs PR-07q:2 frame slots/
residency16ms,2 mailbox slots,1ms service delay, duplicate report every17 frames,
fragment progress8ms/total256ms/frame160. Старые modes и все upper limits прежние;
числа не production defaults. Host link сам serialized; concurrent handoff отдельно
проверяется pthread/ThreadSanitizer, не выдаётся за физический CAN timing.

Host65/sanitizers65/standalone39; ThreadSanitizer2/2 (default env/no suppressions),
CI включает race checks в обычные host jobs. Analyzer20 embedded+2 host;8 guards:
7×6 old methods +7 new mailbox cases (включая missing native atomic barriers).
New archive1038text/data0/bss0/max individual frame64, ceiling4096/frame128,
14 exports. Source178/synthetic link/board/output/scope PASS; старые code/ceilings/
SDK pins/crypto owner/workspace/arena прежние. Host mailbox200/owner1208 bytes
не MCU RAM/full stack approval. Target timing/CPU/ISR/RTOS/cache/DMA/soak не измерены.

Owner F407 SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3.252V/
backup1048576bytes/structural vectors PASS прежние, новых аппаратных наблюдений нет.
Открыты actual target adapter/ISR/RTOS/cache/DMA/completion truth/reset/reconciliation,
persistent authenticated identity/routing/resources/staging/runtime/installer,
operator/physical/recovery и full marking/RAM/HSE/vector/erase/protection/GPIO/CAN.
Flash1MiB не определяет RAM или bootloader addresses. Устройств/SWD/UART/reset/
Flash/CAN/motion не касались; самостоятельного merge/релиза нет.
