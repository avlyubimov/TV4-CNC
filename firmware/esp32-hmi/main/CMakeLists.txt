set(board_sources "")
if(CONFIG_TV4_BOARD_REV12)
    if(NOT CONFIG_SPIRAM OR NOT CONFIG_SPIRAM_MODE_OCT)
        message(FATAL_ERROR "Rev 1.2 bench profile requires verified octal PSRAM configuration")
    endif()
    list(APPEND board_sources "../board/rev12.c")
endif()
idf_component_register(SRCS ${board_sources} "main.c" "../core/diagnostics.c" "../diagnostics/export.c" "../network/wifi_service.c" "../network/wifi_profiles.c" "../network/wifi_link.c" "../ui/wifi_view.c" "../ui/wifi_editor.c" "../ui/boot_view.c" "../core/model.c" "../core/journal.c" "../core/tools.c" "../core/strings.c"
 "../ui/dashboard.c" "../ui/tools_view.c" "../assets/tv4_icons.c" "../assets/tv4_lathe.c" "../assets/tv4_button_12.c" "../assets/tv4_bold_24.c" "../assets/tv4_boot_56.c" "../assets/tv4_ru_12.c" "../assets/tv4_ru_14.c" "../assets/tv4_ru_20.c" "../assets/tv4_digits_36.c" "../assets/tv4_digits_32.c"
 INCLUDE_DIRS "." "../core" "../ui" "../board" "../network" "../diagnostics" REQUIRES lvgl esp_timer esp_lcd esp_driver_gpio esp_driver_i2c esp_wifi esp_netif esp_event nvs_flash)
# lv_font_conv resources use the short LVGL include with managed IDF components.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
