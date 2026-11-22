#pragma once
/* Host-only SDK boundary for the actual app_main + real LVGL integration.
 * No serial/Flash/GPIO implementation, target heap or hardware clock model. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
enum { ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=0x101,ESP_ERR_TIMEOUT=0x107 };
typedef struct esp_lcd_panel_t *esp_lcd_panel_handle_t;
typedef enum {ESP_RST_POWERON=1,ESP_RST_INT_WDT=4,ESP_RST_TASK_WDT=6,ESP_RST_WDT=7} esp_reset_reason_t;
typedef struct {char version[32],idf_ver[32];} esp_app_desc_t;
enum {MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_SPIRAM=4};
const char *esp_err_to_name(esp_err_t);
esp_reset_reason_t esp_reset_reason(void);
const esp_app_desc_t *esp_app_get_description(void);
int64_t esp_timer_get_time(void);
void vTaskDelay(uint32_t);
unsigned uxTaskGetStackHighWaterMark(void *);
void *heap_caps_malloc(size_t,uint32_t);
size_t heap_caps_get_free_size(uint32_t);
size_t heap_caps_get_total_size(uint32_t);
size_t heap_caps_get_minimum_free_size(uint32_t);
size_t heap_caps_get_largest_free_block(uint32_t);
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int,int,int,int,const void *);
void startup_test_log(bool,const char *,...);
#define pdMS_TO_TICKS(ms) ((uint32_t)(ms))
#define ESP_LOGE(tag,...) ((void)(tag),startup_test_log(true,__VA_ARGS__))
#define ESP_LOGW(tag,...) ((void)(tag),startup_test_log(false,__VA_ARGS__))
#define ESP_LOGI(tag,...) ((void)(tag),startup_test_log(false,__VA_ARGS__))
