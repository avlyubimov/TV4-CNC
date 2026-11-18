#include "tls_probe.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(ESP_PLATFORM) && CONFIG_TV4_UTC_TLS_BENCH
#include "sntp_source.h"
#include "wifi_service.h"
#include "esp_http_client.h"
#include "esp_idf_version.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static bool requested;
static const char *TAG="tv4_utc_tls_bench";
/* Isolated 2026 bench acceptance window, NOT a production source policy.
 * Actual SNTP success is required; this is not a build-date fallback. */
static const tv4_time_policy policy={true,true,1767225600u,1798761599u,60000,2000};
typedef struct {
    esp_http_client_handle_t client;
    size_t header_bytes;bool bad_headers;
    int native_error,tls_error,cert_flags;
} head_backend;
static uint32_t now_ms(void *context) {(void)context;return (uint32_t)(esp_timer_get_time()/1000);}
static tv4_time_result clock_read(void *context,tv4_time_sample *out) {(void)context;return tv4_time_clock_read(out);}
static size_t bounded(const char *s,size_t maximum) {
    if(!s)return maximum+1;
    for(size_t i=0;i<=maximum;i++)if(!s[i])return i;
    return maximum+1;
}
static esp_err_t headers_event(esp_http_client_event_t *event) {
    head_backend *b=event->user_data;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6,1,0)
    /* Official IDF5.3 HTTP client event pattern; event data lives during callback. */
    if(event->event_id==HTTP_EVENT_DISCONNECTED && event->data) {
        int code=0,flags=0;
        esp_err_t error=esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)event->data,&code,&flags);
        if(error || code || flags) {b->tls_error=code;b->cert_flags=flags;}
    }
#endif
    if(event->event_id!=HTTP_EVENT_ON_HEADER)return ESP_OK;
    size_t key=bounded(event->header_key,128),value=bounded(event->header_value,8192);
    if(b->bad_headers || key>128 || value>8192 || key+value+4>16384-b->header_bytes) {
        b->bad_headers=true;return ESP_FAIL;
    }
    b->header_bytes+=key+value+4;return ESP_OK;
}
static void errors(head_backend *b,int error) {
    b->native_error=error;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6,1,0)
    if(b->client)esp_http_client_get_and_clear_last_tls_error(b->client,&b->tls_error,&b->cert_flags);
#else
    /* Populated by DISCONNECTED during cleanup when detail is available. */
    if(!b->tls_error && !b->cert_flags) {b->tls_error=-1;b->cert_flags=-1;}
#endif
}
static bool head_open(void *context,uint32_t timeout) {
    head_backend *b=context;
    esp_http_client_config_t config={.url="https://github.com/avlyubimov/TV4-CNC",
        .method=HTTP_METHOD_HEAD,.transport_type=HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach=esp_crt_bundle_attach,.skip_cert_common_name_check=false,
        .disable_auto_redirect=true,.auth_type=HTTP_AUTH_TYPE_NONE,.max_authorization_retries=-1,
        .timeout_ms=(int)timeout,.buffer_size=4096,.buffer_size_tx=512,.keep_alive_enable=false,
        .event_handler=headers_event,.user_data=b,.user_agent="TV4-CNC-TLS-Bench/1"};
    b->client=esp_http_client_init(&config);
    if(!b->client) {errors(b,ESP_ERR_NO_MEM);return false;}
    esp_err_t error=esp_http_client_open(b->client,0);
    if(error!=ESP_OK) {errors(b,error);return false;}
    return true;
}
static int head_headers(void *context,uint32_t timeout) {
    head_backend *b=context;
    esp_err_t error=esp_http_client_set_timeout_ms(b->client,(int)timeout);
    if(error!=ESP_OK) {errors(b,error);return -1;}
    if(esp_http_client_fetch_headers(b->client)<0 || b->bad_headers) {errors(b,ESP_FAIL);return -1;}
    return esp_http_client_get_status_code(b->client);
}
static bool head_close(void *context) {
    head_backend *b=context;
    if(!b->client)return true;
    esp_err_t error=esp_http_client_cleanup(b->client);b->client=NULL;
    if(error!=ESP_OK)b->native_error=error;
    return error==ESP_OK;
}
static void snapshot(const char *stage) {
    const uint32_t internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    ESP_LOGI(TAG,"stage=%s internal_free=%u internal_min=%u internal_largest=%u psram_free=%u psram_min=%u stack=%u",
        stage,(unsigned)heap_caps_get_free_size(internal),(unsigned)heap_caps_get_minimum_free_size(internal),
        (unsigned)heap_caps_get_largest_free_block(internal),(unsigned)heap_caps_get_free_size(external),
        (unsigned)heap_caps_get_minimum_free_size(external),(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static bool monitor_start(const char *stage) {
    /* SDK global min monitor has a tiny bookkeeping allocation. Do not start
     * under severe pressure; no second owner of this monitor is allowed. */
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<8192 ||
       heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<4096) {
        ESP_LOGE(TAG,"stage=%s monitor_guard_failed",stage);return false;
    }
    esp_err_t error=heap_caps_monitor_local_minimum_free_size_start();
    if(error!=ESP_OK) {ESP_LOGE(TAG,"stage=%s monitor_start_failed=%d",stage,error);return false;}
    snapshot(stage);return true;
}
static bool monitor_stop(const char *stage) {
    snapshot(stage); /* Before stop: window minima, includes concurrent HMI/Wi-Fi. */
    esp_err_t error=heap_caps_monitor_local_minimum_free_size_stop();
    ESP_LOGI(TAG,"stage=%s monitor_stop=%d historical_min_restored=%u",stage,error,(unsigned)(error==ESP_OK));
    return error==ESP_OK;
}
static void worker(void *context) {
    (void)context;tv4_time_anchor anchor={0};bool passed=false;const char *stage="network";
    ESP_LOGW(TAG,"ONE UTC/HEAD probe: SNTP unauthenticated; global heap minima temporarily windowed; no body/install");
    uint32_t start=now_ms(NULL);wifi_snapshot_t wifi;bool online=false;
    while((uint32_t)(now_ms(NULL)-start)<15000) {
        if(wifi_service_snapshot(&wifi) && wifi.state==NET_ONLINE) {online=true;break;}
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if(!online)goto done;
    tv4_sntp_report sync;
    snapshot("sntp_before");
    stage="sntp_monitor";if(!monitor_start("sntp_window_start"))goto done;
    tv4_sntp_result synced=tv4_sntp_sync(&anchor,&policy,10000,NULL,NULL,&sync);
    bool monitored=monitor_stop("sntp_window_end");
    ESP_LOGI(TAG,"sntp_result=%u time_result=%u elapsed_ms=%u cleanup=%u source=%u",
        (unsigned)synced,(unsigned)sync.time_result,(unsigned)sync.elapsed_ms,(unsigned)sync.cleanup_ok,(unsigned)anchor.accepted.source);
    stage="sntp";if(!monitored || synced!=TV4_SNTP_OK || !sync.cleanup_ok)goto done;
    snapshot("tls_before");
    stage="tls_monitor";if(!monitor_start("tls_window_start"))goto done;
    head_backend backend={0};tv4_tls_report report;
    tv4_tls_ops ops={&backend,head_open,head_headers,head_close,clock_read,now_ms,NULL};
    tv4_tls_result result=tv4_tls_probe_run(&anchor,15000,&ops,&report);
    monitored=monitor_stop("tls_window_end");
    ESP_LOGI(TAG,"tls_result=%u status=%d elapsed_ms=%u cleanup=%u native_error=%d tls_error=%d cert_flags=%d header_bytes=%u",
        (unsigned)result,report.http_status,(unsigned)report.elapsed_ms,(unsigned)report.cleanup_ok,
        backend.native_error,backend.tls_error,backend.cert_flags,(unsigned)backend.header_bytes);
    stage="tls";passed=monitored && result==TV4_TLS_OK && report.cleanup_ok;
done:
    tv4_time_forget(&anchor);snapshot("worker_end");
    ESP_LOGI(TAG,"finished pass=%u stage=%s; not an OTA/TLS-production-budget approval",(unsigned)passed,stage);
    vTaskDelete(NULL);
}
bool tv4_utc_tls_bench_start(void) {
    if(requested)return false;
    requested=true;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<24576 ||
       heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<8192) {
        ESP_LOGE(TAG,"not_scheduled=headroom");return false;
    }
    if(xTaskCreate(worker,"tv4_tls_probe",6144,NULL,1,NULL)!=pdPASS) {
        ESP_LOGE(TAG,"not_scheduled=task_allocation");return false;
    }
    return true;
}
#else
bool tv4_utc_tls_bench_start(void) {return false;}
#endif
