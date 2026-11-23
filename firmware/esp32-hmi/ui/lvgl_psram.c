/* LVGL9 CUSTOM allocator for the explicit PSRAM-XiP OTA profile.
 * GUI objects are not DMA buffers; keep their allocations out of internal RAM.
 * LVGL is owned by app_main. No internal fallback, task, lock or allocation
 * ledger; the SDK allocator retains its normal realloc/failure semantics. */
#if defined(TV4_LVGL_PSRAM_TEST)
#include "lvgl_psram_sdk.h"
#elif defined(ESP_PLATFORM)
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#endif

#if defined(TV4_LVGL_PSRAM_TEST) || (defined(ESP_PLATFORM) && CONFIG_TV4_OTA_ENABLED)
#include "lvgl.h"
#include <string.h>
#if !CONFIG_TV4_BOARD_REV12 || !CONFIG_SPIRAM || !CONFIG_SPIRAM_XIP_FROM_PSRAM
#error "OTA GUI allocator requires the verified Rev1.2 PSRAM-XiP profile"
#endif
#if LV_USE_STDLIB_MALLOC != LV_STDLIB_CUSTOM
#error "OTA requires LVGL CUSTOM malloc; regenerate this build's sdkconfig from sdkconfig.ota"
#endif

void *lv_malloc_core(size_t size)
{
    return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
void *lv_realloc_core(void *memory,size_t size)
{
    return heap_caps_realloc(memory,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
void lv_free_core(void *memory) {heap_caps_free(memory);}
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}

/* Pool administration and LVGL-only accounting were unsupported by CLIB too.
 * Zero monitor values do not represent the shared PSRAM heap's free space;
 * use the existing SDK capability diagnostics for resource admission. */
lv_mem_pool_t lv_mem_add_pool(void *memory,size_t bytes)
{
    LV_UNUSED(memory);LV_UNUSED(bytes);return NULL;
}
void lv_mem_remove_pool(lv_mem_pool_t pool) {LV_UNUSED(pool);}
void lv_mem_monitor_core(lv_mem_monitor_t *monitor)
{
    if(monitor)memset(monitor,0,sizeof(*monitor));
}
lv_result_t lv_mem_test_core(void)
{
    return heap_caps_check_integrity(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT,false)
        ? LV_RESULT_OK : LV_RESULT_INVALID;
}
#endif
