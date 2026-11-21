# PR-07q — driver completion event inbox evidence

21 ноября 2026, Asia/Almaty. Base main
`efa27275a9de3b0d9c6dc1dd47c4ae10175edbb7`, PR #1–48 merged; no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_DRIVER_EVENTS.md),
[plan](../../../plans/PR07Q_DRIVER_EVENTS.md), [commands](commands.md),
[versions/licenses](versions.json), [source hashes](source-hashes.json).
No devices, deployable firmware, RTOS/ISR/real driver/runtime, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **61/61 PASS**, 4.90s | [summary](host-ctest.txt), [log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **61/61 PASS**, 10.23s; outside ptrace/default env/no options or suppressions | [summary](sanitizer-ctest.txt), [log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **35/35 PASS**, 1.90s | [summary](standalone-ctest.txt), [log](standalone-tests.log) |
| Event owner unit | **256 actual channel/RX messages /8448 frames/21120 reports**, capacities1..4, FIFO/clock wrap, owned copies, one-event service | [selected log](integration-summary.txt) |
| Lifetime/refusal unit | Full inbox external retained terminal retry, duplicate/stale/foreign tickets, actual inbox phase guards, delayed current-time expiry, uncertainty/cancel/sticky cleanup | host-tests.log/update_driver_events |
| New signed mode | **8 positive** (6 release15+2 legacy11), **3 independent refusal gates** (signature/re-signed wrong raw digest/expired image), **2 delayed queued event refusals** | host-tests.log/boot_driver_events_handoff |
| Per-run new mode | 12067 claims=returns=frames; 12773 posted=serviced reports, 706 stale duplicates; max inbox2, 80 dropped fragments; longest4774 synthetic ms | integration-summary.txt |
| Upper authority retained | Producer wiped; original independent delivered signature/raw digest/full image/live RX/lease/model/clean close; original image started/progress/total retained | host-tests.log/boot_driver_events_handoff |
| New separate ARM archive | **768text/data0/bss0/frame56**,10 exports, ceiling4096/frame128 | [budget](driver-events-budget.json) |
| Old4 ARM libraries | **43600text/data8/bss27**, frames984/376/328/64, unchanged | [budget](arm-budget.json) |
| Existing opt-in archives | wire1219/frame32;fragments1904/frame48;metadata1568/frame48;setup3324/frame48;channel1940/frame56;egress1972/frame88, data/bss0, unchanged | [wire](wire-budget.json), [fragments](fragment-budget.json), [metadata](metadata-budget.json), [setup](setup-budget.json), [channel](channel-budget.json), [egress](egress-budget.json) |
| Guard cases | **7 archives ×6 methods PASS** | [new events](driver-events-budget-negative.txt), [egress](egress-budget-negative.txt), [channel](channel-budget-negative.txt), [setup](setup-budget-negative.txt), [metadata](metadata-budget-negative.txt), [fragment](fragment-budget-negative.txt), [wire](wire-budget-negative.txt) |
| Actual-source analyzer | **19 embedded +2 host PASS** | [analysis](static-analysis.txt), [corrected local invocation](analysis-invocation.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspected only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope | **PASS**, no new production runtime/handoff dependency or host fixture/test targets in ARM | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local links/source hashes checked | [check](docs-check.txt) |

61=59 base +update_driver_events/boot_driver_events_handoff; standalone35=33+same2.
All old modes/reference/mutation/refusal tests remain. No executable C changed
following these full suites. Logs retain all CTest rows including first9
extra-space numbering; trailing whitespace/final blanks normalized only. Test
private keys RAM-only; DER/frame/container statistics vary, not golden counts.

New mode explicitly adds1ms event service delay/2 event slots and duplicate local
reports every17 frames. Fragment total256ms is configured before init for this
new mode versus128ms in old modes; progress8ms/frames160 and all original upper
limits/windows remain unchanged. Custody2slots/residency16ms unchanged. These
are synthetic test inputs, not production deadlines/RTOS queue reserves. No
measured CPU/bus/worker/physical latency/worst-case/soak. Event32/owner968 host
bytes and individual compiler frames are not MCU RAM/full-stack approval.

Confirmed owner F407 facts unchanged: working SWD/ST-Link V2, Cortex-M4r0p1,
raw ID0x101f6413, Flash1024KiB,3.252V,backup1048576bytes/structural vectors PASS.
No new hardware observation. Full marking/package/RAM/CCM/HSE/full vector mapping/
Flash erase/protection/GPIO/CAN/physical inhibit/backup SHA/copy/restore remain
unconfirmed. Flash1MiB does not establish RAM or bootloader addresses/sectors.

Engineering contract is serialized local report custody only. ADMITTED is an
owned copy, SENT_LOCAL is unverified driver input; neither remote ACK/authentication,
physical safe state nor installation approval. Real synchronization/ISR/RTOS
adapter, truthful completion provenance/reset/reconciliation, persistent identity/
routing/resources/staging/runtime/installer/operator/physical/recovery gates remain
open. Missing report keeps actual claim blocked; no force-forget or flush shortcut.
No new wire bytes/version/IDs/bitrate, heap/crypto owner/SDK/security/Flash/pin defaults.

Rollback: revert PR-07q, removing optional event owner/new test mode/guard/docs;
old APIs/code/config/limits and hardware state unchanged. CI proof captured after
publication outside Git to avoid circular hashes/source rebuilds for run URLs.
