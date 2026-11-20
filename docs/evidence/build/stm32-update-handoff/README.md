# PR-07c — результаты программных проверок

20 ноября2026 (Asia/Almaty). База main `28fe7bc8cc417ccc75e9a6db1939209b258118a1`,
PR #1–30 слиты. [Контракт](../../../STM32_UPDATE_HANDOFF.md),
[план](../../../plans/PR07C_UPDATE_HANDOFF.md). Все результаты только software.

| Проверка | Результат | Evidence |
|---|---|---|
| Root host/LVGL/logic | **25/25 PASS**,1.96s | [host-tests.log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **25/25 PASS**,4.51s, defaults без suppressions | [sanitizer-tests.log](sanitizer-tests.log) |
| Standalone bootloader/transport | **5/5 PASS**,0.47s | [standalone-tests.log](standalone-tests.log) |
| Сквозной transport → Mbed TLS | **54 signed RAM transfers**,10 lifecycle/fault groups | [handoff-groups.log](handoff-groups.log) |
| Pinned Mbed TLS | **178 source files PASS**,3.6.7 | [source-check.txt](source-check.txt) |
| ARM crypto/admission/handoff/transport | **32017 text,data8,bss27 bytes**; handoff/transport mutable globals0 | [arm-budget.json](arm-budget.json) |
| Max individual static frame | admission984,crypto376,handoff88,transport64bytes | arm-budget.json |
| Actual GCC -fanalyzer -c | admission/metadata/update_handoff **PASS** | [static-analysis.txt](static-analysis.txt) |
| Bootloader board image | Expected rejection **PASS** | [board-gate.log](board-gate.log) |
| Unexpected generated binary | Expected rejection **PASS**, marker удалён | [output-gate.txt](output-gate.txt) |
| Existing synthetic motion link | **11 cases PASS**, ARM не исполнялся | [motion-link-tests.log](motion-link-tests.log), [cases](motion-link-cases.log) |

Root25 =24 suites из объединённого main +boot_update_handoff. Старые22-suite
отчёты PR #29/#30 относятся к независимым веткам и сохранены без переписывания.
Настоящие ephemeral P-256 private keys генерируются OpenSSL только в памяти;
Mbed TLS independently проверяет signature/SHA. Ключей в evidence/файлах нет.

Проверены DATA/final ACK loss, pending и retained final ACK duplicates, разные
block/read boundaries, CRC-valid changed image, descriptor hash/signed length
mismatch, bad signature/rollback, source/boot/policy/RX changes, EOF/cleanup
invalidation, cancel/timeouts/clock/wrap, OOM, reader failures, reentry/deferred
abort, atomic acquisition failure, bad backend token, release retries/quarantine
и запрет clear/reopen с held resource. Mock immutable RAM не доказывает durable
Flash или physical quiescence. Уже скопированный receipt — не persistent token.

Точные версии и source hashes: [versions.json](versions.json),
[source-hashes.json](source-hashes.json). Локальный ARM GCC14.2.1 из Ubuntu;
Binutils отличается от pinned official CI archive14.2.Rel1, SHA записан явно.
LVGL9.2.2/Mbed TLS3.6.7/ESP dependencies не обновлялись. Shell Python3.14.4 и
CTest Python3.14.6 различаются и оба зафиксированы. Sanitizer tests вне ptrace;
LastTest.log проверен на ASan/LSan/UBSan diagnostics, suppressions не добавлялись.

## Воспроизведение

TV4_LVGL_SOURCE — LVGL commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
TV4_MBEDTLS_SOURCE_ARCHIVE — official3.6.7 archive с SHA из origin.json.
Override проверяется тем же SHA; без него CMake скачивает тот же pinned archive.

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLVGL_DIR="$TV4_LVGL_SOURCE" -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure

cmake -S . -B build-sanitized -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLVGL_DIR="$TV4_LVGL_SOURCE" -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j 4
ctest --test-dir build-sanitized --output-on-failure

cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-arm -j 4
python3 tools/check_boot_admission_build.py build-boot-arm
```

Точные static-analysis и negative image/output commands — bootloader.yml;
synthetic link commands — motion.yml. Handoff groups извлечены из успешного
CTest LastTest.log, без отдельного повторного запуска после последнего изменения.
Первый промежуточный прогон также PASS; итоговые логи соответствуют source hashes.

## Пределы результата

Object totals — сумма4 static libraries, не linked bootloader size/Flash sector.
Global data и individual frames не равны runtime RAM/CCM/total stack budget.
Host handoff state2128/receipt136/workspace17664bytes; target state assertion
<=2304bytes, не аппаратный резерв RAM. CPU/throughput/IRQ/stack/watchdog не измерены.

Нет runtime caller/Flash writer/map/install/commit/jump, CAN/GPIO/движения,
device access/SWD/UART/reset/option bytes, network bench, keys/releases или merge.
Owner hardware facts сохранены; package/HSE/RAM/erase/protection/GPIO/CAN/restore
не закрыты. На ESP32 остаётся ordinary8f0d899. Последние CI результаты смотреть
в PR: они не выводятся из локальных логов. Upload allowlist — только text/json.
