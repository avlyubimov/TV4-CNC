#include "sdkconfig.h"
#include <inttypes.h>
#include "dashboard.h"
#include "wifi_service.h"
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
static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
#if CONFIG_TV4_BOARD_REV12
    /* Copies a bounded partial RGB565 buffer into the RGB framebuffer.
       Single framebuffer can tear; no anti-tearing/FPS claim before board tests. */
    esp_err_t err=esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels);
    if(err!=ESP_OK) {
        dashboard_post_event(EV_DISPLAY_FAULT);
        static bool logged;
        if(!logged) { ESP_LOGE(TAG,"RGB flush failed: %s",esp_err_to_name(err)); logged=true; }
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
#if CONFIG_TV4_BOARD_REV12
    lv_indev_t *input=lv_indev_create();
    if(!input) { ESP_LOGE(TAG,"Touch input allocation failed"); return; }
    lv_indev_set_type(input,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input,read_touch);
    lv_indev_set_display(input,display);
#endif
    dashboard_create();
#if CONFIG_TV4_BOARD_REV12
    if(!wifi_service_start()) ESP_LOGE(TAG,"Wi-Fi task creation failed");
#endif
#if CONFIG_TV4_BOARD_REV12
    /* First complete frame precedes backlight activation. */
    lv_refr_now(display);
    if(board_rev12_backlight(true)!=ESP_OK) { ESP_LOGE(TAG,"Backlight initialization failed"); return; }
#endif
#if CONFIG_TV4_BOARD_REV12
    uint32_t last_good_touch=tick_ms();
    bool touch_fault_logged=false;
#endif
    ESP_LOGI(TAG,"UI ready: LVGL %d.%d.%d, 800x480 RGB565, draw buffer=%u bytes",
        LVGL_VERSION_MAJOR,LVGL_VERSION_MINOR,LVGL_VERSION_PATCH,(unsigned)bytes);
    uint32_t last_report=tick_ms(),iterations=0;
    int64_t max_work_us=0;
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
        dashboard_update(); lv_timer_handler();
        int64_t work_us=esp_timer_get_time()-work_started;
        if(work_us>max_work_us) max_work_us=work_us;
        ++iterations;
        uint32_t report_now=tick_ms();
        if((uint32_t)(report_now-last_report)>=5000) {
            /* Owner-task work duration excludes sleep and this infrequent log.
               This is not scanout FPS or end-to-end touch latency. */
            ESP_LOGI(TAG,"UI health: loops=%" PRIu32 " window_ms=%" PRIu32
                " max_work_us=%" PRId64 " internal_free=%u internal_min=%u largest=%u PSRAM_free=%u stack_watermark=%u",
                iterations,(uint32_t)(report_now-last_report),max_work_us,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                (unsigned)uxTaskGetStackHighWaterMark(NULL));
#if CONFIG_TV4_BOARD_REV12
            ESP_LOGI(TAG,"Touch health: last_valid_age_ms=%" PRIu32 " fault_latched=%d",
                (uint32_t)(report_now-last_good_touch),touch_fault_logged);
#endif
            last_report=report_now; iterations=0; max_work_us=0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
