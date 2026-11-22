#include "memory_diagnostics.h"
#include "ota_memory_diag_fake.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static unsigned locks,starts,stops,logs,reads;
static bool sdk_active,fail_start,fail_stop,interleave;
static uint32_t free_internal=200000,largest_internal=100000;
static char lines[32][384];
void fake_memory_lock(void) {assert(!locks);locks=1;}
void fake_memory_unlock(void) {assert(locks);locks=0;}
int64_t esp_timer_get_time(void) {assert(!locks);return INT64_C(4294967300000);}
void heap_caps_get_info(multi_heap_info_t *out,uint32_t caps) {
    assert(!locks);++reads;memset(out,0,sizeof(*out));
    if(interleave) {interleave=false;assert(tv4_ota_memory_window_begin(TV4_MEM_BOOT));}
    if(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)) {
        out->total_free_bytes=free_internal;out->minimum_free_bytes=sdk_active?22000:112;
        out->largest_free_block=largest_internal;
    }else {
        assert(caps==MALLOC_CAP_SPIRAM);out->total_free_bytes=6000000;
        out->minimum_free_bytes=sdk_active?5900000:5800000;out->largest_free_block=5000000;
    }
}
int heap_caps_monitor_local_minimum_free_size_start(void) {
    assert(!locks);++starts;
    if(fail_start)return -1;
    /* Pinned SDK permits nested start; the adapter must prevent calling it. */
    sdk_active=true;return 0;
}
int heap_caps_monitor_local_minimum_free_size_stop(void) {
    assert(!locks);++stops;if(fail_stop)return -1;
    assert(sdk_active);sdk_active=false;return 0;
}
unsigned uxTaskGetStackHighWaterMark(void *p) {assert(!locks && !p);return 2048;}
void fake_memory_log(const char *tag,const char *format,...) {
    assert(!locks && logs<32 && !strcmp(tag,"tv4_ota_mem"));
    va_list args;va_start(args,format);
    int n=vsnprintf(lines[logs],sizeof(lines[logs]),format,args);va_end(args);
    assert(n>=0 && (size_t)n<sizeof(lines[logs]));++logs;
}
int main(int argc,char **argv) {
    assert(argc==2);const char *which=argv[1];tv4_ota_memory_sample sample;
    tv4_ota_memory_capture(NULL);tv4_ota_memory_capture(&sample);
    assert(sample.scope==TV4_HEAP_HISTORY && sample.internal_min==112 && sample.ms==4 && sample.sampler_stack==2048);
    assert(!logs && !starts && !stops);
    if(!strcmp(which,"disabled")) {
        assert(!tv4_ota_memory_startup(TV4_MEM_APP_ENTRY));
        assert(!tv4_ota_memory_window_begin(TV4_MEM_BOOT));
        assert(!tv4_ota_memory_window_end(TV4_MEM_BOOT));
        tv4_ota_memory_report_startup();tv4_ota_memory_report_window();
        assert(!starts && !stops && !logs);return 0;
    }
    if(!strcmp(which,"success")) {
        assert(!tv4_ota_memory_startup((tv4_ota_startup_phase)-1));
        assert(!tv4_ota_memory_startup(TV4_MEM_STARTUP_COUNT));
        assert(tv4_ota_memory_startup(TV4_MEM_APP_ENTRY));
        assert(!tv4_ota_memory_startup(TV4_MEM_APP_ENTRY));
        uint32_t before=tv4_ota_memory_scope_revision();
        assert(tv4_ota_memory_window_begin(TV4_MEM_BOOT));
        assert(tv4_ota_memory_scope_since(before)==TV4_HEAP_UNKNOWN);
        assert(!tv4_ota_memory_window_begin(TV4_MEM_SESSION) && starts==1);
        assert(!tv4_ota_memory_window_end(TV4_MEM_SESSION) && !stops);
        for(unsigned i=1;i<TV4_MEM_STARTUP_COUNT;i++)assert(tv4_ota_memory_startup((tv4_ota_startup_phase)i));
        tv4_ota_memory_capture(&sample);assert(sample.scope==TV4_HEAP_WINDOW && sample.internal_min==22000);
        before=tv4_ota_memory_scope_revision();
        assert(tv4_ota_memory_window_end(TV4_MEM_BOOT));
        assert(tv4_ota_memory_scope_since(before)==TV4_HEAP_UNKNOWN);
        tv4_ota_memory_capture(&sample);assert(sample.scope==TV4_HEAP_HISTORY && sample.internal_min==112);
        assert(!logs);tv4_ota_memory_report_startup();tv4_ota_memory_report_window();
        assert(logs==13 && strstr(lines[7],"startup_mask=127 expected_mask=127 complete=1"));
        assert(strstr(lines[0],"scope=0") && strstr(lines[1],"scope=1"));
        tv4_ota_memory_report_startup();tv4_ota_memory_report_window();assert(logs==13);
        assert(!tv4_ota_memory_startup(TV4_MEM_UI_READY));
        assert(starts==1 && stops==1 && !sdk_active);
    }else if(!strcmp(which,"start-fail") || !strcmp(which,"guard-fail")) {
        fail_start=!strcmp(which,"start-fail");sdk_active=fail_start;
        if(!fail_start) {free_internal=4096;largest_internal=1024;}
        assert(!tv4_ota_memory_window_begin(TV4_MEM_BOOT));
        assert(!tv4_ota_memory_window_end(TV4_MEM_BOOT));
        assert(!stops && starts==(unsigned)fail_start && sdk_active==fail_start);
        tv4_ota_memory_report_window();assert(logs==5 && strstr(lines[4],"complete=0"));
        assert(!tv4_ota_memory_window_begin(TV4_MEM_WORKSPACE));
        assert(!tv4_ota_memory_window_end(TV4_MEM_WORKSPACE));
        assert(starts==(unsigned)fail_start && !stops);
        tv4_ota_memory_capture(&sample);assert(sample.scope==TV4_HEAP_UNKNOWN);
    }else if(!strcmp(which,"stop-fail")) {
        assert(tv4_ota_memory_window_begin(TV4_MEM_BOOT));fail_stop=true;
        assert(!tv4_ota_memory_window_end(TV4_MEM_BOOT));
        tv4_ota_memory_capture(&sample);assert(sample.scope==TV4_HEAP_UNKNOWN);
        assert(!tv4_ota_memory_window_begin(TV4_MEM_SESSION));
        assert(!tv4_ota_memory_window_end(TV4_MEM_SESSION));assert(starts==1 && stops==1);
    }else if(!strcmp(which,"transition")) {
        interleave=true;tv4_ota_memory_capture(&sample);assert(sample.scope==TV4_HEAP_UNKNOWN);
        assert(tv4_ota_memory_window_end(TV4_MEM_BOOT));assert(starts==1 && stops==1);
    }else if(!strcmp(which,"abandon")) {
        assert(tv4_ota_memory_window_begin(TV4_MEM_BOOT));
        assert(tv4_ota_memory_window_end(TV4_MEM_BOOT));assert(starts==1 && stops==1 && !logs);
    }else assert(!"unknown test case");
    assert(!locks && reads);puts("Actual memory diagnostic adapter: bounded owner/scope/cleanup PASS");
}
