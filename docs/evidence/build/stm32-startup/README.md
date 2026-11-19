# PR-02b software evidence — STM32 startup foundation

Дата внесения: 19 ноября2026. База mainb19a40b, PR #1–27 слиты, main4workflow PASS.
[План](../../../plans/PR02B_STM32_STARTUP.md), [контракт](../../../STM32_STARTUP.md),
[отдельные owner hardware facts](../../hardware/stm32f407-owner-swd/README.md).

## Выполненные программные проверки

| Проверка | Результат |
|---|---|
| Полный root host/LVGL32/RGB565 |20/20 CTest suites PASS,1.55s |
| ASan/UBSan/LSan |20/20 PASS,3.68s; detect_leaks=1/halt_on_error=1, без suppressions |
| Standalone motion host |2/2 PASS: existing supervisor и startup/no-I/O |
| ARM static libraries |GCC14.2.1 PASS: core612bytes text, boot support234bytes text, data/bss0 |
| Static analysis |Host GCC15.2.0 -fanalyzer/-Werror, core + init + no-I/O PASS |
| Synthetic link probe |ELF/SP/Thumb/data VMA+actual LMA/BSS/stack/reset IRQ ordering PASS |
| Cross-link fault tests |11 unittest cases PASS,1 CTest suite; ARM-код не исполняется |
| Gates |BOARD_IMAGE=ON отвергнут с probeOFF/ON; native-host probe отвергнут |
| Output scope |Нет firmware .bin или ELF upload; только CMake compiler-ABI cache может иметь suffix.bin |

`core-size.txt`/`boot-support-size.txt` — object regression budgets, **не footprint
готовой платы**. Synthetic ELF: ROM load396bytes, data4/BSS4, stack2048, core
vectors64. Это параметры/результат fixture, не measured F407 RAM/Flash/stack.
Memory profile ROM0x01000000/64KiB и RAM0x30000000/16KiB намеренно синтетический;
не используется сообщённая1MiB для board/bootloader allocation. Probe ELF не запускался.

## Команды

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR=<exact-lvgl9.2.2-source>
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure

cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR=<exact-lvgl9.2.2-source> \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitized --output-on-failure

cmake -S firmware/stm32-motion -B build-motion-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-motion-arm
python3 tools/check_motion_size.py build-motion-arm/libtv4_motion_core.a
python3 tools/check_motion_size.py build-motion-arm/libtv4_motion_boot_support.a

cmake -S firmware/stm32-motion -B build-motion-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_MOTION_LINK_PROBE=ON
cmake --build build-motion-probe
python3 tools/check_motion_link.py build-motion-probe/tv4_motion_link_probe.elf
ctest --test-dir build-motion-probe --output-on-failure
```

`link-cases.log` фиксирует11 individual positive/negative cases. `gates.json` и
board/host gate logs — реальные nonzero configure exits с ожидаемыми причинами.
`static-analysis.log` задаёт точный command/exit0. Никаких emulator/debugger/serial
sessions. Sanitizers запущены вне ptrace sandbox для штатного LeakSanitizer.

Exact local versions — `versions.json`: установленный Ubuntu ARM GCC14.2.1,
Binutils2.45.50. CI сохраняет официальный ARM14.2.Rel1 archive/его SHA-256;
его Binutils/свежий CI checkout не выдаются за локальную Ubuntu сборку.
`source-hashes.json` связывает проверенные STM32 source/tool/workflow inputs.

## Границы

Owner SWD/Flash/voltage/backup подтверждения не являются измерениями агента;
нет новых физических проверок, восстановления или сообщения SHA-256 дампа.
Полный F407 IRQ vector/RAM/CCM/HSE/pinout/erase geometry/GPIO/CAN/recovery остаются
unknown. ELF inspection не подтверждает CPU execution/FPU/частоты/таймеры/питание.
No-I/O adapter не утверждает physical inhibit и удерживает software FAULT.

Новый code только STM32 foundation/проверки; ESP32 C/SDK/locks/layout/UI не менялись.
Локальная ESP matrix повторно не выполнялась; существующий firmware workflow
собирает оба SDK ×4profiles на точном PR checkout. Ни CMake ABI cache.bin, ни
synthetic ELF/archive не публикуются как прошивка. Dump/UID/паролей в evidence нет.
GPIO/STEP/ENABLE/spindle/CAN/Flash/option bytes/SWD/reset не использовались.
