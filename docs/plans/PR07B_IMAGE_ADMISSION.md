# PR-07b — независимая проверка кандидата STM32

База main74a7af9, PR #28 слит. PR #29 transport ещё открыт и не копируется;
новый модуль не зависит от него. Продолжение владельца разрешает software
работу и отдельный PR, без устройств, release и merge.

1. Зафиксировать отдельную Mbed TLS3.6.7/официальный archive SHA-256/лицензию.
   Минимальный P-256/SHA-256 backend и bounded upstream buffer allocator.
2. Проверять точные snapshot bytes подписанных draft metadata локальным
   public key allowlist. После подписи — строгие поля/length/hash/hardware/
   memory-profile/protocol/bootloader/image-counter compatibility checks.
3. Потоковый read-only image SHA с exact EOF, deadline/cancel и cleanup;
   нет writable storage/commit API, никакого default production context.
4. OpenSSL host signer/oracle в памяти, negative/fault/timeout/OOM/reentry/
   snapshot tests, sanitizers, ARM compile/import/frame/code budgets и CI.
5. Обновить статусы/evidence/пределы результата, создать отдельный PR.

Приёмка: только VERIFIED_CANDIDATE после всей проверки; ошибки очищают receipt,
source не читается при недоверенных/несовместимых metadata; workspace освобождён
и очищен. Нет libc allocator/OS/HAL/движения/Flash адресов/готового board image.
Draft schema и software arena не объявляются release format или RAM map F407.
