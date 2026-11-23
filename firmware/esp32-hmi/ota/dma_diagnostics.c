#include "dma_diagnostics.h"
#if TV4_DMA_TRACE
#if defined(TV4_OTA_DMA_DIAG_TEST)
#include "ota_dma_diag_fake.h"
#else
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include <stddef.h>

enum { FAILURE_CAPACITY = 4 };
typedef struct {
    uint32_t sequence,bytes,caps,phase,flags;
    const char *function;
} failure;
/* Callback data is internal DRAM even when PSRAM/Flash caches are unavailable.
 * There is no log, heap query/allocation/free, string access or task wait in
 * the callback. The short ISR-safe critical section protects bounded stores. */
static DRAM_ATTR portMUX_TYPE dma_mux=portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR struct {
    failure events[FAILURE_CAPACITY];
    TaskHandle_t owner;
    uint32_t total,stored,emitted,lost,active;
    bool attempted,installed,invalid,exhausted,closed;
} dma_trace;
_Static_assert(sizeof(dma_trace)+sizeof(dma_mux)<=192,"DMA ledger budget requires review");

static void IRAM_ATTR tv4_dma_allocation_failed(size_t bytes,uint32_t caps,const char *function)
{
    portENTER_CRITICAL_ISR(&dma_mux);
    if(dma_trace.installed && dma_trace.active!=TV4_DMA_NONE) {
        if(dma_trace.total==UINT32_MAX)dma_trace.exhausted=true;
        else ++dma_trace.total;
        if(dma_trace.stored==FAILURE_CAPACITY) {
            if(dma_trace.lost==UINT32_MAX)dma_trace.exhausted=true;
            else ++dma_trace.lost;
        }else {
            const bool isr=xPortInIsrContext()!=0;
            failure *event=&dma_trace.events[dma_trace.stored++];
            event->sequence=dma_trace.total;
            bool oversize=false;
#if SIZE_MAX > UINT32_MAX
            oversize=bytes>UINT32_MAX;
#endif
            event->bytes=oversize?UINT32_MAX:(uint32_t)bytes;
            event->caps=caps;event->phase=dma_trace.active;
            event->flags=(uint32_t)isr | (uint32_t)(!isr && xTaskGetCurrentTaskHandle()==dma_trace.owner)<<1;
            if(oversize) {event->flags|=4U;dma_trace.invalid=true;}
            /* Native SDK passes a static function name. Only the owner reads
             * it later, with caches available; never emit addresses/strings. */
            event->function=function;
        }
    }
    portEXIT_CRITICAL_ISR(&dma_mux);
}

static bool valid_locked(void)
{
    return dma_trace.installed && !dma_trace.invalid && !dma_trace.exhausted && !dma_trace.lost && !dma_trace.closed;
}

bool tv4_ota_dma_init(void)
{
    if(xPortInIsrContext())return false;
    TaskHandle_t caller=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&dma_mux);
    if(dma_trace.attempted) {
        if(dma_trace.owner!=caller)dma_trace.invalid=true;
        bool ok=valid_locked();portEXIT_CRITICAL(&dma_mux);return ok;
    }
    dma_trace.attempted=true;dma_trace.owner=caller;
    portEXIT_CRITICAL(&dma_mux);
    int result=heap_caps_register_failed_alloc_callback(tv4_dma_allocation_failed);
    portENTER_CRITICAL(&dma_mux);
    dma_trace.installed=result==ESP_OK;
    portEXIT_CRITICAL(&dma_mux);
    ESP_LOGI("tv4_ota_dma","kind=0 version=1 installed=%u result=%d capacity=%u",
        (unsigned)(result==ESP_OK),result,(unsigned)FAILURE_CAPACITY);
    return result==ESP_OK;
}

static bool name_is(const char *name,const char *expected)
{
    if(!name)return false;
    for(unsigned i=0;i<48;i++) {
        if(name[i]!=expected[i])return false;
        if(!name[i])return true;
    }
    return false;
}
static unsigned allocator_id(const char *name)
{
    if(name_is(name,"heap_caps_malloc"))return 1;
    if(name_is(name,"heap_caps_calloc"))return 2;
    if(name_is(name,"heap_caps_realloc"))return 3;
    if(name_is(name,"heap_caps_aligned_alloc"))return 4;
    if(name_is(name,"heap_caps_aligned_calloc"))return 5;
    if(name_is(name,"heap_caps_malloc_default"))return 6;
    return 0;
}

static void emit_failures(void)
{
    for(;;) {
        failure event;
        portENTER_CRITICAL(&dma_mux);
        if(dma_trace.emitted==dma_trace.stored) {portEXIT_CRITICAL(&dma_mux);break;}
        event=dma_trace.events[dma_trace.emitted++];
        portEXIT_CRITICAL(&dma_mux);
        /* This is after the native operation unwound, NOT the failure-time
         * heap. The callback itself never traverses the allocator's heaps. */
        unsigned request_free=(unsigned)heap_caps_get_free_size(event.caps);
        unsigned request_largest=(unsigned)heap_caps_get_largest_free_block(event.caps);
        ESP_LOGI("tv4_ota_dma","kind=2 seq=%u phase=%u bytes=%u caps=%u owner=%u isr=%u oversize=%u allocator=%u request_free=%u request_largest=%u",
            (unsigned)event.sequence,(unsigned)event.phase,(unsigned)event.bytes,(unsigned)event.caps,
            (unsigned)((event.flags>>1)&1U),(unsigned)(event.flags&1U),(unsigned)((event.flags>>2)&1U),
            allocator_id(event.function),request_free,request_largest);
    }
}
static void sample(tv4_dma_phase phase,unsigned edge)
{
    /* Exact DMA caps and the stricter internal/8-bit DMA subset are reported
     * separately. Neither is inferred from generic internal8BIT samples. */
    const uint32_t dma=MALLOC_CAP_DMA,idma=MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    const unsigned dma_free=(unsigned)heap_caps_get_free_size(dma);
    const unsigned dma_largest=(unsigned)heap_caps_get_largest_free_block(dma);
    const unsigned idma_free=(unsigned)heap_caps_get_free_size(idma);
    const unsigned idma_largest=(unsigned)heap_caps_get_largest_free_block(idma);
    ESP_LOGI("tv4_ota_dma","kind=1 phase=%u edge=%u dma_caps=%u dma_free=%u dma_largest=%u idma_caps=%u idma_free=%u idma_largest=%u",
        (unsigned)phase,edge,(unsigned)dma,dma_free,dma_largest,(unsigned)idma,idma_free,idma_largest);
}
bool tv4_ota_dma_begin(tv4_dma_phase phase)
{
    if(xPortInIsrContext())return false;
    TaskHandle_t caller=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&dma_mux);
    bool ok=valid_locked() && dma_trace.owner==caller && dma_trace.active==TV4_DMA_NONE &&
        phase>=TV4_DMA_SNTP && phase<=TV4_DMA_HTTP_CLEANUP;
    if(!ok)dma_trace.invalid=true;
    portEXIT_CRITICAL(&dma_mux);
    if(!ok)return false;
    sample(phase,0); /* Logging is outside the failure-capture interval. */
    portENTER_CRITICAL(&dma_mux);
    ok=valid_locked() && dma_trace.owner==caller && dma_trace.active==TV4_DMA_NONE;
    if(ok)dma_trace.active=(uint32_t)phase;
    portEXIT_CRITICAL(&dma_mux);
    return ok;
}
bool tv4_ota_dma_end(tv4_dma_phase phase)
{
    if(xPortInIsrContext())return false;
    TaskHandle_t caller=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&dma_mux);
    bool owner=dma_trace.installed && dma_trace.owner==caller && dma_trace.active==(uint32_t)phase && phase!=TV4_DMA_NONE;
    if(owner)dma_trace.active=TV4_DMA_NONE;
    else dma_trace.invalid=true;
    bool ok=owner && valid_locked();portEXIT_CRITICAL(&dma_mux);
    if(owner) {sample(phase,1);emit_failures();}
    return ok;
}
bool tv4_ota_dma_finish(void)
{
    if(xPortInIsrContext())return false;
    TaskHandle_t caller=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&dma_mux);
    if(dma_trace.owner!=caller) {
        dma_trace.invalid=true;portEXIT_CRITICAL(&dma_mux);return false;
    }
    if(dma_trace.closed) {portEXIT_CRITICAL(&dma_mux);return false;}
    if(dma_trace.active!=TV4_DMA_NONE)dma_trace.invalid=true;
    dma_trace.active=TV4_DMA_NONE;
    bool ok=valid_locked();uint32_t total=dma_trace.total,stored=dma_trace.stored,lost=dma_trace.lost;
    bool exhausted=dma_trace.exhausted;dma_trace.closed=true;
    portEXIT_CRITICAL(&dma_mux);
    emit_failures();
    ESP_LOGI("tv4_ota_dma","kind=3 valid=%u total=%u stored=%u lost=%u exhausted=%u",
        (unsigned)ok,(unsigned)total,(unsigned)stored,(unsigned)lost,(unsigned)exhausted);
    return ok;
}
#endif
