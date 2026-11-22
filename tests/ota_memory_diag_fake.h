#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define MALLOC_CAP_INTERNAL 1u
#define MALLOC_CAP_8BIT 2u
#define MALLOC_CAP_SPIRAM 4u
#define ESP_OK 0
typedef struct {
    size_t total_free_bytes,total_allocated_bytes,largest_free_block,minimum_free_bytes;
    size_t allocated_blocks,free_blocks,total_blocks;
} multi_heap_info_t;
void fake_memory_lock(void);
void fake_memory_unlock(void);
#define portENTER_CRITICAL(mux) ((void)(mux),fake_memory_lock())
#define portEXIT_CRITICAL(mux) ((void)(mux),fake_memory_unlock())
int64_t esp_timer_get_time(void);
void heap_caps_get_info(multi_heap_info_t *,uint32_t);
int heap_caps_monitor_local_minimum_free_size_start(void);
int heap_caps_monitor_local_minimum_free_size_stop(void);
unsigned uxTaskGetStackHighWaterMark(void *);
void fake_memory_log(const char *,const char *,...);
#define ESP_LOGI(...) fake_memory_log(__VA_ARGS__)
