#pragma once
/* SDK boundary only: actual service.c runs, no serial/network/Flash backend. */
#include "ota_sdk_fake.h"
#include <stdint.h>
typedef int portMUX_TYPE;
typedef void *QueueHandle_t;
typedef void *TaskHandle_t;
typedef unsigned UBaseType_t;
typedef int BaseType_t;
typedef void (*TaskFunction_t)(void *);
typedef struct { unsigned unused; } StaticQueue_t;
typedef struct {char version[32];} esp_app_desc_t;
typedef struct {unsigned revision;} esp_chip_info_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) ((uint32_t)(ms))
#define MALLOC_CAP_INTERNAL 1u
#define MALLOC_CAP_SPIRAM 2u
#define MALLOC_CAP_8BIT 4u
#define MALLOC_CAP_DMA 8u
void service_test_lock(void);
void service_test_unlock(void);
#define portENTER_CRITICAL(mux) ((void)(mux),service_test_lock())
#define portEXIT_CRITICAL(mux) ((void)(mux),service_test_unlock())
#define portENTER_CRITICAL_ISR(mux) portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL(mux)
QueueHandle_t xQueueCreateStatic(unsigned,unsigned,uint8_t *,StaticQueue_t *);
BaseType_t xQueueSend(QueueHandle_t,const void *,uint32_t);
BaseType_t xQueueReceive(QueueHandle_t,void *,uint32_t);
BaseType_t xTaskCreate(TaskFunction_t,const char *,uint32_t,void *,UBaseType_t,TaskHandle_t *);
void vTaskDelay(uint32_t);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
esp_err_t esp_task_wdt_add(TaskHandle_t);
esp_err_t esp_task_wdt_reset(void);
esp_err_t esp_task_wdt_delete(TaskHandle_t);
const esp_app_desc_t *esp_app_get_description(void);
void esp_chip_info(esp_chip_info_t *);
int64_t esp_timer_get_time(void);
_Noreturn void esp_restart(void);
typedef enum {ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,
    ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT} esp_reset_reason_t;
typedef void (*shutdown_handler_t)(void);
esp_reset_reason_t esp_reset_reason(void);
esp_err_t esp_register_shutdown_handler(shutdown_handler_t);
void *heap_caps_malloc(size_t,unsigned);
void heap_caps_free(void *);
size_t heap_caps_get_free_size(unsigned);
bool esp_ptr_external_ram(const void *);
void service_test_log(const char *,const char *,...);
#define ESP_LOGI(tag,...) service_test_log(tag,__VA_ARGS__)
