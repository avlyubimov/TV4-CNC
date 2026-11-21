# P0 — первый прошиваемый STM32F407VG bench

База main `610f1745d892d5904ea183b50617a20d8ce755bb` (PR #55 слит).
Один реальный standalone ELF/BIN, профиль `stm32f407vg-hsi-no-motion-v1`.
Статус: **программно проверен, аппаратный запуск ещё не выполнен**.
[План](plans/P0_STM32F407_FIRST_IMAGE.md), [проверки](evidence/build/stm32f407-first-image/README.md),
[установка/восстановление](STM32_STLINK_RUNBOOK.md).

После PR #56 выполнен отдельный [SWD preflight](evidence/hardware/2026-11-22-f407-swd-preflight/README.md):
IDs/Flash size/driver geometry/protection прочитаны, но c09a20e не установлен.
Это уточняет hardware snapshot, не результаты исполнения этого образа.

## Что является исходными данными

Владелец подтвердил SWD/ST-Link V2, Cortex-M4 r0p1, raw device ID
`0x101f6413`, Flash 1024 КиБ, 3,252 В, полный дамп 1 048 576 байт и
структурную проверку его векторов. Сообщён суффикс VGT6, плата DevEBox
STM32F4VX_M Ver. 2.0. Это прежние [owner facts](HARDWARE_STATUS.md), без нового
чтения платы/бэкапа. r0p1 — ревизия ядра, не доказательство silicon/package.

Выбранная ниже карта — **официальная спецификация заявленного STM32F407VGT6**,
а не измерение RAM/CCM или стирания этого экземпляра. Full marking/package,
фактический HSI/LSI/HSE, RAM/CCM, protection/option bytes, boot/reset/restore,
GPIO/CAN и physical inhibit остаются аппаратными вопросами. Неизвестные HSE и
пины приводов не мешают подготовить HSI/no-motion образ.

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
Это геометрия из RM0090; protection/erase конкретной платы не проверены.

Текущий BIN 2596 байт помещается в sector0; SRAM static 268 байт, stacks+
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

IWDG работает от LSI: divider64, reload1023; сначала ready/readback, затем
start. Номинально 2,048 с при LSI32кГц; по диапазону LSI17–47кГц примерно
1,39–3,86 с. Реальный timeout пока не измерен. Main обслуживает его каждые
100 nominal ms только в RUNNING при целых guards и продвижении tick.
SysTick ISR лишь увеличивает ticks; fault/halt/ISR watchdog не обслуживают.
До успешного arm IWDG ошибка приводит к IRQ-masked halt; watchdog, уже
запущенный option bytes, может при этом сбросить плату. После arm отказ
ведёт к watchdog reset. Option bytes здесь только неизвестное исходное состояние.

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
и увеличивающиеся ticks/heartbeat — критерии будущего стенда, не результат этого PR.
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

Root host/sanitizer suite компилирует **те же hardware/diagnostics/runtime C**
с MMIO substitute. Проверяются bounded clock failures, watchdog sequence,
reject-before-writes при неверных IDs, reset flags, warm history corruption,
frame bounds и gated feed. ARM contract tests повреждают реальные ELF/BIN/map,
каждый vector, profile/reference и relink тех же объектов с overflow/ctor/orphan.
Никакого ARM execution/emulator/device access. Default libraries и прежний
synthetic probe остаются отдельными, неподдерживаемые profiles отклоняются.

CI собирает exact PR head, повторяет сборку в другом каталоге и сравнивает
ELF/BIN byte-for-byte; checkout/action/archive SHA закреплены. Debug path mapping
убирает абсолютные пути, generated identity не содержит времени. Reproducibility
проверяется в пределах одной точной toolchain; разные distro binutils не объявлены
побитно эквивалентными. CI публикует **только текстовые результаты**, без ELF/BIN.

До отдельного стенда не подтверждены исполнение startup/HSI/CCM, watchdog reset,
fault context, SWD diagnostics и reset/recovery. Следующий результат — их
измерение по [runbook](STM32_STLINK_RUNBOOK.md) после review/разрешения.
