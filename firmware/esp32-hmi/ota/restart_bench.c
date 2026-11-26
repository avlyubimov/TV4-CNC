#include "sdkconfig.h"
#if !(CONFIG_TV4_OTA_RESTART_BENCH || CONFIG_TV4_OTA_RESTART_TRACE) || (CONFIG_TV4_OTA_RESTART_BENCH && CONFIG_TV4_OTA_RESTART_TRACE) || !CONFIG_TV4_OTA_ENABLED || !CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS
#error "Restart markers belong only to the explicit OTA restart diagnostic image"
#endif
#include "restart_bench.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "esp_system.h"

/* Keep the final boundary independent of RTOS, logging locks and PSRAM.
 * This is observation, not a replacement for the SDK restart implementation. */
static const DRAM_ATTR char tv4_restart_hook_format[]="tv4_restart stage=shutdown_hook core=%u\n";
static const DRAM_ATTR char tv4_restart_noos_format[]="tv4_restart stage=noos core=%u\n";
static void IRAM_ATTR shutdown_marker(void) {
    esp_rom_printf(tv4_restart_hook_format,(unsigned)esp_cpu_get_core_id());
}
#if CONFIG_TV4_OTA_RESTART_BENCH
bool tv4_restart_bench_cold_boot(void) {
    esp_reset_reason_t reason=esp_reset_reason();
    bool cold=reason==ESP_RST_POWERON || reason==ESP_RST_EXT;
    esp_rom_printf("tv4_restart stage=boot reset=%u core=%u eligible=%u\n",
        (unsigned)reason,(unsigned)esp_cpu_get_core_id(),(unsigned)cold);
    return cold; /* SW, WDT, panic and unknown never auto-restart. */
}
#endif
#if CONFIG_TV4_OTA_RESTART_TRACE
void tv4_restart_trace_boot(void) {
    esp_rom_printf("tv4_restart stage=boot reset=%u core=%u operator=1\n",
        (unsigned)esp_reset_reason(),(unsigned)esp_cpu_get_core_id());
}
#endif
bool tv4_restart_bench_prepare(void) {
    return esp_register_shutdown_handler(shutdown_marker)==ESP_OK;
}
_Noreturn void tv4_restart_bench_execute(void) {
    esp_rom_printf("tv4_restart stage=request core=%u\n",(unsigned)esp_cpu_get_core_id());
    esp_restart();
}
void __real_esp_restart_noos(void) __attribute__((noreturn));
void IRAM_ATTR __attribute__((noreturn)) __wrap_esp_restart_noos(void) {
    esp_rom_printf(tv4_restart_noos_format,(unsigned)esp_cpu_get_core_id());
    __real_esp_restart_noos();
}
