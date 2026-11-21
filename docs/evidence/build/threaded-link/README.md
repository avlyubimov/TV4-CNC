# PR-07v — threaded signed HOST update evidence

21 ноября2026, Asia/Almaty. Base main
`5e76626dce295753cf575ab667f9b6b436417582`, PR #1–53 merged, no open PR at start.
[Main CI5/5](base-main-ci.json), [contract](../../../STM32_THREADED_LINK.md),
[plan](../../../plans/PR07V_THREADED_LINK.md), [commands](commands.md),
[versions/licenses](versions.json), [source hashes](source-hashes.json).
No devices, actual target driver/runtime, deployable image, release or self-merge.

| Software check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **75/75 PASS**,5.46s | [log](host-tests.txt) |
| ASan/UBSan/LeakSanitizer | **75/75 PASS**,12.49s; outside ptrace/default environment/no options/suppressions | [log](sanitizer-tests.txt) |
| Standalone bootloader/protocol | **49/49 PASS**,2.19s | [log](standalone-tests.txt) |
| Previous ThreadSanitizer cases | **8/8 PASS**,1.39s; unchanged real pthread unit suites | [log](thread-sanitizer.txt) |
| New signed ThreadSanitizer suite | **1/1 PASS**,0.68s; independent Mbed verifier/protocol caller and dedicated driver/concurrent RX | [log](threaded-signed-sanitizer.txt) |
| Signed integration | **8 positive** (6 release15+2 legacy11), **3 independent refusals**,all12 kinds/raw metadata/signature/image; actual verifier/lease/model/clean close | host-tests.txt/boot_threaded_ingress_handoff |
| Driver lifetime | **2 refusals**: original image timeout/late truthful cancellation cleanup-only; missing completion remains held after join | same suite |
| External capture lifetime | **1 refusal**: capacity1 ring+actual pending+external BUSY copy; original permit/time retained, pause NOT acknowledged, actual claim preserved after join | same suite |
| New-mode positive statistics |12066 frames=claims=returns=publications=reports=services;12730 ingress copies=checks;132 driver ACKs/80 phase advances/52 lower recoveries/48 no-dispatch retirements;706 duplicate callbacks rejected/80 wire drops/longest4740 synthetic ms | [per-run statistics](integration-summary.txt) |
| ARM archives | All **11 opt-in** archives and **4 base** libraries exactly unchanged from PR53; standalone and protocol build results match | [comparison](unchanged-arm-budgets.txt),[old4 budget](arm-budget.json) |
| Guard negatives | **7×6+4×7 PASS**; exact imports/exports/frame/global/native atomics preserved | [commands and logs](guard-commands.json) |
| Native atomics |5 mailbox/14 dispatch/15 ingress/10 owner native DMB; no atomic helper imports | [actual disassembly](atomic-codegen.txt) |
| Actual-source strict analyzer | **23 embedded+3 host PASS** | [log](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 source | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**,inspected only | [test](motion-link-contract.txt),[inspection](motion-link-probe.json) |
| Board/output/scope | **PASS**:four image/probe refusals, actual unexpected .bin rejected/removed;actual ARM targets/commands exclude host helper/pthread/tests | [board](board-gates.txt),[output](output-gate.txt),[scope](scope-gate.txt) |
| Documentation/source | Local links/exact source hashes/text-only evidence checked | [check](docs-check.txt) |

75=74 base+boot_threaded_ingress_handoff; standalone49=48+same suite.
TSan8+1 means two distinct builds/logs: previous eight protocol cases PLUS one
new signed suite; not nine new unit cases. All previous modes/reference/mutation/
fault cases remain. All final runs passed on their first full invocation; no
sanitizer options/diagnostics suppressed. Per-run DER lengths/container bytes/
thread scheduling/retirements vary; numbers above describe this host run only.
Captured whitespace normalized only; all CTest result rows retained.

Engineering preconditions: stable one-use contexts/external pthread publication/
one protocol+one driver actor/exclusive spans/common clock frozen per turn.
TX publish precedes driver wake; RX service overlaps actual driver execution.
Target APIs execute outside the HOST mutex. Existing unit thread cases still
exercise TX publication/cancellation/completion interleavings. This suite does
not cover every scheduling interleaving. Crypto CPU duration is not charged;
synthetic4740ms is not measured bus/CPU/latency or a production budget.

Original explicit inputs unchanged: ingress2/residency8ms/service1ms,custody2/
residency16ms,completion2/service1ms,fragments progress8/total256/frame160,
driver1/3ms. Controlled external-capture refusal explicitly uses capacity1 and
no artificial wire duplicates/holds; this is a different test input, not a
firmware default. No upper deadline extension/reinitialization/counter reset.

Diagnostics never grant phase or upper authority. Exact driver ACK+actual
pending/staged/ring/RX and TX custody drain remain necessary. External BUSY
capture is NOT inner pending: it blocks ACK until reviewed reconciliation.
Negative fixtures intentionally retain mock ownership after join; they do not
claim graceful backend teardown. Stop never returns/discards an actual claim.
Memory backend can retain a job before simulated arrival; physical CAN cannot
be assumed to provide this flow control or hardware loss reconciliation.

Host storage worker536/per-direction104/shared multi-mode fixture15536bytes,
not MCU RAM reserve/full stack approval. All firmware object budgets and pins
are unchanged. No new embedded library or pthread dependency in firmware.
System glibc pthread and TSan runtime licenses/identities are in versions.json;
existing exact SDK matrix8/dependency locks/action SHA/permissions retained.

Confirmed hardware facts are the existing owner report only: F407 SWD/ST-Link V2
works,Cortex-M4r0p1,raw ID0x101f6413,Flash1024KiB,3.252V,backup1048576bytes/
structural vectors PASS. No new hardware/backup reads or observations; ESP32
ordinary8f0d899 remains the last installed owner-confirmed image.

Open hardware blockers: full marking/package/RAM/CCM/HSE/IRQ/vector contract/
erase geometry/protection/GPIO/CAN/physical inhibit/backup SHA/copy/restore.
Flash1MiB reserves no RAM/bootloader/app addresses. Open engineering/runtime
blockers: target ISR/RTOS/cache/DMA/clock/peripheral/remote drain, truthful loss/
completion/reset/route reconciliation, persistent authenticated identity,
resources/staging/installer/operator/physical recovery. Software ACK/take/phase
and historical checked outcome are not install/ARM/START permission.

Rollback: revert the HOST helper/test mode/CI/docs; embedded code/configuration,
device state and old APIs remain. No device/SWD/UART/reset/Flash/erase/CAN/motion/
eFuse/options/keys/tag/release or self-merge. Exact-head CI proof is saved outside
Git and linked in the PR, avoiding circular source hashes/rebuilds for run URLs.
