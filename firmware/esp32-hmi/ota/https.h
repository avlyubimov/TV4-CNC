#pragma once
#include "download.h"
#include "time_gate.h"
/* Synchronous worker-only SDK adapter. HTTPS CA bundle/hostname verification,
 * no auth/cookie/custom-header API, automatic redirects disabled.
 * Requires a fresh local time anchor as well as request.trusted_time intent.
 * Does not start a task or get called by app_main/UI. No Flash/boot actions. */
tv4_http_result tv4_https_read(const tv4_http_request *request,tv4_time_anchor *time,tv4_http_workspace *workspace,
    tv4_http_consume consume,void *consumer,bool (*cancelled)(void *),void *cancel_context,
    tv4_http_report *report);
