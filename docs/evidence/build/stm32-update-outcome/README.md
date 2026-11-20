# PR-07e — software evidence

20 ноября2026, Asia/Almaty. База main `e0bd84e236fe88ac3f392e123641af236d8a1b0e`,
PR #1–32 слиты, пять main workflows PASS. [Контракт](../../../STM32_UPDATE_OUTCOME.md),
[план](../../../plans/PR07E_UPDATE_OUTCOME.md). Только host RAM/ARM compile.

| Проверка | Результат | Evidence |
|---|---|---|
| Root/LVGL/logic | **27/27 PASS**,2.17s | [host-tests.log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **27/27 PASS**,4.80s, без suppressions | [sanitizer-tests.log](sanitizer-tests.log) |
| Standalone bootloader/transport | **7/7 PASS**,0.61s | [standalone-tests.log](standalone-tests.log) |
| Outcome/ACK | **6 groups,1001 sessions**, lost report/final ACK, single delivery | [outcome-groups.log](outcome-groups.log) |
| Boot completion integration | **17 groups**,6 new signed full-chain sessions + negative/reentry/cleanup | [completion-groups.log](completion-groups.log) |
| ARM GCC14.2.1 | **36507text,data8,bss27bytes**,4 static libraries | [arm-budget.json](arm-budget.json) |
| Individual frames | admission984,crypto376,handoff232,transport64bytes | arm-budget.json, [new frames](new-frames.txt) |
| DATA+outcome objects | **1924+1588 text**,data/bss0,8KiB budget PASS | [transport-budget.txt](transport-budget.txt) |
| Protocol imports | libc memory helpers only | [transport-imports.txt](transport-imports.txt) |
| Actual GCC15.2.0 -fanalyzer -c | **7 own C sources PASS**, all warnings as errors | [static-analysis.txt](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source-check.txt](source-check.txt) |
| Deployable image/output gates | Expected rejection **PASS** | [board-gate.log](board-gate.log), [output-gate.txt](output-gate.txt) |
| Existing synthetic motion ARM link | **11 cases PASS**, no execution | [motion-link-tests.log](motion-link-tests.log), [cases](motion-link-cases.log) |

Root27 =26 main suites +update_outcome. Handoff/completion17 includes прежние
54 signed DATA transfers/15 vector inspections и6 новых DATA→P-256/SHA→model→
close→report/ACK sessions. Ephemeral OpenSSL test keys живут только в памяти;
независимая Mbed TLS проверяет их подписи. Private keys/firmware bytes не публикуются.

Fault tests: image/raw metadata binding, canonical result/reason pairs, mutated
caller snapshots, loss/duplicates/final ACK, conflicting reports, bad ACK,
retry/linger/total/late begin, clock/wrap/abort/transport reboot. Completion:
actual signature/hash/profile/vector failure, metadata hash mismatch, cancel,
post-close owner/cancel/deadline, DATA RX expiry, reentry/deferred abort,
pending release -> negative after eventual close и quarantine без clear/handle loss.
Matching peer reports CAN be forged; explicit test подтверждает границу:
это не authenticated/local verified/physical/install authority.

Host completion568bytes, report84bytes, report TX128/RX224bytes; handoff2136 и
borrowed workspace17664bytes прежние. Новых mutable globals/heap/image buffer
нет. Object totals/individual frames — regression ceilings, не linked firmware,
actual Flash sectors или полный RAM/CCM/stack/CPU/throughput/watchdog budget.

Точные версии — [versions.json](versions.json); source/test/config/workflow
hashes — [source-hashes.json](source-hashes.json). Local Ubuntu ARM GCC14.2.1/
Binutils явно отличаются от official pinned CI archive. Mbed TLS3.6.7/LVGL9.2.2/
ESP SDK locks прежние. Shell Python3.14.4 и CTest3.14.6 записаны отдельно.
Sanitizers вне ptrace, defaults без suppressions; LastTest.log проверен на
ASan/LSan/UBSan diagnostics. Case logs извлечены из успешного CTest.

## Воспроизведение

TV4_LVGL_SOURCE — commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
TV4_MBEDTLS_SOURCE_ARCHIVE — official3.6.7 archive с SHA из origin.json;
optional override всё равно проверяется, default configure использует тот же source.

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

Exact analyzer/gate commands — bootloader.yml/motion.yml; protocol analysis
теперь тоже actual -c для обоих C sources, frames проверяются для обоих units.
Все8 SDK profiles сохранены; CI результат смотреть в PR, не выводить из local
logs. Artifact allowlists — text/JSON only. Старые evidence counts не переписаны.
Нет device/SWD/UART/Flash/reset/option bytes/backup reads/network bench/motion/CAN,
actual MCU profile/Flash reservation/writer/commit/jump, production keys/release или
самостоятельного merge. На ESP32 ordinary8f0d899. Hardware/production gates открыты.
