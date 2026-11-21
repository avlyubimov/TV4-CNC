# PR-07s — передача кадра драйверу с явным владением

База main `db50840`, PR #1–50 слиты. [План](plans/PR07S_DRIVER_DISPATCH.md),
[проверки](evidence/build/driver-dispatch/README.md). Optional C11 archive
`tv4_driver_dispatch` связывает actual [completion owner](STM32_COMPLETION_MAILBOX.md)
и одноэлементный owned dispatch slot. Никакого runtime caller, настоящей CAN
передачи, ISR/RTOS/cache/DMA/clock port, GPIO/Flash/wire defaults.

## Два участника, оба направления

Protocol actor вызывает init/begin/pump/publish/poll/service/advance/cancel/clear;
driver actor — take/check/report/retry. Exactly one actor на каждую роль. Init
полностью завершается и публикуется внешним happens-before до запуска actors.
Stable address/lifetime, stop/join обоих перед destruction. Нет live reinit/
copy/move/reset/flush/inner mutable calls. Valid/disjoint spans; нет firmware
I/O/heap/spin/locks. Producer callbacks должны сериализоваться будущим adapter.

Protocol actor сначала claims actual frame у своего completion owner, копирует
его в slot вместе с **исходными** admitted/residency из actual ledger, затем
release-publishes READY. BUSY не берёт второй claim. Driver acquire-takes slot
в private owned snapshot и release-publishes TAKEN. Protocol actor никогда не
читает driver-private current/pending/exposed/clock fields во время работы.
Driver никогда не читает mutable protocol state; binding completion mailbox
immutable. Atomics32 compile gate и ARM codegen/import guard обязательны.

`take` выдаёт UNCHECKED token с exact ticket/deadline/route, payload zero.
Driver читает текущее monotonic uint32 ms **после take**, затем вызывает `check`.
Так clock snapshot, снятый до конкурентной публикации нового job, не будет
ошибочно сравнен с более поздним admission. Owner/driver используют один clock
domain, каждый observation gap<2^31. Без готового clock port это software contract.

`check` сравнивает exact current ticket, monotonic clock, atomic revoked и
original residency. SENDABLE выдаёт owned payload; прочие outputs полностью zero.
Проверять перед backend submission. Это local software check, не hardware send
permission или атомарная отмена: revoke может произойти после проверки, а уже
переданный backend кадр может оставаться в обработке. Реальный adapter обязан
разрешить hardware/DMA/IRQ ownership и возвратить правдивый terminal report.

## Возврат, отмена и повтор

Report требует explicit exact owner/route/generation/serial ticket текущего
driver job. Нет callback «завершить текущий кадр» без ticket. Foreign/old/duplicate
reports → STALE до completion queue. SENT_LOCAL без предшествующего успешного
check запрещён; после реального exposure поздний SENT может быть правдивым
terminal return и принимается для cleanup, не продлевая original deadlines.

Full report mailbox сохраняет **первый** event в driver-private pending. Новый
report не заменяет его, даже если UNCERTAIN сменился на terminal; требуется
`retry` исходного event. UNCERTAIN admission не возвращает actual job. После
terminal admission driver стирает private job/pending и release-publishes RETURNED.
Protocol actor должен acquire-observe RETURNED **и** обработать actual correlated
completion/empty inbox/pending до wipe shared slot и release EMPTY. Одного
внутреннего ledger flight0 или producer RETURNED по отдельности недостаточно.

Poll/service обрабатывают original current generation/time; обнаруженная failure
публикует revoked. Cancel сначала publishes revoked, затем original cancel,
сохраняя untaken READY, taken job, pending report и actual claim. Даже published
но ещё не взятый кадр требует driver take/check/refusal и explicit CANCELLED/
FAILED/UNCERTAIN return. Owner не выдумывает driver completion и не force-forgets.

Clear ждёт actual EMPTY slot и original completion cleanup. Retained ticket/
fragment IDs, phase/time/atomic mailbox positions прежние; owner context/clock/
exhaustion sticky. После recoverable clear снимается только owner revoked;
driver-local clock failure остаётся sticky и не чинится owner clear. Она требует
отдельного reviewed stop/lifetime/clock recovery, не live reinit. Отсутствующий
terminal report продолжает блокировать reuse/phase. Всё volatile, не reboot recovery.

## Проверки и пределы

- Unit: untaken/taken cancellation, independent driver expiry, original admission
  age через delayed publication, driver-clock fault sticky across owner clear,
  owner context/exhaustion, zero unchecked/refused outputs, late SENT cleanup.
- Full completion mailbox/retained first terminal/retry после UNCERTAIN, stale
  callback после clear/new claim, запрет SENT до check, no implicit-current ACK.
- Два реальных pthread actors:256 actual channel/peer-RX messages,8448 frames,
  1056 rejected duplicate callbacks, capacity1..4/clock wrap, без внешней host job
  mailbox. Четыре controlled cancellation/expiry/full-retry сценария. Driver
  expiry изолирована gate до owner poll; если owner revocation выигрывает гонку,
  корректным refusal является REVOKED вместо EXPIRED. Ни один не выдаёт payload.
- HOST `--driver-dispatch-staging`:8 signed positives (6 release15+2 legacy11),
  3 independent signature/raw digest/expired image refusals и2 delayed correlated
  dispatch/completion refusals. Original verifier/lease/model/close и image windows
  прежние. Producer wiped; upper RX/admission не оживает от late cleanup/clear.

Synthetic host link serialized; actual concurrency отдельно pthread/TSan. Mode
сохраняет PR-07q/r inputs:2 custody slots/residency16ms,2 report slots/service1ms,
fragment progress8/total256ms/frame160, driver delay1/3ms. Duplicate callback
every17 теперь rejected самим driver endpoint, а не копируется в completion inbox.
Новых production limits/IDs/bitrate/версии wire, crypto owner/heap/SDK pins нет.

Host68/sanitizers68/standalone42, ThreadSanitizer4/4 default env/no suppressions.
Analyzer21 embedded+2 host,9 guards:7×6 +2×7 methods,178 source/synthetic link/
board/output/scope PASS. New archive1324text/data0/bss0/max individual frame72,
ceiling4096/frame128,13 exports/14 native DMB/no atomic helpers. Старые archives/
budgets/config/workspace/arena прежние. Host dispatch1360/delivery48 bytes и
compiler frames не MCU RAM/full-stack/timing approval. Target latency/CPU/soak
не измерены; software revocation не physical inhibit.

Owner F407 SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3.252V/
backup1048576bytes/structural vectors PASS сохранены. Новых аппаратных наблюдений
нет. Full marking/package/RAM/CCM/HSE/vector/erase/protection/GPIO/CAN/physical
inhibit/backup recovery, target port/driver truth/reset/reconciliation, persistent
authenticated identity/routing/resources/staging/runtime/installer/operator gates
остаются open. Flash1MiB не задаёт RAM/loader addresses. Устройств/SWD/UART/reset/
Flash/CAN/motion не касались; самостоятельного merge/релиза нет.
