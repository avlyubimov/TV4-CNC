cmake_minimum_required(VERSION 3.20)
project(tv4_motion_foundation C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(TV4_MOTION_BOARD_IMAGE "Build the explicit pinless STM32F407VG bench image" OFF)
set(TV4_MOTION_BOARD_PROFILE "" CACHE STRING "Explicit reviewed board profile; no inferred default")
option(TV4_MOTION_LINK_PROBE "Link a synthetic startup test (never board firmware)" OFF)
if(TV4_MOTION_BOARD_IMAGE)
  if(NOT TV4_MOTION_BOARD_PROFILE STREQUAL "stm32f407vg-hsi-no-motion-v1")
    message(FATAL_ERROR "F407 board image blocked: select the explicit reviewed stm32f407vg-hsi-no-motion-v1 bench profile")
  endif()
  if(TV4_MOTION_LINK_PROBE)
    message(FATAL_ERROR "F407 board image blocked: cannot combine real bench and synthetic probe")
  endif()
  if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    message(FATAL_ERROR "F407 bench image requires the pinned ARM toolchain")
  endif()
endif()
add_library(tv4_motion_core STATIC core/supervisor.c)
target_include_directories(tv4_motion_core PUBLIC core)
target_compile_options(tv4_motion_core PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes)
add_library(tv4_motion_boot_support STATIC startup/memory_init.c board/noio.c)
target_include_directories(tv4_motion_boot_support PUBLIC core startup board)
target_compile_options(tv4_motion_boot_support PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes)
if(CMAKE_CROSSCOMPILING)
  target_compile_options(tv4_motion_boot_support PRIVATE -mgeneral-regs-only -fno-builtin -fno-tree-loop-distribute-patterns)
endif()
if(NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(motion_tests tests/supervisor_tests.c)
  target_link_libraries(motion_tests PRIVATE tv4_motion_core)
  target_compile_options(motion_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  add_test(NAME motion_supervisor COMMAND motion_tests)
  set_tests_properties(motion_supervisor PROPERTIES TIMEOUT 10)
  add_executable(motion_startup_tests tests/startup_tests.c)
  target_link_libraries(motion_startup_tests PRIVATE tv4_motion_boot_support tv4_motion_core)
  target_compile_options(motion_startup_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  add_test(NAME motion_startup_noio COMMAND motion_startup_tests)
  set_tests_properties(motion_startup_noio PROPERTIES TIMEOUT 10)
  add_executable(stm32_bench_tests tests/bench_tests.c
    board/stm32f407vg/hardware.c board/stm32f407vg/diagnostics.c board/stm32f407vg/runtime.c)
  target_include_directories(stm32_bench_tests PRIVATE board/stm32f407vg)
  target_compile_definitions(stm32_bench_tests PRIVATE TV4_BENCH_TEST_MMIO=1)
  target_compile_options(stm32_bench_tests PRIVATE -Wall -Wextra -Werror
    -Wconversion -Wshadow -Wstrict-prototypes -UNDEBUG)
  add_test(NAME stm32_bench_hardware_model COMMAND stm32_bench_tests)
  set_tests_properties(stm32_bench_hardware_model PROPERTIES TIMEOUT 10)
  add_test(NAME stm32_bench_reference COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/check_stm32_bench.py" --references)
  set_tests_properties(stm32_bench_reference PROPERTIES TIMEOUT 10)
endif()
if(TV4_MOTION_BOARD_IMAGE)
  enable_language(ASM)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(TV4_BENCH_OBJCOPY arm-none-eabi-objcopy REQUIRED)
  get_filename_component(tv4_bench_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
  set(tv4_bench "${CMAKE_CURRENT_SOURCE_DIR}/board/stm32f407vg")
  add_custom_target(tv4_bench_identity
    COMMAND ${Python3_EXECUTABLE} "${tv4_bench_root}/tools/write_stm32_bench_identity.py"
      --root "${tv4_bench_root}" --output "${CMAKE_CURRENT_BINARY_DIR}/bench-generated/bench_build_identity.h"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/bench-generated/bench_build_identity.h"
    VERBATIM)
  add_executable(tv4_stm32f407_bench "${tv4_bench}/startup.S" "${tv4_bench}/vectors.S"
    "${tv4_bench}/main.c" "${tv4_bench}/hardware.c" "${tv4_bench}/diagnostics.c" "${tv4_bench}/runtime.c"
    startup/memory_init.c)
  add_dependencies(tv4_stm32f407_bench tv4_bench_identity)
  set_target_properties(tv4_stm32f407_bench PROPERTIES SUFFIX ".elf")
  target_include_directories(tv4_stm32f407_bench PRIVATE "${tv4_bench}" startup
    "${CMAKE_CURRENT_BINARY_DIR}/bench-generated")
  target_compile_options(tv4_stm32f407_bench PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-Wconversion;-Wshadow;-Wstrict-prototypes;-Wdate-time;-mgeneral-regs-only;-fno-builtin;-fno-tree-loop-distribute-patterns;-fno-unwind-tables;-fno-asynchronous-unwind-tables;-fstack-usage;-g;-gdwarf-4;-gno-record-gcc-switches>"
    "-ffile-prefix-map=${tv4_bench_root}=."
    "-ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=.")
  target_link_options(tv4_stm32f407_bench PRIVATE -nostdlib
    "-Wl,-T,${tv4_bench}/memory.ld" "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/tv4_stm32f407_bench.map"
    -Wl,--gc-sections,--no-undefined,--fatal-warnings,--orphan-handling=error,--build-id=none
    -Wl,-z,noexecstack)
  set_property(TARGET tv4_stm32f407_bench APPEND PROPERTY LINK_DEPENDS "${tv4_bench}/memory.ld"
    "${tv4_bench}/profile.json" "${tv4_bench_root}/tools/check_stm32_bench.py"
    "${tv4_bench_root}/third_party/stm32f4-reference/origin.json"
    "${tv4_bench_root}/third_party/stm32f4-reference/startup_stm32f407xx.s")
  add_custom_command(TARGET tv4_stm32f407_bench POST_BUILD
    COMMAND ${TV4_BENCH_OBJCOPY} -O binary "$<TARGET_FILE:tv4_stm32f407_bench>"
      "${CMAKE_CURRENT_BINARY_DIR}/tv4_stm32f407_bench.bin"
    COMMAND ${Python3_EXECUTABLE} "${tv4_bench_root}/tools/check_stm32_bench.py"
      "$<TARGET_FILE:tv4_stm32f407_bench>" --bin "${CMAKE_CURRENT_BINARY_DIR}/tv4_stm32f407_bench.bin"
      --map "${CMAKE_CURRENT_BINARY_DIR}/tv4_stm32f407_bench.map"
      --build "${CMAKE_CURRENT_BINARY_DIR}"
      --output "${CMAKE_CURRENT_BINARY_DIR}/image-check.json"
    VERBATIM)
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bench-inputs.txt"
    CONTENT "$<JOIN:$<TARGET_OBJECTS:tv4_stm32f407_bench>,\n>\n")
  enable_testing()
  add_test(NAME stm32_bench_image_contract COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/bench_image_tests.py"
    --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "${CMAKE_C_COMPILER}")
  set_tests_properties(stm32_bench_image_contract PROPERTIES TIMEOUT 60)
endif()
if(TV4_MOTION_LINK_PROBE)
  if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    message(FATAL_ERROR "Synthetic link probe requires the pinned ARM toolchain; never execute its ELF")
  endif()
  enable_language(ASM)
  add_library(tv4_motion_probe_objects OBJECT startup/reset.S startup/entry.c
              tests/link_fixture/vectors.S tests/link_fixture/data.c)
  target_include_directories(tv4_motion_probe_objects PRIVATE core startup board)
  target_compile_options(tv4_motion_probe_objects PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-mgeneral-regs-only;-fno-builtin;-fno-tree-loop-distribute-patterns>")
  add_executable(tv4_motion_link_probe $<TARGET_OBJECTS:tv4_motion_probe_objects>)
  set_target_properties(tv4_motion_link_probe PROPERTIES SUFFIX ".elf")
  target_link_libraries(tv4_motion_link_probe PRIVATE tv4_motion_boot_support)
  set(probe_memory "${CMAKE_CURRENT_SOURCE_DIR}/tests/link_fixture/memory.ld")
  set(probe_sections "${CMAKE_CURRENT_SOURCE_DIR}/linker/sections.ld")
  target_link_options(tv4_motion_link_probe PRIVATE -nostdlib
    "-Wl,-T,${probe_memory}" "-Wl,-T,${probe_sections}"
    -Wl,--gc-sections,--no-undefined,--fatal-warnings,--orphan-handling=error,--build-id=none
    -Wl,-z,noexecstack
    -Wl,--undefined=tv4_probe_data,--undefined=tv4_probe_bss)
  set_property(TARGET tv4_motion_link_probe APPEND PROPERTY LINK_DEPENDS "${probe_memory}" "${probe_sections}")
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/probe-inputs.txt"
       CONTENT "$<JOIN:$<TARGET_OBJECTS:tv4_motion_probe_objects>,\n>\n$<TARGET_FILE:tv4_motion_boot_support>\n")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  enable_testing()
  add_test(NAME motion_link_contract COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/link_contract_tests.py"
    --elf "$<TARGET_FILE:tv4_motion_link_probe>" --compiler "${CMAKE_C_COMPILER}"
    --objects "${CMAKE_CURRENT_BINARY_DIR}/probe-inputs.txt"
    --sections "${probe_sections}" --memory "${probe_memory}")
  set_tests_properties(motion_link_contract PROPERTIES TIMEOUT 45)
endif()
