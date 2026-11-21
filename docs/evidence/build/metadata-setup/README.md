# PR-07n — metadata setup/shared namespace evidence

21 ноября2026, Asia/Almaty. Base main
`d18cf93566d849cb12e690148f970eea27ea4b0e`, PR #1–45 merged, no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_METADATA_SETUP.md),
[plan](../../../plans/PR07N_METADATA_SETUP.md), [commands](commands.md),
[exact versions/licenses](versions.json), [source hashes](source-hashes.json).
No device access, firmware image, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **54/54 PASS**,4.39s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **54/54 PASS**,9.63s, outside ptrace/default environment/no suppressions | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **28/28 PASS** | [summary](standalone-ctest.txt), [full log](standalone-tests.log) |
| Independent MS byte oracle | **4 exact172byte messages**, SHA256 `77da01788acccedaabd4bc0db47b508f53c009ad84670354ac35b0367d0200b7` | [oracle](setup-reference.txt), [bytes](setup-vectors.txt) |
| Setup lifecycle/faults | **8 consecutive image+metadata pairs**; pool held until actual metadata completion; all4 phase losses; responder take before lost CONFIRM retry; sticky context/clock; timeouts/send exhaustion/no reuse | host-tests.log/update_metadata_setup |
| Setup codec mutations | **100000**,82504 structural accepts /17496 zero-output rejects; exact re-encode, truncation/capacity/tail/invalid config | host-tests.log/update_metadata_setup |
| Raw fragments | **279 lengths8..286**, all smaller capacities, mode mismatch, single take/wipe, retained counters/time, sticky clock/context/exhaustion | host-tests.log/update_fragment_raw |
| New signed integration mode | **8 positive** (6 release15 +2 legacy11), **3 independent refusal gates** (bad signature, valid re-sign with wrong raw request digest, expired image RX) | [selected log](integration-summary.txt), host-tests.log/boot_metadata_setup_handoff |
| Real control fragment loss path | Each of11 new integrations:473 control frames,4 dropped fragments across all4 setup kinds,1020 synthetic ms; MS decode before dispatch | integration-summary.txt |
| Original DATA windows | Image RX starts before setup/metadata, no time/progress reset; full independent signature/image/lease/model/clean close | host-tests.log/boot_metadata_setup_handoff |
| New setup ARM archive | **3324text/data0/bss0/frame48**, ceiling4096/frame128 | [budget](setup-budget.json) |
| Old4 ARM libraries | **43600text/data8/bss27**, +238text for retained CLOSED/reservation; frames984/376/328/64 unchanged, old ceilings unchanged | [budget](arm-budget.json) |
| Separate fragments | **1904text/data0/bss0/frame48**, +368text for raw mode; old4096/frame128 ceiling unchanged | [budget](fragment-budget.json) |
| Existing wire/metadata | **1219/frame32** and **1568/frame48**, data/bss0 unchanged | [wire](wire-budget.json), [metadata](metadata-budget.json) |
| Archive guard methods | **6 per archive ×4 PASS**, including actual archives/temporary negative objects | [setup](setup-budget-negative.txt), [fragments](fragments-budget-negative.txt), [wire](wire-budget-negative.txt), [metadata](metadata-budget-negative.txt) |
| Actual-source strict GCC analyzer | **15 embedded +2 host units PASS** | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspection only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope gates | **PASS**, no setup runtime/production handoff dependency or host target in ARM | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local links and exact source SHA checked | [check](docs-check.txt) |

54=50 base +update_metadata_setup_reference/update_metadata_setup/update_fragment_raw/
boot_metadata_setup_handoff; standalone28=24+same4. All prior modes remain.
Logs preserve every CTest row including first9 double-space numbering; only
trailing whitespace/final blank lines normalized. DER length/frame/container
counts vary with RAM-generated signatures; selected numbers belong to this run,
not fixed golden output. Typed DATA frame stats exclude separate control link.

Setup host storage312bytes/message168, not a board RAM reservation. Compiler
individual frame is not full call stack. No CPU crypto cost/real bus latency/
throughput/scheduling/worst case/soak measurement. All timings and routes are
explicit synthetic fixture inputs, no production defaults or CAN IDs/bitrate.

Confirmed owner hardware facts remain: F407 SWD/ST-Link V2, Cortex-M4r0p1,
raw ID0x101f6413, Flash1024KiB,3.252V, backup1048576bytes/structural vectors PASS.
No new hardware observation. Full marking/package/RAM/CCM/HSE/IRQ/pins/erase/
protection/GPIO/CAN, backup SHA/independent copy/restore remain unknown. Shared
volatile ID pairing is an engineering software contract, not reboot uniqueness,
authentication/trusted context or install proof. Raw container SHA remains an
opaque expectation; independent raw digest/signature/full image checks mandatory.
Physical inhibit, production routing/identity/storage/worker/resources/installer
and operator/recovery gates remain open; no Flash addresses were reserved.

Rollback: revert this PR, restoring CLOSED wipe behaviour/API footprint and
removing the optional library/raw mode/new tests/docs. No device state or layout
changed. Commit/PR CI proof is recorded after publication outside this source
snapshot to avoid self-referential hashes/rebuilding solely for run URLs.
