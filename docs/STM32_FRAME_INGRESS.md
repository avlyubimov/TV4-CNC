# PR-07t — очередь входящих кадров с проверкой возраста

База main `25e635c`, PR #1–51 слиты, main CI5/5 PASS, открытых PR перед работой
нет. [План](plans/PR07T_FRAME_INGRESS.md), [проверки](evidence/build/frame-ingress/README.md).
Отдельная opt-in C11 библиотека `tv4_frame_ingress` закрывает software copy/time
boundary между одним driver actor и одним protocol actor. Она не содержит
runtime caller, actual CAN/ISR/RTOS/cache/DMA/clock port или channel phase owner.

## Владение и публикация

Init вызывается один раз для zero-init объекта: explicit capacity1..4,
residency1..2^31-1ms, ненулевые local route/generation и retained last serial.
Завершённый init публикуется внешним happens-before до запуска двух actors.
Адрес/binding неизменны, stop/join обоих требуется до destruction. Нет live
copy/move/reinit/reset/flush/clear, прямых полевых aliases или shared spans.

Driver actor вызывает offer/retry/lost и читает только immutable binding плюс
свои private fields; mutable protocol state ему недоступно. Protocol actor
вызывает stage/check/empty и не читает producer-private fields. faults доступен
обоим через atomic load, invalid object даёт UINT_MAX. C11 native uint32 atomics
compile gate и проверка ARM barriers/imports обязательны. Release/acquire
публикует owned entry и разрешает reuse ring slot только после copy/wipe.

`offer(frame,captured,now)` получает исходное время capture и свежий current
clock отдельно. Producer проверяет monotonic now и возраст до копирования.
ADMITTED означает owned ring copy; HELD означает owned producer-private copy
первого кадра при заполнении. Оба позволяют стереть input. `retry(now)` сохраняет
исходные captured/serial; успешный retry сначала публикует ring copy, потом
снимает atomic held. Новый offer while held возвращает BUSY без копирования/
serial increment/замены pending. Caller сохраняет новый input **и его captured**
внешне, затем предлагает с исходным captured и свежим now; перенос ожидания за
пределы библиотеки не обновляет возраст. Потеря/unretainable input/hardware FIFO
overflow обязательно сообщает `lost`, а не замалчивает drop.

В ring максимум4 entries, дополнительно по одному producer-pending и consumer-
staged. Размеры — software bounds, не утверждение RAM или production capacity.
API ограничивает DLC1..8 и копирует только length bytes, tail zero; содержимое
проверяют прежние fragment/channel/upper layers. Serial не wrap и не reset;
последний UINT32_MAX допустим, следующая выдача защёлкивает exhaustion.

## Свежий clock до выдачи payload

`stage` acquire-копирует один entry в consumer-private storage, wipe/releases
ring slot, возвращает только UNCHECKED token без payload. Пока он held, новый
stage BUSY. Caller читает fresh common monotonic uint32 ms **после stage**, затем
вызывает `check(token,now)`. Clock snapshot до конкурентной публикации не годится.
Каждый observation gap<2^31, capture/producer/consumer в одном clock domain;
достоверный hardware timestamp/пересчёт таймеров — будущий target adapter.

Check требует exact object/route/generation/serial/captured. Stale/wrong token
не потребляет current staged frame и выдаёт zero output. Valid check сверяет
consumer monotonic clock, исходный capture age и sticky fault. Возраст ровно
residency уже expired; future capture/обратный clock запрещён. FRAME выдаёт owned
copy и стирает staged entry; отказ тоже стирает staged payload и выдаёт zero.
FRAME остаётся **непроверенными байтами**, не auth/session/install authority.

Clock/expiry/exhaustion/overrun защёлкиваются в atomic fault и не сбрасываются
empty/повтором/новой фазой. Дальнейшая выдача payload запрещена. Pending/ring
могут остаться для reconciliation до stop/join; нет live force-flush/recovery API.
Библиотека владеет копиями, не hardware RX buffers; реальное их освобождение и
правдивое сообщение потерь — обязанность reviewed backend. Atomic RMW fault
может повторять exclusive store при contention; bounded ISR latency не доказана.
Fault check — software observation, не атомарная hardware cancellation/safety.

При выдаче FRAME consumer всё равно polls/receives protocol **в текущем now**,
не в captured. Ни retry, ни queue residence не продлевают original fragments,
negotiation/image/crypto/lease/model/close windows. При ingress fault runtime
должен fail-close/cancel/reconcile actual active protocol; library не вызывает
upper APIs и не выдаёт permissive callback/boolean вместо них.

## Empty и смена фазы

Empty — consumer snapshot ring/staged/atomic producer-held. WAIT не доказывает,
что producer остановлен или что его текущая операция/реальный backend/remote
queue завершены. Нет API advance/reset/новой generation и нет producer-stop
boolean. Actual target phase cutover/route cancellation потребуют отдельного
reviewed stop/ack/barrier с truthful backend reconciliation. Local binding не
serialized/authenticated и не даёт replay isolation после reboot.

HOST signed fixture сериализован одним владельцем: между проверкой actual
empty и phase advance никаких producer operations не выполняется. Это явный
host scheduling precondition, не переносимое доказательство live cutover.

## Программная приёмка и аппаратные границы

Unit покрывает capacity1..4, source wipe, retained first/retry, BUSY external
capture age, stale token, DLC256, clock wrap/backwards/future, age boundary,
exhaustion/overrun/faults без live reset и zero outputs. Отдельный actual channel
тест получает FRAME, но original fragment timeout отказывает при current service
time: захваченный раньше кадр не оживляет reassembly.

Два реальных pthread actors передают800000 owned FIFO frames и256 actual
channel messages/8448 fragments через capacity1..4; input немедленно wiped.
Thread stress использует fixed near-wrap synthetic clock, не realtime budget;
wrap progression отдельно unit. Три controlled thread faults происходят после
stage: producer loss, producer backwards clock и consumer expiry; payload zero.

Новый HOST `--frame-ingress-staging` сохраняет existing dispatch/completion и
проводит все12 TU/MS kinds, raw metadata/signature/key ID и image через actual
ingress copies/fresh checks. Explicit synthetic inputs:2 slots/residency8ms/
service1ms; прежние custody2/residency16ms, completion2/service1ms, fragments
progress8/total256/frame160, driver delay1/3ms и original upper windows прежние.
Это test inputs, не production defaults. Восемь positive signed chains
(6 release15+2 legacy11),3 independent signature/raw digest/expired image
refusals и2 ingress expiry/loss cases проверяют настоящий независимый verifier,
полный image/live lease/model/clean close; producer arrays wiped. Поздняя cleanup
completion не оживляет original image RX/admission после fault/timeout.

Точные результаты/версии/лицензии/ARM budgets — в evidence. Старые modes/APIs/
embedded core/SDK pins/workspace/arena/Flash/pins сохранены. Нового runtime caller,
driver/ISR/RTOS/cache/DMA/clock/phase-stop provider нет. Target CPU/latency/stack/
RAM/bus/soak не измерены. Нужны persistent authenticated identity/routing,
staging/resources/installer/operator/recovery и actual physical gates.

Owner F407 SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/structural vectors PASS сохранены. Новых аппаратных наблюдений
нет. Full marking/package/RAM/CCM/HSE/vector/erase/protection/GPIO/CAN/physical
inhibit/backup recovery ещё открыты. Flash1MiB не задаёт RAM/loader addresses.
Устройств/SWD/UART/reset/Flash/CAN/motion не касались; merge/релиза нет.
