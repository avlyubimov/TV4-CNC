#pragma once
#include "time_gate.h"
typedef enum { TV4_SNTP_OK,TV4_SNTP_ARGUMENT,TV4_SNTP_POLICY,TV4_SNTP_NO_NETWORK,
    TV4_SNTP_BUSY,TV4_SNTP_START_FAILED,TV4_SNTP_IO,TV4_SNTP_TIMEOUT,
    TV4_SNTP_CANCELLED,TV4_SNTP_CLEANUP,TV4_SNTP_TIME_REJECTED,TV4_SNTP_UNAVAILABLE } tv4_sntp_result;
typedef enum { TV4_SNTP_WAIT,TV4_SNTP_SYNCED,TV4_SNTP_POLL_FAILED } tv4_sntp_poll;
typedef struct {
    tv4_sntp_result result;tv4_time_result time_result;
    uint32_t elapsed_ms;unsigned polls;bool cleanup_ok;
} tv4_sntp_report;
typedef struct {
    void *context;
    bool (*online)(void *);
    tv4_sntp_result (*start)(void *);
    tv4_sntp_poll (*poll)(void *,uint32_t wait_ms);
    bool (*stop)(void *); /* After every start attempt, including failure/BUSY. */
    tv4_time_result (*clock)(void *,tv4_time_sample *);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *);
} tv4_sntp_ops;
/* Sequential worker-only lifecycle. Local reviewed policy must explicitly allow
 * unauthenticated SNTP. Poll budget <=250ms, total 1..30000ms. Old anchor revoked;
 * new anchor adopted only after sync + stop + valid system clock sample.
 * SDK calls/preemption/cleanup may exceed deadline; late data never authorizes TLS. */
tv4_sntp_result tv4_sntp_run(tv4_time_anchor *,const tv4_time_policy *,uint32_t timeout_ms,
    const tv4_sntp_ops *,tv4_sntp_report *);
/* Explicit rev12 SDK source, not called by startup/UI. Uses pool.ntp.org,
 * no DHCP server override, no custom callback pointer. Serializes SDK global
 * ownership in TCP/IP context; never stops a foreign client. Host/headless stub.
 * Caller exclusively owns anchor/clock updates; no concurrent TLS request. */
tv4_sntp_result tv4_sntp_sync(tv4_time_anchor *,const tv4_time_policy *,uint32_t timeout_ms,
    bool (*cancelled)(void *),void *cancel_context,tv4_sntp_report *);
