#include "sntp_source.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(ESP_PLATFORM) && CONFIG_TV4_BOARD_REV12
#include "wifi_service.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static bool busy;
typedef struct { bool owned;bool (*cancel)(void *);void *cancel_context; } backend;
static bool online(void *context) {
    (void)context;wifi_snapshot_t snapshot;
    return wifi_service_snapshot(&snapshot) && snapshot.state==NET_ONLINE;
}
/* Entire init/deinit runs in TCP/IP context. SDK esp_netif nested IPC executes
 * directly in that context; callbacks cannot race storage teardown here. */
static esp_err_t start_tcpip(void *context) {
    backend *b=context;
    esp_netif_t *netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if(!netif || !esp_netif_is_netif_up(netif) || esp_netif_get_ip_info(netif,&ip)!=ESP_OK || !ip.ip.addr)
        return ESP_ERR_NOT_FOUND;
    if(esp_sntp_enabled())return ESP_ERR_INVALID_STATE;
    esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start=false; /* Acquire global storage before changing sync mode. */
    esp_err_t result=esp_netif_sntp_init(&config);
    if(result!=ESP_OK)return result;
    b->owned=true;sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    return esp_netif_sntp_start();
}
static tv4_sntp_result start(void *context) {
    esp_err_t result=esp_netif_tcpip_exec(start_tcpip,context);
    if(result==ESP_OK)return TV4_SNTP_OK;
    if(result==ESP_ERR_NOT_FOUND)return TV4_SNTP_NO_NETWORK;
    return result==ESP_ERR_INVALID_STATE?TV4_SNTP_BUSY:TV4_SNTP_START_FAILED;
}
static tv4_sntp_poll source_poll(void *context,uint32_t wait) {
    (void)context;TickType_t ticks=pdMS_TO_TICKS(wait);if(!ticks)ticks=1;
    esp_err_t result=esp_netif_sntp_sync_wait(ticks);
    if(result==ESP_OK)return TV4_SNTP_SYNCED;
    return result==ESP_ERR_TIMEOUT || result==ESP_ERR_NOT_FINISHED?TV4_SNTP_WAIT:TV4_SNTP_POLL_FAILED;
}
static esp_err_t stop_tcpip(void *context) {
    backend *b=context;
    if(b->owned) {esp_netif_sntp_deinit();b->owned=false;}
    return ESP_OK;
}
static bool stop(void *context) {return esp_netif_tcpip_exec(stop_tcpip,context)==ESP_OK;}
static tv4_time_result clock_read(void *context,tv4_time_sample *out) {(void)context;return tv4_time_clock_read(out);}
static uint32_t now(void *context) {(void)context;return (uint32_t)(esp_timer_get_time()/1000);}
static bool cancelled(void *context) {backend *b=context;return b->cancel && b->cancel(b->cancel_context);}
#endif
tv4_sntp_result tv4_sntp_sync(tv4_time_anchor *a,const tv4_time_policy *p,uint32_t timeout,
    bool (*cancel)(void *),void *cancel_context,tv4_sntp_report *report) {
#if defined(ESP_PLATFORM) && CONFIG_TV4_BOARD_REV12
    taskENTER_CRITICAL(&lock);bool acquired=!busy;if(acquired)busy=true;taskEXIT_CRITICAL(&lock);
    if(!acquired) {
        /* Caller must never share an anchor with another running owner. */
        tv4_time_forget(a);if(report)*report=(tv4_sntp_report){.result=TV4_SNTP_BUSY};return TV4_SNTP_BUSY;
    }
    backend b={false,cancel,cancel_context};
    tv4_sntp_ops ops={&b,online,start,source_poll,stop,clock_read,now,cancelled};
    tv4_sntp_result result=tv4_sntp_run(a,p,timeout,&ops,report);
    /* On uncertain cleanup retain global busy; no automatic second client. */
    if(!b.owned) {taskENTER_CRITICAL(&lock);busy=false;taskEXIT_CRITICAL(&lock);}
    return result;
#else
    (void)p;(void)timeout;(void)cancel;(void)cancel_context;tv4_time_forget(a);
    if(report)*report=(tv4_sntp_report){.result=TV4_SNTP_UNAVAILABLE};
    return TV4_SNTP_UNAVAILABLE;
#endif
}
