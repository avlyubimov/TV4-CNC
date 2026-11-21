# P0 — настоящий STM32F407VG HSI bench image

22 ноября 2026, Asia/Almaty. База main
`610f1745d892d5904ea183b50617a20d8ce755bb`, PR #1–55 слиты;
[base CI5/5](base-main-ci.json). [План](../../../plans/P0_STM32F407_FIRST_IMAGE.md),
[образ/официальные источники](../../../STM32_FIRST_IMAGE.md),
[install/restore](../../../STM32_STLINK_RUNBOOK.md),
[команды](commands.md), [версии/лицензии](versions.json),
[проверенные source hashes](source-hashes.json).

**Результат — реальный standalone ELF/BIN/map**, не библиотека или synthetic
fixture. Profile `stm32f407vg-hsi-no-motion-v1`, GCC14.2.1 MinSizeRel:
BIN2596bytes, SRAMstatic268 + stacks/guards5184, CCMprobe4; 98vectors/512align,
полный Flash/SRAM/CCM/stack/LOAD contract. HSI16MHz, bounded init, RAM/SWD
diagnostics, main-only watchdog, emergency fault stack, никакого motion/CAN I/O.
Программная готовность к отдельно разрешённому стенду; ARM не исполнялся.

| Выполненная software проверка | Результат / evidence |
|---|---|
| Root host/LVGL + actual bench C | **77/77 PASS**,5.81s — [полный log](host-tests.txt) |
| ASan/UBSan/LeakSanitizer | **77/77 PASS**,12.55s — [полный log](sanitizer-tests.txt); вне ptrace, default env/no suppressions |
| Standalone bootloader/protocol | **49/49 PASS**,2.20s — [log](standalone-tests.txt) |
| Существующие ThreadSanitizer suites | **8/8 + 1/1 PASS**,1.31s+0.72s — [protocol](thread-sanitizer.txt),[signed chain](threaded-signed-sanitizer.txt) |
| Real ARM image contract | **1 CTest / 12 Python groups PASS** — [log](image-contract.txt); every vector including IRQ79, ELF/BIN/map/header/LOAD/NOLOAD/stack corruption, identity, BKPT/SVC/FP rejection, actual object relink and overflow/ctor/orphan failures |
| Actual-source hardware model | **24 cold/debug-PLL cases**, bounded IDs/HSI/clock-stop/LSI/IWDG/refusal-before-writes/reset flags; **672 independent single-bit record CRC refusals**; frame limits, feed gate/wrap, memset vs host oracle — inside root/sanitizer logs |
| Old synthetic ARM contract | **1/1 PASS**,inspection only — [test](probe-contract.txt),[metadata](synthetic-probe.json) |
| Actual-source strict analyzers | **36 native C units + actual ARM main PASS**, no diagnostics suppressed — [native](static-analysis.txt),[ARM](main-arm-analysis.txt) |
| Existing ARM libraries/budgets/imports/exports/frames/atomics | All **12 parsed old profiles unchanged** from PR54 — [comparison](unchanged-arm-budgets.txt), old-arm-*.json; all11 negative guard suites rerun |
| Pinned Mbed TLS source | **178 files PASS** — [source check](old-arm-source-check.txt) |
| Profile/output scope | **7 invalid configurations refused** — [commands](board-gates.json), gate-*.txt; unsupported BIN/compiler-frame negatives inside actual contract; actual default core/support budgets retained |
| New actual image inspection | [ELF layout](bench-layout.txt),[symbols](bench-symbols.txt),[disassembly](bench-disassembly.txt),[stack reports](bench-stack.txt),[working-tree metadata](working-tree-image.json) |
| Reproducibility | [Two-directory ELF/BIN byte comparison](reproducibility.txt); same toolchain/identity, no timestamp/absolute debug paths |
| Source/docs/CI | [Links/YAML/scope/source check](docs-check.txt); action/archive pins unchanged; exact8 SDK profiles retained in CI |

77 = previous75 + actual bench C test + pinned ST reference test; standalone49
and TSan8+1 unchanged. Root full tests passed on their first invocation; no
ASAN/LSAN/UBSAN/TSAN options or diagnostics suppressed. During development a new
link/corruption test caught stack alignment and empty linker stub/RWX handling;
linker constraints were tightened and the final 12-group image suite passes.
The tests mutate temporary copies/links, not delivered artifacts. Compiler max
individual frame88≤128 bytes is not measured stack high-water; main4096/fault1024
are software budgets pending hardware. No ARM execution/emulation/SWD/USB/serial/
reset/Flash/erase/backup reads, no binary publication or self-merge.

Working-tree logs embed base610f174+dirty1 deliberately. After commit the
candidate is rebuilt **clean on exact PR head**; final ELF/BIN/map/JSON and hashes
are delivered locally and in PR description, not self-referentially committed.
[Identity note](artifact-identity-note.txt). CI independently verifies clean exact
HEAD/reproducibility and uploads only text metadata. Application licensing/release
gate remains; no real or synthetic ELF/BIN is published as CI artifact/Release.
ST startup reference v2.6.11/Apache2.0 is not linked; no new runtime dependency.
No new local ESP SDK build is claimed; unchanged matrix8 is verified in exact-head CI.

Owner-confirmed hardware remains SWD/ST-Link V2, Cortex-M4r0p1,rawID0x101f6413,
Flash1024KiB,3.252V,backup1048576bytes/structural vectors. The reviewed memory/
sector/vector map is the **documented VGT6 specification**, not physical RAM/erase
measurement. Full marking/package, actual HSI/LSI/HSE, SRAM/CCM/IRQ/watchdog/fault
execution, protection/option bytes, backup original SHA/second copy/restore,
GPIO/CAN/physical inhibit remain open. GPIO outputs are not assigned; software
motion_outputs0 is not a physical safe level. CAN transceivers are absent.

Next measurable result after review/explicit exact-image permission is the first
isolated F407 bench per runbook. Sector0/16KiB image budget does not reserve a
future CAN loader/app/staging layout; P3/P4 and existing crypto/transport remain.
