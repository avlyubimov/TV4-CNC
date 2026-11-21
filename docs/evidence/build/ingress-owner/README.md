# PR-07u — acknowledged ingress owner evidence

21 ноября2026, Asia/Almaty. Base main
`bc35a37aa903655dd4e64fac2612ea58d4447cf7`, PR #1–52 merged, no open PR at start.
[Main CI5/5](base-main-ci.json), [contract](../../../STM32_INGRESS_OWNER.md),
[plan](../../../plans/PR07U_INGRESS_OWNER.md), [commands](commands.md),
[versions/licenses](versions.json), [source hashes](source-hashes.json).
No devices, actual driver/runtime, deployable image, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL |**74/74 PASS**,5.01s |[log](host-tests.txt) |
| ASan/UBSan/LeakSanitizer |**74/74 PASS**,11.63s; outside ptrace/default environment/no options/suppressions |[log](sanitizer-tests.txt) |
| Standalone bootloader/protocol |**48/48 PASS**,1.91s |[log](standalone-tests.txt) |
| ThreadSanitizer |**8/8 PASS**,1.28s; actual pthread actors/outside ptrace/default environment/no suppressions |[log](thread-sanitizer.txt) |
| Actual owner unit |Pending/staged/partial/READY drain, exact epoch/owner, retained clocks/IDs/atomic positions, zero refused take, same-phase resume, one advance per pause, sticky faults, exhaustion, loss after ACK |host-tests.txt/update_ingress_owner |
| Bounded recovery |Only original lower fragment timeout, after actual ACK+pending/staged/ring cleanup; explicit STATE retirement never dispatches into failed RX; no upper deadline reset |host-tests.txt/update_ingress_owner |
| Concurrent channel/cutover |**256 messages/8448 frames/1280 acknowledged phase changes/1276 stale ACKs/1024 stale offers**,capacities1..4/fixed near-wrap synthetic clock |host-tests.txt/update_ingress_owner_threads |
| Controlled races |**5 PASS**,including producer publication after early empty, permit before request, pending/staged while ACK, loss after ACK, lower-timeout cleanup/recovery |thread-sanitizer.txt |
| New signed mode |**8 positive** (6 release15+2 legacy11), **3 independent refusals +2 owner lifetime refusals**,all12 kinds/raw metadata/signature/image; actual independent crypto/lease/model/clean close |host-tests.txt/boot_ingress_owner_handoff |
| Signed statistics |12066 TX frames=claims=returns=reports=services;12730 ingress copies=checks;**132 pauses/80 phase advances/52 timeout recoveries/48 explicit no-dispatch retirements**;706 rejected driver callbacks,80 link losses,longest4740 synthetic ms |[statistics](integration-summary.txt) |
| New ARM archive |**1512text/data0/bss0/frame88**,16 exports/10 native DMB/no atomic helpers;ceiling4096/frame128 |[budget](ingress-owner-budget.json),[standalone](standalone-owner-budget.json),[codegen](atomic-codegen.txt) |
| Previous libraries |Old4 **43600text/data8/bss27**,frames984/376/328/64;all ten earlier opt-in archive sizes/budgets unchanged |[old4](arm-budget.json),[exact results](versions.json) |
| ARM guards |**7×6 previous +4×7 atomic archive cases PASS** |[new guard negatives](ingress-owner-budget-negative.txt) |
| Actual-source strict analyzer |**23 embedded+2 host PASS** |[log](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 source |**178 files PASS** |[check](source-check.txt) |
| Synthetic ARM link |**1/1 PASS**,inspected only |[test](motion-link-contract.txt),[inspection](motion-link-probe.json) |
| Board/output/scope |**PASS**,four image/probe refusals, actual unexpected .bin rejected/removed;no host targets or new runtime/production handoff dependency |[board](board-gates.txt),[output](output-gate.txt),[scope](scope-gate.txt) |
| Documentation/source |Local links/exact source hashes/text-only evidence checked |[check](docs-check.txt) |

74=71 base +update_ingress_owner_threads/update_ingress_owner/boot_ingress_owner_handoff;
standalone48=45+same3. TSan8 retains previous6 and adds owner unit/thread cases.
All previous modes/reference/mutation/refusals remain. Full checks were repeated
after the test correction below; no executable changes followed those checks.
Captured whitespace normalized only; all CTest rows retained, including first9
doubled spaces. DER lengths/counts/thread scheduling are variable, not golden data.

Initial ASan run: one threaded test asserted REQUESTED only, although the driver
can ACK before pause returns, legitimately returning PAUSED. [Original log](sanitizer-initial-failure.txt)
retained. The test now accepts both and still independently requires actual
ACK+drain/quiescent before advance. No production behavior was weakened, no
diagnostic suppressed. All four final full runs passed after rebuilding tests.

Explicit synthetic inputs unchanged from PR #52: ingress2/residency8ms/service1ms,
custody2/residency16ms,completion2/service1ms,fragments progress8/total256/frame160,
driver1/3ms. Original upper windows and verifier/lease/model/close remain. Signed
HOST fixture serialized; actual concurrency separately pthread/TSan. Synthetic
4740ms excludes measured crypto CPU/bus; target latency/IRQ/stack/RAM/soak unknown.
Host owner888/permit16/pause16 and compiler frame88 are not MCU memory/full stack approval.

Confirmed owner hardware facts unchanged: SWD/ST-Link V2 works,Cortex-M4r0p1,
raw ID0x101f6413,Flash1024KiB,3.252V,backup1048576bytes/structural vectors PASS.
No new hardware/backup reads or observations. Full marking/package/RAM/CCM/HSE/
vectors/Flash erase/protection/GPIO/CAN/physical inhibit/backup SHA/copy/restore
remain unconfirmed; Flash1MiB reserves no RAM/loader addresses.

Engineering preconditions: one driver/one protocol actor, externally published
init/stable lifetime/stop+join, valid exclusive spans, common monotonic uint32
clock/gaps<2^31. Permit before capture, original epoch/time retained externally,
no retagging, truthful loss reporting. ACK only proves this software producer
stopped publishing; actual backend/peripheral/IRQ/DMA/cache/RTOS/remote drain/
route reset is not supplied. Loss after ACK can still fail-close the owner.
FRAME/take/phase change is not authentication/upper success/install/physical authority.

Open: actual port/clock/driver loss and completion truth/reset, durable authenticated
identity/routing/resources/staging/runtime/installer/operator/physical/recovery.
No new wire/IDs/bitrate/crypto/dependencies/SDK/security/Flash/pin defaults.
Rollback: revert optional owner/tests/guard/docs; previous APIs/config/budgets
and device state unchanged. Exact-head CI proof is saved outside Git and linked
in the PR, avoiding circular source hashes/rebuilds merely for run URLs.
