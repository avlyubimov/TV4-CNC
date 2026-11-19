# PR-02b — startup/sections/no-I/O foundation

База mainb19a40b, PR #1–27 слиты. [План](plans/PR02B_STM32_STARTUP.md),
[owner SWD evidence](evidence/hardware/stm32f407-owner-swd/README.md),
[следующий этап](NEXT_STAGE.md). Существующий supervisor PR #7 сохраняется.

## Подтверждения и инженерные границы

| Категория | Содержание / предел |
|---|---|
| Подтверждено владельцем | SWD/ST-Link V2, Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB,3,252V, backup1048576bytes/vector structure PASS |
| Программная основа | C11 data/BSS init, pinless no-I/O platform, Cortex-M4 reset object, GNU ld sections и synthetic link/fault tests |
| Инженерные предположения | Cortex-M4/Thumb reset/MSP entry; сохранённый compile-only M4F hard-float ABI GCC14.2.1. Это не аппаратный FPU test или full STM32 device vector/profile |
| Test fixtures | ROM0x01000000/64KiB, RAM0x30000000/16KiB, stack2048, core-only16vectors. Намеренно синтетические; не память F407 и не bootloader/OTA allocation |
| Не подтверждено | Package marking, RAM/CCM, full device vectors/profile, HSE/clock, pinout/GPIO/CAN, erase geometry/protection/recovery |

Сообщённая Flash1MiB не подставляется в production linker. Generic startup
можно готовить без HSE и пинов приводов; окончательный board linker/vector/clock
контракт нельзя вывести из SWD ID/размера дампа. HSE не является необходимым
условием будущего HSI-only этапа, но HSI/фактическая частота сейчас не настроены
и не измерены. Ни один hardware readiness флаг этим кодом не подтверждается.

## Runtime, adapter и reset

`startup/memory_init.c` переносит 32-bit words `.data` из доверенного linker-owned
load span и обнуляет `.bss`. До первой записи проверяет NULL для непустых spans,
alignment, arithmetic overflow и overlaps. Exact in-place copy допустим;
пересечение BSS с load/data и partial data/load overlap отклоняются. Empty spans
могут быть NULL. Ошибочный вызов не делает частичных записей.

Это не parser образа/Flash writer: функция не может проверить физическую RAM
или доступность произвольного pointer. Caller обязан предоставить валидные
массивы/размеры из reviewed linker. Линкер и hardware profile владеют границами.

`board/noio.c` не обращается к регистрам. Inhibit возвращает **false**, watchdog
callback отсутствует. Supervisor получает CONFIG/PLATFORM faults и не переходит
в ready после self-test/healthy inputs/session/ACK. Это намеренно неполный adapter:
он не утверждает, что физические выходы переведены в безопасные уровни.
Полярности/pinout/GPIO/CAN/ENABLE/STEP/DIR/spindle нигде не выбираются.

`startup/reset.S` маскирует IRQ, устанавливает MSP из linker symbol и вызывает
minimal C entry. На возврате/ошибке — halt с IRQ masked, без watchdog feed.
Clock/HSE/PLL/FPU/VTOR/NVIC/peripheral init и app_main не вызываются. Новые C
startup/support объекты собираются `-mgeneral-regs-only`, без потребности во
включённом FPU. Полный application supervisor/driver не запускается в link probe.

Reset block рассчитан на privileged reset/MSP context. Arbitrary bootloader
handoff с PSP/CONTROL/NVIC/FPU state не реализован и не проверен. Future loader
должен утвердить reset/handoff/vector/stack policy; эти символы не разрешают jump.

## Linker contract и fixture

`linker/sections.ld` не содержит MEMORY, Flash origin/size, app offset, sector
номеров или bootloader reserve. Caller должен явно дать ROM/RAM и ненулевой
stack. Assertions проверяют core-vector/alignment, data/BSS/stack, непересечение
ROM/RAM и ROM load overflow. Неподдерживаемые constructors/destructors и ARM
interworking/dynamic stubs должны быть пусты. Неизвестные orphan sections — error.

Единственный текущий caller — `tests/link_fixture/memory.ld`/vectors.S/data.c.
Core-only64bytes не является полной F407 vector table: external IRQs отсутствуют.
Stack/ROM/RAM fixture не подтверждают физическую память, реальный stack budget
или footprint board app. `-z noexecstack` задаёт ELF metadata, не включает MPU.
**Probe ELF нельзя запускать/прошивать/публиковать как STM32 firmware.**

BOARD_IMAGE=ON остаётся fail при любых probe flags. Нет board memory profile,
.bin/objcopy/flash target, HAL/CMSIS dependency или скрытого target main.
CMSIS/HAL/LL выбирать/закреплять с лицензиями при reviewed device profile;
сейчас generic исходники собственные, новых vendor source dependencies нет.

## Программные проверки и команды

```bash
cmake -S firmware/stm32-motion -B build-motion-host -G Ninja
cmake --build build-motion-host
ctest --test-dir build-motion-host --output-on-failure

cmake -S firmware/stm32-motion -B build-motion-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_MOTION_LINK_PROBE=ON
cmake --build build-motion-probe
python3 tools/check_motion_link.py build-motion-probe/tv4_motion_link_probe.elf
ctest --test-dir build-motion-probe --output-on-failure
```

Host тест проверяет sentinels/data/BSS/empty/overflow/alias/error atomicity и
интеграцию no-I/O с реальным supervisor. Cross-link tests проверяют SP/Thumb/
LMA/VMA/zero BSS/stack, missing memory/stack, ROM/RAM/stack overflow/overlap,
alignment, constructors и испорченные vectors/entry/ELF. ARM-код не исполняется.

CI сохраняет прежние core tests/budget и добавляет support/static analysis,
opt-in link probe/tests и BOARD_IMAGE gates. Upload — только compiler/size/
JSON/log/disassembly allowlist; никаких ELF/archives/binaries/dumps. Exact ARM
GNU14.2.Rel1/GCC14.2.1/Actions SHA сохраняются; ESP32 SDK profiles не меняются.
Результаты и точные ограничения: [evidence](evidence/build/stm32-startup/README.md).

## Следующий board/bootloader этап

Нужны package/reference/RAM/CCM/full vectors, reviewed no-I/O clock/reset и
backup/recovery contract для отдельной board сборки. Sector/erase geometry и
protection state дополнительно блокируют Flash writer/CAN bootloader. Выбор
A/B или staging должен учитывать реальную геометрию/образы/metadata/power-loss,
а не2×половину1MiB или адреса fixtures. Ни один Flash сектор сейчас не резервирован.

ESP32 OTA доставляет подписанный компонент; независимый F407 loader сам
валидирует подпись/hash/length/hardware/protocol/ranges до commit/jump. Software
FAULT/no-I/O/ACK не подтверждают fresh physical quiescent state для update.
Новых SWD/read/reset/write/option bytes/GPIO/CAN/drive операций нет. Откат — revert PR.
