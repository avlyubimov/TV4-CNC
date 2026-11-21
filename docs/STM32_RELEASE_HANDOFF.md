# PR-07h — подписанный релиз в живой сессии STM32

21 ноября2026. База main `c17e89c`, PR #1–37 слиты, main CI5/5 PASS.
[План](plans/PR07H_RELEASE_HANDOFF.md), [evidence](evidence/build/stm32-release-handoff/README.md).
Это программное продолжение [release verifier](STM32_RELEASE_BINDING.md) и
[source handoff](STM32_UPDATE_HANDOFF.md). Реального runtime caller пока нет.

## Результат и границы доверия

Новый [release_handoff.h](../firmware/stm32-bootloader/include/release_handoff.h)
явно выбирает strict15-field release draft и связывает его с существующим
completed RX/immutable read lease. Тот же независимый Mbed TLS проверяет raw
P-256 signature, local release/source/channel/component SemVer, target/counters,
полный image SHA/length/EOF. Затем тот же held source проходит существующую
[проверку vector model](STM32_IMAGE_PREFLIGHT.md). Никакой HMI success boolean
или standalone candidate не принимается вместо этих проверок.

`tv4_boot_completion_start_release` соединяет этот путь с прежним
[completion/outcome](STM32_UPDATE_OUTCOME.md). Raw SHA подписанного15-field
документа должен совпасть с binding из [negotiation](STM32_UPDATE_NEGOTIATION.md).
Даже новая корректная подпись на изменённом документе не заменяет согласованный
raw digest. CHECKED_CLOSED — исторический результат проверок после успешного
close, а не разрешение на install, jump или движение.

## Явный режим и один алгоритм

- Legacy `tv4_boot_handoff_begin/verify/preflight` и completion start сохраняют
  strict11-field admission draft. JSON/schema не выбирает локальный режим.
- Новый begin получает отдельный typed config; он копирует local selection/
  versions/policy, raw metadata/signature/key ID и public keys до callbacks.
  Base image policy ссылается только на скопированные core keys.
- Общие private helpers в core выполняют прежние guard, read, model, cleanup
  и completion алгоритмы. Нет второго verifier, arena, allocator owner или
  параллельной копии state machine. Public verifier callback/success stub нет.
- Legacy positive operations/clear отвергают core в release mode без изменения
  его состояния. Legacy poll/close/abort разрешены для conservative guard/cleanup;
  они не публикуют release receipt и не продлевают lifetime.
- Release receipt содержит полный checked release и тот же source object/
  generation, transfer ID и policy generation. Только live peek проверяет его
  актуальность. Копию нельзя отозвать или превратить в установочный token.

## Владение, отмена и ошибки

Zero-init adapter и core; один owner, без copy/move/ISR/threads. Adapter, core,
RX и observer context остаются disjoint/alive до explicit clear. Провайдер
обязан держать exclusive immutable source до успешного release; число generation
само не блокирует запись. Любое изменение selection/version/keys/model/context
должно менять owner policy generation. Эти trusted inputs не берутся из JSON.

Прямой путь: begin → verify → live peek/preflight → close → clear.
Completion path: begin → completion_start_release → completion_poll, если нужен
cleanup → historical snapshot → clear completion/adapter. Не выполнять
независимые операции над adapter/core, пока completion активен; отменять его
через completion_abort. Это альтернативы: start требует ещё BOUND, не уже
VERIFIED core. Ни один путь не продлевает исходные RX progress/total deadlines.

На ошибке signature/SHA/model/owner/source/RX/clock/cancel candidate отзывается,
workspace очищается, lease освобождается по прежнему bounded retry contract.
При release failure положительный отчёт не выдаётся. QUARANTINED удерживает
handle/token и запрещает clear/restart. После ошибки begin, если adapter уже
привязан, также нужен штатный cleanup/clear; invalid early arguments остаются
untouched и ничего не acquire. BUSY/alias не меняют защищаемую caller storage.

## Программные проверки и ресурсы

Новые8 групп дополняют прежние19 handoff/completion групп. Шесть полных
release15 negotiation→DATA→P-256/SHA→vectors→close→outcome/ACK цепочек теряют
каждый handshake message, DATA, final ACK, report и report ACK. На успехе один
release. Проверены неверный selection/source/channel, rollback, signature/hash,
подписанные неверные vectors и re-signed metadata с другим negotiated digest;
смена owner/source/RX, timeout/backward clock, snapshots, BUSY/reentry/abort,
alias, acquire/release failure, retries/quarantine и post-close context faults.

Root/LVGL34, sanitizers34, standalone11, synthetic link contract1 PASS.
Actual-source GCC analyzer7 units, pinned crypto source178 files и прежние
ARM/import/frame/board/output gates PASS. Все8 CI ESP SDK profiles сохранены.
Local ESP builds в этом STM32-only этапе не повторялись.

Host: adapter528/full receipt360 bytes; legacy core2136/receipt136,
completion568/workspace17664 прежние. На ARM4 archives41690text/data8/bss27
(+1460text); handoff/transport не добавляют mutable globals или heap imports.
Максимальные individual compiler frames прежние: admission984, crypto376,
handoff232, transport64. Новый begin frame232, completion wrapper40; их frames
входят в будущий полный stack review. Эти числа не runtime stack/RAM/CPU/latency
измерения и не подтверждают резерв памяти F407.

## Оставшиеся решения

Production bundle↔component schema/linkage, keys/installed local context,
boot epoch provider/persistence/authentication, raw CAN/wire/backend, resource/
fault/soak budget, physical/operator/recovery gates и installer остаются открыты.
Нет actual F407 MEMORY/erase map, default addresses, Flash writer, COMMIT/jump,
ARM/START или GPIO. Owner SWD/core/Flash/backup факты сохранены; HSE, full
physical marking, RAM/CCM/vector/sector/protection/GPIO/CAN/restore не подтверждены.
Software fixtures не подтверждают работу платы или физический inhibit.

Устройства/SWD/UART/reset/Flash/backup reads не выполнялись; на ESP32 остаётся
ordinary8f0d899. Нет ключей/релизов/тегов/изменения partition/security или merge.

## Последующий PR-07i

[RAM staging source](STM32_STAGING_SOURCE.md) добавляет portable consumer/lease
backend и signed integration вместо mock storage. Этот этап не меняет данный
verifier/coordinator или legacy path. API ownership проверен программно, но
production medium/RAM budget/durable backend/runtime/physical gates ещё открыты.
