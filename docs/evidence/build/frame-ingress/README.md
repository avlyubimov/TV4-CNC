# PR-07t — owned frame ingress evidence

21 ноября2026, Asia/Almaty. Base main
`25e635caa4c0f97fc52c19d775cca4890763c104`, PR #1–51 merged, no open PR at start.
[Main CI5/5](base-main-ci.json), [contract](../../../STM32_FRAME_INGRESS.md),
[plan](../../../plans/PR07T_FRAME_INGRESS.md), [commands](commands.md),
[exact versions/licenses](versions.json), [source hashes](source-hashes.json).
No device, deployable image, actual CAN/ISR/RTOS/backend/runtime, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **71/71 PASS**,5.37s | [log](host-tests.txt) |
| ASan/UBSan/LeakSanitizer | **71/71 PASS**,11.58s; outside ptrace/default environment/no options/suppressions | [log](sanitizer-tests.txt) |
| Standalone bootloader/protocol | **45/45 PASS**,2.27s | [log](standalone-tests.txt) |
| ThreadSanitizer | **6/6 PASS**,1.35s; actual pthread actors/outside ptrace/default environment/no suppressions | [log](thread-sanitizer.txt) |
| Unit | Capacity1..4/FIFO/input wipe/exact token/retained first/original capture across pending retry and external BUSY reoffer; DLC256, clock wrap/backwards/future, age39 accepts40 refuses, exhaustion/loss sticky, zero refused outputs | host-tests.txt/update_frame_ingress |
| Original protocol time | Ingress FRAME accepted, but original actual channel fragment deadline still refuses at current service time; captured time never substituted | host-tests.txt/update_frame_ingress |
| Real two-actor ingress | **800000 FIFO frames +256 actual channel messages/8448 fragments**, capacities1..4, input wiped, fixed near-wrap synthetic clock | host-tests.txt/update_frame_ingress_threads |
| Controlled thread faults | **3 PASS** after stage: producer loss, producer backwards clock, consumer expiry; no payload exposure | thread-sanitizer.txt |
| New signed mode | **8 positive** (6 release15+2 legacy11), **3 independent refusals +2 ingress age/loss refusals**; original independent crypto/full image/live RX/lease/model/clean close | host-tests.txt/boot_frame_ingress_handoff |
| Actual signed run |12066 TX frames=claims=returns=reports=services;12730 RX copies=checks;706 rejected duplicate driver callbacks;80 link fragment losses;longest4740 synthetic ms | [statistics](integration-summary.txt) |
| New ARM ingress archive |**1060text/data0/bss0/max individual frame48**,8 exports, ceiling4096/frame128;15 native DMB/no atomic helpers | [budget](ingress-budget.json), [standalone](standalone-ingress-budget.json), [codegen](atomic-codegen.txt) |
| Old4 ARM libraries |**43600text/data8/bss27**,frames984/376/328/64 unchanged | [budget](arm-budget.json) |
| Prior opt-in archives |wire1219/frame32;fragments1904/frame48;metadata1568/frame48;setup3324/frame48;channel1940/frame56;egress1972/frame88;events768/frame56;mailbox1038/frame64;dispatch1324/frame72;data/bss0 unchanged | [exact results](versions.json) |
| ARM guards |**7×6 previous +3×7 atomic archive cases PASS** | [ingress](ingress-budget-negative.txt), [dispatch](driver-dispatch-budget-negative.txt), [mailbox](completion-mailbox-budget-negative.txt), [wire](wire-budget-negative.txt) |
| Actual-source strict analyzer |**22 embedded +2 host PASS** | [log](static-analysis.txt) |
| Mbed TLS3.6.7 pinned source |**178 files PASS** | [check](source-check.txt) |
| Synthetic ARM link |**1/1 PASS**,inspected only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope |**PASS**: four image/probe refusals, actual unexpected .bin rejected/removed, no host target or new production handoff/runtime dependency | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Docs/exact source hashes |Local links, source/config/document hashes and text-only evidence checked | [check](docs-check.txt) |

71=68 base +update_frame_ingress_threads/update_frame_ingress/boot_frame_ingress_handoff;
standalone45=42+same3. TSan6 retains previous4 and adds ingress unit/thread cases.
All old modes/reference/mutation/refusals remain. No executable code changed
after full checks. Only trailing whitespace/final blank lines normalized in
captured text; all CTest rows retained, including first9 doubled spaces.
DER lengths/frame counts vary with RAM-only test keys; retry/thread scheduling
statistics are not golden data or measured hardware performance.

Explicit new signed fixture inputs:2 ingress slots/residency8ms/service1ms.
Existing custody2/residency16ms,completion2/service1ms,fragments progress8/
total256ms/frame160 and driver1/3ms preserved; all original upper deadlines,
crypto/lease/model/close remain. One-frame ingress service charges the shared
current clock. The longest4740ms is synthetic scheduling, not bus/CPU/crypto
timing. Host signed link is serialized; actual concurrent actors tested separately.
Host ingress208/token32/entry20 and compiler frames are not MCU RAM/full-stack
or timing approval. Native fault RMW can retry exclusive stores under contention.

Confirmed owner hardware unchanged: SWD/ST-Link V2 works,Cortex-M4r0p1,
raw ID0x101f6413,Flash1024KiB,3.252V,backup1048576bytes/structural vectors PASS.
No new observation or device/backup read. Full marking/package/RAM/CCM/HSE/full
vectors/Flash erase/protection/GPIO/CAN/physical inhibit/backup SHA/copy/restore
remain unconfirmed. Flash1MiB defines no RAM/loader address or sector reservation.

Engineering preconditions: one driver/one protocol actor, externally published
init/stable lifetime/stop+join; immutable binding; actual capture time separate
from fresh offer/check time; original capture retained even for external BUSY
input. Missing/unretainable/hardware-lost frames must call lost. No live reset/
flush/clear; faults deny further payload. FRAME is unverified; software fault
observation is not atomic hardware cancellation/physical inhibit. Empty is a
snapshot, not producer-stop/phase-cutover/backend/remote drain proof. Serialized
host fixture has no producer operations between empty observation and phase
advance; actual concurrent target cutover requires reviewed stop/ack/reconciliation.

Open: real ISR/RTOS/cache/DMA/clock/driver loss and completion truth/reset,
persistent authenticated identity/routing/route lifetime, worker/resources/
staging/runtime/installer/operator/physical/recovery. No new wire/IDs/bitrate/
heap/crypto/dependency/SDK/security/Flash/pin defaults. Existing factory layout
and device images remain unchanged. No board test is claimed by this software stage.

Rollback: revert optional ingress/tests/guard/docs; previous APIs/config/budgets
and device state unchanged. Exact-head CI proof after publication is saved
outside Git and linked in the PR, avoiding circular hashes/rebuilds for run URLs.
