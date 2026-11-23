#pragma once
#include "ota_service_sdk_fake.h"
typedef struct {size_t total_free_bytes,minimum_free_bytes,largest_free_block;} multi_heap_info_t;
void heap_caps_get_info(multi_heap_info_t *,unsigned);
size_t heap_caps_get_minimum_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
esp_err_t heap_caps_monitor_local_minimum_free_size_start(void);
esp_err_t heap_caps_monitor_local_minimum_free_size_stop(void);
typedef void (*esp_alloc_failed_hook_t)(size_t,uint32_t,const char *);
esp_err_t heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t);
