#include "memory_probe.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(ESP_PLATFORM) && CONFIG_TV4_OTA_MEMORY_BENCH
#include "memory.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
static bool requested;
static const char *TAG="tv4_memory_bench";
/* Explicit isolated allocation-probe floors, NOT production TLS policy.
 * No TLS/network test is called by this worker. */
static const tv4_ota_memory_policy probe_policy={true,16384,8192,1024*1024};
static void report(unsigned cycle,const char *phase,tv4_ota_memory_result result,int64_t duration,int patterns) {
    const uint32_t internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    ESP_LOGI(TAG,"cycle=%u phase=%s result=%u pattern_ok=%d us=%" PRId64 " workspace=%u internal_free=%u internal_min=%u internal_largest=%u psram_free=%u psram_largest=%u stack=%u",
        cycle,phase,(unsigned)result,patterns,duration,(unsigned)tv4_ota_workspace_bytes,
        (unsigned)heap_caps_get_free_size(internal),(unsigned)heap_caps_get_minimum_free_size(internal),
        (unsigned)heap_caps_get_largest_free_block(internal),(unsigned)heap_caps_get_free_size(external),
        (unsigned)heap_caps_get_largest_free_block(external),(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static void worker(void *context) {
    (void)context;
    ESP_LOGW(TAG,"Allocation probe only; floors are NOT a TLS budget; three cycles, no HTTPS/install");
    unsigned completed=0;bool passed=true;
    for(unsigned cycle=1;cycle<=3;cycle++) {
        tv4_ota_memory lease={0};report(cycle,"before",TV4_MEMORY_OK,0,-1);
        int64_t start=esp_timer_get_time();
        tv4_ota_memory_result result=tv4_ota_memory_acquire_psram(&lease,&probe_policy);
        int64_t acquired=esp_timer_get_time()-start;
        if(result!=TV4_MEMORY_OK) {report(cycle,"acquire_failed",result,acquired,-1);passed=false;break;}
        report(cycle,"allocated",result,acquired,-1);
        start=esp_timer_get_time();bool ok=tv4_ota_memory_probe_buffers(lease.session);
        int64_t duration=esp_timer_get_time()-start;
        report(cycle,"pattern",TV4_MEMORY_OK,duration,ok);
        /* Always release; probe touches no hash/context/control fields. */
        start=esp_timer_get_time();result=tv4_ota_memory_release(&lease);
        report(cycle,"released",result,esp_timer_get_time()-start,ok);
        if(!ok || result!=TV4_MEMORY_OK) {passed=false;break;}
        completed++;if(cycle<3)vTaskDelay(pdMS_TO_TICKS(250));
    }
    ESP_LOGI(TAG,"Probe finished: completed=%u pass=%u; worker exits. Not a TLS/soak/bootability result",completed,(unsigned)passed);
    vTaskDelete(NULL);
}
bool tv4_ota_memory_bench_start(void) {
    if(requested)return false;
    requested=true;
    /* Budget includes internal task stack/TCB before workspace acquisition.
     * Fail closed, never retry repeatedly under memory pressure. */
    uint32_t caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    if(heap_caps_get_free_size(caps)<24576 || heap_caps_get_largest_free_block(caps)<8192) {
        ESP_LOGE(TAG,"Probe not scheduled: insufficient internal headroom");return false;
    }
    if(xTaskCreate(worker,"tv4_mem_probe",4096,NULL,1,NULL)!=pdPASS) {
        ESP_LOGE(TAG,"Probe not scheduled: task allocation failed");return false;
    }
    return true;
}
#else
bool tv4_ota_memory_bench_start(void) {return false;}
#endif
