# F407 — исправление первого аппаратного запуска

База main4fe84f1 после PR56. Эти результаты относятся к **изменённому рабочему
дереву**, source_dirty=true в [image JSON](working-tree-image.json). Они не являются
хешами финального clean кандидата. Exact-head CI и clean image SHA после коммита
прикладываются к PR; новые бинарники остаются локально, не public release.
[Аппаратный FAIL и успешный restore](../../hardware/2026-11-22-f407-swd-preflight/README.md).

| Программная проверка | Фактический результат |
|---|---|
| Root host/LVGL | [77/77](host-tests.txt) |
| ASan/UBSan/LeakSanitizer | [77/77](sanitizer-tests.txt), вне ptrace, default environment, без suppressions |
| Standalone motion | [4/4](motion-host-tests.txt), включая26 MMIO cases и672 CRC single-bit refusals |
| Actual ELF/BIN/map contract | [1/1 CTest,12 Python groups](image-contract.txt), memory/segments/vectors/frames/negative layouts, missing и commented-out DBG_SLEEP отказ |
| Старый дефект | [Те же тесты с неизменённым c09 hardware.c](old-order-regression.txt) отказывают на старом wait-before-start; ожидаемый assertion failure |
| Static analysis | [Actual native3 units и ARM main](static-analysis.txt), warnings-as-errors |
| Воспроизводимость | [ELF/BIN в двух каталогах](reproducibility.txt), byte-identical в одной toolchain; map path bytes не заявлены reproducible |
| OpenOCD config | [noinit syntax check](openocd-offline.txt), без USB/target init; actual debug recovery описан в hardware evidence |

Исправление запускает IWDG до настройки/синхронизации PR/RLR. started=1 выставляется
сразу, ошибки sync/readback не feed. Модель держит flags pending при остановленном
IWDG и проверяет delayed ready, отдельно stuck PVU/RVU, readback faults, сохранение
reset flags и started/no-feed при init failure. Это регрессия найденного дефекта,
**не измерение фактических IWDG_SR bits**. [ST reference/tag/commit](iwdg-reference.json).
HAL не добавлен в runtime. DBG_SLEEP удерживает только Sleep debug access;
Stop/Standby/trace/GPIO не включаются. LINK_DEPENDS включает OpenOCD config.

Новый BIN2540bytes (c09 был2596), SRAM static268, CCM4, main stack4096,
fault stack1024, два guards по32; max individual compiler frame88 при ceiling128.
Нет данных о runtime high-water, timing, SRAM/CCM stress или успешном новом boot.
HSI16MHz/dividers1/noHSE/noPLL, no heap/RTOS/CAN/Flash writer/motion сохранены.

CI уже выполняет эти actual-source тесты в host и sanitizer jobs, ARM contract/
repro/static checks в motion job. Существующие8 ESP SDK profiles, standalone
bootloader49 и TSan8+1 сохраняются. Эта работа не меняет их код/лимиты и не
выдаёт прошлые результаты за повторный локальный прогон. [Команды](commands.md),
[точные инструменты](versions.json), [source hashes](source-hashes.json).
Никаких новых hardware действий в software checks, self-merge или release.
