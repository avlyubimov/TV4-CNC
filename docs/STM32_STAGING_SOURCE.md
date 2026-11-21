# PR-07i — временное RAM-хранилище принятого образа

21 ноября2026, база main `2ea51b6`, PR #1–38 слиты. Main CI5/5 PASS;
при старте ESP matrix ещё завершалась, её итог сохранён в evidence.
[План](plans/PR07I_STAGING_SOURCE.md), [проверки](evidence/build/stm32-staging-source/README.md).

## Назначение и ограничения

[staging_source.h](../firmware/stm32-bootloader/include/staging_source.h) даёт
переносимый RAM backend для [handoff](STM32_UPDATE_HANDOFF.md) и
[release handoff](STM32_RELEASE_HANDOFF.md). Ранее их tests использовали mock
storage. Новый backend сам копирует pending DATA в явно переданный буфер и
предоставляет тот же read lease verifier. Runtime caller пока отсутствует.

Нет собственного heap, default capacity, адресов MCU или резервирования RAM/
Flash. Весь образ должен помещаться в caller buffer. **Flash1024KiB не означает,
что такой образ помещается в RAM F407.** Production staging medium, фактические
RAM/CCM bounds и бюджет ещё не выбраны; это один программно проверенный вариант.
Данные и counters теряются при уничтожении объекта/настоящем reboot, без durable
recovery или persistent identity/anti-rollback. Нет Flash writer, CAN driver,
COMMIT/jump/ARM/START, physical inhibit или разрешения установки.

## Жизненный цикл

| Состояние | Допустимые операции |
|---|---|
| EMPTY | begin с новым receiving RX или idempotent discard |
| RECEIVING | consume pending block, WAIT без данных, discard |
| SEALED | acquire или discard; повтор последнего release без изменения данных |
| LEASED | inspect/read/release с текущим token; writes/begin/init/discard запрещены |
| INVALID | discard после ошибки; partial source нельзя acquire |

`init` принимает explicit buffer/capacity/boot/object только у zero-init объекта;
повторный init сконфигурированного объекта запрещён, в том числе после discard.
Буфер остаётся у одного владельца. Его нельзя независимо менять через raw C
pointers/DMA/другой thread. Защита API не является MPU или блокировкой чужих
writes. Source, region, RX, verifier/core/workspace и outputs должны быть disjoint
и оставаться живыми по их контрактам. Нет ISR/threads/callbacks внутри backend.

Begin требует fresh receiving RX: cursor0, без retained block, matching local
boot и transfer выше сохранённого high-water. Descriptor/limits/time identity
копируются, generation увеличивается даже для впоследствии сорванной передачи.
Begin не читает clock; consume использует существующий RX polling/deadline API.

`consume` получает bounded block через `tv4_update_rx_pending`, проверяет binding/
cursors/limits/time identity, копирует до256 bytes и только затем вызывает
существующий consume/ACK. Никакого второго wire parser или нового ACK protocol.
Повторы retained DATA/final ACK обслуживает прежний RX без повторной записи.
На ошибке context/order/clock/timeout source становится INVALID; ACK не выдаётся.
После последнего consumer ACK source SEALED и RX pointer отсоединён.

SEALED означает только завершённое копирование. Opaque image hash из descriptor,
CRC и ACK не являются криптографической проверкой. Standalone source не следит
за дальнейшими RX deadlines: этим по-прежнему занимается live handoff/coordinator.
Истёкший RX не оживает от SEALED или нового source token.

## Lease, освобождение и очистка

Acquire сравнивает полный descriptor и создаёт новый nonzero monotonic token.
Inspect/read требуют текущий token; generation обозначает конкретное содержимое,
object/boot задаёт локальный владелец. Их происхождение не аутентифицировано.
Read ограничен256 bytes, поддерживает exact EOF и не пишет в source region.
Неизвестные tokens/aliases/invalid spans оставляют outputs untouched.

Release O(1), не стирает образ. Повтор успешен только для последнего released
token в SEALED; старый token не освобождает новую lease. При внешней ошибке
release wrapper coordinator сохраняет held handle/retry/quarantine; backend
остаётся LEASED и не принимает discard/init. Force unlock/reset API нет.

Discard — отдельная worker-операция O(written bytes): стирает записанную часть,
сбрасывает partial binding и отсоединяет RX. Не abort/reinit transport и не
сбрасывает boot/object, transfer high-water, generation или token counter.
Exhaustion отвергается без wrap. Для нового transfer нужны discard и свежий RX;
данный backend не является cache, который перепривязывает старый image к новому
сеансу. Новый production cache/recovery contract требует отдельного review.

Положительный completion/outcome остаётся историей после release/discard.
Он не переносит candidate через operator wait. Будущая установка заново проверяет
сохранённые bytes и fresh physical/operator/recovery/session условия.

## Проверки и ресурсы

Новые7 unit groups:45 staging/read variants, blocks1/31/256, fragments1/7/256,
точные EOF/границы/sentinels, aliases, context/cursor/limit/reset/clock faults,
lease exclusion/idempotence/stale tokens,1536 последовательных циклов и exhaustion.
Новые4 integration groups:8 реальных signed chains (6 release15 +2 legacy11)
через backend, потери handshake/DATA/final ACK/report/ACK, попытки записи во время
crypto/vector read, SHA/signature/vector/source-selection/raw metadata mismatch,
owner change, release retries/quarantine и истёкший RX. Outcome проверен после
фактического стирания source. Все прежние suites также выполнены.

Host/LVGL36, ASan/UBSan/LSan36, standalone13, synthetic ARM link1 PASS;
analyzer8 actual units/source178 files/board/output/import/frame gates PASS.
Все8 ESP SDK CI profiles сохранены; local ESP builds здесь не повторялись.

Host source state160 bytes плюс весь caller buffer. Прежние core2136,
completion568, adapter528, workspace17664 не выросли. ARM4 archives43344text/
data8/bss27 (+1654text); нет новых mutable globals или heap/OS/driver imports.
Max individual handoff frame теперь328 (consume, раньше232), лимит512 не менялся.
Admission984/crypto376/transport64 прежние. Это не полный stack/RAM/CPU/latency
замер; O(image size) discard нельзя переносить в UI callback.

Новых devices/SWD/UART/reset/Flash/backup reads или измерений не было. Owner
F407 SWD/core/Flash1024KiB/3,252V/backup/vector facts прежние. HSE/full marking/
RAM/CCM/IRQ/erase/protection/GPIO/CAN/backup SHA/restore неизвестны. На ESP32
ordinary8f0d899, owner HMI/touch/Wi-Fi PASS. Physical/production gates не закрыты.
