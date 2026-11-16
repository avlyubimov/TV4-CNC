#include "wifi_service.h"
#include <stdio.h>
#include <string.h>
bool wifi_credentials_valid(const char *ssid,const char *password) {
    if(!ssid || !password)return false;
    size_t s=strlen(ssid),p=strlen(password);
    return s>0 && s<=32 && (p==0 || (p>=8 && p<=63));
}
#ifdef ESP_PLATFORM
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
typedef struct { unsigned kind; char ssid[33],password[64]; } command_t;
static QueueHandle_t commands;
static SemaphoreHandle_t lock;
static wifi_snapshot_t state;
static bool service_ready;

static void publish(net_state_t value,int error) {
    xSemaphoreTake(lock,portMAX_DELAY);state.state=value;state.error=error;
    if(value!=NET_ONLINE) state.ip[0]=0;
    state.revision++;xSemaphoreGive(lock);
}
static void event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        publish(NET_IDLE,((wifi_event_sta_disconnected_t *)data)->reason);
    } else if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got=data;
        xSemaphoreTake(lock,portMAX_DELAY);state.state=NET_ONLINE;state.error=0;
        snprintf(state.ip,sizeof(state.ip),IPSTR,IP2STR(&got->ip_info.ip));
        state.revision++;xSemaphoreGive(lock);
    } else if(base==IP_EVENT && id==IP_EVENT_STA_LOST_IP) publish(NET_IDLE,0);
}
static esp_err_t initialize(void) {
    esp_err_t err=nvs_flash_init();if(err!=ESP_OK)return err; /* Never erase existing NVS. */
    if((err=esp_netif_init())!=ESP_OK)return err;
    err=esp_event_loop_create_default();if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE)return err;
    if(!esp_netif_create_default_wifi_sta())return ESP_ERR_NO_MEM;
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
    if((err=esp_wifi_init(&config))!=ESP_OK)return err;
    if((err=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK)return err;
    if((err=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,NULL))!=ESP_OK)return err;
    if((err=esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,event,NULL))!=ESP_OK)return err;
    if((err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK)return err;
    return esp_wifi_start();
}
static void worker(void *arg) {
    (void)arg;esp_err_t err=initialize();
    if(err!=ESP_OK) { publish(NET_FAILED,err);vTaskDelete(NULL);return; }
    xSemaphoreTake(lock,portMAX_DELAY);service_ready=true;xSemaphoreGive(lock);
    publish(NET_IDLE,0);
    command_t cmd;
    int64_t connect_started=0;
    for(;;) {
        if(xQueueReceive(commands,&cmd,pdMS_TO_TICKS(1000))!=pdTRUE) {
            wifi_snapshot_t snapshot;
            if(wifi_service_snapshot(&snapshot) && snapshot.state==NET_CONNECTING &&
               esp_timer_get_time()-connect_started>=30000000) {
                esp_wifi_disconnect();wifi_config_t empty={0};esp_wifi_set_config(WIFI_IF_STA,&empty);
                publish(NET_FAILED,ESP_ERR_TIMEOUT);
            }
            continue;
        }
        if(cmd.kind==1) {
            wifi_snapshot_t current={0};if(!wifi_service_snapshot(&current)) { memset(&cmd,0,sizeof(cmd));continue; }
            if(current.state!=NET_IDLE)continue;
            net_state_t previous=current.state;
            publish(NET_SCANNING,0);
            wifi_scan_config_t scan={0};scan.show_hidden=false;
            err=esp_wifi_scan_start(&scan,true);
            wifi_ap_record_t records[WIFI_UI_NETWORKS];uint16_t count=WIFI_UI_NETWORKS;
            if(err==ESP_OK)err=esp_wifi_scan_get_ap_records(&count,records);
            if(err==ESP_OK) {
                xSemaphoreTake(lock,portMAX_DELAY);state.count=count;
                for(unsigned i=0;i<count;i++) { memcpy(state.ssids[i],records[i].ssid,32);state.ssids[i][32]=0;state.rssi[i]=records[i].rssi; }
                state.state=previous==NET_ONLINE?NET_ONLINE:NET_IDLE;state.revision++;xSemaphoreGive(lock);
            } else publish(NET_FAILED,err);
        } else if(cmd.kind==2) {
            wifi_snapshot_t current={0};if(!wifi_service_snapshot(&current)) { memset(&cmd,0,sizeof(cmd));continue; }
            if(current.state==NET_ONLINE || current.state==NET_CONNECTING) { memset(&cmd,0,sizeof(cmd));continue; }
            wifi_config_t config={0};memcpy(config.sta.ssid,cmd.ssid,strlen(cmd.ssid));
            memcpy(config.sta.password,cmd.password,strlen(cmd.password));
            config.sta.pmf_cfg.capable=true;config.sta.pmf_cfg.required=false;
            err=esp_wifi_set_config(WIFI_IF_STA,&config);
            memset(&config,0,sizeof(config));memset(&cmd,0,sizeof(cmd));
            connect_started=esp_timer_get_time();publish(NET_CONNECTING,0);
            if(err==ESP_OK)err=esp_wifi_connect();
            if(err!=ESP_OK)publish(NET_FAILED,err);
        } else {
            err=esp_wifi_disconnect();
            wifi_config_t empty={0};esp_wifi_set_config(WIFI_IF_STA,&empty);
            publish(NET_IDLE,err==ESP_ERR_WIFI_NOT_CONNECT?0:err);
        }
        memset(&cmd,0,sizeof(cmd));
    }
}
bool wifi_service_start(void) {
    if(commands)return true;
    lock=xSemaphoreCreateMutex();if(!lock)return false;
    commands=xQueueCreate(2,sizeof(command_t));
    if(!commands) { vSemaphoreDelete(lock);lock=NULL;return false; }
    state.state=NET_OFF;
    if(xTaskCreate(worker,"tv4_wifi",6144,NULL,2,NULL)!=pdPASS) {
        vQueueDelete(commands);commands=NULL;vSemaphoreDelete(lock);lock=NULL;return false;
    }
    return true;
}
static bool submit(command_t *cmd) {
    wifi_snapshot_t current;
    if(!commands || !lock || xSemaphoreTake(lock,0)!=pdTRUE)return false;
    current=state;bool ready=service_ready;xSemaphoreGive(lock);
    if(!ready)return false;
    if(cmd->kind==1 && current.state!=NET_IDLE)return false;
    if(cmd->kind==2 && current.state!=NET_IDLE && current.state!=NET_FAILED)return false;
    return xQueueSend(commands,cmd,0)==pdTRUE;
}
bool wifi_service_scan(void) { command_t c={.kind=1};return submit(&c); }
bool wifi_service_disconnect(void) { command_t c={.kind=3};return submit(&c); }
bool wifi_service_connect(const char *ssid,const char *password) {
    if(!wifi_credentials_valid(ssid,password))return false;
    command_t c={.kind=2};memcpy(c.ssid,ssid,strlen(ssid));memcpy(c.password,password,strlen(password));
    bool ok=submit(&c);memset(&c,0,sizeof(c));return ok;
}
bool wifi_service_snapshot(wifi_snapshot_t *out) {
    if(!out || !lock || xSemaphoreTake(lock,0)!=pdTRUE)return false;
    *out=state;xSemaphoreGive(lock);return true;
}
#else
bool wifi_service_start(void) { return true; }
bool wifi_service_scan(void) { return false; }
bool wifi_service_connect(const char *s,const char *p) { (void)s;(void)p;return false; }
bool wifi_service_disconnect(void) { return false; }
bool wifi_service_snapshot(wifi_snapshot_t *out) { if(!out)return false;memset(out,0,sizeof(*out));return true; }
#endif
