# PR-07m — bounded metadata transport evidence

21 ноября2026, Asia/Almaty. Base main
`7640f432f540eec0162afdf93989a76cc5ee3d33`, PR #1–44 merged, no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_METADATA_TRANSPORT.md),
[plan](../../../plans/PR07M_METADATA_TRANSPORT.md), [versions/licenses](versions.json),
[source hashes](source-hashes.json). No devices, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **50/50 PASS**,4.29s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **50/50 PASS**,9.46s, outside ptrace/default env/no suppressions | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **24/24 PASS**,1.19s | [summary](standalone-ctest.txt), [full log](standalone-tests.log) |
| Independent container oracle | **2 exact bundles**,98/1215bytes | [oracle](metadata-reference.txt), [actual bytes](metadata-vectors.txt) |
| Document/receiver bounds | **1024 document lengths PASS**; final ACK loss/duplicates after take/owned snapshot after discard | host-tests.log/update_metadata |
| Malformed input | Every truncation/capacity/header/binding/field range; complete malformed RX failure/wipe;100000 mutations | host-tests.log/update_metadata |
| Mutation results | **61930 structural accepts /38070 rejected and zeroed**, not crypto acceptance | host-tests.log/update_metadata |
| Real delivered-metadata chains | **8 positive**,6 release15 +2 legacy11; original producer wiped | host-tests.log/boot_metadata_handoff |
| Independent refusal gates | Delivered invalid signature, correctly re-signed wrong raw digest, expired separate image RX | host-tests.log/boot_metadata_handoff |
| New separate ARM archive | **1568text/data0/bss0/frame48** | [budget](metadata-budget.json) |
| Old4 ARM libraries | **43362text/data8/bss27/frames984,376,328,64**, unchanged | [budget](arm-budget.json) |
| Separate codec/fragments | **1219/frame32**, **1536/frame48**, data/bss0, unchanged | [codec](wire-budget.json), [fragments](fragment-budget.json) |
| Archive guard methods | **6 metadata +6 codec +6 fragments PASS**, actual archives/temporary negative objects | [metadata](metadata-budget-negative.txt), [codec](wire-budget-negative.txt), [fragments](fragment-budget-negative.txt) |
| Actual-source strict GCC analyzer | **14 embedded +2 host units PASS** | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 source | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspect only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope gates | **PASS**, no new MCU caller/production handoff link or host test leakage | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local links/source SHA-256 checked | [check](docs-check.txt) |

50 =47 base +update_metadata_reference/update_metadata/boot_metadata_handoff;
standalone24 =21 +same3. Old modes remain. All test rows retained, including
CTest first9 double-space numbering. Logs normalized only for trailing whitespace
and final empty lines. ASAN_OPTIONS/LSAN_OPTIONS/UBSAN_OPTIONS were absent.

## What changed and what was verified

Separate opt-in codec/RX library around unchanged DATA/CRC/pending/consume/ACK.
Container is counted raw document/signature/key ID with exact LE image/request
binding, no C ABI/padding/JSON rewrite. Descriptor setup is an explicit local
input, not an authenticated on-wire discovery protocol. Metadata ID differs
from image ID; both wrong-role directions reject before copy. Own high-water,
generation and monotonic clock persist across take/discard, no live reinit or ID
wrap. Global cross-role freshness/persistence still needs an external provider.

ACK only follows copy/consume. Exact duplicates never repeat writes/progress.
Take copies one owned TAKEN_UNVERIFIED snapshot, wipes aggregate bytes, retains
only the last DATA block for bounded ACK retries. Discard wipes the cache too;
caller output survives discard. Completed malformed container zeroes output and
wipes storage; recovery requires explicit discard/fresh ID. Exact progress/
total/ready deadlines, slow progress without total extension, clock wrap/backward,
sticky generation cancellation, CRC and role isolation pass.

Independent Python stdlib constructs its own header/fields/bodies, without
importing C headers/using C to generate expected bytes. Golden SHA-256:
`e8bffb2885cc650c2be225dfb8f1956f8f2244e081361c8fd11ed7c67603e8d0`.
Vectors contain fake IDs/opaque digest/signature bytes, not CAN IDs, keys or
executable images. Structural acceptance of altered raw bytes is intentional:
container/transport does not verify raw SHA/bundle SHA/signature or confer trust.

New --metadata-staging uses the existing shared-clock byte/fragment link for
actual metadata bytes before actual image transfer. Producer arrays are wiped;
the independent Mbed verifier reads delivered snapshot arrays. Original image
RX starts before metadata and retains its original windows. Eight real signed
chains check P-256/full image SHA/raw request digest/model/live lease/clean close/
post-wipe historical outcome. Three refusal cases keep signature/binding/expired
image RX gates effective, the last before source acquire. No new crypto owner/
arena/heap/default key or actual release context. Private test keys stay in RAM.

DER length varies with ephemeral signatures, affecting frame/container totals.
These are per-run observations, **not fixed golden/performance assertions**:
[statistics](signed-run-statistics.json). Longest positive chain was2804 synthetic
ms in each recorded run; CPU crypto/storage/bus duration was not measured/charged.
Fixture timing inputs are not production defaults, scheduling or hardware budgets.

## Hardware facts, assumptions and blockers

Owner SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3.252V/
backup1048576bytes/vector structure PASS preserved; no new reads/measurements.
Full physical marking/HSE/RAM/CCM/IRQ/erase/protection/GPIO/CAN/backup SHA/
second copy/restore remain unconfirmed. Flash capacity does not select RAM,
image addresses or bootloader sectors. ESP32 ordinary8f0d899 remains last
owner-confirmed HMI/touch/Wi-Fi baseline. No SWD/UART/reset/Flash/backup reread.

Host receiver1728/snapshot1212bytes and ARM compile bounds<=2048/1280 are not
actual MCU RAM reserves. Object/frame figures are not final firmware/full stack/
CPU/performance approval. Existing runtime/MCU callers/old core C/API/configs/
partitions/dependencies stay unchanged. New library is separately compiled, not
linked into production handoff/app. No images/install/COMMIT/jump/ARM actions.

Production authenticated setup/descriptor exchange/global ID provider/routing,
durable immutable staging, measured queue/bus/crypto budgets, actual trust/context,
physical/operator/recovery/installer gates remain. No Flash addresses reserved,
no releases/tags/secrets/self-merge. Rollback is a PR revert, no device writes.

## Reproduce

Exact dependencies/toolchains/actions are pinned as before. Use explicit reviewed
LVGL9.2.2 source and official SHA-verified Mbed TLS3.6.7 archive paths:

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
python3 tools/check_update_metadata_build.py build-boot-arm/update-transport
python3 protocol/can/tests/wire_budget_tests.py --metadata --build build-boot-arm/update-transport
```

CI adds metadata archive/import/global/frame guard, negative cases, sixth protocol
unit analysis and required contract document. Host test exclusions include the
new metadata executable in both ARM projects. Exact8 ESP-IDF profiles/actions/
locks and text-only uploads remain; no local ESP rebuild for an opt-in library
without runtime integration. New head CI belongs to PR Checks, not base CI above.
