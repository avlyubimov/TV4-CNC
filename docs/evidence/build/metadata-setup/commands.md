# Reproduce PR-07n software checks

From the branch root; no devices or firmware binary produced. Exact compiler,
archive/dependency identities and licenses are in [versions.json](versions.json).
The local LVGL9.2.2 checkout and official Mbed TLS3.6.7 archive are sibling inputs.

```bash
set -euo pipefail
TV4_LVGL_DIR="$PWD/../lvgl"
TV4_MBED_ARCHIVE="$PWD/../deps/mbedtls-3.6.7.tar.bz2"
TV4_ARM_TOOLCHAIN="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake"
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_DIR" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE"
cmake --build build-host -j4
ctest --test-dir build-host -V
cmake -S . -B build-sanitize -G Ninja -DLVGL_DIR="$TV4_LVGL_DIR" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE" \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build-sanitize -j4
# Outside ptrace; default environment; no ASAN/LSAN/UBSAN options/suppressions.
ctest --test-dir build-sanitize -V
cmake -S firmware/stm32-bootloader -B build-boot-host -G Ninja \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE"
cmake --build build-boot-host -j4
ctest --test-dir build-boot-host -V
cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TV4_ARM_TOOLCHAIN" -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBED_ARCHIVE"
cmake --build build-boot-arm -j4
python3 tools/check_boot_admission_build.py build-boot-arm
python3 tools/check_update_wire_build.py build-boot-arm/update-transport
python3 tools/check_update_fragment_build.py build-boot-arm/update-transport
python3 tools/check_update_metadata_build.py build-boot-arm/update-transport
python3 tools/check_metadata_setup_build.py build-boot-arm/update-transport
python3 protocol/can/tests/wire_budget_tests.py --build build-boot-arm/update-transport
python3 protocol/can/tests/wire_budget_tests.py --fragments --build build-boot-arm/update-transport
python3 protocol/can/tests/wire_budget_tests.py --metadata --build build-boot-arm/update-transport
python3 protocol/can/tests/wire_budget_tests.py --setup --build build-boot-arm/update-transport
python3 tools/check_boot_crypto_source.py --source build-boot-arm/_deps/tv4_boot_mbedtls-src \
  --lock third_party/mbedtls/origin.json
cmake -S protocol/can -B build-update-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TV4_ARM_TOOLCHAIN" -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-update-arm -j4
cmake -S firmware/stm32-motion -B build-motion-probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DTV4_MOTION_LINK_PROBE=ON
cmake --build build-motion-probe
python3 tools/check_motion_link.py build-motion-probe/tv4_motion_link_probe.elf
ctest --test-dir build-motion-probe --output-on-failure
```

Actual `-fanalyzer -c` units use strict C11 warnings as errors, exactly the
commands in motion.yml/bootloader.yml/host.yml; no mock replacement translation
units. Bootloader includes pinned Mbed headers/config, source list unchanged.
Board-image ON/host-probe negative cases and throwaway unexpected `.bin` guard
were exercised, removed in finally; all outputs in this directory are text.
The synthetic ELF is inspected only, never executed or used as a board image.
