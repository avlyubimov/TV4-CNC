# PR-07b — software evidence

20 ноября2026 (Asia/Almaty). База `74a7af9fb9a84ffee5587f4956ccb485e725b848`,
PR #29 transport ещё открыт и не скопирован. [Контракт](../../../STM32_IMAGE_ADMISSION.md),
[план](../../../plans/PR07B_IMAGE_ADMISSION.md).

| Проверка | Результат | Evidence |
|---|---|---|
| Root host/LVGL/logic | **22/22 PASS**, 2.03 s | [host-tests.log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **22/22 PASS**, 4.15 s, без suppressions | [sanitizer-tests.log](sanitizer-tests.log) |
| Admission + independent OpenSSL oracle | **14 groups PASS** | [admission-cases.log](admission-cases.log) |
| Dependency source integrity | **5 unittest cases PASS**, tamper/missing/extra/symlink rejected | [source-cases.log](source-cases.log) |
| Pinned Mbed TLS3.6.7 source | **178 files PASS** | [source-check.log](source-check.log) |
| ARM GCC14.2.1 objects | **27745 text, data8, bss27** | [arm-budget.json](arm-budget.json), [object-size.txt](object-size.txt) |
| Individual compiler frame | Admission984, crypto376 bytes, static | [compiler-frames.txt](compiler-frames.txt) |
| Cortex-M4 / hard-float ABI | PASS | [arm-attributes.txt](arm-attributes.txt) |
| Imports | Только libc memory/string + __aeabi_uldivmod; без allocator/OS/HAL | arm-budget.json |
| Actual GCC -fanalyzer compilation | Оба собственных C sources PASS | [static-checks.log](static-checks.log) |
| Deployable image / generated binary gates | Expected rejection PASS | [board-gate.log](board-gate.log), static-checks.log |

22 suites =20 существовавших в main +2 новых boot tests. Отдельные transport
tests PR #29 в эту независимую ветку не включены. После объединения веток
общее количество будет иным; результат выше относится к этому checkout.

Реальная P-256 подпись raw metadata и image SHA проверяются отдельной Mbed TLS,
не mock verifier. Host signer/oracle — OpenSSL, ephemeral private keys только
в RAM. Неверный signer/key/DER/signature, signed malformed schema, duplicates/
unknowns/overflow, hardware/profile/protocol/bootloader/counter mismatch,
truncated/extra/no-progress/reader failure/hash mismatch, clock/wrap/deadline/
cancel/cleanup, arena1/16/64/256bytes, reentry, caller mutation и aliases проверены.
Никакой test key не provisioned в устройство/production trust store.

Точные локальные версии — [versions.json](versions.json),
хеши проверенного кода/тестов/config/workflow — [source-hashes.json](source-hashes.json).
Shell Python и найденный CMake/CTest interpreter могут отличаться; оба записаны.
Локальный ARM GCC — Ubuntu14.2.1; Binutils отличается от официального CI archive,
это явно записано. CI сохраняет прежний официальный GCC14.2.Rel1 + archive hash.

## Команды

`TV4_LVGL_SOURCE` — LVGL9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
`TV4_MBEDTLS_SOURCE_ARCHIVE` — официальный mbedtls-3.6.7.tar.bz2 с SHA-256 из
third_party/mbedtls/origin.json. Optional archive override по-прежнему проверяется;
без него CMake скачивает тот же pinned release. Default HTTPS configure
с TLS_VERIFY TRUE и source integrity также выполнен: [default-download.log](default-download.log).

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure

cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitized --output-on-failure

cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-boot-arm -j 4
python3 tools/check_boot_admission_build.py build-boot-arm
python3 tools/check_boot_crypto_source.py --source build-boot-arm/_deps/tv4_boot_mbedtls-src \
  --lock third_party/mbedtls/origin.json
```

Sanitizers выполнялись вне ptrace с leak detection. Буферный allocator ограничен
arena; ASan не является полной проверкой границ каждого внутреннего allocation.
Case logs извлечены из фактического CTest LastTest.log, без дополнительных runs.
Команды настоящего GCC -fanalyzer -c и отрицательных gates — в новом
bootloader.yml; они выполнены и локально. Объекты static analysis не публикуются.

## Что результат не подтверждает

Compile-only static library totals включают общие upstream primitives, которые
будущий linker может удалить. Это не размер bootloader image/Flash section и не
основание резервировать sector. Global data и individual frames не равны полному
RAM/total stack budget (caller/libc/IRQ и call depth ещё нужны). 17664-byte
workspace (включая arena16384) — software ceiling, не проверка физического RAM/CCM экземпляра F407.
CPU/latency/throughput/watchdog budgets на MCU не измерены, ARM не исполнялся.

В official source archive есть4 public binary test inputs; их точные хеши
allowlisted. Это dependency inputs, не firmware outputs. Generated .bin guard
проверен на удаляемом literal test marker; реального STM32 image/ELF нет.
CI upload allowlist — только text/json, без библиотек, образов или ключей.

Устройства, UART/SWD/Flash/reset/option bytes не использовались. Raw backups,
UID/пароли не читались. Нет network/device bench, движения, installer/commit/
jump, production release/keys, выбора Flash sectors или самостоятельного merge.
Общий release manifest/SemVer/context, сохранённые bytes/vector/recovery и
fresh quiescent/operator approval gates остаются открытыми. Текущий статус
workflow смотрится у PR, не выводится из локальных логов.
