# PR-07r — SPSC completion mailbox evidence

21 ноября 2026, Asia/Almaty. Base main
`2c6fd1c76563047f617fb86c59f1d8608c4b8e3a`, PR #1–49 merged, no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_COMPLETION_MAILBOX.md),
[plan](../../../plans/PR07R_COMPLETION_MAILBOX.md), [commands](commands.md),
[exact versions/licenses](versions.json), [source hashes](source-hashes.json).
No devices, deployable image, actual ISR/RTOS/CAN driver/runtime, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **65/65 PASS**, 5.07s | [summary](host-ctest.txt), [log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **65/65 PASS**, 10.83s; outside ptrace/default env/no options/suppressions | [summary](sanitizer-ctest.txt), [log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **39/39 PASS**, 1.85s | [summary](standalone-ctest.txt), [log](standalone-tests.log) |
| ThreadSanitizer | **2/2 PASS**, 0.66s; actual pthread actors, outside ptrace/default env/no suppressions | [summary](thread-sanitizer-ctest.txt), [log](thread-sanitizer-tests.log) |
| Mailbox unit | Capacities1..4,16384 FIFO cycles/40960 owned reports, full/retry/wipe/foreign/invalid inputs, capacity3 ring wrap/no ledger changes | [selected log](integration-summary.txt) |
| Concurrent FIFO | **1200000 reports**, ordered ticket/result/contents verified; no observed races in TSan | integration-summary.txt, thread-sanitizer-tests.log |
| Concurrent actual owner | **256 actual channel/RX messages /8448 claims /9504 reports /1056 stale duplicates**, capacities1..4/clock wrap | integration-summary.txt |
| Actual custody/phase faults | Mailbox blocks begin/claim/phase with inner IDLE/empty ledger; full external terminal retry, stale after clear, delayed/uncertain/cancel/sticky cleanup, IDs/phase/atomic positions retained | host-tests.log/update_completion_owner |
| New signed mode | **8 positive** (6 release15+2 legacy11), **3 independent refusals**, **2 delayed queued terminal/uncertain refusals**; original verifier/lease/model/close/image deadlines | host-tests.log/boot_completion_mailbox_handoff |
| Per-run signed statistics | 12066 claims=returns=frames; 12772 copied reports=services/706 stale duplicates, 80 lost fragments, longest4774 synthetic ms | integration-summary.txt |
| New ARM archive | **1038text/data0/bss0/frame64**,14 exports, ceiling4096/frame128;5 native DMB/no atomic helpers | [budget](completion-mailbox-budget.json), [code generation](atomic-codegen.txt) |
| Old4 ARM libraries | **43600text/data8/bss27**, frames984/376/328/64 unchanged | [budget](arm-budget.json) |
| Existing opt-in archives | wire1219/frame32;fragment1904/frame48;metadata1568/frame48;setup3324/frame48;channel1940/frame56;egress1972/frame88;events768/frame56, data/bss0 unchanged | [wire](wire-budget.json), [fragment](fragment-budget.json), [metadata](metadata-budget.json), [setup](setup-budget.json), [channel](channel-budget.json), [egress](egress-budget.json), [events](driver-events-budget.json) |
| ARM guards | **7 previous archives ×6 methods +7 mailbox cases PASS**; new missing-barrier negative | [mailbox](completion-mailbox-budget-negative.txt), [events](driver-events-budget-negative.txt), [egress](egress-budget-negative.txt), [channel](channel-budget-negative.txt), [setup](setup-budget-negative.txt), [metadata](metadata-budget-negative.txt), [fragment](fragment-budget-negative.txt), [wire](wire-budget-negative.txt) |
| Actual-source analyzer | **20 embedded +2 host PASS** | [analysis](static-analysis.txt) |
| Mbed TLS3.6.7 pinned sources | **178 files PASS** | [source](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspected only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope | **PASS**, no new production handoff/runtime dependency, no host pthread/tests in ARM | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local links/source hashes checked | [check](docs-check.txt) |

65=61 base +completion_mailbox_threads/completion_mailbox/completion_owner/
boot_completion_mailbox_handoff; standalone39=35+same4. All previous modes/
reference/mutation/refusal tests remain. HOST metadata test flags refactored to
an enum without changing old modes. No executable code changed after full
checks. All CTest rows retained (including first9 double-space numbers); only
trailing whitespace/final blank lines normalized. Thread retries depend on OS
scheduling, DER/frame/container statistics vary with RAM-only generated test keys.

New signed mode uses PR-07q inputs unchanged:2 custody slots/residency16ms,
2 mailbox slots/service delay1ms, duplicate report every17 frames, fragment
progress8ms/total256ms/frame160. Older modes keep their own prior configuration.
All original upper windows remain unchanged; crypto CPU time is not simulated.
Host link is serialized; actual concurrency is separately tested with pthread/
TSan, not claimed as target timing or physical CAN. Host mailbox200/owner1208
bytes and individual compiler frames are not MCU RAM/full stack approval.

Confirmed owner F407 facts unchanged: working SWD/ST-Link V2, Cortex-M4r0p1,
raw ID0x101f6413,Flash1024KiB,3.252V,backup1048576bytes/structural vectors PASS.
No new hardware observation. Full marking/package/RAM/CCM/HSE/full vector mapping/
Flash erase/protection/GPIO/CAN/physical inhibit/backup SHA/copy/restore unconfirmed.
Flash1MiB does not establish RAM/bootloader addresses. Hardware facts retained.

Engineering contract: exactly one producer/consumer, externally published init,
stable lifetime/stop+join, release/acquire owned copies/reuse. No MPSC/reentrant
callbacks/live reset. Empty snapshot is not producer-stop/physical drain. Actual
terminal claim persists even with empty mailbox; later duplicate cannot free a
new claim because IDs/tickets retained. ADMITTED/SENT_LOCAL are local unverified
inputs, not authentication/remote delivery/physical readiness/install authority.
Target ISR/RTOS/cache/DMA/clock/bus/CPU/soak/real driver truth/reset/reconciliation,
persistent identity/routing/resources/staging/runtime/installer/operator/physical/
recovery remain open. No new wire/IDs/bitrate/heap/crypto/SDK/security/Flash/pin defaults.

Rollback: revert PR-07r optional mailbox/consumer/tests/guard/docs. Previous
APIs/config/limits and device state unchanged. CI proof after publication is saved
outside Git, avoiding circular hashes/source rebuilds for run URLs.
