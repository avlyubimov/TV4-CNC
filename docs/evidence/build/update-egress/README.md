# PR-07p — bounded egress custody evidence

21 ноября 2026, Asia/Almaty. Base main
`555007d3ad48bf87fe47f0233634e184e7ae938f`, PR #1–47 merged; no open PR at start.
[Base CI 5/5](base-ci.json), [contract](../../../STM32_UPDATE_EGRESS.md),
[plan](../../../plans/PR07P_UPDATE_EGRESS.md), [commands](commands.md),
[exact toolchains/dependencies/licenses](versions.json), [source hashes](source-hashes.json).
No devices, deployable firmware image, real driver/runtime, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **59/59 PASS**, 4.43s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **59/59 PASS**, 10.03s; outside ptrace/default env/no options or suppressions | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **33/33 PASS**, 1.36s | [summary](standalone-ctest.txt), [full log](standalone-tests.log) |
| Queue lifetime unit | Capacities 1..4, **4096 FIFO cycles/10240 frames**; original residency, cancel/uncertainty/late return, retained counters, no wrap | [selected log](integration-summary.txt), host-tests.log/update_frame_queue |
| Ticket correlation | Exact actual queue address + route + generation + serial; duplicate/stale/cross-queue reports cannot free another claim | host-tests.log/update_frame_queue |
| Bound egress unit | **132 frames**, capacities 1..4; actual channel → owned queue → driver copy → exact return → RX; final held frame blocks phase even with inner channel IDLE | host-tests.log/update_egress |
| Shared signed mode | **8 positive** chains (6 release15 +2 legacy11), **3 independent refusal gates** (signature, correctly re-signed wrong raw digest, expired image RX) | host-tests.log/boot_egress_handoff |
| Driver lifetime refusals | **2 PASS**: uncertain/missing completion retains actual claim through original image timeout; late return and queue clear cannot revive RX/admission | integration-summary.txt |
| Per-run signed statistics | 12067 claims = returns = frames, 80 dropped fragments, 4797 container bytes, DER70..72; longest3710 synthetic ms, max pending2 | integration-summary.txt |
| Independent upper authority | Producer arrays wiped; delivered snapshot verified by original signature/raw digest/full image/live RX/lease/model/clean close; original image windows retained | host-tests.log/boot_egress_handoff |
| New separate egress ARM archive | **1972text/data0/bss0/frame88**, 17 exports; ceiling4096/frame128 | [budget](egress-budget.json) |
| Old4 ARM libraries | **43600text/data8/bss27**, frames984/376/328/64, unchanged | [budget](arm-budget.json) |
| Existing opt-in archives | Wire1219/frame32; fragments1904/frame48; metadata1568/frame48; setup3324/frame48; channel1940/frame56; data/bss0, unchanged | [wire](wire-budget.json), [fragments](fragment-budget.json), [metadata](metadata-budget.json), [setup](setup-budget.json), [channel](channel-budget.json) |
| Archive guard methods | **6 methods per archive ×6 PASS**, actual archives and temporary negative objects | [egress](egress-budget-negative.txt), [channel](channel-budget-negative.txt), [setup](setup-budget-negative.txt), [metadata](metadata-budget-negative.txt), [fragments](fragments-budget-negative.txt), [wire](wire-budget-negative.txt) |
| Actual-source strict GCC analyzer | **18 embedded +2 host units PASS**, final ticket binding recompiled | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspected only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope | **PASS**; no new runtime caller or production handoff dependency; host fixture/tests excluded from ARM | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Relative links and source hashes verified | [check](docs-check.txt) |

59 =56 base +update_frame_queue/update_egress/boot_egress_handoff; standalone33 =30+same3.
All old modes/reference vectors/mutations/refusal tests remain. Logs retain every
CTest row, including the first9 double-space numbers. Only trailing whitespace
and final blank lines normalized. Signed statistics vary with RAM-only generated
test keys; they are per-run values, not golden counts or physical throughput.
The final owner/route-bound ticket version was rechecked in all three suites.

Host queue128/owned driver snapshot40/egress owner832 bytes are not MCU RAM
reserves. Individual compiler frames are not full-stack measurements. CPU/bus
latency/throughput/worker/worst-case/soak remain unmeasured. Fixture capacity2,
residency16ms and driver delay1/3ms are explicit synthetic inputs, not production
FIFO depth or timing defaults. No new wire bytes/version/IDs/bitrate.

Confirmed owner F407 facts: working SWD/ST-Link V2, Cortex-M4r0p1,
raw ID0x101f6413, Flash1024KiB, 3.252V, backup1048576bytes and structural vector
check PASS. No new hardware observation. Full marking/package/RAM/CCM/HSE/vector
mapping/erase/protection/GPIO/CAN, backup SHA/independent copy/restore remain
unconfirmed. Flash1MiB does not establish RAM, bootloader addresses or sectors.

Engineering contracts cover local ownership only. SENT_LOCAL is unverified local
driver input, not remote ACK/authentication/upper completion/physical safety or
installation permission. Real trusted adapter completion/reset/reconciliation,
completion event queue, persistent epoch/identity/routing/resources/staging medium,
runtime and installer remain open. A driver that never returns its claim blocks
reuse; there is no force-forget/flush-success shortcut. Original crypto/lease/model/
deadline/clean-close and future operator/physical/recovery gates remain mandatory.

Rollback: revert PR-07p, removing the optional egress archive/test mode and docs;
old libraries/codecs/limits/config/board state unchanged. PR CI proof is captured
after publication outside Git, avoiding circular hashes/source rebuilds for URLs.
