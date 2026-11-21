# PR-07k — bounded fragments evidence

21 ноября2026, Asia/Almaty. Base main
`4db99df1e8bdc842e99ad87f609468f7ced72dda`, PR #1–42 merged; no open PR before
this stage. [Main CI5/5](base-ci.json). [Contract](../../../STM32_UPDATE_FRAGMENTS.md),
[plan](../../../plans/PR-07K-update-fragments.md), [versions/licenses](versions.json),
[tested source hashes](source-hashes.json). No devices or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **45/45 PASS**,4.14s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **45/45 PASS**,9.20s, outside ptrace/default env/no suppressions | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **19/19 PASS** | [summary](standalone-ctest.txt), [full log](standalone-tests.log) |
| Independent envelope oracle | **3 full sequences PASS**, ACK24/DATA31/DATA286 | [oracle](fragment-reference.txt), [actual vectors](fragment-vectors.txt) |
| All kinds/DATA lengths | **263 roundtrips PASS**, up to72frames | host-tests.log/update_fragment |
| Malformed fields | **256 DLC /32768 START lengths /65536 continuation fields PASS** | host-tests.log/update_fragment |
| Deterministic faults | **100000 chains**,34470 structural takes/65530 rejected with zero output | host-tests.log/update_fragment |
| Actual signed RAM integration | **8 signed chains**,22980frames/1252 exact duplicates, original negative gates | host-tests.log/boot_fragment_handoff |
| New separate ARM library | **1536text/data0/bss0/frame48** | [budget](fragment-budget.json), [standalone protocol budget](fragment-protocol-budget.json), [objects](fragment-size.txt), [frames](fragment-frames.txt) |
| Old4 ARM libraries | **43362text/data8/bss27/frames984,376,328,64**, unchanged | [budget](arm-budget.json), [objects](admission-size.txt) |
| Existing separate codec | **1219text/data0/bss0/frame32**, unchanged | [budget](wire-budget.json) |
| Negative archive/frame/import guards | **6 fragment +6 old codec methods PASS** | [fragment](fragment-budget-negative.txt), [codec](wire-budget-negative.txt) |
| Actual-source strict GCC analyzer | **13 embedded +1 host PASS** | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 source | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, compile/inspect only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/runtime gates | **PASS**, temporary marker removed | [boot gate](board-gate.txt), [motion gates](motion-board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local links/source digests checked | [check](docs-check.txt) |

45 =42 base +update_fragment_reference/update_fragment/boot_fragment_handoff;
standalone19 =16 base +same3. Full test rows retained, including first9 with
CTest double-space numbering. Logs normalized only for trailing whitespace and
final empty lines. Sanitizer environment overrides are absent in versions.json.
The integration report newline was corrected before the final logged reruns;
no library changes were needed after the first full successful run.

## What the checks prove

Owned TX snapshot/repeated deterministic peek/exact queue commit; local commit
is not remote delivery. RX order/missing frames, exact/conflicting duplicates,
other route/ID isolation, no silent restart, high-water/counter retention,
non-wrapping exhaustion, frame quota, READY backpressure and single codec take.
Whole-message loss test explicitly waits timeout, clears and retries a fresh ID.
Exact progress/total/READY deadlines, duplicate drip/peek without progress,
wrap/backward clock/generation revocation, sticky lifetime faults, cancellation
and buffer/output zeroing pass. Malformed inner packet reaches unchecked READY
then fails decode. Corrupted DATA can take successfully but the original logical
RX rejects CRC before progress: fragment/codec shape is not integrity/authentication.

Independent Python stdlib packs its own complete codec fixtures and LE envelope,
without importing C headers/using C to generate expected bytes. Vector SHA-256:
`0e2abb4d14cacfd05d66f9c4e021efab0395643230ed6e689bc7b00364b9ec8a`.
Vectors are fake IDs/opaque digests/data, not CAN arbitration IDs, real boot
epochs, keys or executable images.100000 fault chains include DLC corruption,
byte mutation, missing frames and exact duplicate insertion.34470 structural
takes do not imply authentication of the mutated data.

The existing4 staged integration groups run in new --fragment-staging mode:
6 release15 +2 legacy11 real independent Mbed TLS signature/full-image SHA/
model/lease/clean close chains, plus crypto/binding/cleanup/quarantine/revocation/
receiver deadline negatives. All8 message kinds cross fragments with counts
39/22/22/17/1017/500/16/8. Raw signed metadata/signature still enter from fixture
directly; this PR does not implement their network transport. Transport clock
is a separate host fixture, not an on-bus latency/logical deadline composition
measurement. Existing typed/byte modes and production dependencies are unchanged.
Private test keys stay in RAM; no key files/binary artifacts were published.

## Hardware facts, assumptions and blockers

Owner confirms SWD/ST-Link V2, Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB,
3.252V, backup1048576bytes and structural vector PASS. No new reads/measurements.
Full physical marking/package/HSE/RAM/CCM/IRQ/Flash geometry/protection/GPIO/CAN/
backup SHA/second copy/restore remain unconfirmed. Flash capacity does not select
RAM size, bootloader sectors or image addresses. ESP32 ordinary8f0d899 remains
the last owner-confirmed HMI/touch/Wi-Fi baseline. No firmware was written.

Envelope v1/header/counter/route/generation are engineering draft choices,
not a production bus contract. Local identity is not serialized/authenticated;
reboot replay isolation/routing/queue flush provider do not exist. Production
metadata transport, whole-message retry/worker/bus budget, identity/authentication,
durable staging, trust/context, physical/operator/recovery/installer gates remain.
Host TX360/RX352 and ARM each<=384 compile bound are not approved MCU RAM reserves.
1536text and frame48 are object/compiler figures, not final firmware/full stack/
CPU/latency/throughput/soak results. Old allocator/workspace/source/SDK pins and
all existing object/frame limits remain unchanged.

## Reproduce

Use the exact LVGL9.2.2 and official hash-verified Mbed TLS3.6.7 source archive
from versions.json, with explicit local paths:

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
cmake -S protocol/can -B build-update-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-update-arm -j4
python3 tools/check_update_fragment_build.py build-update-arm
python3 protocol/can/tests/wire_budget_tests.py --fragments --build build-update-arm
python3 tools/check_update_wire_build.py build-update-arm
python3 protocol/can/tests/wire_budget_tests.py --build build-update-arm
```

Boot ARM/source/board/output and motion synthetic-link gates use the existing
workflows. Motion CI adds actual analysis of the fifth protocol unit and the
fragment guard/negative checks; boot CI adds separate fragment archive guard.
Artifacts remain text only. Exact ESP-IDF6.1.1/5.3.2 ×4 profiles/actions/locks
are unchanged. No local ESP rebuild needed for this opt-in library stage;
new head CI results belong to the PR Checks, not the historical base CI above.

No devices/SWD/UART/reset/Flash/backup rereads/CAN/motor/installer/releases/tags/
secrets or self-merge. Rollback is a PR revert without device writes/layout changes.
