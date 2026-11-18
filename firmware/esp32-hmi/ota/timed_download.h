#pragma once
#include "download.h"
#include "time_gate.h"
typedef tv4_time_result (*tv4_time_reader)(void *,tv4_time_sample *);
/* Worker-only time checks around every backend operation and final cleanup.
 * A time failure overrides transport success, closes resources, revokes anchor.
 * Both request intent and fresh anchor required; no network/clock provider setup. */
tv4_http_result tv4_http_fetch_timed(const tv4_http_request *,const tv4_http_ops *,
    tv4_time_anchor *,tv4_time_reader,void *clock_context,tv4_http_workspace *,
    tv4_http_consume,void *consumer,tv4_http_report *);
