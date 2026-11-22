# P0 — первый прошиваемый STM32F407VG bench

База исправления main07fb406 после слитого владельцем PR #57.
**Базовый аппаратный стенд clean ee7d179 PASS22ноября**: запись/verify/identity,
RUNNING/progress/data-BSS/CCM word/guards, watchdog reset и UsageFault capture/reset.
Этот exact образ2540bytes оставлен работающим; [фактический отчёт](evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md).
Предыдущий c09 FAIL8 и snapshot restore сохранены в
[истории](evidence/hardware/2026-11-22-f407-swd-preflight/README.md).
[Software evidence](evidence/build/stm32f407-bringup-fix/README.md),
[runbook](STM32_STLINK_RUNBOOK.md). Новая сборка не наследует hardware PASS
по profile name: нужна привязка к exact source/ELF/BIN и отдельный scope.

## Что является исходными данными

Владелец сверил STM32F407VGT6. SWD/ST-Link V2/core/r0p1/ID0x101f6413 и
Flash1024KiB подтверждены owner и агентом. Driver12sectors/RDP0/nWRP0xFFF
прочитаны, current snapshot2×1MiB/SHA/independent NVMe copy проверены; полный
snapshot restore/verify/reset выполнен. Подробные пределы — [HARDWARE_STATUS](HARDWARE_STATUS.md).
REV_ID0x101f означает silicon5/6, не точную ревизию или package pinout.

Карта ниже — официальная спецификация выбранного устройства. Частоты HSI/LSI/HSE,
RAM/CCM/stack stress, GPIO/CAN/physical inhibit и power-loss recovery неизвестны.
Нулевые RAM/CCM pass fields прежнего c09 означали, что self-test не достигнут
из-за IWDG init timeout; на ee7d179 базовые self-tests пройдены. Неизвестный HSE не блокирует HSI/no-motion сборку.

## Проверенная спецификация и происхождение

| Источник | Использование |
|---|---|
| [ST DS8626 Rev12, March 2026](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf) | STM32F407VG: Flash 1 МиБ; SRAM1 112 КиБ + SRAM2 16 КиБ; CCM data RAM 64 КиБ; HSI/LSI характеристики |
| [ST RM0090 Rev22](https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) | §3.3/table5 Flash sectors; §7 **F405/407** RCC; §21 IWDG; §38 debug registers. §6 другого подсемейства для RCC не использован |
| [ST PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf) | Cortex-M4 exception frame/EXC_RETURN, SysTick, VTOR §4.4.4: alignment 128 words/512 bytes |
| [ST cmsis-device-f4 v2.6.11](https://github.com/STMicroelectronics/cmsis-device-f4/tree/0fa0e489e053fa1ca7790bb40b4d76458f64c55d) | Полные 98 vector entries, IRQ names/reserved IRQ79, сверка register addresses/bit masks и SystemInit reference |

Точные SHA-256 PDF и upstream/reference files —
[origin.json](../third_party/stm32f4-reference/origin.json).
В Git сохранены только официальный reference startup и Apache-2.0 license.
Отмечены локальные изменения: поясняющая первая строка и удаление trailing
whitespace, без изменения upstream инструкций/векторов. `--references` проверяет
их SHA и полное совпадение profile vector list. PDF/header/SystemInit не vendored.
Upstream SystemInit **не компилируется**: его optional external-memory paths
могут назначать GPIO. Runtime не зависит от CMSIS/HAL/LL/RTOS/newlib/Mbed TLS.
Vector layout сохраняет ST attribution; [application license pending](../LICENSE-NOTICE.md),
публичные ELF/BIN/Release этим PR не разрешены.

## Память и реальный образ

| Область | Адреса включительно | Размер / использование |
|---|---|---|
| Flash | `0x08000000–0x080fffff` | 1024 КиБ; vectors/text/rodata/data load; bench image budget 16 КиБ |
| SRAM1 | `0x20000000–0x2001bfff` | 112 КиБ |
| SRAM2 | `0x2001c000–0x2001ffff` | 16 КиБ; contiguous SRAM MEMORY в linker — 128 КиБ |
| CCM | `0x10000000–0x1000ffff` | 64 КиБ **data only**, не executable и недоступна DMA; отдельный 4-byte probe после clock enable |
| Main MSP stack | `0x2001f000–0x2001ffff` | 4096 байт; initial MSP `0x20020000` |
| Emergency fault stack | `0x2001ebe0–0x2001efdf` | 1024 байта; top `0x2001efe0` |
| Stack guards | `0x2001ebc0–0x2001ebdf`, `0x2001efe0–0x2001efff` | По 32 байта; guard corruption прекращает обслуживание watchdog |

Flash sectors: 0–3 — по 16 КиБ с `0x08000000`; sector4 — 64 КиБ с
`0x08010000`; sectors5–11 — по 128 КиБ с `0x08020000` до `0x08100000`.
Все 12 точных адресов/размеров зафиксированы в
[profile.json](../firmware/stm32-motion/board/stm32f407vg/profile.json).
Геометрия RM0090 совпала с прочитанным driver report; protection snapshot
и program/verify/full snapshot restore проверены. Это не erase-boundary/stress test.

Исправленный BIN 2540 байт помещается в sector0; SRAM static 268 байт, stacks+
guards 5184 байта, CCM probe 4 байта. Размеры проверяются из **самого ELF**,
не из предположения по суффиксу/размеру дампа. Числа относятся к текущему GCC
14.2.1 MinSizeRel; SHA финального clean commit — в его `image-check.json`.
16 КиБ — ограничение этого стендового приложения, **не резерв загрузчика**.
CAN bootloader/app/staging/metadata addresses пока не назначены.

98 words = initial MSP + 15 core entries + 82 external IRQ entries.
Reserved vector indices `7,8,9,10,13,95` равны нулю; последний соответствует
external IRQ79. Reset и SysTick имеют собственные Thumb handlers, остальные
не reserved направлены в общий fault entry. VTOR указывает на Flash table.
Неиспользуемые NVIC IRQ отключаются/очищаются; включается только core SysTick.

## Запуск, диагностика и watchdog

Reset masks IRQ, устанавливает MSP/CONTROL, копирует `.data`, обнуляет `.bss`,
оставляет отдельную `.noinit` history, готовит guards/VTOR/fault traps.
Hardware setup проверяет CPUID Cortex-M4, device ID low12 `0x413`, Flash size
1024 КиБ, сохраняет исходные reset flags. Он включает HSI, ждёт ready, сначала
переключает SYSCLK на HSI, затем устанавливает AHB/APB dividers 1 и выключает
HSE/CSS/PLL/PLLI2S/MCO. HSI trim сохраняется. HSEBYP очищается после HSE off.
Никаких guessed crystal/PLL frequencies. `SystemCoreClock=16000000`, SysTick
reload15999 — номинальный 1 мс, **не аппаратное измерение точности**.
Все ожидания ограничены 100000 итерациями; это не измеренный time budget.

CCM clock включается до доступа к CCM. Проверяются data/BSS cookies, один CCM
word и guards; это минимальный boot self-test, **не полный RAM/stress test**.
Нет GPIO/AF/TIM/DMA/CAN/STEP/DIR/ENABLE/spindle setup или Flash writer.
`motion_outputs=0` — программный факт; физическая полярность/inhibit не известны.

IWDG работает от LSI: divider64, reload1023. **Start key перед настройкой и
ожиданием PR/RLR sync**, сразу watchdog_started=1, затем bounded wait/readback
и reload. Порядок сверён с ST HAL F4 v1.8.5 (reference only, без runtime HAL). Номинально 2,048 с при LSI32кГц; по диапазону LSI17–47кГц примерно
1,39–3,86 с. Реальный timeout пока не измерен. Main обслуживает его каждые
100 nominal ms только в RUNNING при целых guards и продвижении tick.
SysTick ISR лишь увеличивает ticks; fault/halt/ISR watchdog не обслуживают.
До start key ошибка приводит к IRQ-masked halt; hardware-started watchdog
также может сбросить плату. После start key, включая sync/readback failure,
ошибка не обслуживает активный IWDG и ведёт к reset. Readiness timeout не
маскируется как watchdog_started0. Option bytes не изменяются.

`DBGMCU_APB1_FZ.DBG_IWDG_STOP` намеренно замораживает IWDG при CPU HALT.
Без остановки CPU watchdog действует; debugger halt не моделирует его timeout.
Это volatile debug policy, не изменение option bytes и не независимая защита машины.

Fault assembly выбирает original MSP/PSP, сохраняет EXC_RETURN/IPSR и переключает
MSP на emergency stack **до вызова C**. Capture читает CFSR/HFSR/MMFAR/BFAR;
core frame читается только при допустимом MSP EXC_RETURN, нормальном диапазоне
основного стека и отсутствии stacking/unstacking/lazy faults. PSP/повреждённый
frame не разыменовывается. Nested capture останавливается без feed. Stack guards
не являются MPU и не гарантируют восстановление при произвольной порче RAM.

| SWD symbol | Смысл |
|---|---|
| `tv4_bench_build` (104 байта) | magic/ABI/profile, exact source commit, dirty flag; без даты/пути/ключей |
| `tv4_bench_diag` (168 байт, ABI1) | state/error, boot count/reset flags, IDs/clock snapshot, ticks/heartbeat/feeds, self-test/guards, previous fault, local test request |
| `tv4_bench_history` (84 байта, `.noinit`) | Warm-reset record с magic-last + DMB + CRC; invalid/torn record отклоняется, boot count saturating |

RUNNING=4/error=0, data_bss_pass=ccm_probe_pass=stack_guards_pass=watchdog_started=1
и увеличивающиеся ticks/heartbeat — критерии, подтверждённые на ee7d179.
Сохранённый build-time image-check/profile hardware_tested=false не переписывается:
новая аппаратная проверка привязана к exact SHA в отдельном hardware evidence.
History не переживает гарантированно power loss, не является durable/authenticated
identity или anti-rollback. New reset flags фиксируются до RMVF clear.
Коды errors: 1 core,2 device,3 Flash size,4 HSI,5 clock switch,6 clock stop,
7 LSI,8 IWDG ready,9 IWDG readback,10 memory cookie,11 exception,12 stack guard.

## Сборка и программная приёмка

Из корня checkout с clean exact commit и ARM GNU Toolchain14.2.Rel1/GCC14.2.1:

```bash
cmake -S firmware/stm32-motion -B build-stm32f407-bench -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_MOTION_BOARD_IMAGE=ON \
  -DTV4_MOTION_BOARD_PROFILE=stm32f407vg-hsi-no-motion-v1
cmake --build build-stm32f407-bench -j4
ctest --test-dir build-stm32f407-bench -V
```

Получаются **настоящие** `tv4_stm32f407_bench.elf`, `.bin`, `.map` и
`image-check.json`. Post-build checker обязателен: все allocated sections/LOADs,
word init/NOLOAD/overlap/bounds, stack/MSP/entry/все vectors, exact BIN bytes,
map correspondence, build identity, no undefined/driver/heap symbols, no FP/
semihosting, main-only feed, emergency entry, compiler static frames ≤128 bytes
и BIN allowlist. `.su` maximum88 bytes — отдельный compiler frame,
не измеренный total stack/high-water. Runtime integer-only, hard-float ABI
сохранён; CPACR/FPU не требуется, float instructions отсутствуют.

OpenOCD устанавливает DBG_SLEEP только для Sleep debug access и IWDG halt
freeze. Проверка config требует обе настройки; missing DBG_SLEEP отклоняется.

Root host/sanitizer suite компилирует **те же hardware/diagnostics/runtime C**
с MMIO substitute. Проверяются bounded clock failures, watchdog sequence,
26 cold/debug-entry случаев, pending sync без старта, отдельные PVU/RVU
timeouts, active-watchdog/no-feed при init fault, reject-before-writes при
неверных IDs, reset flags, warm history corruption,
frame bounds и gated feed. ARM contract tests повреждают реальные ELF/BIN/map,
каждый vector, profile/reference и relink тех же объектов с overflow/ctor/orphan.
Никакого ARM execution/emulator/device access. Default libraries и прежний
synthetic probe остаются отдельными, неподдерживаемые profiles отклоняются.

CI собирает exact PR head, повторяет сборку в другом каталоге и сравнивает
ELF/BIN byte-for-byte; checkout/action/archive SHA закреплены. Debug path mapping
убирает абсолютные пути, generated identity не содержит времени. Reproducibility
проверяется в пределах одной точной toolchain; разные distro binutils не объявлены
побитно эквивалентными. CI публикует **только текстовые результаты**, без ELF/BIN.

Базовый стенд ee7d179 подтвердил RUNNING/progress/CCM/guards, watchdog reset
и UsageFault capture. Предыдущий c09 отказал до этих self-tests; исторический
FAIL8 не удаляется. Полные memory stress/stack high-water/точные частоты/physical
GPIO/CAN и power-loss recovery не подтверждены. Следующий приоритет — ESP32 OTA P1.
Новая аппаратная операция по exact image/reset/capture требует нового разрешения.
