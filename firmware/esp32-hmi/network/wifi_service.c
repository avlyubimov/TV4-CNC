#include "wifi_service.h"
#include "wifi_link.h"
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
#include "nvs.h"
#include "esp_timer.h"
_Static_assert(ESP_ERR_TIMEOUT==263,"Update the timeout diagnostic mapping");
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
typedef struct { unsigned kind; char ssid[33],password[64]; } command_t;
static QueueHandle_t commands;
static SemaphoreHandle_t lock;
static EventGroupHandle_t events;
static wifi_snapshot_t state;
static bool service_ready;
/* Worker-owned buffers: credentials never enter the UI snapshot. */
static wifi_profiles_t profiles;
static wifi_profile_t pending;
static bool pending_valid,auto_connect=true;
static int64_t connect_started,next_scan;
static wifi_ap_record_t records[WIFI_UI_NETWORKS];
static void increment(uint32_t *value) { if(*value<UINT32_MAX)++*value; }
static void publish(net_state_t value,int error) {
    xSemaphoreTake(lock,portMAX_DELAY);state.state=value;state.error=error;
    if(value==NET_CONNECTING)increment(&state.connect_attempts);
    if(value!=NET_ONLINE && value!=NET_SCANNING)state.ip[0]=0;
    state.revision++;xSemaphoreGive(lock);
}
static wifi_snapshot_t read_state(void) {
    wifi_snapshot_t out;xSemaphoreTake(lock,portMAX_DELAY);out=state;xSemaphoreGive(lock);return out;
}
static void publish_profiles(int error) {
    xSemaphoreTake(lock,portMAX_DELAY);state.saved_count=profiles.count;state.storage_error=error;
    memset(state.saved_ssids,0,sizeof(state.saved_ssids));
    for(unsigned i=0;i<profiles.count;i++)memcpy(state.saved_ssids[i],profiles.entries[i].ssid,33);
    state.revision++;xSemaphoreGive(lock);
}
static esp_err_t store_profiles(const wifi_profiles_t *value) {
    nvs_handle_t handle;esp_err_t err=nvs_open("tv4_wifi",NVS_READWRITE,&handle);
    if(err!=ESP_OK)return err;
    err=nvs_set_blob(handle,"profiles_v1",value,sizeof(*value));
    if(err==ESP_OK)err=nvs_commit(handle);
    nvs_close(handle);return err;
}
static void load_profiles(void) {
    wifi_profiles_init(&profiles);nvs_handle_t handle;
    esp_err_t err=nvs_open("tv4_wifi",NVS_READONLY,&handle);
    if(err==ESP_ERR_NVS_NOT_FOUND) { publish_profiles(0);return; }
    if(err==ESP_OK) {
        wifi_profiles_t loaded;size_t size=sizeof(loaded);
        err=nvs_get_blob(handle,"profiles_v1",&loaded,&size);nvs_close(handle);
        if(err==ESP_OK && size==sizeof(loaded) && wifi_profiles_valid(&loaded))profiles=loaded;
        else if(err==ESP_OK)err=ESP_ERR_INVALID_STATE;
        if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
        memset(&loaded,0,sizeof(loaded));
    }
    publish_profiles(err);
}
static void event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        xSemaphoreTake(lock,portMAX_DELAY);increment(&state.disconnect_events);xSemaphoreGive(lock);
        publish(NET_IDLE,((wifi_event_sta_disconnected_t *)data)->reason);
        xEventGroupSetBits(events,1);
    } else if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got=data;
        xSemaphoreTake(lock,portMAX_DELAY);state.state=NET_ONLINE;state.error=0;increment(&state.ip_events);
        snprintf(state.ip,sizeof(state.ip),IPSTR,IP2STR(&got->ip_info.ip));
        state.revision++;xSemaphoreGive(lock);
    } else if(base==IP_EVENT && id==IP_EVENT_STA_LOST_IP)publish(NET_IDLE,0);
}
static esp_err_t initialize(void) {
    esp_err_t err=nvs_flash_init();if(err!=ESP_OK)return err; /* Never erase existing NVS. */
    if((err=esp_netif_init())!=ESP_OK)return err;
    err=esp_event_loop_create_default();if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE)return err;
    if(!esp_netif_create_default_wifi_sta())return ESP_ERR_NO_MEM;
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
    if((err=esp_wifi_init(&config))!=ESP_OK)return err;
    /* Own versioned profiles; do not overwrite vendor Wi-Fi storage. */
    if((err=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK)return err;
    if((err=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,NULL))!=ESP_OK)return err;
    if((err=esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,event,NULL))!=ESP_OK)return err;
    if((err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK)return err;
    return esp_wifi_start();
}
typedef struct { esp_err_t error; } link_context_t;
static bool link_associated(void *context) {
    (void)context;wifi_ap_record_t info;return esp_wifi_sta_get_ap_info(&info)==ESP_OK;
}
static bool link_disconnect(void *context) {
    link_context_t *link=context;link->error=esp_wifi_disconnect();
    return link->error==ESP_OK || link->error==ESP_ERR_WIFI_NOT_CONNECT;
}
static bool link_wait(void *context) {
    bool received=(xEventGroupWaitBits(events,1,pdTRUE,pdFALSE,pdMS_TO_TICKS(2000))&1)!=0;
    if(!received)((link_context_t *)context)->error=ESP_ERR_TIMEOUT;
    return received;
}
static bool link_clear(void *context) {
    wifi_config_t empty={0};link_context_t *link=context;
    link->error=esp_wifi_set_config(WIFI_IF_STA,&empty);return link->error==ESP_OK;
}
static esp_err_t disconnect_current(void) {
    pending_valid=false;memset(&pending,0,sizeof(pending));
    wifi_snapshot_t current=read_state();link_context_t context={.error=ESP_OK};
    wifi_link_ops_t ops={&context,link_associated,link_disconnect,link_wait,link_clear};
    xEventGroupClearBits(events,1);
    return wifi_link_prepare(&ops,current.state==NET_CONNECTING)?ESP_OK:context.error;
}
static void connect_profile(const wifi_profile_t *profile) {
    wifi_profile_t selected=*profile; /* May alias pending. */
    esp_err_t err=disconnect_current();
    if(err==ESP_OK) {
        wifi_config_t config={0};memcpy(config.sta.ssid,selected.ssid,strlen(selected.ssid));
        memcpy(config.sta.password,selected.password,strlen(selected.password));
        config.sta.pmf_cfg.capable=true;config.sta.pmf_cfg.required=false;
        config.sta.sae_pwe_h2e=WPA3_SAE_PWE_BOTH;
        err=esp_wifi_set_config(WIFI_IF_STA,&config);memset(&config,0,sizeof(config));
    }
    if(err==ESP_OK) {
        pending=selected;pending_valid=true;connect_started=esp_timer_get_time();
        xSemaphoreTake(lock,portMAX_DELAY);memcpy(state.active_ssid,selected.ssid,33);xSemaphoreGive(lock);
        publish(NET_CONNECTING,0);err=esp_wifi_connect();
    }
    memset(&selected,0,sizeof(selected));
    if(err!=ESP_OK) { pending_valid=false;memset(&pending,0,sizeof(pending));publish(NET_FAILED,err); }
}
static void scan_networks(void) {
    wifi_snapshot_t current=read_state();if(current.state==NET_CONNECTING)return;
    bool online=current.state==NET_ONLINE;
    publish(NET_SCANNING,current.error);wifi_scan_config_t scan={0};scan.show_hidden=false;
    esp_err_t err=esp_wifi_scan_start(&scan,true);uint16_t count=WIFI_UI_NETWORKS;
    if(err==ESP_OK)err=esp_wifi_scan_get_ap_records(&count,records);
    if(err==ESP_OK) {
        xSemaphoreTake(lock,portMAX_DELAY);state.count=0;
        for(unsigned i=0;i<count;i++) {
            if(!records[i].ssid[0])continue;
            bool duplicate=false;
            for(unsigned j=0;j<state.count;j++)if(!strncmp(state.ssids[j],(char *)records[i].ssid,32))duplicate=true;
            if(duplicate)continue;
            unsigned n=state.count++;memcpy(state.ssids[n],records[i].ssid,32);state.ssids[n][32]=0;
            state.rssi[n]=records[i].rssi;state.secured[n]=records[i].authmode!=WIFI_AUTH_OPEN;
        }
        if(state.state==NET_SCANNING) { state.state=online?NET_ONLINE:NET_IDLE;state.error=current.error; }
        state.revision++;xSemaphoreGive(lock);
    } else publish(online?NET_ONLINE:NET_FAILED,err);
    next_scan=esp_timer_get_time()+30000000;
    current=read_state();
    if(auto_connect && current.state==NET_IDLE) {
        /* Scan results arrive in descending RSSI order. Try an available known AP. */
        int found=wifi_profiles_choose(&profiles,(const char (*)[33])current.ssids,current.count);
        if(found>=0)connect_profile(&profiles.entries[found]);
    }
}
static void worker(void *arg) {
    (void)arg;esp_err_t err=initialize();
    if(err!=ESP_OK) { publish(NET_FAILED,err);vTaskDelete(NULL);return; }
    load_profiles();
    xSemaphoreTake(lock,portMAX_DELAY);service_ready=true;xSemaphoreGive(lock);
    publish(NET_IDLE,0);next_scan=0;
    command_t cmd;int64_t last_stack=0;
    for(;;) {
        int64_t sample_time=esp_timer_get_time();
        if(sample_time-last_stack>=1000000) {
            uint32_t watermark=uxTaskGetStackHighWaterMark(NULL);
            xSemaphoreTake(lock,portMAX_DELAY);state.stack_watermark=watermark;xSemaphoreGive(lock);
            last_stack=sample_time;
        }
        wifi_snapshot_t snapshot=read_state();
        if(snapshot.state==NET_ONLINE && pending_valid) {
            wifi_profiles_t updated=profiles;
            wifi_profiles_remember(&updated,pending.ssid,pending.password);
            if(memcmp(&updated,&profiles,sizeof(profiles))) {
                err=store_profiles(&updated);if(err==ESP_OK)profiles=updated;
                publish_profiles(err);
            }
            memset(&updated,0,sizeof(updated));pending_valid=false;memset(&pending,0,sizeof(pending));
        }
        if(snapshot.state==NET_CONNECTING && esp_timer_get_time()-connect_started>=30000000) {
            disconnect_current();publish(NET_FAILED,ESP_ERR_TIMEOUT);next_scan=esp_timer_get_time()+30000000;
        }
        if(xQueueReceive(commands,&cmd,pdMS_TO_TICKS(250))!=pdTRUE) {
            snapshot=read_state();
            if(snapshot.state!=NET_CONNECTING && snapshot.state!=NET_ONLINE && esp_timer_get_time()>=next_scan)scan_networks();
            continue;
        }
        if(cmd.kind==1)scan_networks();
        else if(cmd.kind==2 || cmd.kind==5) {
            auto_connect=true;
            if(cmd.kind==5) {
                int found=wifi_profiles_find(&profiles,cmd.ssid);
                if(found>=0)connect_profile(&profiles.entries[found]);else publish(NET_FAILED,ESP_ERR_NOT_FOUND);
            } else { wifi_profile_t requested={0};memcpy(requested.ssid,cmd.ssid,33);memcpy(requested.password,cmd.password,64);
                connect_profile(&requested);memset(&requested,0,sizeof(requested)); }
        } else if(cmd.kind==4) {
            /* Commit removal before disconnecting; a failed write never claims success. */
            wifi_profiles_t updated=profiles;
            if(wifi_profiles_forget(&updated,cmd.ssid)) {
                err=store_profiles(&updated);
                if(err==ESP_OK) {
                    profiles=updated;snapshot=read_state();
                    if(!strcmp(snapshot.active_ssid,cmd.ssid)) { disconnect_current();publish(NET_IDLE,0); }
                }
                publish_profiles(err);
            }
            memset(&updated,0,sizeof(updated));next_scan=esp_timer_get_time()+30000000;
        } else {
            auto_connect=false;err=disconnect_current();publish(err==ESP_OK?NET_IDLE:NET_FAILED,err);
        }
        memset(&cmd,0,sizeof(cmd));
    }
}
bool wifi_service_start(void) {
    if(commands)return true;
    lock=xSemaphoreCreateMutex();if(!lock)return false;
    commands=xQueueCreate(2,sizeof(command_t));events=xEventGroupCreate();
    if(!commands || !events) {
        if(commands) { vQueueDelete(commands);commands=NULL; }
        if(events) { vEventGroupDelete(events);events=NULL; }
        vSemaphoreDelete(lock);lock=NULL;return false;
    }
    state.state=NET_OFF;
    if(xTaskCreate(worker,"tv4_wifi",8192,NULL,2,NULL)!=pdPASS) {
        vQueueDelete(commands);commands=NULL;vEventGroupDelete(events);events=NULL;vSemaphoreDelete(lock);lock=NULL;return false;
    }
    return true;
}
static bool submit(command_t *cmd) {
    if(!commands || !lock || xSemaphoreTake(lock,0)!=pdTRUE)return false;
    bool ready=service_ready;
    bool ok=ready && xQueueSend(commands,cmd,0)==pdTRUE;
    if(ready && !ok)increment(&state.queue_drops);
    xSemaphoreGive(lock);return ok;
}
bool wifi_service_scan(void) { command_t c={.kind=1};return submit(&c); }
bool wifi_service_disconnect(void) { command_t c={.kind=3};return submit(&c); }
bool wifi_service_connect(const char *ssid,const char *password) {
    if(!wifi_credentials_valid(ssid,password))return false;
    command_t c={.kind=2};memcpy(c.ssid,ssid,strlen(ssid));memcpy(c.password,password,strlen(password));
    bool ok=submit(&c);memset(&c,0,sizeof(c));return ok;
}
static bool named_command(const char *ssid,unsigned kind) {
    if(!ssid || !*ssid || strlen(ssid)>32)return false;
    command_t c={.kind=kind};memcpy(c.ssid,ssid,strlen(ssid));return submit(&c);
}
bool wifi_service_connect_saved(const char *ssid) { return named_command(ssid,5); }
bool wifi_service_forget(const char *ssid) { return named_command(ssid,4); }
bool wifi_service_snapshot(wifi_snapshot_t *out) {
    if(!out || !lock || xSemaphoreTake(lock,0)!=pdTRUE)return false;
    *out=state;xSemaphoreGive(lock);return true;
}
bool wifi_service_online(void) {
    if(!lock || xSemaphoreTake(lock,0)!=pdTRUE)return false;
    bool online=state.state==NET_ONLINE;xSemaphoreGive(lock);return online;
}
#else
bool wifi_service_start(void) { return true; }
bool wifi_service_scan(void) { return false; }
bool wifi_service_connect(const char *s,const char *p) { (void)s;(void)p;return false; }
bool wifi_service_connect_saved(const char *s) { (void)s;return false; }
bool wifi_service_forget(const char *s) { (void)s;return false; }
bool wifi_service_disconnect(void) { return false; }
static wifi_snapshot_t host_fixture;
void wifi_service_host_fixture(const wifi_snapshot_t *value) {
    if(value)host_fixture=*value;else memset(&host_fixture,0,sizeof(host_fixture));
}
bool wifi_service_snapshot(wifi_snapshot_t *out) { if(!out)return false;*out=host_fixture;return true; }
bool wifi_service_online(void) { return host_fixture.state==NET_ONLINE; }
#endif
