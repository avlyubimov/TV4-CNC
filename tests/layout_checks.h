#pragma once
#include "lvgl.h"
/* Verify the rendered object tree, independent of dashboard coordinates. */
void check_dashboard_layout(lv_obj_t *root);
