#include "startup_sdk_fake.h"
#include "lvgl.h"
#include "rev12.h"
#include "boot_view.h"
#include "dashboard.h"
#include "export.h"
#include "wifi_service.h"
#include "service.h"
#include "memory_diagnostics.h"
#include "layout_checks.h"
#include <assert.h>
#include <execinfo.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_main(void);
bool __real_wifi_service_start(void);
bool __real_boot_view_finish(void);
void __real_lv_init(void);
void *__real_lv_malloc_core(size_t);
void *__real_lv_realloc_core(void *,size_t);
static jmp_buf done;
static const char *mode;
static bool audit,lv_initialized,boot_removed,frame_complete,fatal,returned;
static bool wifi_at_first_frame,export_at_first_frame;
static unsigned wifi_calls,export_calls,health_calls,board_calls,backlight_calls;
static unsigned reports,touch_advances,stale_health,stale_logs;
static bool resource_snapshot;
static uint32_t clock_ms,ui_ms;
static unsigned phases[TV4_MEM_STARTUP_COUNT],phase_count;
static uint16_t pixels[800*480];
static uint8_t covered[800*480];
static unsigned covered_count;
_Alignas(4) static uint8_t draw_buffer[800*20*2];
static size_t ui_live_peak,after_dashboard_peak,largest_after_dashboard,largest_early;
static bool dashboard_ready,ui_ready,peak_wifi_overlap;
static void *allocation_frames[12];
static int allocation_frame_count;
static uint64_t first_frame_hash;

void __wrap_lv_init(void) {
    /* The builtin pool is not registered while lv_init allocates its own list.
     * Observe subsequent real UI requests, not allocator initialization. */
    __real_lv_init();lv_initialized=true;
}

static uint64_t frame_hash(void) {
    uint64_t value=UINT64_C(14695981039346656037);
    for(unsigned i=0;i<800*480;i++) {
        value^=pixels[i]&255;value*=UINT64_C(1099511628211);
        value^=pixels[i]>>8;value*=UINT64_C(1099511628211);
    }
    return value;
}
static void allocation(size_t request) {
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    size_t live=memory.total_size-memory.free_size;
    if(live>ui_live_peak)ui_live_peak=live;
    if(!ui_ready && request>largest_early)largest_early=request;
    if(dashboard_ready && !ui_ready) {
        if(live>after_dashboard_peak) {after_dashboard_peak=live;peak_wifi_overlap=wifi_calls!=0;}
        if(request>largest_after_dashboard) {
            largest_after_dashboard=request;
            allocation_frame_count=backtrace(allocation_frames,12);
        }
    }
}
void *__wrap_lv_malloc_core(size_t size) {
    void *p=__real_lv_malloc_core(size);if(p && lv_initialized)allocation(size);return p;
}
void *__wrap_lv_realloc_core(void *p,size_t size) {
    void *out=__real_lv_realloc_core(p,size);if(out && lv_initialized)allocation(size);return out;
}
bool __wrap_boot_view_finish(void) {
    bool ok=__real_boot_view_finish();
    if(ok) {boot_removed=true;memset(covered,0,sizeof(covered));covered_count=0;}
    return ok;
}
bool __wrap_wifi_service_start(void) {
    ++wifi_calls;
    if(!audit)assert(frame_complete && ui_ready && boot_removed);
    return strcmp(mode,"wifi-fail") && __real_wifi_service_start();
}
bool diag_export_start(void) {
    ++export_calls;
    if(!audit)assert(frame_complete && ui_ready && boot_removed);
    return strcmp(mode,"export-fail")!=0;
}
static bool touch_mode(void) {return !strcmp(mode,"touch-clock-race") || !strcmp(mode,"touch-report-race") || !strcmp(mode,"touch-stale");}
static unsigned health_limit(void) {return touch_mode()?600:8;}
bool diag_export_submit(const diag_record_t *record) {
    assert(record);++reports;
    if(!strcmp(mode,"touch-stale"))assert(record->touch_age_ms>=1000);
    else if(touch_mode())assert(record->touch_age_ms<100);
    return true;
}
bool __wrap_tv4_ota_service_start(void) {assert(!board_calls);return true;}
void __wrap_tv4_ota_service_health(bool healthy) {
    assert(frame_complete);
    if(!strcmp(mode,"touch-stale")) {if(!healthy)++stale_health;}
    else assert(healthy);
    ++health_calls;
}
bool tv4_ota_memory_startup(tv4_ota_startup_phase phase) {
    assert((unsigned)phase<TV4_MEM_STARTUP_COUNT && phase_count<TV4_MEM_STARTUP_COUNT);
    phases[phase_count++]=phase;
    if(phase==TV4_MEM_DASHBOARD_READY)dashboard_ready=true;
    if(phase==TV4_MEM_UI_READY) {assert(frame_complete);ui_ready=true;ui_ms=clock_ms;}
    return true;
}
uint32_t tv4_ota_memory_scope_revision(void) {return 0;}
tv4_heap_scope tv4_ota_memory_scope_since(uint32_t revision) {(void)revision;return TV4_HEAP_HISTORY;}
const char *esp_err_to_name(esp_err_t error) {(void)error;return "HOST_INJECTED";}
esp_reset_reason_t esp_reset_reason(void) {return ESP_RST_POWERON;}
const esp_app_desc_t *esp_app_get_description(void) {
    static const esp_app_desc_t app={.version="startup-test",.idf_ver="host-sdk"};return &app;
}
int64_t esp_timer_get_time(void) {return (int64_t)clock_ms*1000;}
void vTaskDelay(uint32_t ticks) {
    clock_ms+=ticks;
    assert(clock_ms<10000);
    if(fatal || health_calls>=health_limit())longjmp(done,1);
}
unsigned uxTaskGetStackHighWaterMark(void *task) {assert(!task);return 16384;}
void *heap_caps_malloc(size_t size,uint32_t caps) {
    assert(size==sizeof(draw_buffer) && caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    return !strcmp(mode,"buffer-fail")?NULL:draw_buffer;
}
size_t heap_caps_get_free_size(uint32_t caps) {
    (void)caps;
    if(ui_ready && !strcmp(mode,"touch-report-race"))resource_snapshot=true;
    return 100000;
}
size_t heap_caps_get_total_size(uint32_t caps) {(void)caps;return 8*1024*1024;}
size_t heap_caps_get_minimum_free_size(uint32_t caps) {(void)caps;return 50000;}
size_t heap_caps_get_largest_free_block(uint32_t caps) {(void)caps;return 50000;}
void startup_test_log(bool error,const char *format,...) {
    char line[256];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(error && strstr(line,"Startup stopped:"))fatal=true;
    if(error && strstr(line,"Touch data invalid/stale"))++stale_logs;
}
esp_err_t board_rev12_init(esp_lcd_panel_handle_t *panel) {
    ++board_calls;*panel=(esp_lcd_panel_handle_t)(uintptr_t)1;
    return !strcmp(mode,"board-fail")?ESP_FAIL:ESP_OK;
}
esp_err_t board_rev12_backlight(bool on) {
    assert(on);++backlight_calls;
    return !strcmp(mode,"backlight-fail")?ESP_FAIL:ESP_OK;
}
esp_err_t board_rev12_touch_init(void) {return !strcmp(mode,"touch-fail")?ESP_FAIL:ESP_OK;}
bool board_rev12_touch(board_touch_t *sample) {
    /* Model a higher-priority producer publishing during queue-peek. The
     * actual app_main must obtain its age/freshness clock AFTER this copy. */
    if(ui_ready && (!strcmp(mode,"touch-clock-race") || resource_snapshot)) {
        ++clock_ms;++touch_advances;resource_snapshot=false;
    }
    *sample=(board_touch_t){.x=224,.y=346,.pressed=!strcmp(mode,"held-touch") && health_calls<2,
        .valid=true,.ms=clock_ms-(ui_ready && !strcmp(mode,"touch-stale")?1000u:0u),.errors=0,.stack_watermark=2048};return true;
}
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel,int x0,int y0,int x1,int y1,const void *map) {
    assert(panel && 0<=x0 && x0<x1 && x1<=800 && 0<=y0 && y0<y1 && y1<=480);
    if(!strcmp(mode,"flush-fail") || (boot_removed && !strcmp(mode,"dashboard-flush-fail")))return ESP_FAIL;
    const uint16_t *data=map;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++) {
        unsigned i=(unsigned)y*800+(unsigned)x;pixels[i]=*data++;
        if(boot_removed && !covered[i]) {covered[i]=1;++covered_count;}
    }
    if(!frame_complete && covered_count==800*480) {
        frame_complete=true;first_frame_hash=frame_hash();
        wifi_at_first_frame=wifi_calls!=0;export_at_first_frame=export_calls!=0;
    }
    return ESP_OK;
}
static void audit_output(void) {
    printf("{\"host_only\":true,\"target_heap_placement_tested\":false,\"clock_simulated\":true,"
        "\"first_frame_hash\":\"%016" PRIx64 "\",\"wifi_at_first_frame\":%s,\"export_at_first_frame\":%s,"
        "\"lvgl_pool_peak\":%zu,\"after_dashboard_pool_peak\":%zu,\"largest_before_ui\":%zu,"
        "\"largest_after_dashboard\":%zu,\"wifi_at_after_dashboard_peak\":%s,\"phase_order\":[",
        first_frame_hash,wifi_at_first_frame?"true":"false",export_at_first_frame?"true":"false",
        ui_live_peak,after_dashboard_peak,largest_early,largest_after_dashboard,peak_wifi_overlap?"true":"false");
    for(unsigned i=0;i<phase_count;i++)printf("%s%u",i?",":"",phases[i]);
    printf("],\"largest_request_frames\":[");
    for(int i=0;i<allocation_frame_count;i++)printf("%s\"%p\"",i?",":"",allocation_frames[i]);
    puts("]}");
    FILE *file=fopen("startup-first-frame.ppm","wb");assert(file);
    fprintf(file,"P6\n800 480\n255\n");
    for(unsigned i=0;i<800*480;i++) {
        unsigned char rgb[]={((pixels[i]>>11)&31)*255/31,((pixels[i]>>5)&63)*255/63,(pixels[i]&31)*255/31};
        assert(fwrite(rgb,1,3,file)==3);
    }
    assert(!fclose(file));
}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];audit=!strcmp(mode,"audit");
    if(!setjmp(done)) {app_main();returned=true;}
    bool failure=!strcmp(mode,"board-fail") || !strcmp(mode,"buffer-fail") ||
        !strcmp(mode,"backlight-fail") || !strcmp(mode,"touch-fail") ||
        !strcmp(mode,"flush-fail") || !strcmp(mode,"dashboard-flush-fail");
    if(failure) {
        assert(!wifi_calls && !export_calls && !health_calls && !frame_complete);
        assert(returned || fatal);
    }else {
        assert(!returned && frame_complete && ui_ready && ui_ms>=1200);
        assert(wifi_calls==1 && export_calls==1 && health_calls==health_limit());
        assert(backlight_calls==1 && phase_count==TV4_MEM_STARTUP_COUNT);
        if(!audit) {
            static const unsigned expected[]={0,1,2,3,5,6,4};
            assert(!memcmp(phases,expected,sizeof(expected)));
            assert(!wifi_at_first_frame && !export_at_first_frame && !peak_wifi_overlap);
        }
        check_dashboard_layout(lv_screen_active());
        assert(dashboard_snapshot().x_um==25430);
        if(touch_mode()) {
            assert(reports>=1);
            if(!strcmp(mode,"touch-stale"))assert(stale_health>0 && stale_logs==1);
            else assert(touch_advances>0 && !stale_logs && !stale_health);
        }
        if(audit)audit_output();
    }
    if(lv_initialized)lv_deinit();
    puts("Actual app_main / real LVGL startup integration PASS");
    return 0;
}
