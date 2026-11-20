# PR-07d — software evidence

20 ноября2026, Asia/Almaty. База main `4b7631e6188a73c23a2b92ee497876a5d26368d7`,
PR #1–31 слиты, main пять workflows PASS. [Контракт](../../../STM32_IMAGE_PREFLIGHT.md),
[план](../../../plans/PR07D_IMAGE_PREFLIGHT.md). Это не аппаратное испытание.

| Проверка | Результат | Evidence |
|---|---|---|
| Root/LVGL/logic | **26/26 PASS**,2.13s | [host-tests.log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **26/26 PASS**,4.65s, без suppressions | [sanitizer-tests.log](sanitizer-tests.log) |
| Standalone bootloader/transport | **6/6 PASS**,0.54s | [standalone-tests.log](standalone-tests.log) |
| Profile validation | **6 groups**,241 table sizes, every slot in256-entry table | [profile-groups.log](profile-groups.log) |
| Signed image + lease + vectors | **13 groups**,54 prior transfers +15 signed inspections/snapshot | [handoff-groups.log](handoff-groups.log) |
| Mbed TLS3.6.7 integrity | **178 source files PASS** | [source-check.txt](source-check.txt) |
| ARM GCC14.2.1 | **33359 text,data8,bss27 bytes** across4 libraries | [arm-budget.json](arm-budget.json) |
| Individual frames | admission984,crypto376,handoff232,transport64bytes | arm-budget.json, [new frames](preflight-frames.txt) |
| GCC15.2.0 -fanalyzer -c | **4 own C sources PASS**, warnings as errors | [static-analysis.txt](static-analysis.txt) |
| Deployable image/output gates | Expected rejection **PASS** | [board-gate.log](board-gate.log), [output-gate.txt](output-gate.txt) |
| Synthetic motion ARM link | **11 cases PASS**, not executed | [motion-link-tests.log](motion-link-tests.log), [cases](motion-link-cases.log) |

26 suites =25 из main +boot_image_profile. Предыдущие evidence не переписываются:
их counts/sizes относятся к соответствующим историческим source snapshots.
Fixture keys ephemeral в памяти OpenSSL; подписанные bytes независимо проверяет
Mbed TLS. Private keys не записываются в файлы/evidence или устройство.

Проверены unknown/zero profile, alignment/count/mask/size bounds, overlapping и
adjacent RAM/gaps, top-of-bank/minimum stack/overflow, reserved slots, null/even/
outside/image-table/RAM/truncated Thumb target, exact signed image length,
aliased output и отсутствие утечки trailing ID bytes. Интеграция: невозможно
начать preflight до signature/SHA, valid signature с invalid vectors отзывается,
profile snapshot/ID binding, source/owner/RX/deadline/cancel/reentry changes,
cleanup guard, scratch wipe и release retry/quarantine с сохранённым token.

Profile116bytes, inspection44bytes, handoff2136bytes, borrowed workspace17664bytes
на host. Не добавлено mutable globals или heap/image buffer. Target state
assertion<=2304bytes; total runtime RAM/CCM/stack/CPU/throughput не измерены.
Object totals — не linked image/Flash reservation. Read-only range check не
проверяет инструкции, memory attributes или физическую доступность адресов.

Точные версии: [versions.json](versions.json). Code/test/config/workflow hashes:
[source-hashes.json](source-hashes.json). Ubuntu ARM GCC14.2.1 и local Binutils
отличаются от official CI archive; его точный SHA закреплён прежним значением.
Mbed TLS3.6.7/LVGL9.2.2/ESP locks не обновлялись. Python shell3.14.4 и найденный
CTest3.14.6 записаны отдельно. Sanitizers вне ptrace, defaults без suppressions;
LastTest.log проверен на ASan/LSan/UBSan diagnostics. Case logs извлечены из CTest.

## Команды

TV4_LVGL_SOURCE — commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
TV4_MBEDTLS_SOURCE_ARCHIVE — official3.6.7 archive, SHA из origin.json.
Override всё равно проверяется; без него CMake загружает тот же pinned source.

```bash
set -euo pipefail
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

Static analysis/negative output commands — bootloader.yml, motion link — motion.yml.
Все8 ESP SDK jobs сохраняются; актуальный CI результат смотреть в PR, он не
выводится из local tests. Upload allowlists только text/json, не binary/keys.
Ни actual board profile/default address, ни Flash writer/commit/jump не добавлены.
Device/SWD/UART/reset/Flash/option bytes/backup reads/network bench/motion/CAN
hardware не использовались. Owner facts сохранены; actual RAM/HSE/package/IRQ/
erase/protection/GPIO/CAN/restore остаются blockers. На ESP32 ordinary8f0d899.
Нет production keys/releases/signing или самостоятельного merge.
