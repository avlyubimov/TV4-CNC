# PR-07c — transport → read-only image admission

20 ноября 2026. База main `28fe7bc8cc417ccc75e9a6db1939209b258118a1`;
PR #1–30 слиты, открытых PR нет, пять workflow PASS. Не повторять PR-07a/07b.

## Результат и границы

Связать COMPLETE_UNVERIFIED receiver с существующим независимым verifier.
Только локальная read lease неизменяемого source, boot/transfer identity,
generation политики, signature/SHA/length, ограниченный срок и освобождение.
После изменения identity/source/policy, отмены или deadline результат отзывается.
Нет backend записи, board image, Flash addresses/geometry, CAN codec/driver,
quiescent proof, install/commit/jump или действий с устройствами.

## Работа и приёмка

1. Отдельная portable library: снимок bounded inputs/keys, один owner, borrow RX,
   явный lease lifecycle. Успешная проверка удерживает immutable lease; receipt
   выдаётся только после повторной проверки контекста, не является install token.
2. Cleanup точно учитывает ресурс: не теряет handle при release failure,
   одна попытка на вызов, конечный лимит retries/quarantine; reset/reopen не
   скрывают незакрытый lease. Reentry отвергается; abort откладывает cleanup.
3. Сквозные RAM transfer → OpenSSL-signed metadata → Mbed TLS admission tests,
   loss/duplicate/fault/deadline/reset/context/source changes, cleanup/reentry.
   Это не persistent Flash recovery или физический safe state.
4. Root host/LVGL и sanitizer suites, standalone host, ARM object/import/frame/
   source checks, static analysis, image/output gates. CI сохраняет прежние
   восемь ESP SDK profiles и только текстовые evidence. Точные dependencies прежние.
5. Обновить текущие docs/status, сохранить исторические отчёты и hardware unknowns.
   Evidence в docs/evidence/build/stm32-update-handoff/. Отдельный PR, без merge.

Immutable source обязан обеспечить будущий reviewed backend. Generation stamp
сам по себе не блокировка и не подпись; RAM mock не назначает staging во Flash.
RX completion deadline не продлевается для проверки/ожидания: истёкший RX
отзывает candidate. Долгое ожидание оператора потребует нового handoff contract.
