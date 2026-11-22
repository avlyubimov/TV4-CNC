/* Execute the actual native HTTP backend and diagnostic adapter. All SDK I/O
 * is fault-injected here: no sockets, SNTP, device handles or Flash APIs. */
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_tls.h"
#include "tls_probe.h"
#include "sntp_source.h"
#include "memory_diagnostics.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct fake_http_client {esp_http_client_config_t config;};
static struct fake_http_client client;
static const char *mode;
static uint32_t clock_ms;
static unsigned locks,starts,stops,syncs,inits,opens,headers,cleanups,creates,deletes,windows;
static bool active,was_cancelled,finished,passed;
static TaskFunction_t standalone;
static jmp_buf escape;
static int cancel_cookie;
static unsigned result;
static bool is(const char *s) {return !strcmp(mode,s);}
void service_test_lock(void) {assert(!locks);++locks;}
void service_test_unlock(void) {assert(locks==1);--locks;}
int64_t esp_timer_get_time(void) {return (int64_t)clock_ms*1000;}
void vTaskDelay(uint32_t ticks) {assert(!locks && ticks==250);clock_ms+=ticks;assert(clock_ms<=30000);}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) {assert(!locks && !task);return 6000;}
size_t heap_caps_get_free_size(unsigned caps) {(void)caps;return is("guard")?1000:100000;}
size_t heap_caps_get_minimum_free_size(unsigned caps) {return heap_caps_get_free_size(caps)-128;}
size_t heap_caps_get_largest_free_block(unsigned caps) {return heap_caps_get_free_size(caps)/2;}
void heap_caps_get_info(multi_heap_info_t *info,unsigned caps) {
    assert(!locks);*info=(multi_heap_info_t){heap_caps_get_free_size(caps),
        heap_caps_get_minimum_free_size(caps),heap_caps_get_largest_free_block(caps)};
}
esp_err_t heap_caps_monitor_local_minimum_free_size_start(void) {
    assert(!locks && !active);++starts;
    if((starts==1 && is("start1-fail")) || (starts==2 && is("start2-fail")))return -1;
    active=true;return ESP_OK;
}
esp_err_t heap_caps_monitor_local_minimum_free_size_stop(void) {
    assert(!locks && active);++stops;
    if((stops==1 && is("stop1-fail")) || (stops==2 && is("stop2-fail")))return -1;
    active=false;return ESP_OK;
}
static bool cancelled(void *context) {
    assert(context==&cancel_cookie && !locks);
    return is("cancel-settle") || was_cancelled;
}
bool wifi_service_online(void) {assert(!locks && !active);return !is("offline");}
tv4_time_result __wrap_tv4_time_clock_read(tv4_time_sample *out) {
    *out=(tv4_time_sample){INT64_C(1770000000000000)+(int64_t)clock_ms*1000,
        (uint64_t)clock_ms*1000,TV4_TIME_UNKNOWN};
    if(is("clock-jump"))out->epoch_us+=10000000;
    return TV4_TIME_OK;
}
tv4_sntp_result tv4_sntp_sync(tv4_time_anchor *anchor,const tv4_time_policy *policy,uint32_t timeout,
    bool (*cancel)(void *),void *context,tv4_sntp_report *report) {
    assert(!locks && active && starts==1 && stops==0 && timeout==10000);++syncs;
    assert(policy->reviewed && policy->allow_unauthenticated_sntp);
    assert(policy->earliest_epoch==1767225600u && policy->latest_epoch==1798761599u);
    if(TLS_TEST_STANDALONE)assert(!cancel && !context);
    else assert(cancel && context==&cancel_cookie && !cancel(context));
    *report=(tv4_sntp_report){.result=TV4_SNTP_OK,.time_result=TV4_TIME_OK,.cleanup_ok=true};
    if(is("sntp-fail"))return report->result=TV4_SNTP_TIMEOUT;
    if(is("cancel-sntp")) {was_cancelled=true;assert(cancel(context));return report->result=TV4_SNTP_CANCELLED;}
    tv4_time_sample sample;__wrap_tv4_time_clock_read(&sample);sample.source=TV4_TIME_SNTP_UNAUTHENTICATED;
    /* Model a clock jump only after successful adoption. */
    if(is("clock-jump"))sample.epoch_us-=10000000;
    assert(tv4_time_adopt(anchor,policy,&sample)==TV4_TIME_OK);return TV4_SNTP_OK;
}
esp_err_t esp_crt_bundle_attach(void *tls) {(void)tls;assert(!"bundle attachment is SDK-owned");return 0;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
    assert(!locks && active && starts==2 && stops==1 && syncs==1);++inits;
    assert(!strcmp(config->url,"https://github.com/avlyubimov/TV4-CNC"));
    assert(config->method==HTTP_METHOD_HEAD && config->transport_type==HTTP_TRANSPORT_OVER_SSL);
    assert(config->crt_bundle_attach==esp_crt_bundle_attach && !config->skip_cert_common_name_check);
    assert(config->disable_auto_redirect && config->auth_type==HTTP_AUTH_TYPE_NONE && config->max_authorization_retries==-1);
    assert(config->timeout_ms>0 && config->timeout_ms<=5000 && config->buffer_size==2048 && config->buffer_size_tx==512);
    assert(!config->keep_alive_enable && config->event_handler && config->user_data);
    assert(!strcmp(config->user_agent,"TV4-CNC-TLS-Bench/1"));
    client.config=*config;return is("init-fail")?NULL:&client;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t handle,int write_len) {
    assert(!locks && active && handle==&client && write_len==0);++opens;
    if(is("cancel-open"))was_cancelled=true;
    if(is("timeout-open"))clock_ms+=16000;
    return is("open-fail")?ESP_FAIL:ESP_OK;
}
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t handle,int timeout) {
    assert(handle==&client && timeout>0 && timeout<=5000);return ESP_OK;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t handle) {
    assert(!locks && active && handle==&client);++headers;
    esp_http_client_event_t event={.event_id=HTTP_EVENT_ON_HEADER,.user_data=client.config.user_data,
        .header_key="Content-Length",.header_value="0"};
    if(is("large-headers")) {
        static char large[8194];memset(large,'a',sizeof(large)-1);large[sizeof(large)-1]=0;event.header_value=large;
        assert(client.config.event_handler(&event)==ESP_FAIL);
    }else assert(client.config.event_handler(&event)==ESP_OK);
    return is("headers-fail")?-1:0;
}
int esp_http_client_get_status_code(esp_http_client_handle_t handle) {assert(handle==&client);return is("status")?302:200;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t handle) {
    assert(!locks && active && handle==&client && cleanups==0);++cleanups;
#if TLS_TEST_SDK == 5
    if(is("open-fail")) {
        esp_http_client_event_t event={.event_id=HTTP_EVENT_DISCONNECTED,.data=&client,.user_data=client.config.user_data};
        assert(client.config.event_handler(&event)==ESP_OK);
    }
#endif
    return is("cleanup-fail")?ESP_FAIL:ESP_OK;
}
esp_err_t esp_tls_get_and_clear_last_error(esp_tls_error_handle_t handle,int *code,int *flags) {
    assert(handle==&client);*code=-0x2700;*flags=8;return ESP_FAIL;
}
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t handle,int *code,int *flags) {
    assert(handle==&client);*code=-0x2700;*flags=8;return ESP_FAIL;
}
void service_test_log(const char *tag,const char *format,...) {
    assert(!locks);char line[512];va_list args;va_start(args,format);
    int n=vsnprintf(line,sizeof(line),format,args);va_end(args);assert(n>=0 && (size_t)n<sizeof(line));
    if(!strcmp(tag,"tv4_ota_mem") && !strncmp(line,"window=",7)) {
        unsigned id;assert(sscanf(line,"window=%u",&id)==1);++windows;
        assert(id==(windows==1?TV4_MEM_SNTP:TV4_MEM_TLS_PROBE));
    }
    if(strstr(line,"tls_result=")) {
        assert(sscanf(line,"tls_result=%u",&result)==1);
        if(is("open-fail"))assert(strstr(line,"cert_flags=8"));
    }
    if(strstr(line,"finished pass=")) {unsigned value;assert(sscanf(line,"finished pass=%u",&value)==1);passed=value;finished=true;}
}
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *task) {
    assert(TLS_TEST_STANDALONE && !strcmp(name,"tv4_tls_probe") && stack==6144 && !arg && priority==1 && !task);
    ++creates;standalone=fn;return pdPASS;
}
void vTaskDelete(TaskHandle_t task) {assert(TLS_TEST_STANDALONE && !task);++deletes;longjmp(escape,1);}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];
    if(TLS_TEST_STANDALONE) {
        assert(tv4_utc_tls_bench_start());assert(!tv4_utc_tls_bench_start());
        if(!setjmp(escape)) {standalone(NULL);assert(!"standalone worker returned");}
        assert(creates==1 && deletes==1 && !windows);
    }else {
        assert(!tv4_utc_tls_bench_start());
        assert(!tv4_utc_tls_bench_run_in_ota_worker(NULL,NULL));
        bool ok=tv4_utc_tls_bench_run_in_ota_worker(cancelled,&cancel_cookie);
        assert(ok==is("success") && !creates && !deletes);
        assert(!tv4_utc_tls_bench_run_in_ota_worker(cancelled,&cancel_cookie));
        assert(windows==starts+(unsigned)is("guard"));
    }
    assert(finished && passed==is("success") && !locks);
    if(is("cancel-settle") || is("offline")) {assert(!starts && !stops && !syncs && !inits);}
    else if(is("guard") || is("start1-fail")) {assert(!syncs && !stops && !inits);}
    else if(is("sntp-fail") || is("cancel-sntp") || is("stop1-fail")) {assert(syncs==1 && stops==1 && !inits);}
    else if(is("start2-fail")) {assert(starts==2 && stops==1 && !inits);}
    else {
        assert(starts==2 && stops==2 && syncs==1);
        if(is("clock-jump"))assert(!inits && result==TV4_TLS_TIME);
        else {
            assert(inits==1 && opens==!is("init-fail") && cleanups==!is("init-fail"));
            if(is("cancel-open"))assert(!headers && result==TV4_TLS_CANCELLED);
            else if(is("timeout-open"))assert(!headers && result==TV4_TLS_TIMEOUT);
            else if(is("open-fail") || is("init-fail"))assert(!headers && result==TV4_TLS_OPEN);
            else if(is("cleanup-fail"))assert(result==TV4_TLS_CLEANUP);
            else if(is("status"))assert(result==TV4_TLS_STATUS);
            else if(is("large-headers") || is("headers-fail"))assert(result==TV4_TLS_HEADERS);
            else assert(result==TV4_TLS_OK);
        }
    }
    assert(active==(is("stop1-fail") || is("stop2-fail")));
    printf("native UTC/HEAD SDK%d standalone=%d mode=%s: PASS (SDK doubles, no network)\n",TLS_TEST_SDK,TLS_TEST_STANDALONE,mode);
    return 0;
}
