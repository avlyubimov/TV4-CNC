#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int portMUX_TYPE;
typedef void *TaskHandle_t;
typedef int BaseType_t;
typedef void (*esp_alloc_failed_hook_t)(size_t,uint32_t,const char *);
#define DRAM_ATTR
#define IRAM_ATTR
#define ESP_OK 0
#define portMUX_INITIALIZER_UNLOCKED 0
#define MALLOC_CAP_8BIT 4u
#define MALLOC_CAP_DMA 8u
#define MALLOC_CAP_INTERNAL 2048u
void dma_test_lock(void);
void dma_test_unlock(void);
#define portENTER_CRITICAL(mux) ((void)(mux),dma_test_lock())
#define portEXIT_CRITICAL(mux) ((void)(mux),dma_test_unlock())
#define portENTER_CRITICAL_ISR(mux) portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL(mux)
BaseType_t xPortInIsrContext(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
int heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t);
size_t heap_caps_get_free_size(uint32_t);
size_t heap_caps_get_largest_free_block(uint32_t);
void dma_test_log(const char *,const char *,...);
#define ESP_LOGI(tag,...) dma_test_log(tag,__VA_ARGS__)
