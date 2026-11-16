#pragma once
#include "lvgl.h"
#include "strings.h"
typedef enum { BOOT_DISPLAY, BOOT_TOUCH, BOOT_NETWORK, BOOT_INTERFACE, BOOT_READY, BOOT_STAGE_COUNT } boot_stage_t;
void boot_view_create(void);
void boot_view_stage(boot_stage_t stage);
void boot_view_fail(string_id reason,const char *code);
bool boot_view_finish(void);
