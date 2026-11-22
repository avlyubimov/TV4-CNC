include(${CMAKE_CURRENT_LIST_DIR}/../../../third_party/cjson/check.cmake)
set(platform_sources "../ota/memory_diagnostics.c")
if(CONFIG_TV4_BOARD_REV12)
    if(NOT CONFIG_SPIRAM OR NOT CONFIG_SPIRAM_MODE_OCT)
        message(FATAL_ERROR "Rev 1.2 bench profile requires verified octal PSRAM configuration")
    endif()
    list(APPEND platform_sources "../board/rev12.c")
endif()
idf_component_register(SRCS ${platform_sources} "main.c" "../ota/verification.c" "../ota/manifest.c" "../ota/hash.c" "../ota/image_stream.c" "../ota/json_guard.c" "../ota/envelope.c" "../ota/download.c" "../ota/https.c" "../ota/session.c" "../ota/installer_sdk.c" "../ota/service.c" "../ota/resource_guard.c" "../ota/memory.c" "../ota/memory_probe.c" "../ota/memory_bench.c" "../ota/time_gate.c" "../ota/timed_download.c" "../ota/sntp_source.c" "../ota/sntp_sdk.c" "../ota/tls_probe.c" "../ota/utc_tls_bench.c" "../../../third_party/cjson/cJSON.c" "../core/diagnostics.c" "../diagnostics/export.c" "../network/wifi_service.c" "../network/wifi_profiles.c" "../network/wifi_link.c" "../ui/wifi_view.c" "../ui/wifi_editor.c" "../ui/boot_view.c" "../core/model.c" "../core/journal.c" "../core/tools.c" "../core/strings.c" "../core/firmware_info.c" "../ui/firmware_view.c"
 "../ui/dashboard.c" "../ui/tools_view.c" "../assets/tv4_icons.c" "../assets/tv4_lathe.c" "../assets/tv4_button_12.c" "../assets/tv4_bold_24.c" "../assets/tv4_boot_56.c" "../assets/tv4_ru_12.c" "../assets/tv4_ru_14.c" "../assets/tv4_ru_20.c" "../assets/tv4_digits_36.c" "../assets/tv4_digits_32.c"
 INCLUDE_DIRS "." "../core" "../ui" "../board" "../network" "../diagnostics" "../ota" "../../../third_party/cjson" REQUIRES esp_app_format lvgl esp_timer esp_lcd esp_driver_gpio esp_driver_i2c esp_wifi esp_netif esp_event nvs_flash mbedtls esp_http_client esp_hw_support heap lwip esp-tls app_update spi_flash)
# lv_font_conv resources use the short LVGL include with managed IDF components.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
# Keep the verification entry point in the ELF so both SDK jobs resolve its
# crypto symbols. Ordinary profiles remain read-only; rev12-ota explicitly
# links the worker/backend below and still requires a local reviewed context.
target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_ota_verify_manifest")

set_source_files_properties("../../../third_party/cjson/cJSON.c" PROPERTIES COMPILE_DEFINITIONS "CJSON_NESTING_LIMIT=8;CJSON_HIDE_SYMBOLS"
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/../../../third_party/cjson/tv4_namespace.h")
target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_ota_manifest_read")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_image_start" "LINKER:--undefined=tv4_image_feed" "LINKER:--undefined=tv4_image_finish" "LINKER:--undefined=tv4_image_abort" "LINKER:--undefined=tv4_image_close")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_ota_signed_manifest_read" "LINKER:--undefined=tv4_https_read" "LINKER:--undefined=tv4_ota_session_run")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_ota_memory_acquire_psram" "LINKER:--undefined=tv4_ota_memory_release" "LINKER:--undefined=tv4_ota_workspace_bytes")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_time_adopt" "LINKER:--undefined=tv4_time_check" "LINKER:--undefined=tv4_time_clock_read")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_sntp_sync" "LINKER:--undefined=tv4_sntp_run")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_tls_probe_run")

target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_ota_session_run_guarded" "LINKER:--undefined=tv4_ota_resource_check" "LINKER:--undefined=tv4_ota_resource_sample_current")

if(CONFIG_TV4_OTA_WORKER_TLS_BENCH AND
   (NOT CONFIG_TV4_OTA_ENABLED OR NOT CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS OR
    NOT CONFIG_TV4_OTA_WORKER_STACK_BENCH OR CONFIG_TV4_UTC_TLS_BENCH OR TV4_OTA_CONTEXT))
    message(FATAL_ERROR "Worker TLS bench requires exclusive unprovisioned OTA stack/trace diagnostics")
endif()
if(CONFIG_TV4_OTA_ENABLED)
    if(CONFIG_TV4_OTA_WORKER_STACK_BENCH AND
       (NOT CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS OR TV4_OTA_CONTEXT))
        message(FATAL_ERROR "Worker stack bench requires unprovisioned OTA diagnostics")
    endif()
    set(ota_context_args "")
    if(TV4_OTA_CONTEXT)
        list(APPEND ota_context_args --context "${TV4_OTA_CONTEXT}")
    endif()
    execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../../tools/prepare_ota_context.py"
        ${ota_context_args} --output "${CMAKE_BINARY_DIR}/ota-local"
        RESULT_VARIABLE ota_context_result)
    if(NOT ota_context_result EQUAL 0)
        message(FATAL_ERROR "Local public OTA context rejected")
    endif()
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_BINARY_DIR}/ota-local")
    target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=tv4_ota_sdk_begin"
        "LINKER:--undefined=tv4_ota_sdk_activate" "LINKER:--undefined=tv4_ota_sdk_write"
        "LINKER:--undefined=tv4_ota_sdk_seal" "LINKER:--undefined=tv4_ota_session_install_guarded")
elseif(TV4_OTA_CONTEXT)
    message(FATAL_ERROR "Local OTA context only belongs to explicit rev12-ota")
endif()

# Compiler side files only, no runtime profiling hooks. Keep the new adapter's
# own frame bounded; these numbers are not a full SDK/logging call-stack bound.
set_source_files_properties("../ota/memory_diagnostics.c" PROPERTIES
    COMPILE_OPTIONS "-fstack-usage;-Wframe-larger-than=192")
set_property(SOURCE "main.c" "../ota/service.c" "../ota/utc_tls_bench.c" APPEND PROPERTY COMPILE_OPTIONS "-fstack-usage")
