#include "service.h"
#include <string.h>
tv4_ota_boot_result tv4_ota_boot_observe(tv4_ota_boot_window *w,uint32_t now,bool healthy,uint32_t last) {
    if((uint32_t)(now-w->started)>=30000)return TV4_BOOT_TIMEOUT;
    if((uint32_t)(now-w->checked)>=500)w->observing=false;
    w->checked=now;
    if(!healthy || (uint32_t)(now-last)>=500)w->observing=false;
    else if(!w->observing) {w->observing=true;w->continuous=now;}
    return w->observing && (uint32_t)(now-w->continuous)>=3000?TV4_BOOT_CHECK_MEMORY:TV4_BOOT_WAIT;
}
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#if CONFIG_TV4_OTA_WORKER_TLS_BENCH && (!CONFIG_TV4_OTA_WORKER_STACK_BENCH || !CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS || !CONFIG_TV4_OTA_ENABLED || CONFIG_TV4_UTC_TLS_BENCH)
#error "Worker TLS bench requires the unprovisioned OTA stack/trace profile exclusively"
#endif
#if CONFIG_TV4_OTA_WORKER_STACK_BENCH && (!CONFIG_TV4_OTA_ENABLED || !CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS)
#error "Worker stack bench requires explicit OTA memory diagnostics"
#endif
#endif
#if defined(ESP_PLATFORM) && CONFIG_TV4_OTA_ENABLED
#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE || CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK || CONFIG_SECURE_BOOT || CONFIG_SECURE_FLASH_ENC_ENABLED || !CONFIG_ESP_TASK_WDT_INIT || !CONFIG_ESP_TASK_WDT_PANIC || !CONFIG_ESP_INT_WDT || !CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT
#error "OTA requires reversible app rollback only; no eFuse/security changes"
#endif
#if !CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC || !CONFIG_MBEDTLS_HAVE_TIME_DATE || !CONFIG_SPIRAM || !CONFIG_SPIRAM_XIP_FROM_PSRAM || !CONFIG_SPIRAM_FETCH_INSTRUCTIONS || !CONFIG_SPIRAM_RODATA
#error "OTA profile requires PSRAM TLS and X.509 certificate dates"
#endif
#include "ota_local_context.h"
#if CONFIG_TV4_OTA_WORKER_STACK_BENCH && TV4_OTA_PROVISIONED
#error "Worker stack bench must remain unprovisioned"
#endif
#if TV4_OTA_PROVISIONED || CONFIG_TV4_OTA_WORKER_STACK_BENCH
#define OTA_WORKER_STACK_BYTES 8192
#else
#define OTA_WORKER_STACK_BYTES 3072
#endif
#include "installer_sdk.h"
#include "memory.h"
#include "memory_diagnostics.h"
#include "sntp_source.h"
#include "tls_probe.h"
#include "wifi_service.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static tv4_update_snapshot status={.enabled=true,.state=TV4_UPDATE_BOOT};
static bool health,cancel_requested,started;static uint32_t health_ms;
static QueueHandle_t commands;static StaticQueue_t queue_state;static uint8_t queue_bytes[sizeof(tv4_update_command)];
static tv4_install_approval approved; /* bounded local identity; no release notes */
static const tv4_ota_memory_policy mem_policy={TV4_OTA_PROVISIONED,OTA_FLOOR_INTERNAL_FREE,OTA_FLOOR_INTERNAL_LARGEST,OTA_FLOOR_PSRAM_FREE};
static const tv4_ota_resource_policy resources={TV4_OTA_PROVISIONED,OTA_FLOOR_INTERNAL_FREE,OTA_FLOOR_INTERNAL_LARGEST,OTA_FLOOR_PSRAM_FREE,OTA_FLOOR_PSRAM_LARGEST,OTA_FLOOR_STACK};
static const tv4_time_policy clock_policy={TV4_OTA_PROVISIONED,true,OTA_TIME_EARLIEST_EPOCH,OTA_TIME_LATEST_EPOCH,OTA_TIME_MAXIMUM_AGE_MS,OTA_TIME_MAXIMUM_SKEW_MS};
static const tv4_ota_context trust={OTA_KEY_ID,OTA_INSTALLED_VERSION,OTA_BOOTLOADER_VERSION,OTA_PROTOCOL_VERSION,NULL,"tv4-16m-ab-v1",OTA_PRERELEASE,true,ota_public_key,sizeof(ota_public_key)};
static uint32_t now_ms(void *c) {(void)c;return (uint32_t)(esp_timer_get_time()/1000);}
static bool cancelled(void *c) {
    (void)c;portENTER_CRITICAL(&mux);uint32_t stamp=now_ms(NULL);
    bool v=cancel_requested || !health || (uint32_t)(stamp-health_ms)>=500;
    portEXIT_CRITICAL(&mux);return v;
}
/* Only the actual OTA worker calls this, including after diagnostic logging.
 * A failed xTaskCreate in main must never publish main's stack as OTA's. */
static void worker_stack(void) {
    uint32_t stack=uxTaskGetStackHighWaterMark(NULL);
    portENTER_CRITICAL(&mux);status.stack_watermark=stack;status.stack_valid=true;portEXIT_CRITICAL(&mux);
}
static void publish(tv4_update_state state,unsigned error) {
    portENTER_CRITICAL(&mux);if(state==TV4_UPDATE_READY && cancel_requested) {state=TV4_UPDATE_ERROR;error=TV4_SESSION_CANCEL+4;}
    status.state=state;status.error=error;portEXIT_CRITICAL(&mux);
}
static bool commit(void *c) {
    (void)c;portENTER_CRITICAL(&mux);uint32_t stamp=now_ms(NULL);
    bool ok=!cancel_requested && health && (uint32_t)(stamp-health_ms)<500 && status.state==TV4_UPDATE_WRITING;
    if(ok)status.state=TV4_UPDATE_COMMIT;
    portEXIT_CRITICAL(&mux);return ok;
}
static tv4_http_result fetch(void *c,const tv4_http_request *r,tv4_http_workspace *w,tv4_http_consume consume,void *consumer,tv4_http_report *report) {
    return tv4_https_read(r,c,w,consume,consumer,cancelled,NULL,report);
}
static bool write_chunk(void *c,const uint8_t *p,size_t n) {
    tv4_ota_sdk *sdk=c;if(!tv4_ota_sdk_write(c,p,n))return false;
    portENTER_CRITICAL(&mux);status.received=sdk->written;portEXIT_CRITICAL(&mux);return true;
}
static bool configured(void) {
    const esp_app_desc_t *d=esp_app_get_description();
    return TV4_OTA_PROVISIONED && d && !strcmp(d->version,OTA_INSTALLED_VERSION) &&
        tv4_ota_p256_key(ota_public_key,sizeof(ota_public_key)) && tv4_time_policy_validate(&clock_policy)==TV4_TIME_OK &&
        tv4_ota_resource_policy_validate(&resources)==TV4_RESOURCE_OK;
}
static void operation(bool install) {
    tv4_ota_memory lease={0};tv4_time_anchor anchor={0};unsigned error=1;bool committed=false;tv4_ota_sdk sdk={.approval=&approved,.commit_allowed=commit};
    bool measured=false;tv4_ota_memory_window measuring=TV4_MEM_WORKSPACE;
    publish(install?TV4_UPDATE_WRITING:TV4_UPDATE_CHECKING,0);
    if(!configured() || !tv4_ota_sdk_layout_valid() || !wifi_service_online())goto done;
    tv4_ota_memory_window_begin(measuring);measured=true;
    if(tv4_ota_memory_acquire_psram(&lease,&mem_policy)!=TV4_MEMORY_OK) {error=2;goto done;}
    tv4_ota_memory_window_end(measuring);tv4_ota_memory_report_window();measured=false;
    tv4_sntp_report sync;
    measuring=TV4_MEM_SNTP;tv4_ota_memory_window_begin(measuring);measured=true;
    tv4_sntp_result synced=tv4_sntp_sync(&anchor,&clock_policy,10000,cancelled,NULL,&sync);
    tv4_ota_memory_window_end(measuring);tv4_ota_memory_report_window();measured=false;
    if(synced!=TV4_SNTP_OK || !sync.cleanup_ok) {error=3;goto done;}
    esp_chip_info_t chip;esp_chip_info(&chip);
    tv4_session_request r={OTA_MANIFEST_URL,&trust,{15000,300000,(uint16_t)chip.revision,true},15000,300000,5000,3,true,install};
    tv4_session_io io={&anchor,fetch,now_ms,cancelled};tv4_ota_resource_report report;
    bool ok;
    measuring=TV4_MEM_SESSION;tv4_ota_memory_window_begin(measuring);measured=true;
    if(install) {
        tv4_session_sink sink={&sdk,tv4_ota_sdk_begin,write_chunk,tv4_ota_sdk_seal,tv4_ota_sdk_activate,tv4_ota_sdk_abort};
        ok=tv4_ota_session_install_guarded(lease.session,&r,&io,&resources,tv4_ota_resource_sample_current,NULL,&report,&sink);
    }else ok=tv4_ota_session_run_guarded(lease.session,&r,&io,&resources,tv4_ota_resource_sample_current,NULL,&report);
    tv4_ota_memory_window_end(measuring);tv4_ota_memory_report_window();measured=false;
    if(!ok) {error=4+(unsigned)lease.session->result.error;goto done;}
    committed=install && lease.session->result.state==TV4_SESSION_INSTALLED;
    if(!install) {
        const tv4_ota_candidate *a=&lease.session->candidate;
        memcpy(approved.version,a->version,sizeof(approved.version));memcpy(approved.release,a->release,sizeof(approved.release));
        memcpy(approved.url,a->url,sizeof(approved.url));memcpy(approved.sha256,a->sha256,32);approved.size=a->size_bytes;
        portENTER_CRITICAL(&mux);memcpy(status.version,a->version,sizeof(status.version));memcpy(status.sha256,a->sha256,32);status.total=a->size_bytes;status.received=0;portEXIT_CRITICAL(&mux);
    }
    error=0;
done:
    if(measured) {tv4_ota_memory_window_end(measuring);tv4_ota_memory_report_window();}
    /* SDK monitor start allocates/asserts: never add that operation after
     * successful boot selection. Preserve the existing manual-reboot path. */
    if(!committed)tv4_ota_memory_window_begin(TV4_MEM_CLEANUP);
    tv4_time_forget(&anchor);tv4_ota_memory_result released=tv4_ota_memory_release(&lease);
    if(!committed) {tv4_ota_memory_window_end(TV4_MEM_CLEANUP);tv4_ota_memory_report_window();}
    worker_stack();
    /* No failing condition after boot selection: cleanup cannot revoke an
     * already committed update. Reboot remains an explicit operator action. */
    if(committed) {
        publish(TV4_UPDATE_RESTART,0);
    }else if(!error && released==TV4_MEMORY_OK)publish(install?TV4_UPDATE_RESTART:TV4_UPDATE_READY,0);
    else {memset(&approved,0,sizeof(approved));publish(TV4_UPDATE_ERROR,error?error:20);}
    tv4_update_snapshot final;tv4_ota_service_snapshot(&final);
    ESP_LOGI("tv4_ota","stage=%u error=%u internal=%u psram=%u stack=%u",(unsigned)final.state,final.error,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static void worker(void *c) {
    (void)c;
    if(esp_task_wdt_add(NULL)!=ESP_OK) {publish(TV4_UPDATE_BOOT_FAILED,21);esp_restart();}
    tv4_ota_boot_window window={.started=now_ms(NULL)};bool passed=false;
    for(;;) {
        esp_task_wdt_reset();portENTER_CRITICAL(&mux);uint32_t stamp=now_ms(NULL);bool good=health;uint32_t last=health_ms;portEXIT_CRITICAL(&mux);
        tv4_ota_boot_result check=tv4_ota_boot_observe(&window,stamp,good,last);
        if(check==TV4_BOOT_TIMEOUT)break;
        if(check==TV4_BOOT_CHECK_MEMORY) {
            volatile uint32_t *probe=heap_caps_malloc(256,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(probe && esp_ptr_external_ram((const void *)probe)) {
                passed=true;for(unsigned i=0;i<64;i++)probe[i]=0xa55a0000u+i;
                for(unsigned i=0;i<64;i++)if(probe[i]!=0xa55a0000u+i)passed=false;
            }
            heap_caps_free((void *)probe);break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    passed=passed && tv4_ota_sdk_layout_valid();
    tv4_ota_memory_window_end(TV4_MEM_BOOT);
    tv4_ota_memory_report_startup();tv4_ota_memory_report_window();
    bool finished=tv4_ota_sdk_finish_boot(passed);
#if CONFIG_TV4_OTA_WORKER_STACK_BENCH
    ESP_LOGI("tv4_ota","boot_self_test checks_pass=%u sdk_finish=%u worker_stack_bytes=%u stack_bench=1 provisioned=0",
        (unsigned)passed,(unsigned)finished,(unsigned)OTA_WORKER_STACK_BYTES);
#else
    ESP_LOGI("tv4_ota","boot_self_test checks_pass=%u sdk_finish=%u",(unsigned)passed,(unsigned)finished);
#endif
    worker_stack();
    if(!finished)esp_restart();
    if(esp_task_wdt_delete(NULL)!=ESP_OK)esp_restart();
    publish(!passed?TV4_UPDATE_BOOT_FAILED:configured()?TV4_UPDATE_IDLE:TV4_UPDATE_SETUP,0);
#if CONFIG_TV4_OTA_WORKER_TLS_BENCH
    if(passed) {
        bool probed=tv4_utc_tls_bench_run_in_ota_worker(cancelled,NULL);
        ESP_LOGI("tv4_ota","worker_tls_bench pass=%u; SETUP/install gates unchanged",(unsigned)probed);
        worker_stack();
    }
#endif
    for(;;) {
        worker_stack();
        tv4_update_command cmd;if(xQueueReceive(commands,&cmd,portMAX_DELAY)!=pdTRUE)continue;
        if(cmd==TV4_UPDATE_REBOOT)esp_restart();
        else operation(cmd==TV4_UPDATE_INSTALL);
    }
}
bool tv4_ota_service_start(void) {
    if(started)return false;
    started=true;commands=xQueueCreateStatic(1,sizeof(tv4_update_command),queue_bytes,&queue_state);
    tv4_ota_memory_window_begin(TV4_MEM_BOOT);
    bool ok=commands && xTaskCreate(worker,"tv4_ota",OTA_WORKER_STACK_BYTES,NULL,1,NULL)==pdPASS;
    if(!ok) {tv4_ota_memory_window_end(TV4_MEM_BOOT);publish(TV4_UPDATE_SETUP,21);const esp_partition_t *r=esp_ota_get_running_partition();esp_ota_img_states_t state;
        if(r && esp_ota_get_state_partition(r,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY)esp_restart();}
    return ok;
}
void tv4_ota_service_health(bool value) {
    uint32_t timestamp=now_ms(NULL);portENTER_CRITICAL(&mux);
    if(!value) {
        health=false;
        if(status.state==TV4_UPDATE_CHECKING || status.state==TV4_UPDATE_WRITING)cancel_requested=true;
    }else {health=true;health_ms=timestamp;}
    portEXIT_CRITICAL(&mux);
}
void tv4_ota_service_snapshot(tv4_update_snapshot *out) {if(out) {portENTER_CRITICAL(&mux);*out=status;portEXIT_CRITICAL(&mux);}}
bool tv4_ota_service_request(tv4_update_command cmd) {
    if(!commands)return false;
    portENTER_CRITICAL(&mux);uint32_t stamp=now_ms(NULL);tv4_update_state previous=status.state;
    bool healthy=health && (uint32_t)(stamp-health_ms)<500;
    bool busy=previous==TV4_UPDATE_CHECKING || previous==TV4_UPDATE_WRITING;
    bool ok=false;
    if(cmd==TV4_UPDATE_CANCEL && busy) {cancel_requested=true;ok=true;}
    else if((healthy && cmd==TV4_UPDATE_CHECK && (previous==TV4_UPDATE_IDLE || previous==TV4_UPDATE_READY || previous==TV4_UPDATE_ERROR)) ||
            (healthy && cmd==TV4_UPDATE_INSTALL && previous==TV4_UPDATE_READY) || (cmd==TV4_UPDATE_REBOOT && previous==TV4_UPDATE_RESTART)) {
        cancel_requested=false;status.state=cmd==TV4_UPDATE_CHECK?TV4_UPDATE_CHECKING:cmd==TV4_UPDATE_INSTALL?TV4_UPDATE_WRITING:TV4_UPDATE_RESTART;
        ok=xQueueSend(commands,&cmd,0)==pdTRUE;if(!ok)status.state=previous;
    }
    portEXIT_CRITICAL(&mux);return ok;
}
#else
bool tv4_ota_service_start(void) {return false;}
void tv4_ota_service_health(bool value) {(void)value;}
void tv4_ota_service_snapshot(tv4_update_snapshot *out) {if(out)memset(out,0,sizeof(*out));}
bool tv4_ota_service_request(tv4_update_command cmd) {(void)cmd;return false;}
#endif
