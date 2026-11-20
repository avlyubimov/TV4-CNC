#include "sdkconfig.h"
#include <inttypes.h>
#include "dashboard.h"
#include "boot_view.h"
#include "wifi_service.h"
#include "export.h"
#if CONFIG_TV4_OTA_MEMORY_BENCH
#include "memory_probe.h"
#endif
#if CONFIG_TV4_UTC_TLS_BENCH
#include "tls_probe.h"
#endif
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#if CONFIG_TV4_BOARD_REV12
#include "rev12.h"
static esp_lcd_panel_handle_t panel;
#endif
static const char *TAG="tv4";
static bool display_failed;
static uint32_t display_errors;
static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
#if CONFIG_TV4_BOARD_REV12
    /* Copies a bounded partial RGB565 buffer into the RGB framebuffer.
       Single framebuffer can tear; no anti-tearing/FPS claim before board tests. */
    esp_err_t err=esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels);
    if(err!=ESP_OK) {
        display_failed=true;
        diag_increment(&display_errors); /* Owner consumes count; no logging/UI I/O here. */
    }
#else
    (void)area; (void)pixels;
#endif
    lv_display_flush_ready(display);
}
#if CONFIG_TV4_BOARD_REV12
static void read_touch(lv_indev_t *input,lv_indev_data_t *data) {
    (void)input; board_touch_t sample;
    data->state=LV_INDEV_STATE_RELEASED;
    if(board_rev12_touch(&sample) && sample.valid && (uint32_t)(tick_ms()-sample.ms)<100) {
        data->point.x=sample.x; data->point.y=sample.y;
        data->state=sample.pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
    }
}
#endif
static void boot_error(lv_display_t *display,string_id reason,esp_err_t error) {
    ESP_LOGE(TAG,"Startup stopped: %s",esp_err_to_name(error));
    boot_view_fail(reason,esp_err_to_name(error));lv_refr_now(display);
    for(;;) { lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(20)); }
}
void app_main(void) {
#if CONFIG_TV4_BOARD_REV12
    ESP_LOGW(TAG,"Uncommissioned Rev 1.2 display bench profile; CAN and motion absent");
    if(board_rev12_init(&panel)!=ESP_OK) { ESP_LOGE(TAG,"Board initialization failed; backlight remains off"); return; }
#else
    ESP_LOGW(TAG,"Headless simulation: no GPIO, display, touch or CAN initialized");
#endif
    ESP_LOGI(TAG,"Memory before UI: internal=%u PSRAM=%u bytes",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    /* app_main is the sole LVGL owner for its entire lifetime. */
    lv_init(); lv_tick_set_cb(tick_ms);
    lv_display_t *display=lv_display_create(800,480);
    const size_t bytes=800*20*2;
    void *buffer=heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!display || !buffer) { ESP_LOGE(TAG,"UI allocation failed"); return; }
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer,NULL,bytes,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    boot_view_create();lv_refr_now(display);
#if CONFIG_TV4_BOARD_REV12
    /* The boot screen is visible before GT911 initialization. */
    if(board_rev12_backlight(true)!=ESP_OK) { ESP_LOGE(TAG,"Backlight initialization failed");return; }
#endif
    uint32_t boot_started=tick_ms();
    boot_view_stage(BOOT_TOUCH);lv_refr_now(display);
#if CONFIG_TV4_BOARD_REV12
    esp_err_t touch_error=board_rev12_touch_init();
    if(touch_error!=ESP_OK)boot_error(display,S_BOOT_TOUCH_FAILED,touch_error);
    bool touch_ready=false;
    while((uint32_t)(tick_ms()-boot_started)<1000) {
        board_touch_t sample;
        if(board_rev12_touch(&sample) && sample.valid && (uint32_t)(tick_ms()-sample.ms)<100) { touch_ready=true;break; }
        lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(!touch_ready)boot_error(display,S_BOOT_TOUCH_FAILED,ESP_ERR_TIMEOUT);
    lv_indev_t *input=lv_indev_create();
    if(!input)boot_error(display,S_BOOT_INPUT_FAILED,ESP_ERR_NO_MEM);
    lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
    lv_indev_set_display(input,display);lv_indev_wait_release(input);
#endif
    boot_view_stage(BOOT_NETWORK);lv_refr_now(display);
#if CONFIG_TV4_BOARD_REV12
    /* Networking is optional; connection/DHCP never holds up the HMI boot. */
    if(!wifi_service_start()) ESP_LOGE(TAG,"Wi-Fi task creation failed");
#endif
    boot_view_stage(BOOT_INTERFACE);lv_refr_now(display);
    dashboard_create();
    /* Local running-build metadata, sampled once by LVGL owner at startup.
     * No network, partition scan, verifier or OTA worker is invoked. */
    firmware_info_t info;firmware_info_reset(&info);
    const esp_app_desc_t *app=esp_app_get_description();
    firmware_info_profile_t profile=FIRMWARE_PROFILE_HEADLESS;
#if CONFIG_TV4_OTA_MEMORY_BENCH
    profile=FIRMWARE_PROFILE_MEMORY_BENCH;
#elif CONFIG_TV4_UTC_TLS_BENCH
    profile=FIRMWARE_PROFILE_TLS_BENCH;
#elif CONFIG_TV4_BOARD_REV12
    profile=FIRMWARE_PROFILE_REV12;
#endif
    if(app)firmware_info_set(&info,app->version,sizeof(app->version),app->idf_ver,
        sizeof(app->idf_ver),FIRMWARE_INFO_DEVICE,profile);
    dashboard_set_firmware_info(&info);
    diagnostics_t *diagnostics=dashboard_diagnostics();
    diagnostics->record.board=true;
#if CONFIG_TV4_BOARD_REV12
    diagnostics->record.rev12=true;
#endif
    esp_reset_reason_t reset=esp_reset_reason();
    diagnostics->record.reset_reason=(uint32_t)reset;
    diagnostics->record.watchdog_boot=reset==ESP_RST_INT_WDT || reset==ESP_RST_TASK_WDT || reset==ESP_RST_WDT;
    if(!diag_export_start())ESP_LOGE(TAG,"Diagnostic export worker unavailable");
    boot_view_stage(BOOT_READY);lv_refr_now(display);
    /* Keep the splash readable without blocking LVGL or waiting for Wi-Fi. */
    while((uint32_t)(tick_ms()-boot_started)<1200) {
        if(display_failed)boot_error(display,S_FAULT_DISPLAY,ESP_FAIL);
        lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(display_failed)boot_error(display,S_FAULT_DISPLAY,ESP_FAIL);
    boot_view_finish();
#if CONFIG_TV4_BOARD_REV12
    lv_indev_wait_release(input); /* Never carry a held boot-time touch into JOG. */
#endif
    lv_refr_now(display);
#if CONFIG_TV4_BOARD_REV12
    uint32_t last_good_touch=tick_ms();
    bool touch_fault_logged=false;
#endif
    ESP_LOGI(TAG,"UI ready: LVGL %d.%d.%d, 800x480 RGB565, draw buffer=%u bytes",
        LVGL_VERSION_MAJOR,LVGL_VERSION_MINOR,LVGL_VERSION_PATCH,(unsigned)bytes);
#if CONFIG_TV4_OTA_MEMORY_BENCH || CONFIG_TV4_UTC_TLS_BENCH
    uint32_t bench_ready=tick_ms();bool bench_requested=false;
#endif
    uint32_t last_report=tick_ms(),last_resources=last_report;
    for(;;) {
        int64_t work_started=esp_timer_get_time();
#if CONFIG_TV4_BOARD_REV12
        board_touch_t sample;
        uint32_t now=tick_ms();
        if(board_rev12_touch(&sample) && sample.valid && (uint32_t)(now-sample.ms)<100) last_good_touch=now;
        if((uint32_t)(now-last_good_touch)>=500) {
            dashboard_post_event(EV_TOUCH_FAULT); /* Keep fault active until valid data resumes; never auto-ack. */
            if(!touch_fault_logged) { ESP_LOGE(TAG,"Touch data invalid/stale; UI fault latched"); touch_fault_logged=true; }
        }
#endif
        if(display_failed)dashboard_post_event(EV_DISPLAY_FAULT);
        dashboard_update(); lv_timer_handler();
        int64_t work_us=esp_timer_get_time()-work_started;
        diagnostics_work(diagnostics,work_us>UINT32_MAX?UINT32_MAX:(uint32_t)work_us);
        uint32_t report_now=tick_ms();
        if((uint32_t)(report_now-last_resources)>=1000) {
            diag_record_t *r=&diagnostics->record;
            r->resources_valid=true;r->display_errors=display_errors;
            r->internal_free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            r->internal_min=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            r->internal_largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            r->psram_free=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            r->psram_min=heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
            r->psram_total=heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
            r->owner_stack=uxTaskGetStackHighWaterMark(NULL);
            wifi_snapshot_t wifi;
            if(wifi_service_snapshot(&wifi)) {
                r->wifi_attempts=wifi.connect_attempts;r->wifi_disconnects=wifi.disconnect_events;
                r->wifi_ip_events=wifi.ip_events;r->wifi_queue_drops=wifi.queue_drops;r->wifi_stack=wifi.stack_watermark;
            }
#if CONFIG_TV4_BOARD_REV12
            board_touch_t health;
            if(board_rev12_touch(&health)) {r->touch_stack=health.stack_watermark;r->touch_errors=health.errors;r->touch_age_ms=report_now-health.ms;}
#endif
            last_resources=report_now;
        }
#if CONFIG_TV4_OTA_MEMORY_BENCH || CONFIG_TV4_UTC_TLS_BENCH
        /* Defer stack allocation until the startup/connect peak has passed.
         * Owner only schedules; worker performs all memory I/O and logging. */
        if(!bench_requested && (uint32_t)(report_now-bench_ready)>=15000) {
            bench_requested=true;
#if CONFIG_TV4_OTA_MEMORY_BENCH
            if(!tv4_ota_memory_bench_start())ESP_LOGE(TAG,"Memory probe unavailable; HMI continues");
#else
            if(!tv4_utc_tls_bench_start())ESP_LOGE(TAG,"UTC/TLS probe unavailable; HMI continues");
#endif
        }
#endif
        if((uint32_t)(report_now-last_report)>=5000) {
            if(!diag_export_submit(&diagnostics->record))diag_increment(&diagnostics->record.export_drops);
            last_report=report_now;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
