# Программная проверка исправления, без доступа к платам

Из корня worktree. Сборка и tests не вызывают programmer/device commands.

```bash
cmake -S firmware/stm32-motion -B build-motion-host -G Ninja
cmake --build build-motion-host -j4
ctest --test-dir build-motion-host -V
for directory in build-stm32f407-bench build-stm32f407-repro; do
  cmake -S firmware/stm32-motion -B "$directory" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DTV4_MOTION_BOARD_IMAGE=ON -DTV4_MOTION_BOARD_PROFILE=stm32f407vg-hsi-no-motion-v1
  cmake --build "$directory" -j4
done
ctest --test-dir build-stm32f407-bench -V
cmp build-stm32f407-bench/tv4_stm32f407_bench.bin build-stm32f407-repro/tv4_stm32f407_bench.bin
cmp build-stm32f407-bench/tv4_stm32f407_bench.elf build-stm32f407-repro/tv4_stm32f407_bench.elf
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$PWD/../lvgl" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$PWD/../deps/mbedtls-3.6.7.tar.bz2"
cmake --build build-host -j4
ctest --test-dir build-host -V
cmake -S . -B build-sanitize -G Ninja -DLVGL_DIR="$PWD/../lvgl" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$PWD/../deps/mbedtls-3.6.7.tar.bz2" \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build-sanitize -j4
# Outside ptrace/default environment, no ASAN/UBSAN/LSAN options/suppressions.
ctest --test-dir build-sanitize -V
```

Native actual-source analyzer: for hardware/diagnostics/runtime, `gcc -std=c11
-Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes -fanalyzer
-DTV4_BENCH_TEST_MMIO=1 -Ifirmware/stm32-motion/board/stm32f407vg -c` each unit.
ARM main analyzer uses the same flags/toolchain as motion.yml, generated include
directory from actual bench build, -mcpu=cortex-m4/-mthumb/-mfloat-abi=hard/
-mfpu=fpv4-sp-d16/-mgeneral-regs-only/-ffreestanding/-fno-builtin.
Old-order regression compiled the same updated bench_tests.c, diagnostics.c and
runtime.c against `git show c09a20e:firmware/stm32-motion/board/stm32f407vg/hardware.c`
in a temporary output directory; expected nonzero/assertion checked, core dump disabled.

```bash
python3 tools/check_stm32_bench.py --references
openocd -c noinit -f firmware/stm32-motion/board/stm32f407vg/openocd.cfg -c shutdown
```

Последний вызов проверяет Tcl config без init/USB/MCU, не новый аппаратный сеанс.
