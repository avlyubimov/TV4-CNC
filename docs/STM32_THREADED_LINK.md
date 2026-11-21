# PR-07v — подписанное обновление с отдельным HOST-потоком драйвера

База main `5e76626`, PR #1–53 слиты, main CI5/5 PASS, открытых PR перед работой
нет. [План](plans/PR07V_THREADED_LINK.md), [evidence](evidence/build/threaded-link/README.md).
Это новый opt-in режим тестов `--threaded-owner-staging`, а не firmware worker
или реальный CAN adapter. Embedded C/API, deadlines, crypto/arena/workspace,
один framebuffer, SDK/dependency pins, GPIO/Flash/security прежние.

## Участники, время и границы проверки

Calling thread владеет протоколом, TX consumer, RX consumer, independent verifier,
lease/model/completion. Один pthread владеет driver take/check/report/retry и
RX permit/offer/retry/pause_request/ack. Init полностью выполнен до pthread_create;
объекты имеют стабильные адреса до stop/join. Нет live copy/move/reinit/flush.
Driver получает только immutable bindings/config, actual публичные APIs и свои
bounded copies; не читает mutable protocol state или `f.now` другого потока.

Один synthetic uint32 clock, increment один раз на step. Protocol сначала
poll/service/pump/publish TX, затем запускает bounded driver turn и **параллельно**
обслуживает RX через stage/receive. TX publication здесь предшествует wake;
гонки publish/cancel/report отдельно сохраняются в прежних pthread unit tests.
Новый signed mode проверяет всю композицию через отдельного driver actor и
конкурентный RX, не все возможные interleavings target scheduler.

Clock заморожен на время turn. Значение после take/stage принадлежит этому
же current tick; следующая millisecond не начинается до driver completion.
Mutex/condition только планируют turns/передают время/копируют diagnostics;
**target APIs не вызываются под HOST mutex**. Crypto выполняется одним owner,
его CPU duration по-прежнему не тарифицируется в logical clock. Нет wall-clock
realtime/throughput/latency/IRQ/stack/RAM доказательства или новых default budgets.

## Bounded backend и исходные captures

На направление не больше одного owned fake-backend job и одной внешней BUSY
capture. Take даёт payload-free token; check проверяет original admission/
residency до получения owned bytes, повторный check — перед simulated arrival.
Kind/drop script определяется из actual START bytes TU/MS, не из mutable owner
metadata. Loss/drop/duplicate inputs прежние; это потери simulated wire,
не скрытые локальные overruns. Exact terminal report относится к сохранённому
ticket, duplicate callback отвергается; первый BUSY report остаётся для retry.

Перед **новой** simulated capture driver получает permit. BUSY удерживает
original epoch/time/bytes, HELD передаёт actual owned copy ingress. Source copy
после admission стирается. Pending retry не освежает captured time. Backend
может ждать с ещё не прибывшим job, пока gate закрыт; это свойство memory test,
не обещание физического CAN backpressure или сохранности периферийного FIFO.

Pause/recovery control turn работает в том же now, не прибавляет скрытое время.
Exact request ACK выполняется только driver thread после actual pending retry.
Caller затем проверяет actual ring/staged/held/RX drain, actual TX claims и
прежние upper states; diagnostic counters не дают phase/install authority.
Lower timeout recovery сохраняет original phase/IDs/clocks/counters, fresh epoch
выдаётся прежним owner. Upper image RX/crypto/lease/model/close окна не меняются.

Внешняя BUSY capture **не покрыта** inner pending ACK. Если она ещё удерживается,
test driver не ACK-ает pause, не retag-ает/не выбрасывает её и не делает force
flush. Контролируемый refusal сохраняет блокировку до join; дальнейший настоящий
adapter потребует отдельного truthful reconciliation/loss/reset contract.
Это намеренно незавершённый lifetime, не подтверждённый physical drain.

Stop выполняет только pthread join/destroy HOST synchronization. Он не создаёт
terminal reports, не освобождает actual claims и не очищает protocol state.
Positive fixtures предварительно дожидаются actual idle; negative missing-return
fixture явно доказывает сохранение claim после join и original image expiry.
Late truthful cancellation в другом refusal только возвращает ownership;
expired upper RX/admission не оживает. Test context одноразовый, restart запрещён.

## Программные проверки

- Восемь полных signed chains: 6 release15 +2 legacy11; все12 TU/MS kinds,
  raw document/signature/key ID/image, real independent P-256/SHA/lease/model/
  clean close/outcome. Producer arrays wiped; verifier читает доставленный snapshot.
- Три independent refusals: invalid signature, valid re-sign с wrong negotiated
  raw digest, expired original image receiver. Transport success их не обходит.
- Два retained driver lifetime refusals: original image timeout, late terminal
  cleanup-only и missing completion после stop/join. Никакого force-forget.
- Один controlled external BUSY capture refusal: capacity1 ring+actual pending+
  external owned capture, retained original epoch/time, pause остаётся REQUESTED,
  actual TX claim не исчезает после join. Synthetic clock около wrap.
- Host/LVGL75, ASan/UBSan/LSan75, standalone49 PASS. TSan сохраняет8 прежних cases
  и отдельно выполняет новую signed suite1/1; default environment/no suppressions.
- Actual-source analyzer23 embedded+3 host, прежние11 exact ARM archive guards/
  negatives/source178/synthetic/board/output/scope PASS; ни один firmware budget
  не расширен. Host pthread/helper не входят в ARM targets/commands/handoff.

Числа DER/retirements/scheduling — per-run observations в evidence, не golden
timing. Explicit inputs прежние: ingress2/residency8ms/service1ms, custody2/
residency16ms, completion2/service1ms, fragments progress8/total256/frame160,
driver1/3ms. Сохраняются все старые serialized modes и thread unit cases.

## Подтверждённое аппаратное состояние и блокеры

Только прежний owner report: F407 SWD/ST-Link V2 работает, Cortex-M4r0p1,
raw ID0x101f6413, Flash1024KiB,3,252V,backup1048576bytes/structural vectors PASS.
Новых аппаратных действий/наблюдений нет; ESP32 остаётся ordinary8f0d899.
Full marking/package/RAM/CCM/HSE/IRQ/vector/erase/protection/GPIO/CAN/backup
SHA/copy/restore неизвестны. Flash1MiB не задаёт RAM, sectors или app offset.

Engineering preconditions HOST: pthread publication/stable lifetime/one driver+
one protocol actor/common frozen clock/exclusive spans/bounded truthful mock.
Target ISR/RTOS/cache/DMA/clock/peripheral/remote drain/completion/loss/reset/
route reconciliation, persistent authenticated identity/resources/staging/runtime/
installer/operator/physical gates остаются открытыми. Software ACK/phase/take
не authentication/ARM/START/install permission. Никаких devices/SWD/UART/reset/
Flash/erase/CAN/motion/eFuse/option bytes/self-merge/release.
