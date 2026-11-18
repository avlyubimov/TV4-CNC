> **Историческое задание первого HMI. Не является текущей командой Codex.**
> Программная основа уже реализована; аппаратные и performance проверки
> остаются отдельными задачами. Продолжение — [ROADMAP.md](ROADMAP.md),
> текущий PR-01 — [PR01_REVIEW.md](PR01_REVIEW.md). Не создавать HMI заново.

# Codex Task 001 — First buildable HMI

## Objective
Produce a **simulation-only**, hardware-targeted ESP-IDF + LVGL 9 project for Waveshare ESP32-S3-Touch-LCD-7 **Rev 1.2**, with a smooth 800×480 Russian-only CNC dashboard. No motor control in this task.

## Required work
1. Identify and cite official vendor example, correct LCD/touch controller, pin map, board revision differences, USB/CAN sharing, and compatible pinned ESP-IDF/LVGL/BSP versions. If Rev 1.2 support is unclear, document the gap rather than guessing.
2. Create firmware/esp32-hmi project with ESP-IDF CMake, reproducible dependency lock/config, README and build/flash instructions for Ubuntu. Do not overwrite the factory firmware until backup instructions and flash size are verified.
3. Implement responsive home screen: X/Z values, spindle RPM, feed, active tool, JOG increments, controller connection and mode indicators. The initial screen MUST prominently display «СИМУЛЯЦИЯ» (not English SIMULATION).
4. Implement simulated machine-state provider (fixed deterministic test data or controllable simulation), clear mode state machine, UI event queue, non-blocking touch feedback and placeholder CAN transport interface (no real CAN transmission by default).
5. Implement Russian-only user-facing UI with centralized string IDs, default locale `ru-RU`, Cyrillic LVGL font (including Ё/ё), and future extensibility for `kk-KZ`/`en-US`. Do not expose language selection or include partial KK/EN UI in this phase. Missing locale keys fall back to Russian. Use compact, properly licensed assets and a memory-aware design.
6. Implement host-testable state/protocol logic and fault scenarios (disconnect, stale heartbeat, invalid values, simulated emergency condition). No real motion commands.
7. Add a performance instrumentation page or log (FPS, UI frame time, heap, touch latency); report measurements only if run on physical device.
8. Document flash backup procedure, rollback, known unknowns and board-specific setup.

## Acceptance criteria
- Clean reproducible `idf.py build` with pinned dependencies and no fabricated output.
- Project runs on actual board only when hardware test evidence exists; otherwise state 'not hardware tested'.
- No touch action triggers physical motion or unsafe startup; mock data always visibly labeled.
- No blocking code inside LVGL callbacks; no concurrent LVGL object access.
- Code review checklist and screenshots/mockups provided in PR. All visible UI text must be Russian; verify Cyrillic font coverage, no untranslated English labels, and Russian fallback tests.
- If a dependency cannot be verified, stop that part and report precisely what is missing.

## PR title
`feat(hmi): initial LVGL9 simulation dashboard for ESP32-S3 Touch LCD 7`
