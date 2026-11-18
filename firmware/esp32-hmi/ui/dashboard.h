#pragma once
#include "lvgl.h"
void dashboard_create(void);
void dashboard_update(void);

#include "model.h"
/* Called only by LVGL owner; returns an independent copy. */
model_t dashboard_snapshot(void);

/* LVGL-owner task only; same bounded ingress used by buttons and input faults. */
bool dashboard_post_event(event_t event);

typedef enum { VIEW_HOME, VIEW_DIAGNOSTICS, VIEW_LOG, VIEW_TOOLS, VIEW_WIFI } dashboard_view_t;
/* Owner-only requests; navigation callback queues the requested view. */
bool dashboard_set_view(dashboard_view_t view);
dashboard_view_t dashboard_get_view(void);

#include "diagnostics.h"
/* Owner-only instrumentation; resources are sampled in app_main, not callbacks. */
diagnostics_t *dashboard_diagnostics(void);
