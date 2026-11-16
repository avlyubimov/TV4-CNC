#pragma once
#include "lvgl.h"
#include "model.h"
lv_obj_t *tools_view_create(lv_obj_t *parent);
/* Sole LVGL owner calls this; callbacks only enqueue bounded intentions. */
void tools_view_update(model_t *model);
