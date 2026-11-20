# PR-07c — от приёма байтов к проверенному кандидату

20 ноября 2026. База main `28fe7bc`, PR #1–30 слиты; transport #29 и
independent admission #30 используются без копирования. [План](plans/PR07C_UPDATE_HANDOFF.md),
[API](../firmware/stm32-bootloader/include/update_handoff.h),
[evidence](evidence/build/stm32-update-handoff/README.md).

`update_handoff` связывает живой COMPLETE_UNVERIFIED receiver с существующим
[Mbed TLS verifier](STM32_IMAGE_ADMISSION.md) и временем жизни источника.
Результат — временный проверенный кандидат. Нет runtime caller, storage writer,
Flash addresses/reservation, board image, CAN driver/codec, COMMIT или jump.
Подтверждения physical quiescent state и разрешения оператора здесь нет.

## Источник и доверие

Begin получает completed logical receiver и bounded local metadata/signature/
key/policy. До первого callback копируются raw inputs, до4 public keys,
policy и ops. Изменение caller buffers не меняет подписанный snapshot.
Production context/версия/trust store/release schema ещё не назначены;
используется явно экспериментальный draft PR-07b.

Локальный backend предоставляет **read lease**:

1. acquire атомарно отвергает запрос либо возвращает ненулевые token/object/
   generation, boot+transfer и длину. Успех фиксирует immutable/exclusive snapshot
   до успешного release, без изменения/освобождения байтов в других owner paths.
2. inspect подтверждает тот же token/object/generation/identity/length;
   read читает snapshot по relative offset с capacity до256bytes.
   Подпись, exact length/EOF и SHA независимо проверяет прежний verifier.
3. release идемпотентен. Backend/context/RX живут до CLOSED; ошибка освобождения
   не позволяет забыть lease или переиспользовать его для другого образа.

Generation число **не создаёт блокировку или подпись**. Snapshot гарантирует
будущий reviewed backend; тесты замораживают RAM buffer. Нет durable staging,
резервирования полной app в RAM MCU или Flash access. Signature/SHA также
не подтверждают допустимые SP/PC/vectors, bootability или recovery.

Owner stamp — trusted local boot identity (та же, что у RX) и ненулевой
policy_generation. Изменение keys/counters/compatibility требует нового
generation/отмены сеанса. CAN inputs не являются local trust context.
Уникальность boot/transfer/generation обеспечивает будущая negotiation;
переиспользование/ABA и нечестный backend этими проверками не решаются.

## Состояния и срок действия

EMPTY → CLAIMING → BOUND → VERIFYING → VERIFIED. Verify синхронный worker-only,
один раз на lease; повтор не переиспользует прошлый успех. Успех удерживает
lease, чтобы candidate не переживал изменение storage.

До/после read, вокруг admission и после cleanup проверяются cancel/clock/
lifetime, owner stamp, source view и живой RX: identity/hash/size/cursors/
limits/start/progress. Signed length и проверенный SHA сверяются с transport
descriptor. CRC/consumer ACK/opaque hash сами по себе доверия не создают.

peek повторяет guard и выдаёт candidate + boot/transfer/policy generation/
source object/generation. Ошибка очищает disjoint output; BUSY/alias возвращают
false без записи в live storage. Уже скопированный receipt нельзя удалённо
обнулить: это результат для текущего owner turn, **не install/commit/jump или
ARM/START token**. Перед каждым использованием проверять живой coordinator;
после reset/reopen проверять сохранённые байты снова.

Local lifetime1..INT32_MAX ms без default, monotonic uint32 modulo clock;
elapsed>=deadline или обнаружимый обратный ход дают отказ. Admission имеет
собственный timeout. RX progress/total deadline продолжают действовать:
handoff их не продлевает. Exact final ACK duplicate разрешён в прежнем bounded
окне transport. Poll нужен регулярно даже без пакетов. Долгое ожидание оператора
потребует нового reviewed protocol/source contract. Cooperative checks не
прерывают зависший backend/crypto и не заменяют аппаратный watchdog.

## Отмена, ошибки и ресурс

Close/любой отказ сразу отзывают candidate, сохраняют первую причину и
admission result (только при admission_attempted=true), затем освобождают lease. Публичные keys/metadata/signature
очищаются при закрытии; workspace очищает существующий verifier.
Release failure сохраняет token и не изображает успешное освобождение.

Не более одной release attempt на close/poll/failed peek. Явный лимит1..8:
до исчерпания RELEASING, после — QUARANTINED. Clear/begin не могут забыть
held resource. Успех acquire с token0 — backend contract fault и quarantine,
без выдумывания handle. Объект остаётся у owner для recovery review; force
close/reset/hardware API нет. CLOSED повторно не освобождается, clear разрешает
новый EMPTY объект только после доказанного release.

Статическое размещение, один owner, никаких threads/ISR/перемещения live object.
Callback context/RX живут до CLOSED, вне очищаемого workspace. Callbacks bounded
и правдивые, clock/stamp/inspect только наблюдают. Ordinary reentry возвращает
false без изменения live storage. Reentrant abort лишь ставит cancel; cleanup
следует после возврата callback и не освобождает читаемый buffer внутри read.
Прежний global Mbed TLS allocator остаётся single-owner.

## Проверки и следующие gates

10 handoff groups: 54 RAM transfers (6 sizes ×3 block sizes ×3 fragments),
реальная OpenSSL подпись/Mbed TLS verification, lost DATA/final ACK/pending
duplicates; invalid signature/SHA/length/rollback, stale source/owner/RX,
generation changes до/во время/после verification и cleanup, timeouts/wrap/
cancel, acquire/read/release failures, retries/quarantine/retained handle/reentry.
Host/LVGL25 suites, sanitizers25, standalone5; ARM/source/import/frame/static/
image/output checks — [evidence](evidence/build/stm32-update-handoff/README.md).
CI сохраняет прежние8 ESP SDK jobs; dependency versions/locks неизменны.

Host state2128bytes + отдельно borrowed workspace17664bytes. ARM state ограничен
compile assertion2304bytes. Новых mutable globals/heap нет. Это не runtime
RAM/CCM/total stack/throughput budget физического STM32.

Owner SWD/core/ID/Flash1024KiB/3,252V/backup1048576bytes/vector structure facts
сохранены. Package/RAM/CCM/HSE/erase/protection/pinout/GPIO/CAN/restore неизвестны.
До writer/commit/jump нужны reviewed memory/vector/clock/erase/protection/
recovery, production metadata/keys/version, fresh physical quiescent proof и
approval конкретного образа. No-I/O/ACK/SIMULATION не подтверждают останов;
E-stop независим. Устройства не использовались. Откат — revert PR без миграции.
