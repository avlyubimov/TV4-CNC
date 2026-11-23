cmake_minimum_required(VERSION 3.20)
project(tv4_host C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(HMI ${CMAKE_CURRENT_SOURCE_DIR}/firmware/esp32-hmi)
add_library(tv4_core ${HMI}/core/diagnostics.c ${HMI}/core/model.c ${HMI}/core/journal.c ${HMI}/core/tools.c ${HMI}/core/strings.c ${HMI}/core/firmware_info.c ${HMI}/ota/service.c)
target_include_directories(tv4_core PUBLIC ${HMI}/core)
target_compile_options(tv4_core PRIVATE -Wall -Wextra -Werror)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME build_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/build_validation_tests.py)
add_test(NAME build_candidate COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/build_candidate_tests.py)
add_executable(core_tests tests/core_tests.c)
target_link_libraries(core_tests PRIVATE tv4_core)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME core COMMAND core_tests)
add_executable(firmware_info_tests tests/firmware_info_tests.c)
target_link_libraries(firmware_info_tests PRIVATE tv4_core)
target_compile_options(firmware_info_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME firmware_info COMMAND firmware_info_tests)
add_executable(wifi_profiles_tests tests/wifi_profiles_tests.c ${HMI}/network/wifi_profiles.c ${HMI}/network/wifi_link.c ${HMI}/network/wifi_service.c)
target_include_directories(wifi_profiles_tests PRIVATE ${HMI}/network)
target_compile_options(wifi_profiles_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME wifi_profiles COMMAND wifi_profiles_tests)
if(DEFINED LVGL_DIR)
 set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/simulator/lv_conf.h CACHE STRING "" FORCE)
 set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
 set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
 set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
 add_subdirectory(${LVGL_DIR} lvgl)
 # Compile the actual OTA allocator against the pinned LVGL API, not a copied
 # allocator interface. This target has only an SDK boundary and no hardware.
 add_executable(lvgl_psram_tests tests/lvgl_psram_tests.c ${HMI}/ui/lvgl_psram.c)
 target_include_directories(lvgl_psram_tests PRIVATE tests ${LVGL_DIR})
 target_compile_definitions(lvgl_psram_tests PRIVATE TV4_LVGL_PSRAM_TEST=1 LV_CONF_SKIP=1 LV_USE_STDLIB_MALLOC=255)
 target_compile_options(lvgl_psram_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
 add_test(NAME lvgl_psram_heap COMMAND lvgl_psram_tests)
 add_test(NAME lvgl_psram_build_guards COMMAND ${Python3_EXECUTABLE}
   ${CMAKE_CURRENT_SOURCE_DIR}/tests/lvgl_psram_build_tests.py ${LVGL_DIR})
 set(tv4_ui_sources ${HMI}/ui/firmware_view.c ${HMI}/ui/boot_view.c ${HMI}/ui/dashboard.c ${HMI}/ui/tools_view.c ${HMI}/ui/wifi_view.c ${HMI}/ui/wifi_editor.c ${HMI}/network/wifi_service.c ${HMI}/network/wifi_profiles.c ${HMI}/network/wifi_link.c ${HMI}/assets/tv4_icons.c ${HMI}/assets/tv4_lathe.c ${HMI}/assets/tv4_button_12.c ${HMI}/assets/tv4_bold_24.c ${HMI}/assets/tv4_boot_56.c ${HMI}/assets/tv4_ru_12.c ${HMI}/assets/tv4_ru_14.c ${HMI}/assets/tv4_ru_20.c ${HMI}/assets/tv4_digits_36.c ${HMI}/assets/tv4_digits_32.c)
 add_executable(preview simulator/preview.c tests/layout_checks.c ${tv4_ui_sources})
 target_include_directories(preview PRIVATE ${HMI}/ui ${HMI}/network ${CMAKE_CURRENT_SOURCE_DIR}/tests)
 target_link_libraries(preview PRIVATE tv4_core lvgl m)
 target_link_options(preview PRIVATE "LINKER:--wrap=tv4_ota_service_snapshot" "LINKER:--wrap=tv4_ota_service_request")
 target_compile_options(preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
 add_test(NAME render COMMAND preview)
 add_test(NAME render_rgb565 COMMAND preview --rgb565)
 set_tests_properties(render render_rgb565 PROPERTIES TIMEOUT 15)
 # Compile the actual ESP32 app_main against a host-only SDK boundary and real LVGL.
 add_executable(startup_tests tests/startup_tests.c tests/layout_checks.c ${HMI}/main/main.c ${tv4_ui_sources})
 target_include_directories(startup_tests PRIVATE tests/startup_sdk tests ${HMI}/ui ${HMI}/network ${HMI}/ota ${HMI}/board ${HMI}/diagnostics)
 target_link_libraries(startup_tests PRIVATE tv4_core lvgl m)
 target_compile_options(startup_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG -fno-pie)
 target_link_options(startup_tests PRIVATE -no-pie -rdynamic
   "LINKER:--wrap=wifi_service_start" "LINKER:--wrap=boot_view_finish"
   "LINKER:--wrap=tv4_ota_service_start" "LINKER:--wrap=tv4_ota_service_health"
   "LINKER:--wrap=lv_init" "LINKER:--wrap=lv_malloc_core" "LINKER:--wrap=lv_realloc_core")
 foreach(case success held-touch board-fail buffer-fail backlight-fail touch-fail flush-fail dashboard-flush-fail export-fail wifi-fail touch-clock-race touch-report-race touch-stale)
   add_test(NAME startup_${case} COMMAND startup_tests ${case})
   set_tests_properties(startup_${case} PROPERTIES TIMEOUT 15)
 endforeach()
endif()
# Independent motion supervisor is exercised by the existing host/sanitizer matrix.
add_subdirectory(firmware/stm32-motion)
add_subdirectory(protocol/can)

add_executable(diagnostics_tests tests/diagnostics_tests.c)
target_link_libraries(diagnostics_tests PRIVATE tv4_core)
target_compile_options(diagnostics_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME diagnostics COMMAND diagnostics_tests)

# Host crypto backend only; ESP32 uses SDK-provided mbedTLS, no new component.
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson/check.cmake)
add_library(tv4_json STATIC third_party/cjson/cJSON.c)
target_include_directories(tv4_json PUBLIC third_party/cjson)
target_compile_options(tv4_json PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson/tv4_namespace.h)
target_compile_definitions(tv4_json PRIVATE CJSON_NESTING_LIMIT=8 CJSON_HIDE_SYMBOLS)
add_library(tv4_ota_verification firmware/esp32-hmi/ota/verification.c firmware/esp32-hmi/ota/manifest.c firmware/esp32-hmi/ota/hash.c firmware/esp32-hmi/ota/image_stream.c firmware/esp32-hmi/ota/json_guard.c firmware/esp32-hmi/ota/envelope.c firmware/esp32-hmi/ota/download.c firmware/esp32-hmi/ota/https.c firmware/esp32-hmi/ota/session.c firmware/esp32-hmi/ota/installer_sdk.c firmware/esp32-hmi/ota/resource_guard.c firmware/esp32-hmi/ota/memory.c firmware/esp32-hmi/ota/memory_probe.c firmware/esp32-hmi/ota/memory_bench.c firmware/esp32-hmi/ota/time_gate.c firmware/esp32-hmi/ota/timed_download.c firmware/esp32-hmi/ota/sntp_source.c firmware/esp32-hmi/ota/sntp_sdk.c firmware/esp32-hmi/ota/tls_probe.c firmware/esp32-hmi/ota/utc_tls_bench.c)
target_include_directories(tv4_ota_verification PUBLIC firmware/esp32-hmi/ota)
target_link_libraries(tv4_ota_verification PUBLIC OpenSSL::Crypto tv4_json)
target_compile_options(tv4_ota_verification PRIVATE -Wall -Wextra -Werror)
add_executable(ota_crypto_tests tests/ota_crypto_tests.c)
target_link_libraries(ota_crypto_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_crypto_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_crypto COMMAND ota_crypto_tests)
add_test(NAME ota_candidate COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_candidate_tests.py)
add_test(NAME ota_signed_check_bundle COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_release_tests.py)

add_executable(ota_manifest_tests tests/ota_manifest_tests.c)
target_link_libraries(ota_manifest_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_manifest_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_manifest COMMAND ota_manifest_tests)

add_executable(ota_json_tests tests/ota_json_tests.c)
target_link_libraries(ota_json_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_json_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_json COMMAND ota_json_tests)
set_tests_properties(ota_json PROPERTIES TIMEOUT 15)

add_executable(ota_policy_driver tests/ota_policy_driver.c)
target_link_libraries(ota_policy_driver PRIVATE tv4_ota_verification)
target_compile_options(ota_policy_driver PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_policy_conformance COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_policy_conformance_tests.py
  --driver $<TARGET_FILE:ota_policy_driver>)
set_tests_properties(ota_policy_conformance PROPERTIES TIMEOUT 60)

add_executable(ota_stream_tests tests/ota_stream_tests.c)
target_link_libraries(ota_stream_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_stream_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_stream COMMAND ota_stream_tests)

add_executable(ota_http_tests tests/ota_http_tests.c)
target_link_libraries(ota_http_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_http_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_http COMMAND ota_http_tests)

add_executable(ota_session_tests tests/ota_session_tests.c)
target_link_libraries(ota_session_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_session_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_session COMMAND ota_session_tests)

add_executable(ota_memory_tests tests/ota_memory_tests.c)
target_link_libraries(ota_memory_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_memory_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_memory COMMAND ota_memory_tests)

add_executable(ota_time_tests tests/ota_time_tests.c)
target_link_libraries(ota_time_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_time_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_time COMMAND ota_time_tests)

add_executable(ota_sntp_tests tests/ota_sntp_tests.c)
target_link_libraries(ota_sntp_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_sntp_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_sntp COMMAND ota_sntp_tests)

add_executable(ota_tls_probe_tests tests/ota_tls_probe_tests.c)
target_link_libraries(ota_tls_probe_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_tls_probe_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_tls_probe COMMAND ota_tls_probe_tests)

add_executable(ota_resource_tests tests/ota_resource_tests.c)
target_link_libraries(ota_resource_tests PRIVATE tv4_ota_verification)
target_compile_options(ota_resource_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_resource COMMAND ota_resource_tests)

# Independent read-only admission and logical transport handoff; no runtime caller.
add_subdirectory(firmware/stm32-bootloader)

# Host-only signed artifact/cohort review; no new runtime/firmware caller.
foreach(group cli cohort)
  add_test(NAME release_review_${group} COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/release_cohort_tests.py
    --driver $<TARGET_FILE:boot_motion_review> --signer $<TARGET_FILE:ota_policy_driver> --group ${group})
  set_tests_properties(release_review_${group} PROPERTIES TIMEOUT 60)
endforeach()

# Actual failure-only DMA callback and task/ISR ownership, no device access.
find_package(Threads REQUIRED)
add_test(NAME ota_dma_artifact_guards COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_dma_diagnostics_build_tests.py)
set_tests_properties(ota_dma_artifact_guards PROPERTIES TIMEOUT 30)
foreach(dma_enabled 0 1)
  set(dma_test ota_dma_diagnostics_${dma_enabled}_tests)
  add_executable(${dma_test} tests/ota_dma_diagnostics_tests.c ${HMI}/ota/dma_diagnostics.c)
  target_include_directories(${dma_test} PRIVATE tests ${HMI}/ota)
  target_compile_definitions(${dma_test} PRIVATE TV4_OTA_DMA_DIAG_TEST=1 TV4_OTA_DMA_DIAG_ENABLED=${dma_enabled})
  target_compile_options(${dma_test} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(${dma_test} PRIVATE Threads::Threads)
  if(dma_enabled)
    foreach(case happy outside isr other-task unknown overflow wrong-owner nested mismatch init-fail oversize concurrent begin-race)
      add_test(NAME ota_dma_${case} COMMAND ${dma_test} ${case})
      set_tests_properties(ota_dma_${case} PROPERTIES TIMEOUT 15)
    endforeach()
  else()
    add_test(NAME ota_dma_disabled COMMAND ${dma_test} disabled)
  endif()
endforeach()

# Native UTC/HEAD and the same memory adapter, against both pinned API shapes.
# These executables contain SDK doubles only and never access a live network.
add_test(NAME ota_worker_tls_build_guards COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_worker_tls_build_tests.py)
set_tests_properties(ota_worker_tls_build_guards PROPERTIES TIMEOUT 30)
foreach(tls_sdk 5 6)
  foreach(standalone 0 1)
    set(tls_test ota_worker_tls_${tls_sdk}_${standalone}_tests)
    add_executable(${tls_test} tests/ota_worker_tls_tests.c
      ${HMI}/ota/utc_tls_bench.c ${HMI}/ota/memory_diagnostics.c ${HMI}/ota/dma_diagnostics.c)
    target_include_directories(${tls_test} PRIVATE tests/ota_tls_sdk tests/ota_service_sdk
      tests ${HMI}/ota ${HMI}/network)
    if(standalone)
      set(tls_worker 0)
    else()
      set(tls_worker 1)
    endif()
    target_compile_definitions(${tls_test} PRIVATE ESP_PLATFORM=1 TLS_TEST_SDK=${tls_sdk}
      TLS_TEST_STANDALONE=${standalone} SERVICE_TEST_OTA=${tls_worker}
      SERVICE_TEST_DIAGNOSTICS=${tls_worker} SERVICE_TEST_BENCH=${tls_worker}
      SERVICE_TEST_TLS_BENCH=${tls_worker})
    target_compile_options(${tls_test} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    target_link_libraries(${tls_test} PRIVATE tv4_ota_verification)
    target_link_options(${tls_test} PRIVATE "LINKER:--wrap=tv4_time_clock_read")
    if(standalone)
      set(tls_cases success open-fail)
    else()
      set(tls_cases success offline cancel-settle cancel-sntp cancel-open timeout-open
        sntp-fail start1-fail start2-fail stop1-fail stop2-fail guard init-fail
        open-fail headers-fail status cleanup-fail large-headers clock-jump
        dma-init-fail aes-no-memory dma-overflow)
    endif()
    foreach(case IN LISTS tls_cases)
      add_test(NAME ota_worker_tls_${tls_sdk}_${standalone}_${case} COMMAND ${tls_test} ${case})
      set_tests_properties(ota_worker_tls_${tls_sdk}_${standalone}_${case} PROPERTIES TIMEOUT 15)
    endforeach()
  endforeach()
endforeach()

add_test(NAME release_build_binding COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/release_build_tests.py
  --driver $<TARGET_FILE:boot_motion_review> --signer $<TARGET_FILE:ota_policy_driver>)
set_tests_properties(release_build_binding PROPERTIES TIMEOUT 60)

# The production esp_ota_ops adapter compiled against a fault-injecting SDK.
add_executable(ota_installer_tests tests/ota_installer_tests.c ${HMI}/ota/installer_sdk.c ${HMI}/ota/service.c)
target_include_directories(ota_installer_tests PRIVATE ${HMI}/ota tests)
target_compile_definitions(ota_installer_tests PRIVATE TV4_OTA_SDK_TEST=1)
target_compile_options(ota_installer_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_installer COMMAND ota_installer_tests)
add_test(NAME ota_setup COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_setup_tests.py)
add_executable(ota_memory_diagnostics_tests tests/ota_memory_diagnostics_tests.c ${HMI}/ota/memory_diagnostics.c)
target_include_directories(ota_memory_diagnostics_tests PRIVATE ${HMI}/ota tests)
target_compile_definitions(ota_memory_diagnostics_tests PRIVATE TV4_OTA_MEMORY_DIAG_TEST=1 TV4_OTA_MEMORY_DIAG_ENABLED=1)
target_compile_options(ota_memory_diagnostics_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
foreach(case success start-fail guard-fail stop-fail transition abandon)
  add_test(NAME ota_memory_diagnostics_${case} COMMAND ota_memory_diagnostics_tests ${case})
endforeach()
add_executable(ota_memory_diagnostics_disabled_tests tests/ota_memory_diagnostics_tests.c ${HMI}/ota/memory_diagnostics.c)
target_include_directories(ota_memory_diagnostics_disabled_tests PRIVATE ${HMI}/ota tests)
target_compile_definitions(ota_memory_diagnostics_disabled_tests PRIVATE TV4_OTA_MEMORY_DIAG_TEST=1 TV4_OTA_MEMORY_DIAG_ENABLED=0)
target_compile_options(ota_memory_diagnostics_disabled_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME ota_memory_diagnostics_disabled COMMAND ota_memory_diagnostics_disabled_tests disabled)
add_test(NAME ota_memory_diagnostics_artifacts COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_memory_diagnostics_build_tests.py)

# Execute the real ESP service against an SDK boundary, never a board/network.
add_test(NAME ota_worker_stack_build_guards COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ota_worker_stack_build_tests.py)
set_tests_properties(ota_worker_stack_build_guards PROPERTIES TIMEOUT 30)
foreach(stack_bench 0 1 2 3)
  set(service_test ota_worker_stack_${stack_bench}_tests)
  add_executable(${service_test} tests/ota_worker_stack_tests.c ${HMI}/ota/service.c)
  target_include_directories(${service_test} PRIVATE tests/ota_service_sdk tests ${HMI}/ota ${HMI}/network)
  target_compile_definitions(${service_test} PRIVATE ESP_PLATFORM=1
    SERVICE_TEST_BENCH=$<OR:$<STREQUAL:${stack_bench},1>,$<STREQUAL:${stack_bench},2>>
    SERVICE_TEST_TLS_BENCH=$<STREQUAL:${stack_bench},2>
    SERVICE_TEST_PROVISIONED=$<STREQUAL:${stack_bench},3>)
  target_compile_options(${service_test} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(${service_test} PRIVATE tv4_ota_verification)
  target_link_options(${service_test} PRIVATE "LINKER:--wrap=tv4_ota_sdk_layout_valid" "LINKER:--wrap=tv4_ota_sdk_finish_boot")
  foreach(case success task-fail pending-fail probe-fail health-timeout)
    add_test(NAME ota_worker_stack_${stack_bench}_${case} COMMAND ${service_test} ${case})
    set_tests_properties(ota_worker_stack_${stack_bench}_${case} PROPERTIES TIMEOUT 15)
  endforeach()
endforeach()
