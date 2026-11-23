/* Actual DMA diagnostic source, with a mutex modelling the SDK cross-core
 * critical section. No device, network, or heap instrumentation of this host. */
#include "dma_diagnostics.h"
#include "ota_dma_diag_fake.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static int tokens[4];
static _Thread_local TaskHandle_t task=&tokens[0];
static _Thread_local bool isr,in_callback;
static _Thread_local unsigned locked;
static esp_alloc_failed_hook_t hook;
static unsigned registrations,points,events,summaries,last_total,last_stored,last_lost;
static unsigned last_owner,last_isr,last_allocator,last_oversize,last_bytes;
static bool registration_failed,last_valid,inject_logging_failure,inject_wrong_owner;
static unsigned last_sequence;

void dma_test_lock(void) {assert(!locked);assert(!pthread_mutex_lock(&mutex));locked=1;}
void dma_test_unlock(void) {assert(locked==1);locked=0;assert(!pthread_mutex_unlock(&mutex));}
BaseType_t xPortInIsrContext(void) {return isr;}
TaskHandle_t xTaskGetCurrentTaskHandle(void) {assert(!isr);return task;}
int heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t callback)
{
    assert(!locked && !in_callback && !hook && callback);++registrations;
    if(registration_failed)return -1;
    hook=callback;return ESP_OK;
}
static void fail(size_t bytes,const char *function)
{
    assert(hook && !locked && !in_callback);in_callback=true;
    hook(bytes,MALLOC_CAP_DMA,function);in_callback=false;
}
size_t heap_caps_get_free_size(uint32_t caps)
{
    assert(!locked && !in_callback);
    assert(caps==MALLOC_CAP_DMA || caps==(MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    return caps==MALLOC_CAP_DMA?4096:3000;
}
size_t heap_caps_get_largest_free_block(uint32_t caps)
{
    assert(!locked && !in_callback);
    assert(caps==MALLOC_CAP_DMA || caps==(MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    return caps==MALLOC_CAP_DMA?2048:1600;
}
void dma_test_log(const char *tag,const char *format,...)
{
    assert(!locked && !in_callback && task==&tokens[0] && !isr);
    assert(!strcmp(tag,"tv4_ota_dma"));
    char line[384];va_list args;va_start(args,format);
    int n=vsnprintf(line,sizeof(line),format,args);va_end(args);
    assert(n>=0 && (size_t)n<sizeof(line));unsigned kind;
    assert(sscanf(line,"kind=%u",&kind)==1);
    if(kind==1) {
        ++points;
        assert(strstr(line,"dma_caps=8 dma_free=4096 dma_largest=2048 idma_caps=2060 idma_free=3000 idma_largest=1600"));
        if(inject_logging_failure)fail(77,"heap_caps_malloc");
        if(inject_wrong_owner) {
            inject_wrong_owner=false;task=&tokens[1];
            assert(!tv4_ota_dma_begin(TV4_DMA_SNTP));task=&tokens[0];
        }
    }else if(kind==2) {
        unsigned sequence,phase,caps;
        assert(sscanf(line,"kind=2 seq=%u phase=%u bytes=%u caps=%u owner=%u isr=%u oversize=%u allocator=%u",
            &sequence,&phase,&last_bytes,&caps,&last_owner,&last_isr,&last_oversize,&last_allocator)==8);
        assert(sequence==++last_sequence && phase==TV4_DMA_HTTP_HEADERS && caps==MALLOC_CAP_DMA);++events;
        assert(strstr(line,"request_free=4096 request_largest=2048"));
    }else if(kind==3) {
        unsigned valid,exhausted;
        assert(sscanf(line,"kind=3 valid=%u total=%u stored=%u lost=%u exhausted=%u",
            &valid,&last_total,&last_stored,&last_lost,&exhausted)==5);
        last_valid=valid;++summaries;assert(!exhausted);
    }else assert(kind==0);
}
static void *producer(void *arg)
{
    task=arg;for(unsigned i=0;i<2000;i++)fail(1600,"heap_caps_aligned_alloc");return NULL;
}
int main(int argc,char **argv)
{
    assert(argc==2);const char *mode=argv[1];
#if !TV4_OTA_DMA_DIAG_ENABLED
    (void)producer;
    assert(!tv4_ota_dma_init() && !tv4_ota_dma_begin(TV4_DMA_SNTP) &&
        !tv4_ota_dma_end(TV4_DMA_SNTP) && !tv4_ota_dma_finish());
    assert(!registrations && !points && !events && !summaries);(void)mode;
#else
    registration_failed=!strcmp(mode,"init-fail");
    if(registration_failed) {
        assert(!tv4_ota_dma_init() && !tv4_ota_dma_init());
        assert(registrations==1 && !hook && !tv4_ota_dma_finish() && !last_valid);
    }else {
        assert(tv4_ota_dma_init() && tv4_ota_dma_init() && registrations==1);
        if(!strcmp(mode,"begin-race")) {
            inject_wrong_owner=true;assert(!tv4_ota_dma_begin(TV4_DMA_HTTP_HEADERS));
            fail(1600,"heap_caps_aligned_alloc");assert(!tv4_ota_dma_finish());
            assert(!events && !last_total);
        }else {
            if(!strcmp(mode,"outside")) {
                inject_logging_failure=true;fail(77,"heap_caps_malloc");
            }
            assert(tv4_ota_dma_begin(TV4_DMA_HTTP_HEADERS));
            if(!strcmp(mode,"concurrent")) {
                pthread_t threads[4];
                for(unsigned i=0;i<4;i++)assert(!pthread_create(&threads[i],NULL,producer,&tokens[i]));
                for(unsigned i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));
            }else if(!strcmp(mode,"overflow")) {
                for(unsigned i=0;i<5;i++)fail(1600,"heap_caps_aligned_alloc");
            }else if(!strcmp(mode,"isr")) {
                isr=true;fail(1600,"heap_caps_aligned_alloc");isr=false;
            }else if(!strcmp(mode,"other-task")) {
                task=&tokens[1];fail(1600,"heap_caps_aligned_alloc");task=&tokens[0];
            }else if(!strcmp(mode,"unknown"))fail(1600,"unknown_allocator");
            else if(!strcmp(mode,"oversize"))fail(SIZE_MAX,"heap_caps_malloc");
            else fail(1600,"heap_caps_aligned_alloc");
            bool bad=!strcmp(mode,"overflow") || !strcmp(mode,"concurrent") || !strcmp(mode,"oversize");
            if(!strcmp(mode,"wrong-owner")) {
                task=&tokens[1];assert(!tv4_ota_dma_finish());assert(!events && !summaries);
                assert(!tv4_ota_dma_init());task=&tokens[0];bad=true;
            }
            if(!strcmp(mode,"nested")) {assert(!tv4_ota_dma_begin(TV4_DMA_SNTP));bad=true;}
            if(!strcmp(mode,"mismatch")) {assert(!tv4_ota_dma_end(TV4_DMA_SNTP));bad=true;}
            assert(tv4_ota_dma_end(TV4_DMA_HTTP_HEADERS)==!bad);
            assert(tv4_ota_dma_finish()==!bad && last_valid==!bad && summaries==1);
            assert(events==last_stored && last_stored<=4);
            if(!strcmp(mode,"concurrent"))assert(last_total==8000 && last_stored==4 && last_lost==7996);
            else if(!strcmp(mode,"overflow"))assert(last_total==5 && last_stored==4 && last_lost==1);
            else assert(last_total==1 && last_stored==1 && !last_lost);
            if(!strcmp(mode,"isr"))assert(last_isr && !last_owner);
            else if(!strcmp(mode,"other-task"))assert(!last_isr && !last_owner);
            if(!strcmp(mode,"oversize"))assert(last_oversize && last_bytes==UINT32_MAX);
            if(!strcmp(mode,"unknown"))assert(last_allocator==0);
            assert(!tv4_ota_dma_init() && !tv4_ota_dma_begin(TV4_DMA_SNTP));
            assert(!tv4_ota_dma_finish() && summaries==1);
            fail(99,"heap_caps_malloc");assert(events==last_stored);
        }
    }
#endif
    assert(!locked);printf("DMA trace %s PASS; SDK doubles only\n",mode);return 0;
}
