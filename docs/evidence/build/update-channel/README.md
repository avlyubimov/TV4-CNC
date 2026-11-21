# PR-07o — shared update channel evidence

21 ноября2026, Asia/Almaty. Base main
`389de778c5aff470640f2e5714bcb553e62c6ba6`, PR #1–46 merged, no open PR at start.
[Base CI5/5](base-ci.json), [contract](../../../STM32_UPDATE_CHANNEL.md),
[plan](../../../plans/PR07O_UPDATE_CHANNEL.md), [commands](commands.md),
[exact toolchains/versions/licenses](versions.json), [source hashes](source-hashes.json).
No device access, firmware image, driver/runtime, release or self-merge.

| Check | Actual result | Evidence |
|---|---|---|
| Root host/LVGL | **56/56 PASS**,4.54s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **56/56 PASS**,10.06s; outside ptrace/default env/no suppressions | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **30/30 PASS**,1.31s | [summary](standalone-ctest.txt), [full log](standalone-tests.log) |
| New channel unit path | **512 phase cycles/7168 mixed TU/MS messages**, shared retained counters/wrap clock; no reinit | host-tests.log/update_channel |
| Owned queue and boundaries | Exact commit/BUSY/repeated peek, active final outbox with lower IDLE, progress/total expiry, single take/wipe, cancel/clear counter/policy retention | host-tests.log/update_channel |
| Phase/identity/codec refusals | Phase skips, invalid pairs, late MS in IMAGE, old metadata ACK/image mismatch, malformed prefix/version/reserved/no fallback, sticky context/clock/exhaustion | host-tests.log/update_channel |
| New shared signed mode | **8 positive** (6 release15 +2 legacy11), **3 independent refusal gates** (signature, correctly re-signed wrong raw digest, expired image RX) | [selected log](integration-summary.txt), host-tests.log/boot_channel_handoff |
| Actual shared fragment link | One link/counter space through image negotiation/MS/metadata/image/outcome; retained IDs equal accepted message attempts; all12 TU/MS statistic kinds delivered/lost as configured | host-tests.log/boot_channel_handoff |
| Per-run new signed statistics |12066 frames/80 dropped fragments/4799 container bytes/DER70..72/longest3710 synthetic ms; includes shared control frames | integration-summary.txt |
| Independent upper authority | Producer arrays wiped, delivered snapshot checked by original independent signature/raw digest/full image/live RX/lease/model/clean close; image windows not restarted | host-tests.log/boot_channel_handoff |
| New separate channel ARM archive | **1940text/data0/bss0/frame56**, ceiling4096/frame128 | [budget](channel-budget.json) |
| Old4 ARM libraries | **43600text/data8/bss27**, frames984/376/328/64, unchanged | [budget](arm-budget.json) |
| Existing opt-in archives | Wire1219/frame32;fragments1904/frame48;metadata1568/frame48;setup3324/frame48, data/bss0, unchanged | [wire](wire-budget.json), [fragments](fragment-budget.json), [metadata](metadata-budget.json), [setup](setup-budget.json) |
| Archive guard methods | **6 per archive ×5 PASS**, actual archives and temporary negative objects | [channel](channel-budget-negative.txt), [setup](setup-budget-negative.txt), [metadata](metadata-budget-negative.txt), [fragments](fragments-budget-negative.txt), [wire](wire-budget-negative.txt) |
| Actual-source strict GCC analyzer | **16 embedded +2 host units PASS** | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, inspect only | [test](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope | **PASS**, no production handoff dependency, runtime caller or host test target in ARM | [board](board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Relative links/source hashes verified | [check](docs-check.txt) |

56=54 base +update_channel/boot_channel_handoff; standalone30=28+same2.
Existing TU/MS reference vectors, mutations, typed/byte/fragment/timed/metadata/
setup positive/refusal modes and known generation/reboot replay blocker test
remain. Logs retain every CTest row including first9 double-space numbering;
only trailing whitespace/final blanks normalized. DER signatures/frame/container
stats vary with test keys generated only in RAM; numbers are per-run, not golden
physical throughput. Source comments/guard diagnostic wording clarified after
functional checks; no executable C behaviour changed afterward.

Host channel TX696/RX664/message288, not MCU RAM reserves. Individual compiler
frames are not full stack. No measured CPU/bus latency/throughput/worker/worst
case/soak. Clock milliseconds and routes are explicit synthetic inputs; no
production CAN IDs/bitrate/default timing or new wire bytes/version were added.

Confirmed owner F407 facts unchanged: SWD/ST-Link V2, Cortex-M4r0p1,
raw ID0x101f6413,Flash1024KiB,3.252V,backup1048576bytes/structural vectors PASS.
No new hardware observation. Full marking/package/RAM/CCM/HSE/vector/erase/
protection/GPIO/CAN, backup SHA/independent copy/restore remain blockers.
Paired role IDs/local phase filtering are engineering software contracts, not
production epoch/persistence/authentication/reboot replay/remote queue drain or
physical/install proof. External driver queues may outlive local commit. Original
upper state/CRC/independent crypto/live lease/model/deadline/clean-close and
future physical/operator/recovery gates remain mandatory. Production runtime/
identity/routing/staging medium/resources/installer are open; no Flash addresses.

Rollback: revert PR-07o, removing optional channel/new test mode and docs; old
libraries/codecs/limits/board state unchanged. PR CI proof is captured after
publication outside Git to avoid circular hashes/rebuilding only for run URLs.
