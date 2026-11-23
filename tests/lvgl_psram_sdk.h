#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef CONFIG_TV4_BOARD_REV12
#define CONFIG_TV4_BOARD_REV12 1
#endif
#ifndef CONFIG_SPIRAM
#define CONFIG_SPIRAM 1
#endif
#ifndef CONFIG_SPIRAM_XIP_FROM_PSRAM
#define CONFIG_SPIRAM_XIP_FROM_PSRAM 1
#endif
#define MALLOC_CAP_SPIRAM (1U<<10)
#define MALLOC_CAP_8BIT (1U<<2)
void *heap_caps_malloc(size_t,uint32_t);
void *heap_caps_realloc(void *,size_t,uint32_t);
void heap_caps_free(void *);
bool heap_caps_check_integrity(uint32_t,bool);
