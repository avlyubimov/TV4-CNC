# Reproducible HMI artifact pairs

Base main 5fb177b / PR #90. [Usage](../../../REPRODUCIBLE_HMI.md),
[plan](../../../plans/P2_REPRODUCIBLE_HMI.md). Software/compilation only;
ESP32 remains bench.4 OTA1 VALID and F407 remains ee7d179 from the prior session.
No device was opened, no owner signing key was used and no release was published.

## Actual clean build pairs: PASS

Exact SDK artifact source 6b84584f71a9eef5058ae2bbd349b23a20d7e9ad. In each
existing immutable official SDK image, the exact clean git archive was compiled
twice, sequentially, with compiler caches disabled. Compiler project paths were
/repro-first and /different/depth/repro-second. Fresh build trees had no objects;
all dependency locks remained unchanged. No owner context/key was mounted.

| SDK | App BIN bytes | App ELF bytes | First build seconds | Second build seconds |
|---|---:|---:|---:|---:|
| 5.3.2 | 1553040 | 13760940 | 82.999 | 58.731 |
| 6.1.1 | 1600064 | 14533544 | 51.001 | 61.150 |

Within **each** SDK the entire app BIN, ELF, bootloader BIN and partition table
matched byte for byte; all effective configuration fields matched. Different SDKs
are not expected to produce the same image. [Full numeric results and hashes](results.json).
Actual app descriptor date/time bytes are zero; ELF32 Xtensa format and actual
app/ELF SHA binding passed. The normal CPU/PSRAM/XiP/RGB/LVGL allocator/TLS/
watchdog/rollback/layout gates passed without changing their expected values.
The 16 MiB partition table is identical in all four builds.

## Preserved initial failure and fix

[Source 848fd54 first attempt](first-attempt.json) compiled on both SDKs but
failed equality. Three absolute third_party/cjson header directory strings
remained in each ELF. The bootloader and table were equal, and every allocated
ELF section matched within the SDK (15 sections on 5.3.2, 16 on 6.1.1); debug bytes
and the resulting ELF hash embedded into the app differed. Mapping only cJSON.c
was insufficient because other main translation units include cJSON.h.

The corrected source maps this one external directory for **all main units**
when the explicit reproducible option is selected. No stripping, timestamp or
path-byte rewriting was applied to outputs. The original failure and artifacts
remain preserved; both new independent pairs passed the original exact criteria.

## Software checks and limits

[291 host](host-tests.log) and [291 ASan/UBSan/LeakSanitizer tests](sanitizer-tests.log) PASS. The new equality suite has
10 cases, covering all four artifact differences, config/date/ELF/SDK mismatch,
path alias, explicit memory trace, shell gates and CLI refusal under python -O.
The real-crypto authoring suite includes a new reproducible INSTALL-candidate
case for both SDK formats with ephemeral host-only keys and non-executable
fixtures. Existing signed schema, source/hash binding and closed operational
flags remain intact; there are 29 authoring cases total.

The paired source is unprovisioned and non-release display-versioned, for build
qualification only. Equal files do not authenticate source or establish
independent compilation by themselves: the recorded clean archive/build steps
supply that evidence. The comparator labels its source SHA an external claim.
SDK/toolchain installation paths and host OS were not varied. This is not runtime
memory/FPS/touch latency, power-loss qualification, release or installation authority.

The owner separately approved fresh bench-only .4→.5 settings until 27 November 2026
08:00 Asia/Almaty. Those new local contexts are separate from these unprovisioned
build pairs; previous contexts and published assets remain unchanged. A new
configured signed candidate and exact publication/hardware packages are still
required. Private logs, contexts, key paths and dumps are outside Git.

[Exact SDK source CI](source-ci.json): 6b84584, all 17 jobs PASS, including the
five original profiles on both SDKs and both separate clean-pair jobs. The new
signed-authoring integration case is also included in the final host logs.
