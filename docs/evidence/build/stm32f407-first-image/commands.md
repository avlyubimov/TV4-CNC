# Reproduce P0 software checks — no hardware commands

From this branch root. Actual install/restore commands are separate,
unexecuted [runbook](../../../STM32_STLINK_RUNBOOK.md).
Exact versions/dependency hashes/licenses: [versions.json](versions.json).

```bash
set -euo pipefail
cmake -S firmware/stm32-motion -B build-stm32f407-bench -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_MOTION_BOARD_IMAGE=ON -DTV4_MOTION_BOARD_PROFILE=stm32f407vg-hsi-no-motion-v1
cmake --build build-stm32f407-bench -j4
ctest --test-dir build-stm32f407-bench -V
cmake -S firmware/stm32-motion -B build-stm32f407-repro -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_MOTION_BOARD_IMAGE=ON -DTV4_MOTION_BOARD_PROFILE=stm32f407vg-hsi-no-motion-v1
cmake --build build-stm32f407-repro -j4
cmp build-stm32f407-bench/tv4_stm32f407_bench.bin build-stm32f407-repro/tv4_stm32f407_bench.bin
cmp build-stm32f407-bench/tv4_stm32f407_bench.elf build-stm32f407-repro/tv4_stm32f407_bench.elf
python3 tools/check_stm32_bench.py --references
TV4_LVGL_DIR="$PWD/../lvgl"
TV4_MBED_ARCHIVE="$PWD/../deps/mbedtls-3.6.7.tar.bz2"
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_DIR" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE"
cmake --build build-host -j4
ctest --test-dir build-host -V
cmake -S . -B build-sanitize -G Ninja -DLVGL_DIR="$TV4_LVGL_DIR" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE" \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build-sanitize -j4
# Outside ptrace/default environment; no options or suppressions.
ctest --test-dir build-sanitize -V
cmake -S firmware/stm32-bootloader -B build-boot-host -G Ninja \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE"
cmake --build build-boot-host -j4
ctest --test-dir build-boot-host -V
```

Existing protocol TSan8 and signed TSan1 are built/run exactly as host.yml,
with the same local official archive override for the standalone signed build.
Previous full commands remain in
[PR54 evidence](../threaded-link/commands.md); no tests/deadlines/budgets changed.
All CTest result rows retained in this evidence; whitespace normalization only.

Actual ARM main analyzer (same executable flags, no host pointer-size substitute):

```bash
mkdir -p build-static-analysis
arm-none-eabi-gcc -std=c11 -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
  -mgeneral-regs-only -ffreestanding -fno-builtin -Wall -Wextra -Werror \
  -Wconversion -Wshadow -Wstrict-prototypes -fanalyzer \
  -Ifirmware/stm32-motion/board/stm32f407vg -Ifirmware/stm32-motion/startup \
  -Ibuild-stm32f407-bench/bench-generated \
  -c firmware/stm32-motion/board/stm32f407vg/main.c -o build-static-analysis/bench-main.o
```

Native analyzer commands/results for all36 actual C units are fully captured in
[static-analysis.txt](static-analysis.txt). Native bench C uses only
`TV4_BENCH_TEST_MMIO=1` substitution for MMIO/barriers and excludes the ARM-only
`memset` alias, so system libc/sanitizers are never overridden. No warning masks.

Existing ARM libraries are built with pinned toolchain and verified archive:

```bash
cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE"
cmake --build build-boot-arm -j4
python3 tools/check_boot_admission_build.py build-boot-arm
python3 tools/check_boot_crypto_source.py --source build-boot-arm/_deps/tv4_boot_mbedtls-src \
  --lock third_party/mbedtls/origin.json
```

All11 codec/transport/archive checks plus actual negative guard commands/exits
are [old-arm-commands.json](old-arm-commands.json), with full old-arm-*-negative.txt
and budget JSON retained. Exact parsed comparison to PR54 versions evidence is
[unchanged-arm-budgets.txt](unchanged-arm-budgets.txt). Old default core/support
object sizes also pass, not presented as usage of the new real ELF.

Synthetic probe command unchanged (never executed as firmware):

```bash
cmake -S firmware/stm32-motion -B build-motion-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_MOTION_LINK_PROBE=ON
cmake --build build-motion-probe
python3 tools/check_motion_link.py build-motion-probe/tv4_motion_link_probe.elf
ctest --test-dir build-motion-probe -V
```

Seven configuration refusals/exact flags/results: [board-gates.json](board-gates.json).
The real-image test inserts unexpected BIN and oversized stack report in temporary
copies and requires refusal; live artifacts are not changed. CI motion.yml builds
two actual directories, runs the same contract/ARM analyzer and requires clean
exact source identity before its text-only evidence upload. No programmer/reset/
device commands are present in build, checker, tests or CI.
