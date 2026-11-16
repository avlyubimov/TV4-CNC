# Codex operating rules — TV4 CNC

These rules apply to the entire repository. More specific AGENTS.md files may add constraints but may not weaken safety requirements.

## Mission
Build a reversible, safe-to-test CNC retrofit for a TV-4-07-20 KA lathe. Prioritize deterministic motion, operator safety, maintainability, and a fast, legible HMI. Do not claim a feature works on hardware without measured evidence.

## Fixed architecture
- ESP32-S3 HMI: ESP-IDF, LVGL **9**, C/C++, FreeRTOS; 800×480 landscape. Actual board: Waveshare ESP32-S3-Touch-LCD-7 **Rev 1.2**. Vendor factory demo is LVGL 8; LVGL 9 integration requires verification against matching BSP, drivers and revision.
- STM32F407: motion planning, hardware timer-based STEP/DIR, encoders, machine state, interlocks. Do not generate real-time motion from ESP32 or CAN callbacks.
- CAN is a non-safety HMI↔motion transport. Verify actual CAN transceiver, termination, and USB/CAN pin multiplexing against Rev 1.2 schematic. No guessed GPIO mapping.
- STM32F103 Blue Pill: optional **bench-only** protocol test target; not a substitute for production motion firmware.

## Non-negotiable safety
1. Physical hardwired normally-closed E-stop and independent removal of hazardous drive power/torque (appropriate contactor/STO architecture after hardware review). A touchscreen STOP is **not** an E-stop.
2. Manual/CNC changeover must be mechanically and electrically interlocked; never permit competing drive engagement. Machine cannot move merely because HMI boots, resets, reconnects or loses CAN.
3. Default state on power-up, watchdog reset, CAN timeout, sensor fault, or invalid command: **motion disabled / safe state**. Fault acknowledgement does not automatically restart motion.
4. Never infer machine readiness from a UI mock, firmware simulation, or driver ENABLE alone. All real outputs remain disabled by default behind explicit hardware-enable configuration and commissioning procedure.
5. No automatic spindle starts or axis motion on boot. Limit switches, homing, soft limits and travel direction must be verified before any hardware movement.
6. Do not implement or suggest bypassing E-stop, guards, limit switches, fault detection or interlocks. Mains/servo/VFD wiring requires qualified review, proper earthing, isolation and protection.
7. Any machining cycle, threading or C-axis indexing is **design-only** until mechanics, encoder direction, backlash, holding torque and stop behavior are validated. Software backlash compensation does not remove physical play.

## Software engineering
- No blocking I/O, sleep, filesystem writes, CAN waits, or heavy computation in LVGL callbacks. Only LVGL owner task may mutate UI objects; use queues/snapshots to transfer state.
- Prefer static allocation where practical, bounded queues, explicit timeouts, and predictable memory use. Avoid unbounded dynamic allocations in high-frequency paths.
- Use hardware timers/DMA/interrupts for step generation; target 1 kHz planning only after measured feasibility, never promise deterministic timing from an RTOS task alone.
- Use explicit units (mm, mm/min, mm/rev, rpm, degrees), X diameter/radius mode, machine/work coordinate frames, and signed direction conventions. Do not silently choose machine dimensions.
- No invented pinouts, vendor specifications, performance claims, or CAN IDs. Record unknowns in docs/OPEN_QUESTIONS.md.
- Version CAN messages, validate payload lengths/ranges, use sequence numbers, state machine and heartbeat/timeout; no motion commands without acknowledgement and authorization.
- **Russian (ru-RU) is the only user-facing language in MVP and the default on first boot.** All visible UI text must be Russian: navigation, buttons, status, warnings, dialogs, units where appropriate, and simulation labels (e.g. «СИМУЛЯЦИЯ», not «SIMULATION»). Do not mix English UI strings into the interface. Internal identifiers, protocol tokens, logs, and established technical abbreviations (X/Z, CAN, RPM where unavoidable) may remain in English.
- Implement centralized localization by stable string IDs, UTF-8 RU translations, and a future-ready locale interface. Do **not** expose a language switcher or ship incomplete KK/EN translations in MVP. Reserve support for kk-KZ and en-US later; missing translations must fall back to Russian. No hardcoded user-visible strings in view code.
- Bundle or configure a legally usable LVGL font with full Cyrillic coverage (including Ё/ё); test rendering, clipping and touch layout on 800×480. Keep fonts/assets appropriately licensed and small; do not commit proprietary vendor binaries or fonts.
- No web stack required for core HMI. No remote machine control in MVP.

## HMI quality bar
- iOS-like *responsiveness*, not an iOS clone. Dark industrial theme, accessible contrast, large touch targets, minimal visual clutter.
- Performance targets to **measure**, not assert: touch feedback <50 ms, smooth 30+ FPS where achievable, coordinate display 20–30 Hz, no stalls during CAN loss; log FPS, frame time, heap and touch latency.
- Clearly distinguish «СИМУЛЯЦИЯ», «НЕТ СВЯЗИ», «ГОТОВ», «ОШИБКА» and «ДВИЖЕНИЕ» in the UI (internal enum names may remain English). Never show simulated telemetry as live machine readings.
- UI STOP may request a controlled stop but is not safety-rated. Never depict it as the only E-stop.

## Codex workflow
1. Inspect repository, vendor docs/schematic, hardware revision and open questions before editing.
2. Write a short plan and acceptance criteria. Keep PRs small and scoped.
3. Build reproducibly with pinned toolchain/component versions; document exact commands and outputs.
4. Add unit/host tests for protocol and state logic; simulate fault/timeout/disconnect; do not test movement on real machine without explicit commissioning authorization.
5. Include in PR: changed files, build/test results, screenshots where relevant, performance measurements (or clearly marked unmeasured), hardware assumptions, risks, rollback plan.
6. Never merge your own PR unless explicitly instructed. Never claim a successful build or test that was not executed.
7. Preserve manual operation and avoid irreversible modifications to the lathe.

## First assignment
Read docs/CODEX_TASK_001.md. Deliver a simulation-only HMI skeleton with verifiable LVGL 9 build, UI state machine and tests.
