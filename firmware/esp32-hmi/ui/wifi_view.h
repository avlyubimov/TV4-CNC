#pragma once
#include "lvgl.h"
lv_obj_t *wifi_view_create(lv_obj_t *root);
void wifi_view_update(void);

void wifi_view_hide(void);

void wifi_view_show(void);

void wifi_view_set_indicator(lv_obj_t *icon);
