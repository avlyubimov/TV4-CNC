# PR-07q — owned driver completion event inbox

База main `efa2727`, PR #1–48 слиты. [План](plans/PR07Q_DRIVER_EVENTS.md),
[проверки](evidence/build/driver-events/README.md). Opt-in C11 archive
`tv4_update_driver_events`, без runtime/RTOS/ISR/driver или устройств.
Продолжает [egress custody](STM32_UPDATE_EGRESS.md), не меняя прежние APIs,
wire bytes/IDs/bitrate, independent crypto/lease/model/close и image deadlines.

## Владение и сериализация

`tv4_driver_events` содержит собственные actual egress и FIFO до4 event copies.
Явная event capacity1..4, отдельная от frame custody capacity/residency. Ceiling4
не размер peripheral FIFO, RTOS reserve или измеренный RAM budget. Один
serialized/exclusive owner, zero-init once, stable address, valid/disjoint spans;
без live copy/move/reinit/heap/I/O/threads/ISR/callback calls. Внутренние mutable
APIs не вызывать; поля доступны для inspection. Будущий RTOS adapter обязан
предоставить синхронизацию и копирование между настоящими execution contexts.

Event — exact local queue owner/route/generation/serial ticket + driver result,
без timestamp/данных образа. Owner pointer не сериализовать и не логировать.
Tickets/events не переживают actual owner. Local report truth/provenance остаётся
обязательством реального trusted adapter, не capability/authentication proof.

`post` проверяет bound owner/route/generation, ненулевой serial и enum. Wrong
owner/context → STALE, invalid enum → ARGUMENT, inbox не меняется. Valid-shaped
stale serial/duplicate допускается в FIFO, затем проходит original exact-claim
check при service; не освобождает другой кадр. Нет dedup, перезаписи или silent
drop: uncertain и terminal reports одного ticket могут иметь разный смысл.

ADMITTED означает только сохранённую owned event copy. Caller может стереть
свою копию отчёта после admission, но UNCERTAIN не завершает владение actual
driver frame. При BUSY adapter сохраняет event и повторяет post после появления
места. Успешный post ещё не изменяет ledger flight и не разрешает phase/clear.
Posting не observes clock; регулярный poll/service обязателен независимо от traffic.

## Ограниченная обработка и ошибки

`service(generation, now)` сначала polls original egress при **текущем времени
owner**, затем consumes не более одного FIFO event. Timestamp от reporter не
принимается. `poll` observes те же original deadlines без consumption, когда
event work budget исчерпан. Service не вызывает driver/worker/UI/crypto и не
продлевает residency или original image progress/total windows.

Original exact completion API обрабатывает report, затем удаляется только
обработанная event copy. SENT_LOCAL возвращает LOCAL_COMPLETE_UNVERIFIED только
при живом local egress; after expiry/cancel/fault terminal return → RECLAIMED_FAILED.
UNCERTAIN сохраняет original claim; STALE ничего не освобождает. Error/sticky
state сохраняется независимо от return STALE/HELD; последующий poll/service
остаётся обязательным. Cleanup exact terminal доступен и после context/clock fault.

Begin/claim/phase advance ждут **actual bound inbox empty**, затем проходят все
original egress guards. Даже stale/duplicate events после освобождения claim не
считаются drained по caller boolean. Pump bounded one frame, может заполнять
original custody slots; он не consumes completion events и не вызывает transmit.
Cancel сохраняет весь inbox и driver claim; original egress стирает лишь unclaimed
frame records. Clear запрещён до фактического consumption inbox и original exact
claim return; сохраняет fragment/ticket counters, phase/context/clock. CLOCK/
CONTEXT/EXHAUSTED sticky. Нет flush/drop-all/force-forget/reinit recovery API.

Отсутствующий terminal report по-прежнему блокирует reuse. Volatile inbox не
обеспечивает reboot/power-loss recovery. Actual hardware reset/reconciliation и
провенанс завершения требуют отдельного reviewed adapter contract; совпавший
ticket не превращает произвольный caller report в аппаратное доказательство.

## Проверки и границы

Unit path: 256 actual channel/RX messages,8448 frames/21120 reports, capacities1..4,
FIFO/clock wrap, owned copy wipe, full BUSY/external retained report retry,
one-event service, duplicate/stale/foreign tickets, inbox phase guards, delayed
terminal/current-time expiry, uncertain/cancel/sticky cleanup.

HOST `--driver-events-staging`: original signed setup/metadata/image/outcome path,
2 frame slots/residency16ms,2 event slots/service delay1ms, duplicate local reports
every17 claimed frames. Это explicit fixture inputs, не production defaults.
Новый mode задаёт fragment total256ms до init вместо128ms старого mode, чтобы
учесть дополнительный serialized event hop. Progress8ms/frames160 и все original
upper negotiation/setup/image/crypto/lease/model/close windows прежние, не меняются
после begin. Все старые modes сохраняют128ms и старое поведение.

8 signed positives (6 release15+2 legacy11),3 independent signature/raw digest/
expired image refusals;2 new delayed queued terminal/uncertain event refusals.
Producer buffers wiped, actual delivered snapshot проверяется original verifier.
Original image timeout продолжает истекать, пока event ждёт service; позднее
consumption/clear не оживляет RX/admission. Local terminal reports/frames counted
отдельно от duplicate events, queue empty проверяется перед каждой фазой.

Host61/sanitizers61/standalone35, analyzer19 embedded+2 host,7 archive guards×6,
source178files/synthetic link/board/output/scope PASS. New separate archive768text/
data0/bss0/max individual frame56, ceiling4096/frame128,10 exact exports. Все
старые archive sizes/ceilings/SDK/dependency pins/workspace/16384 arena прежние.
Host event32/owner968 bytes и compiler frames не MCU RAM/full-stack approval.
Physical CPU/bus/worker timing/worst-case/soak не измерены.

Подтверждения владельца F407 SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/
Flash1024KiB/3.252V/backup1048576bytes/structural vectors PASS прежние. Полная
маркировка/package/RAM/CCM/HSE/full vector mapping/Flash erase/protection/GPIO/CAN/
physical inhibit/backup recovery не подтверждены. Никаких Flash addresses/pins.
Открыты actual synchronized driver/event provenance/reset/reconciliation,
authenticated persistent identity/routing/resources/staging/runtime/installer и
operator/physical/recovery gates. Нет устройств/SWD/UART/reset/Flash/CAN/motion,
самостоятельного merge или релиза. Queue success не remote ACK/install authority.
