#pragma once
#include "time_gate.h"
typedef enum { TV4_TLS_OK,TV4_TLS_ARGUMENT,TV4_TLS_TIME,TV4_TLS_TIMEOUT,
    TV4_TLS_CANCELLED,TV4_TLS_OPEN,TV4_TLS_HEADERS,TV4_TLS_STATUS,TV4_TLS_CLEANUP } tv4_tls_result;
typedef struct { tv4_tls_result result;int http_status;uint32_t elapsed_ms;bool cleanup_ok; } tv4_tls_report;
typedef struct {
    void *context;
    bool (*open)(void *,uint32_t io_timeout_ms);
    int (*headers)(void *,uint32_t io_timeout_ms); /* HEAD status; no body consumer. */
    bool (*close)(void *); /* Even after partially failed open. */
    tv4_time_result (*clock)(void *,tv4_time_sample *);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *);
} tv4_tls_ops;
/* One verified HEAD backend, one worker, fresh anchor; no URL/auth/body/Flash API.
 * Logical deadline <=15s, each backend I/O <=5s; no hard preemption of SDK calls.
 * Backend must provide CA/hostname checks; portable tests use mock I/O only. */
tv4_tls_result tv4_tls_probe_run(tv4_time_anchor *,uint32_t timeout_ms,const tv4_tls_ops *,tv4_tls_report *);
/* Explicit diagnostic image only, schedules once after UI startup delay. */
bool tv4_utc_tls_bench_start(void);
