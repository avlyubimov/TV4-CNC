#include "memory_diagnostics.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(TV4_OTA_MEMORY_DIAG_TEST)
#include "ota_memory_diag_fake.h"
#elif defined(ESP_PLATFORM) && CONFIG_TV4_OTA_ENABLED
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#if defined(TV4_OTA_MEMORY_DIAG_TEST) || (defined(ESP_PLATFORM) && CONFIG_TV4_OTA_ENABLED)
#if defined(TV4_OTA_MEMORY_DIAG_TEST)
#define TV4_MEM_TRACE TV4_OTA_MEMORY_DIAG_ENABLED
#elif CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS
#define TV4_MEM_TRACE 1
#else
#define TV4_MEM_TRACE 0
#endif
#if TV4_MEM_TRACE
_Static_assert(TV4_MEM_STARTUP_COUNT==7,"Fixed startup diagnostic ledger requires explicit review");
#if defined(ESP_PLATFORM) && (CONFIG_TV4_OTA_MEMORY_BENCH || CONFIG_TV4_UTC_TLS_BENCH)
#error "OTA service is the sole SDK minimum monitor owner in this profile"
#endif
static portMUX_TYPE memory_mux=portMUX_INITIALIZER_UNLOCKED;
static tv4_ota_memory_sample startup[TV4_MEM_STARTUP_COUNT];
static uint32_t startup_ready,revision;
static bool startup_closed,scope_active,scope_transition,scope_unknown;
/* Lifecycle fields below have one serialized service owner, not main/worker
 * concurrent callers. Only the scope metadata/startup slots use memory_mux. */
static bool owned,disabled,window_open,window_reported;
static tv4_ota_memory_window window_id;
static int start_result,stop_result;
static tv4_ota_memory_sample before,start,end,restored;
static void transition(bool entering,bool active,bool unknown) {
    portENTER_CRITICAL(&memory_mux);
    ++revision;scope_transition=entering;scope_active=active;scope_unknown=unknown;
    portEXIT_CRITICAL(&memory_mux);
}
#endif
uint32_t tv4_ota_memory_scope_revision(void) {
#if TV4_MEM_TRACE
    portENTER_CRITICAL(&memory_mux);uint32_t value=revision;portEXIT_CRITICAL(&memory_mux);return value;
#else
    return 0;
#endif
}
tv4_heap_scope tv4_ota_memory_scope_since(uint32_t first) {
#if TV4_MEM_TRACE
    portENTER_CRITICAL(&memory_mux);
    tv4_heap_scope scope=revision!=first || scope_transition || scope_unknown?TV4_HEAP_UNKNOWN:
        scope_active?TV4_HEAP_WINDOW:TV4_HEAP_HISTORY;
    portEXIT_CRITICAL(&memory_mux);return scope;
#else
    (void)first;return TV4_HEAP_HISTORY;
#endif
}
void tv4_ota_memory_capture(tv4_ota_memory_sample *out) {
    if(!out)return;
    uint32_t first=tv4_ota_memory_scope_revision();
    multi_heap_info_t info;
    out->ms=(uint32_t)(esp_timer_get_time()/1000);
    heap_caps_get_info(&info,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    out->internal_free=(uint32_t)info.total_free_bytes;
    out->internal_min=(uint32_t)info.minimum_free_bytes;
    out->internal_largest=(uint32_t)info.largest_free_block;
    heap_caps_get_info(&info,MALLOC_CAP_SPIRAM);
    out->psram_free=(uint32_t)info.total_free_bytes;
    out->psram_min=(uint32_t)info.minimum_free_bytes;
    out->psram_largest=(uint32_t)info.largest_free_block;
    out->sampler_stack=(uint32_t)uxTaskGetStackHighWaterMark(NULL);
    out->scope=tv4_ota_memory_scope_since(first);
}
#if TV4_MEM_TRACE
bool tv4_ota_memory_startup(tv4_ota_startup_phase phase) {
    if((unsigned)phase>=TV4_MEM_STARTUP_COUNT)return false;
    tv4_ota_memory_sample sample;tv4_ota_memory_capture(&sample);
    bool ok;uint32_t bit=UINT32_C(1)<<(unsigned)phase;
    portENTER_CRITICAL(&memory_mux);
    ok=!startup_closed && !(startup_ready&bit);
    if(ok) {startup[phase]=sample;startup_ready|=bit;}
    portEXIT_CRITICAL(&memory_mux);return ok;
}
bool tv4_ota_memory_window_begin(tv4_ota_memory_window id) {
#if defined(ESP_PLATFORM) && CONFIG_TV4_OTA_WORKER_TLS_BENCH
    const unsigned last=TV4_MEM_TLS_PROBE;
#else
    const unsigned last=TV4_MEM_CLEANUP;
#endif
    if(window_open || (unsigned)id<TV4_MEM_BOOT || (unsigned)id>last)return false;
    window_id=id;window_open=true;window_reported=false;owned=false;
    start_result=stop_result=-1;
    tv4_ota_memory_capture(&before);
    /* The pinned SDK asserts if its bookkeeping allocation fails, and start
     * does NOT reject an already active monitor despite its header contract.
     * This opt-in trace has one profile-exclusive owner; never use these
     * guards as production admission floors or a guaranteed reservation. */
    if(!disabled && before.internal_free>=8192 && before.internal_largest>=4096 && before.psram_largest>=4096) {
        transition(true,false,false);
        start_result=heap_caps_monitor_local_minimum_free_size_start();
        owned=start_result==ESP_OK;
        if(!owned)disabled=true;
        transition(false,owned,!owned);
    }else if(!disabled) {
        start_result=-2;disabled=true;transition(false,false,true);
    }
    if(!owned)before.scope=TV4_HEAP_UNKNOWN;
    tv4_ota_memory_capture(&start);return owned;
}
bool tv4_ota_memory_window_end(tv4_ota_memory_window id) {
    if(!window_open || id!=window_id)return false;
    tv4_ota_memory_capture(&end);
    if(owned) {
        transition(true,true,false);
        stop_result=heap_caps_monitor_local_minimum_free_size_stop();
        owned=false;disabled=stop_result!=ESP_OK;
        transition(false,false,disabled);
    }
    tv4_ota_memory_capture(&restored);window_open=false;
    return start_result==ESP_OK && stop_result==ESP_OK;
}
static void report(unsigned kind,unsigned id,const tv4_ota_memory_sample *s) {
    ESP_LOGI("tv4_ota_mem","kind=%u id=%u sample_ms=%u scope=%u internal=%u minimum=%u largest=%u psram=%u psram_min=%u psram_largest=%u sample_stack=%u worker_stack_at_emit=%u",
        kind,id,(unsigned)s->ms,(unsigned)s->scope,(unsigned)s->internal_free,
        (unsigned)s->internal_min,(unsigned)s->internal_largest,(unsigned)s->psram_free,
        (unsigned)s->psram_min,(unsigned)s->psram_largest,(unsigned)s->sampler_stack,(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
void tv4_ota_memory_report_window(void) {
    if(window_open || window_reported || !window_id)return;
    window_reported=true;
    report(1,window_id,&before);report(2,window_id,&start);
    report(3,window_id,&end);report(4,window_id,&restored);
    ESP_LOGI("tv4_ota_mem","window=%u start_result=%d stop_result=%d complete=%u",
        (unsigned)window_id,start_result,stop_result,(unsigned)(start_result==ESP_OK && stop_result==ESP_OK));
}
void tv4_ota_memory_report_startup(void) {
    portENTER_CRITICAL(&memory_mux);
    if(startup_closed) {portEXIT_CRITICAL(&memory_mux);return;}
    startup_closed=true;uint32_t ready=startup_ready;portEXIT_CRITICAL(&memory_mux);
    for(unsigned i=0;i<TV4_MEM_STARTUP_COUNT;i++)if(ready&(UINT32_C(1)<<i))report(0,i,&startup[i]);
    ESP_LOGI("tv4_ota_mem","startup_mask=%u expected_mask=%u complete=%u",(unsigned)ready,
        (unsigned)((UINT32_C(1)<<TV4_MEM_STARTUP_COUNT)-1),
        (unsigned)(ready==((UINT32_C(1)<<TV4_MEM_STARTUP_COUNT)-1)));
}
#else
bool tv4_ota_memory_startup(tv4_ota_startup_phase p) {(void)p;return false;}
bool tv4_ota_memory_window_begin(tv4_ota_memory_window w) {(void)w;return false;}
bool tv4_ota_memory_window_end(tv4_ota_memory_window w) {(void)w;return false;}
void tv4_ota_memory_report_window(void) {}
void tv4_ota_memory_report_startup(void) {}
#endif
#else
void tv4_ota_memory_capture(tv4_ota_memory_sample *out) {if(out) {memset(out,0,sizeof(*out));out->scope=TV4_HEAP_UNKNOWN;}}
uint32_t tv4_ota_memory_scope_revision(void) {return 0;}
tv4_heap_scope tv4_ota_memory_scope_since(uint32_t first) {(void)first;return TV4_HEAP_UNKNOWN;}
bool tv4_ota_memory_startup(tv4_ota_startup_phase p) {(void)p;return false;}
bool tv4_ota_memory_window_begin(tv4_ota_memory_window w) {(void)w;return false;}
bool tv4_ota_memory_window_end(tv4_ota_memory_window w) {(void)w;return false;}
void tv4_ota_memory_report_window(void) {}
void tv4_ota_memory_report_startup(void) {}
#endif
