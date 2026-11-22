#include "ota_service_sdk_fake.h"
#include "sdkconfig.h"
#include "service.h"
#include "memory_diagnostics.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf escape;
static const char *mode;
static TaskFunction_t entry;
static unsigned requested,creates,locks,stack_reads,restarts,wdt_resets,wdt_deletes;
static unsigned window_starts,window_ends,startup_reports,window_reports,sends,network_calls;
static unsigned probes,finishes;
static unsigned tls_probes;
static uint32_t clock_ms;
static bool running,start_result,finish_passed;
static void *probe;
static char boot_log[256];

void service_test_lock(void) {assert(!locks);++locks;}
void service_test_unlock(void) {assert(locks==1);--locks;}
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,uint8_t *bytes,StaticQueue_t *state) {
    assert(!locks && n==1 && size==sizeof(tv4_update_command) && bytes && state);
    return state;
}
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,uint32_t size,void *arg,UBaseType_t priority,TaskHandle_t *task) {
    assert(!locks && fn && !strcmp(name,"tv4_ota") && !arg && priority==1 && !task);
    entry=fn;requested=size;++creates;
    return strcmp(mode,"task-fail") && strcmp(mode,"pending-fail");
}
static void commands_blocked(void) {
    for(int c=TV4_UPDATE_CHECK;c<=TV4_UPDATE_REBOOT;c++)assert(!tv4_ota_service_request((tv4_update_command)c));
    assert(!sends && !network_calls);
}
BaseType_t xQueueSend(QueueHandle_t queue,const void *value,uint32_t wait) {
    (void)queue;(void)value;(void)wait;++sends;assert(!"unprovisioned command entered queue");return 0;
}
BaseType_t xQueueReceive(QueueHandle_t queue,void *out,uint32_t wait) {
    assert(!locks && running && queue && out && wait==portMAX_DELAY);
    tv4_update_snapshot state;tv4_ota_service_snapshot(&state);
    assert(state.enabled && state.state==TV4_UPDATE_SETUP && !state.error);
    assert(state.stack_valid && state.stack_watermark==requested-256);
    commands_blocked();longjmp(escape,1);
}
void vTaskDelay(uint32_t ticks) {
    assert(!locks && running && ticks==50);clock_ms+=ticks;assert(clock_ms<=30000);
    tv4_ota_service_health(strcmp(mode,"health-timeout")!=0);
}
int64_t esp_timer_get_time(void) {return (int64_t)clock_ms*1000;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) {
    assert(!locks && running && !task);++stack_reads;return requested-256;
}
esp_err_t esp_task_wdt_add(TaskHandle_t task) {assert(!locks && running && !task);return ESP_OK;}
esp_err_t esp_task_wdt_reset(void) {assert(!locks && running);++wdt_resets;return ESP_OK;}
esp_err_t esp_task_wdt_delete(TaskHandle_t task) {assert(!locks && running && !task);++wdt_deletes;return ESP_OK;}
_Noreturn void esp_restart(void) {assert(!locks);++restarts;longjmp(escape,1);}
const esp_app_desc_t *esp_app_get_description(void) {
    static const esp_app_desc_t desc={.version="host-service-test"};return &desc;
}
void esp_chip_info(esp_chip_info_t *chip) {(void)chip;assert(!"network operation entered");}
const esp_partition_t *esp_ota_get_running_partition(void) {
    static const esp_partition_t factory={.address=0x10000,.size=0x200000};return &factory;
}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *part,esp_ota_img_states_t *state) {
    assert(part && state);*state=!strcmp(mode,"pending-fail")?ESP_OTA_IMG_PENDING_VERIFY:ESP_OTA_IMG_VALID;return ESP_OK;
}
bool __wrap_tv4_ota_sdk_layout_valid(void) {assert(!locks && running);return true;}
bool __wrap_tv4_ota_sdk_finish_boot(bool passed) {assert(!locks && running);++finishes;finish_passed=passed;return passed;}
void *heap_caps_malloc(size_t bytes,unsigned caps) {
    assert(!locks && running && bytes==256 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++probes;
    probe=!strcmp(mode,"probe-fail")?NULL:malloc(bytes);return probe;
}
void heap_caps_free(void *p) {assert(!locks && p==probe);free(p);probe=NULL;}
bool esp_ptr_external_ram(const void *p) {assert(!locks);return p && p==probe;}
size_t heap_caps_get_free_size(unsigned caps) {(void)caps;return 100000;}
bool wifi_service_online(void) {++network_calls;assert(!"network entered while SETUP");return false;}
bool tv4_utc_tls_bench_run_in_ota_worker(bool (*cancelled)(void *),void *context) {
    assert(SERVICE_TEST_TLS_BENCH && running && !locks && !context && cancelled);
    assert(finishes==1 && finish_passed && wdt_deletes==1 && window_ends==1 && window_reports==1);
    commands_blocked();tv4_update_snapshot state;tv4_ota_service_snapshot(&state);
    assert(state.state==TV4_UPDATE_SETUP && !cancelled(context));
    tv4_ota_service_health(false);assert(cancelled(context));
    tv4_ota_service_health(true);assert(!cancelled(context));
    ++tls_probes;return false; /* A failed optional probe must stay in SETUP. */
}
bool tv4_ota_memory_window_begin(tv4_ota_memory_window id) {
    assert(!locks && !running && id==TV4_MEM_BOOT);++window_starts;return true;
}
bool tv4_ota_memory_window_end(tv4_ota_memory_window id) {
    assert(!locks && id==TV4_MEM_BOOT);++window_ends;return true;
}
void tv4_ota_memory_report_startup(void) {assert(!locks && running);++startup_reports;}
void tv4_ota_memory_report_window(void) {assert(!locks && running);++window_reports;}
void service_test_log(const char *tag,const char *format,...) {
    assert(!locks && running && !strcmp(tag,"tv4_ota"));
    char message[256];va_list args;va_start(args,format);int n=vsnprintf(message,sizeof(message),format,args);va_end(args);
    assert(n>0 && (size_t)n<sizeof(boot_log));
    if(strstr(message,"boot_self_test"))memcpy(boot_log,message,(size_t)n+1);
    else assert(SERVICE_TEST_TLS_BENCH && tls_probes==1 && strstr(message,"worker_tls_bench pass=0"));
}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];
    if(!setjmp(escape)) {
        start_result=tv4_ota_service_start();
        assert(!tv4_ota_service_start()); /* Never create a second owner/task. */
        tv4_update_snapshot state;tv4_ota_service_snapshot(&state);assert(!state.stack_valid && !stack_reads);
        tv4_ota_service_health(true);commands_blocked();
        if(!strcmp(mode,"task-fail")) {assert(!start_result && state.state==TV4_UPDATE_SETUP && state.error==21);}
        else {assert(start_result && entry);running=true;entry(NULL);assert(!"worker returned");}
    }
    assert(creates==1 && requested==(SERVICE_TEST_BENCH?8192u:3072u));
    assert(!locks && !sends && !network_calls && !probe && window_starts==1 && window_ends==1);
    assert(tls_probes==(unsigned)(SERVICE_TEST_TLS_BENCH && !strcmp(mode,"success")));
    if(!strcmp(mode,"task-fail") || !strcmp(mode,"pending-fail")) {
        assert(!stack_reads && !probes && !finishes && !startup_reports && !window_reports);
        assert(restarts==(!strcmp(mode,"pending-fail")));
    }else {
        bool success=!strcmp(mode,"success");assert(finishes==1 && finish_passed==success);
        assert(startup_reports==1 && window_reports==1 && stack_reads>0 && wdt_resets>0);
        assert(wdt_deletes==(unsigned)success && restarts==(unsigned)!success);
        assert(probes==(unsigned)(strcmp(mode,"health-timeout")!=0));
        if(!strcmp(mode,"health-timeout"))assert(clock_ms==30000);
        if(SERVICE_TEST_BENCH)assert(strstr(boot_log,"worker_stack_bytes=8192 stack_bench=1 provisioned=0"));
        else assert(!strstr(boot_log,"stack_bench="));
    }
    printf("actual ESP service: requested=%u mode=%s closed SETUP commands PASS\n",requested,mode);
    return 0;
}
