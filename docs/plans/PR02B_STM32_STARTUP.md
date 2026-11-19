# PR-02b — owner SWD facts и проверяемая основа startup

База mainb19a40b, PR #1–27 слиты; четыре workflow main PASS, открытых PR нет.
Источник аппаратных результатов — новое сообщение владельца, не чтение агентом.

## Результат

1. Зафиксировать SWD/ST-Link V2, Cortex-M4r0p1, raw device ID0x101f6413,
   Flash1024KiB, probe voltage3.252V и backup1048576bytes/vector structure PASS.
2. Разделить эти факты и unknown package/HSE/RAM/CCM/pinout/erase geometry/
   physical GPIO/CAN; не приписывать backup не сообщённые SHA/restore/дату теста.
3. Подготовить portable data/BSS startup runtime, Cortex-M4 reset object,
   board adapter без I/O, который не подтверждает physical inhibit/watchdog.
4. Linker sections без MEMORY/default board addresses; opt-in synthetic link
   probe с отдельными test-only ROM/RAM, core-only fixture vectors и fault tests.
   BOARD_IMAGE=ON по-прежнему fail; нет .bin, physical driver или artifact ELF.
5. Host/sanitizers, exact ARM GCC14.2.1/static analysis/link/negative checks,
   CI и evidence; сохранить существующую ESP SDK матрицу/безопасность.

## Приёмка и граница

- Data/BSS init проверяет аргументы до записи; bounds/alias/zero/overflow tests.
- No-I/O adapter не разрешает healthy inputs/ACK превратить unknown hardware
  в готовность; нет GPIO/CAN/clock/Flash register access, STEP/ENABLE/spindle.
- Link probe проверяет vectors/SP/Thumb, data LMA/VMA/BSS/stack и отказы linker
  при overflow/overlap/missing memory/unaligned stack/unsupported constructors.
- Адреса test fixture явно синтетические и не являются разметкой STM32F407.
  Ни bootloader reserve/app offset/sector policy, ни A/B/staging не выбираются.
- Startup/sections reuse допустимы после reviewed board memory/vector/clock
  contract. HSE не нужен для будущего HSI-only пути, но частоты не подтверждены.
- Никаких device/SWD/read/reset/write/option bytes/drive/CAN операций и merge.

Откат — revert PR; память/прошивки реальных устройств не меняются.
