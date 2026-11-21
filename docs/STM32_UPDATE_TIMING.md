# PR-07l — одна шкала времени для fragments и logical update

База main `277801e`, PR #1–43 слиты, main CI5/5 PASS.
[План](plans/PR07L_FRAGMENT_TIMING.md), [evidence](evidence/build/update-fragment-timing/README.md).
Изменение только host tests/CI/docs/CMake host targets, без устройств и merge.

## Какой пробел проверяется

PR-07k проверил [bounded fragments](STM32_UPDATE_FRAGMENTS.md), но его signed
integration использовал отдельные часы фрагментов. Новый HOST-ONLY
[test link](../protocol/can/tests/fragment_test_link.h) использует один caller
clock и для fragment poll/очереди/delivery, и для negotiation/DATA/outcome,
staging/lease/admission `now_ms`. Задержка кадра расходует время исходного
логического сеанса. Lower-layer progress/timeout/clear не сбрасывает upper
started/progress/counters и не продлевает время проверки кандидата.

Production C/headers/API/defaults/configs/разметка/драйверы не менялись. Это
не production owner/transport worker, routing/identity provider или CAN schedule.
No-I/O/board/output/physical/operator/recovery/install gates остаются закрытыми.

## Ограниченный тестовый канал

Два направления, по одному frame slot на направление. Payload никогда не
выходит за прежний envelope bound. Слот хранит собственную копию и due time;
локальный commit выполняется при приёме в слот, не доказывает remote delivery.
Занятый слот удерживает deterministic peek, не двигает cursor/progress/counter.
Сборщик удерживает READY_BYTES_UNCHECKED до single take с прежним total limit.
Нет heap, threads, sleep, UI callbacks, I/O, CAN driver или автоматического
dispatch. Внешний test owner явно вызывает logical APIs после take.

`step` продвигает единственный synthetic clock на1ms, polls оба направления,
доставляет due frame и наполняет освободившийся слот. На продолжительность
компьютерного исполнения он не опирается: **это не measured bus/CPU latency**.
Clock pointer принадлежит caller и живёт дольше link, объекты exclusive/disjoint.
Fixtures не копируют/переинициализируют live fragment contexts и не обнуляют IDs.

Заданы explicit synthetic delays, периодические задержки/дубликаты и один
выбранный потерянный START/middle/final fragment на kind либо постоянная потеря.
Повторная доставка занимает тот же ограниченный слот, не растит очередь.
Test owner может явно clear только recoverable fragment timeout, сохраняя
high-water/counter/time; CLOCK/CONTEXT остаются sticky. Это политика теста,
не автоматический recovery в firmware. Поздние части прежнего ID stale/order;
новый START не заменяет активный RX. Whole-message retry задаёт прежний upper
protocol и получает новый fragment ID. Consumer writes/outcome delivery не
дублируются. Очередь/remaining frames не становятся ACK или install permission.

## Проверенные композиции и отказ

- 34 DATA scenarios: blocks1/31/256, потерянные START/middle/final и DATA_ACK,
  оба kinds вместе, bounded queue hold/duplicates, wrap, постоянная потеря до
  штатного retry exhaustion. Ровно один consumer write на принятый logical block.
- При потере START RX ещё не открыт: subsequent offsets order reject, а fragment
  timeout ему не требуется. При потере continuation RX уже открыт и до нового
  сообщения должен истечь/быть явно очищен. Оба случая проверены отдельно.
- Exact READY total128ms и queue progress8ms; repeated peek/BUSY не продлевают
  сроки. Сообщение после clear получает новый ID, занятый slot не исчезает.
- ACK удержан до exact DATA total300ms на той же шкале: unchanged DATA TX
  отказывает; поздний структурно valid ACK не возобновляет передачу.
- Negotiation total50ms истекает, пока фрагменты ещё делают progress. Поздний
  REQUEST/OFFER проходит fragment codec, но старый requester остаётся EXPIRED,
  last-issued/start time прежние. Для responder началом остаётся первый valid
  logical REQUEST по прежнему API, а не init/приём первого fragment.
- Backward clock/generation mismatch sticky. Отдельная отрицательная проверка
  ожидает принятие old START новым RX с другим local generation: это доказывает,
  что **reboot replay isolation пока не реализована**, а не её успешную защиту.

Shared-clock signed integration:8 positive chains (6 release15 +2 legacy11)
через все8 kinds. Существующие real RAM staging/P-256/full-image SHA/model/
live lease/clean close выполняются после time-consuming delivery. В шести
chains теряется по одному fragment всех8 kinds, whole-message retries проходят
без reset deadlines. Report отправляется после wipe source, доставляется один
раз; потеря OUTCOME_ACK вызывает повтор/ACK, не повторное применение результата.
Реальные test private keys только в RAM, новые key files отсутствуют.

Ещё два shared-clock refusal scenarios: неверная подпись после успешной передачи
не проходит independent verifier; expired DATA отказывает до acquire lease.
Ни fragment completion, ни new local generation не заменяет live receiver,
crypto, физическое состояние или подтверждение оператора.

Подписанные raw metadata/signature по-прежнему поступают из fixture напрямую,
их network transport не реализован. CPU crypto/storage duration в этой модели
не измеряется и отдельно не тарифицируется; используется тот же `now_ms`, без
подмены verifier/lease/clock callback. Остальные старые typed/byte/fragment
integration modes и fault cases сохранены.

## Лимиты — инженерные входы тестов

| Scenario | Explicit synthetic limits |
|---|---|
| Common fragment channel | progress8ms / total128ms /160 readable same-ID attempts |
| Queue | delivery1ms, held3ms every11 frames, duplicate every17 deliveries |
| DATA positive | ACK200ms / progress3000ms / total10000ms /4 retries |
| Negotiation positive | retry200ms / total4000ms /8 phase sends |
| Outcome positive | ACK200ms / linger500ms / total2000ms /4 retries |
| Delayed ACK negative | DATA ACK100ms / progress=total300ms; fragment total1000ms |
| Negotiation expiry negative | delivery3ms, logical total50ms, fragment total200ms |

Значения выбраны до begin для конкретных сценариев, не меняются у active object
и не заменяют прежние fixture/default limits. Они не выведены из bitrate,
арбитража/загрузки шины, MCU performance или измеренного worker budget.
Прошедшая симуляция не утверждает, что любая комбинация положительных timeouts
совместима. Для production ещё нужны upper/lower retry/queue/drain composition,
bounded scheduling и измерения; важны как bus latency, так и длительность crypto/
source/storage. Без них нельзя объявлять эти test numbers runtime defaults.

## Фактические результаты и границы

Host47/47, ASan/UBSan/LeakSanitizer47/47 вне ptrace/default env/no suppressions,
standalone21/21.34 DATA scenarios:40700frames/43drops/31 timeout clears/8464
repeated peeks.8 positive signed chains:7640frames/48drops/32 timeout clears/
1611 repeated peeks, longest2551 **synthetic** ms; это не стендовый throughput.

ARM archives прежние:4 libraries43362text/data8/bss27/frames984,376,328,64;
wire1219text/frame32 и fragments1536text/frame48, data/bss0. Host scheduler/
timing/reviewer targets отсутствуют в ARM. Строгий analyzer13 embedded+2 host,
Mbed source178, archive guards/negative cases, synthetic link и board/output/
scope gates PASS. CI добавляет анализ host fixture и scope checks, сохраняет
exact dependencies/actions и обе SDK ×4 profiles, uploads text only.
Local ESP builds для этого host-only этапа не повторялись.

Owner hardware facts сохранены в [HARDWARE_STATUS](HARDWARE_STATUS.md).
HSE/full marking/RAM/CCM/IRQ/erase/protection/GPIO/CAN/backup recovery остаются
блокерами. Нет SWD/UART/reset/Flash/backup rereads, устройств не прошивали.
Следующий software этап — bounded metadata/signature transfer после review
identity/routing/queue lifetime; production trust/context/durable storage и
installer отдельно. Flash addresses/bootloader reserve не выбирались.
Откат — revert PR, без изменения устройств, security settings или разметки.
