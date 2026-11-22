#pragma once
#include "lvgl.h"
#include "firmware_info.h"
#include "../ota/service.h"
/* One LVGL owner, zero-initialize; no worker/ISR callbacks or I/O. */
typedef struct { lv_obj_t *panel,*source,*value[5],*update_state,*update_note,*action[3],*caption[3];
    bool confirm;uint8_t confirm_digest[32];tv4_update_snapshot rendered;
} firmware_view_t;
void firmware_view_create(firmware_view_t *view,lv_obj_t *parent,
    lv_event_cb_t back,void *back_context);
/* info comes from firmware_info_set/reset, never raw downloaded fields. */
void firmware_view_update(firmware_view_t *view,const firmware_info_t *info);
void firmware_view_destroy(firmware_view_t *view);

void firmware_view_ota_update(firmware_view_t *,const tv4_update_snapshot *);
