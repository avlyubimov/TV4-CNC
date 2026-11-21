# PR-07s — owned driver dispatch evidence

21 ноября 2026, Asia/Almaty. Base main
`db50840c82a6f27270b3a2ed90af198d2ec7e9bf`, PR #1–50 merged, no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_DRIVER_DISPATCH.md),
[plan](../../../plans/PR07S_DRIVER_DISPATCH.md), [commands](commands.md),
[exact versions/licenses](versions.json), [source hashes](source-hashes.json).
No devices, deployable image, actual ISR/RTOS/CAN/backend/runtime, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **68/68 PASS**, 5.02s | [summary](host-ctest.txt), [log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **68/68 PASS**, 11.38s; outside ptrace/default env/no options/suppressions | [summary](sanitizer-ctest.txt), [log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **42/42 PASS**, 1.95s | [summary](standalone-ctest.txt), [log](standalone-tests.log) |
| ThreadSanitizer | **4/4 PASS**, 0.70s, actual pthread actors/default env/no suppressions/outside ptrace | [summary](thread-sanitizer-ctest.txt), [log](thread-sanitizer-tests.log) |
| Dispatch unit | Published untaken/taken claim retention; unchecked token/fresh time/check; original admission vs publication, zero refused output, late SENT cleanup, owner context/exhaustion, sticky driver clock | [selected log](integration-summary.txt) |
| Report lifecycle | Full mailbox retains first terminal/retry after uncertain; stale callback after clear/new job; SENT forbidden before check, no implicit-current callback, no force-forget | host-tests.log/update_driver_dispatch |
| Actual two-actor roundtrip | **256 channel/peer-RX messages /8448 frames /1056 rejected duplicate callbacks**, capacities1..4/clock wrap, actual dispatch +completion paths | host-tests.log/update_driver_dispatch_threads |
| Controlled thread faults | **4 PASS**: cancel before/after take, independent driver expiry, full uncertain→retained terminal retry | integration-summary.txt |
| New signed mode | **8 positive** (6 release15+2 legacy11), **3 independent refusals**, **2 dispatch/completion lifetime refusals**; original independent verifier/live RX/lease/model/close/deadlines | host-tests.log/boot_driver_dispatch_handoff |
| Per-run signed statistics | 12067 publications=claims=returns=frames; 12067 copied reports=services; 706 rejected driver callbacks,0 stale reports in completion; 80 dropped fragments, longest4715 synthetic ms | integration-summary.txt |
| New ARM archive | **1324text/data0/bss0/frame72**,13 exports, ceiling4096/frame128;14 native DMB/no atomic helper imports | [budget](driver-dispatch-budget.json), [codegen](atomic-codegen.txt) |
| Old4 ARM libraries | **43600text/data8/bss27**, frames984/376/328/64 unchanged | [budget](arm-budget.json) |
| Existing opt-in archives | wire1219/frame32;fragment1904/frame48;metadata1568/frame48;setup3324/frame48;channel1940/frame56;egress1972/frame88;events768/frame56;mailbox1038/frame64; data/bss0 unchanged | [wire](wire-budget.json), [fragment](fragment-budget.json), [metadata](metadata-budget.json), [setup](setup-budget.json), [channel](channel-budget.json), [egress](egress-budget.json), [events](driver-events-budget.json), [mailbox](completion-mailbox-budget.json) |
| ARM guards | **7×6 previous cases +2×7 atomic archive cases PASS** | [dispatch](driver-dispatch-budget-negative.txt), [mailbox](completion-mailbox-budget-negative.txt), [events](driver-events-budget-negative.txt), [egress](egress-budget-negative.txt), [channel](channel-budget-negative.txt), [setup](setup-budget-negative.txt), [metadata](metadata-budget-negative.txt), [fragment](fragment-budget-negative.txt), [wire](wire-budget-negative.txt) |
| Actual-source analyzer | **21 embedded +2 host PASS** | [analysis](static-analysis.txt) |
| Mbed TLS3.6.7 pinned sources | **178 files PASS** | [source](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspected only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope | **PASS**, no new production handoff/runtime dependency or host pthread/tests in ARM | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local links/source hashes checked | [check](docs-check.txt) |

68=65 base +driver_dispatch_threads/driver_dispatch/boot_driver_dispatch_handoff;
standalone42=39+same3. TSan4 includes previous SPSC/actual completion owner and
new dispatch unit/thread faults. All previous modes/reference/mutation/refusal
checks remain. No executable code changed after full checks. All CTest rows
retained (including first9 double-space numbers), only trailing whitespace/final
blanks normalized. DER/frame statistics vary with RAM-only test keys; thread
retries/scheduling not golden data. Controlled independent-driver-expiry case
orders check/report before owner poll; if owner revocation wins, REVOKED is a
valid refusal instead of EXPIRED. Neither may expose payload.

New signed mode keeps PR-07q/r inputs:2 custody slots/residency16ms,2 report
slots/service1ms, fragment progress8/total256ms/frame160, driver delay1/3ms.
Duplicate callbacks every17 are now refused at the driver endpoint, not admitted
as extra completion records. All original upper windows/crypto/lease/model/close
remain; crypto CPU time is not simulated. Host link serialized, actual two-actor
handoff separately tested by pthread/TSan, not target clock/bus/CPU/ISR/soak.
Host dispatch1360/delivery48 bytes/compiler frames are not MCU RAM/full-stack approval.

Confirmed owner F407 facts unchanged: working SWD/ST-Link V2,Cortex-M4r0p1,
raw ID0x101f6413,Flash1024KiB,3.252V,backup1048576bytes/structural vectors PASS.
No new hardware observation. Full marking/package/RAM/CCM/HSE/full vector mapping/
Flash erase/protection/GPIO/CAN/physical inhibit/backup SHA/copy/restore unconfirmed.
Flash1MiB does not define RAM/loader addresses/sectors. No raw backup/device reads.

Engineering contract: one protocol actor and one driver actor, externally published
init/stable lifetime/stop+join, owned publication/take/check/return. Fresh time is
read AFTER token take; check before backend exposure/submission is not atomic
hardware cancellation. Already submitted work requires real backend reconciliation.
Published/taken/pending/uncertain claims survive cancellation and expiry until
explicit correlated terminal and consumer cleanup; no force-forget. Actual
backend/target ISR/RTOS/cache/DMA/clock/driver truth/reset, authenticated persistent
identity/routing/resources/staging/runtime/installer/operator/physical/recovery
remain open. No new wire/IDs/bitrate/heap/crypto/SDK/security/Flash/pin defaults.

Rollback: revert optional dispatch/tests/guard/docs; previous APIs/config/limits
and device state unchanged. CI proof after publication is saved outside Git to
avoid circular hashes/source rebuilds for run URLs.
