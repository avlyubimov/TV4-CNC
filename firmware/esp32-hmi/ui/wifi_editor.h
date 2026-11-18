#pragma once
#include "lvgl.h"
void wifi_editor_open(lv_obj_t *target,bool secret);
void wifi_editor_update(void);
void wifi_editor_hide(void);
bool wifi_editor_is_open(void);
