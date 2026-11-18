#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum { TV4_TIME_UNKNOWN,TV4_TIME_LOCAL_CONFIRMED,TV4_TIME_SNTP_UNAUTHENTICATED } tv4_time_source;
typedef enum { TV4_TIME_OK,TV4_TIME_ARGUMENT,TV4_TIME_UNREVIEWED,TV4_TIME_POLICY,
    TV4_TIME_SOURCE,TV4_TIME_RANGE,TV4_TIME_NOT_READY,TV4_TIME_EXPIRED,
    TV4_TIME_CLOCK_CHANGED,TV4_TIME_INVALID_CLOCK,TV4_TIME_UNAVAILABLE } tv4_time_result;
typedef struct { int64_t epoch_us;uint64_t monotonic_us;tv4_time_source source; } tv4_time_sample;
typedef struct {
    bool reviewed,allow_unauthenticated_sntp;
    uint32_t earliest_epoch,latest_epoch; /* Local reviewed UTC seconds; no default date. */
    uint32_t maximum_age_ms,maximum_skew_ms; /* <=24h and <=60s, nonzero. */
} tv4_time_policy;
typedef struct { bool ready;tv4_time_sample accepted;tv4_time_policy policy; } tv4_time_anchor;
/* Local provider only; never build date, static HMI clock or downloaded fields.
 * SNTP provenance is explicit; this gate does not authenticate NTP or set UTC.
 * One worker owns anchor/provider/system clock updates throughout a request. */
void tv4_time_forget(tv4_time_anchor *);
tv4_time_result tv4_time_adopt(tv4_time_anchor *,const tv4_time_policy *,const tv4_time_sample *);
/* Rejection revokes anchor until explicit new adoption; no automatic revival. */
tv4_time_result tv4_time_check(tv4_time_anchor *,const tv4_time_sample *current);
/* SDK gettimeofday+esp_timer only; no SNTP/DNS/settimeofday/TZ/task or I/O.
 * Host UNAVAILABLE. Does not establish provenance/eligibility by itself. */
tv4_time_result tv4_time_clock_read(tv4_time_sample *);
