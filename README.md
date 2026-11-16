# TV4 CNC

Reversible CNC retrofit of TV-4-07-20 KA lathe. **Engineering prototype, not production-ready machine control.**

## Hardware
- HMI: Waveshare ESP32-S3-Touch-LCD-7 **PCB Rev 1.2** (observed), 800×480 touchscreen; factory LVGL v8 demo tested.
- Motion: STM32F407VGT6 (pending delivery); optional STM32F103 Blue Pill for communication bench tests.
- Axis test drive: one purchased closed-loop NEMA23 3.1 N·m with STEP/DIR driver, exact model/encoder details pending verification.
- Planned X/Z ballscrews, optional spindle encoder directly on spindle, spindle servo and removable milling attachment: **not yet validated or installed**.

## Initial scope
Create a responsive, testable **simulation-only HMI** in ESP-IDF + LVGL 9, based on the Waveshare board support package. No real motor outputs until separately reviewed and hardware-tested.

See [AGENTS.md](AGENTS.md), [architecture](docs/ARCHITECTURE.md), [roadmap](docs/ROADMAP.md), and [first Codex task](docs/CODEX_TASK_001.md).

## Getting started
1. Read AGENTS.md and docs/CODEX_TASK_001.md.
2. Verify vendor example and exact PCB revision; pin mapping must come from schematics, not assumptions.
3. Implement in a feature branch; create a PR with reproducible build steps and test evidence.

## Status
Simulation core and LVGL 9 host renderer implemented. Host tests are available.
ESP-IDF 5.3.2 builds are now verified for headless and Rev 1.2 bench profiles.
Physical device testing and flash backup remain unverified. See docs/BUILD_VALIDATION.md.
See [implementation status](docs/IMPLEMENTATION.md) and [build instructions](firmware/esp32-hmi/README.md).

## Язык интерфейса

Первая версия HMI полностью на русском (`ru-RU`). Архитектура локализации предусматривает добавление казахского и английского позже; см. [docs/LOCALIZATION.md](docs/LOCALIZATION.md).
