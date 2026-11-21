# PR-07l — shared-clock fragment integration evidence

21 ноября2026, Asia/Almaty. Base main
`277801e4e876b751c83c424b97311f2f19a576f0`, PR #1–43 merged, no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_UPDATE_TIMING.md),
[plan](../../../plans/PR07L_FRAGMENT_TIMING.md), [versions/licenses](versions.json),
[source hashes](source-hashes.json). No devices or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **47/47 PASS**,4.30s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **47/47 PASS**,9.32s, default env/no suppressions outside ptrace | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **21/21 PASS**,1.15s | [summary](standalone-ctest.txt), [full log](standalone-tests.log) |
| Shared-clock DATA | **34 scenarios**,40700frames/43drops/31 timeout clears/8464 repeated peeks | host-tests.log/update_fragment_timing |
| Shared-clock signed chains | **8 positive**,6 release15 +2 legacy11;7640frames/48drops/32 timeout clears/1611 repeated peeks | host-tests.log/boot_fragment_timing |
| Admission refusals | Invalid signature after delivery; expired DATA before lease acquire | host-tests.log/boot_fragment_timing |
| Existing4 ARM libraries | **43362text/data8/bss27**, frames984/376/328/64, unchanged | [budget](arm-budget.json) |
| Separate codec | **1219text/data0/bss0/frame32**, unchanged | [budget](wire-budget.json) |
| Separate fragments | **1536text/data0/bss0/frame48**, unchanged | [budget](fragment-budget.json) |
| Actual-source GCC analyzer | **13 embedded +2 host units PASS** | [analysis](static-analysis.txt) |
| Archive/import/frame guards | **6 old codec +6 fragment methods PASS** | [codec](wire-budget-negative.txt), [fragments](fragment-budget-negative.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspect only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope gates | **PASS**, host timing targets absent in both ARM projects | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt), [protocol scope](protocol-scope.txt) |
| Documentation/source | Checked links and source SHA-256 | [check](docs-check.txt) |

47 =45 base +update_fragment_timing/boot_fragment_timing; standalone21 =19 +same2.
Old typed/byte/fragment modes remain. Test result rows are complete, including
CTest first9 double-space numbering. Logs normalized only for trailing whitespace
and final empty lines. ASAN_OPTIONS/LSAN_OPTIONS/UBSAN_OPTIONS were absent.

## Software findings and actual scope

The HOST-only test link uses the same exclusive caller clock as upper negotiation,
DATA, outcome, source lease and actual independent verifier now_ms. Queue/delivery/
drain delays consume that clock. One slot per direction bounds local pressure;
the owner only commits a frame after local queue acceptance and takes complete
bytes once. Synthetic drops/duplicates/holds neither reset IDs nor extend upper
started/progress deadlines. Fragment timeout recovery is explicit test policy,
not a firmware feature. No driver/ISR/sleep/thread/heap/runtime integration added.

34 DATA scenarios cover block sizes1/31/256, lost START/middle/final DATA/ACK,
both kinds together, wrap, repeated peek/queue hold, original retry exhaustion,
and exact consumer write count. Dropped START leaves RX idle and needs no timeout;
dropped continuation leaves an active RX that must time out/clear before retry.
READY total128ms and queue progress8ms expire exactly. Delayed ACK at original
DATA total300ms cannot revive failed TX. Negotiation expires at50ms while
fragments still advance; a late valid OFFER does not restart the old requester.
Context/backward-clock remain sticky. A fresh local generation accepts an old
START in the explicit negative test: reboot replay isolation is still missing.

Eight positive actual signed RAM chains cross all8 message kinds; six lose one
fragment of every kind. Whole-message retries retain original upper deadlines.
Independent Mbed TLS P-256/full SHA/model/live lease/clean close remain real;
staging consumer and historical outcome deliver only once. Source bytes are
wiped before the report/ACK exchange. Signature corruption after transfer and
expired live DATA still refuse admission, the latter before acquire.

The longest positive signed chain is2551 **synthetic** ms. It is not measured
throughput/bus/CPU/latency or a performance regression. Queue delays and upper
limits are explicit per-scenario inputs chosen before begin, not new defaults.
Crypto CPU/storage duration is not measured/charged; actual now_ms uses the same
clock, but this model is not a worst-case runtime resource/scheduling proof.
Raw signed metadata/signature still enter from fixture directly, not via frames.
Test keys remain RAM-only; no private files/production keys or binary artifacts.

## Confirmed hardware, assumptions and blockers

Owner SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3.252V/
backup1048576bytes/vector structure PASS are preserved. No new reads/measurements.
Full physical marking/HSE/RAM/CCM/IRQ/Flash geometry/protection/GPIO/CAN/backup
SHA/second copy/restore remain unconfirmed. Flash capacity is not a RAM budget
or bootloader address. ESP32 ordinary8f0d899 remains the last owner-confirmed
HMI/touch/Wi-Fi baseline. No device/SWD/UART/reset/Flash/backup reread performed.

Local route/generation, fixture IDs and timing inputs are engineering assumptions,
not authenticated production identities/defaults or physical safe-state proof.
Production routing/reboot identity/authentication, metadata transport, immutable
production staging, measured queue/bus/crypto budgets, actual trust/context,
physical/operator/recovery/installer gates remain. No Flash sectors/addresses
reserved, no real board image enabled, no keys/releases/tags/secrets/self-merge.

## Reproduce

Exact dependencies/toolchains/actions are pinned as before; use explicit reviewed
LVGL source and official SHA-verified Mbed TLS archive paths:

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j4
ctest --test-dir build-host -V
cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j4
ctest --test-dir build-sanitized -V
cmake -S firmware/stm32-bootloader -B build-boot-host -G Ninja \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-host -j4
ctest --test-dir build-boot-host -V
cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-arm -j4
python3 tools/check_boot_admission_build.py build-boot-arm
python3 tools/check_update_wire_build.py build-boot-arm/update-transport
python3 tools/check_update_fragment_build.py build-boot-arm/update-transport
```

CI adds actual-source analyzer of host fixture and ARM host-target exclusions.
Existing archive/negative/source/board/output/synthetic link gates remain; both
ESP-IDF versions ×4 exact profiles/actions/locks are preserved. Uploads text only,
no local ESP rebuild for a host-only change. Head CI results belong to the PR
Checks, not the historical base result above. Rollback: revert PR, no device writes.
