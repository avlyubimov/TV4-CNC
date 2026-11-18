#include "time_gate.h"
#include <string.h>
#include <limits.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include <sys/time.h>
#endif
void tv4_time_forget(tv4_time_anchor *a) {if(a)memset(a,0,sizeof(*a));}
tv4_time_result tv4_time_policy_validate(const tv4_time_policy *p) {
    if(!p)return TV4_TIME_ARGUMENT;
    if(!p->reviewed)return TV4_TIME_UNREVIEWED;
    if(!p->earliest_epoch || p->latest_epoch<=p->earliest_epoch || !p->maximum_age_ms ||
       p->maximum_age_ms>86400000u || !p->maximum_skew_ms || p->maximum_skew_ms>60000u)return TV4_TIME_POLICY;
    return TV4_TIME_OK;
}
static bool source_allowed(const tv4_time_policy *p,tv4_time_source source) {
    return source==TV4_TIME_LOCAL_CONFIRMED || (source==TV4_TIME_SNTP_UNAUTHENTICATED && p->allow_unauthenticated_sntp);
}
static tv4_time_result sample_valid(const tv4_time_policy *p,const tv4_time_sample *s) {
    if(!s)return TV4_TIME_ARGUMENT;
    if(s->epoch_us<0 || s->monotonic_us>INT64_MAX)return TV4_TIME_INVALID_CLOCK;
    int64_t seconds=s->epoch_us/1000000;
    if(seconds<p->earliest_epoch || seconds>p->latest_epoch)return TV4_TIME_RANGE;
    return TV4_TIME_OK;
}
tv4_time_result tv4_time_adopt(tv4_time_anchor *a,const tv4_time_policy *p,const tv4_time_sample *s) {
    if(!a)return TV4_TIME_ARGUMENT;
    /* Copy inputs before revoking old anchor, including aliased members. */
    tv4_time_policy policy={0};tv4_time_sample sample={0};
    if(p)policy=*p;
    if(s)sample=*s;
    tv4_time_forget(a);
    tv4_time_result result=tv4_time_policy_validate(p?&policy:NULL);
    if(result!=TV4_TIME_OK)return result;
    result=sample_valid(&policy,s?&sample:NULL);if(result!=TV4_TIME_OK)return result;
    if(!source_allowed(&policy,sample.source))return TV4_TIME_SOURCE;
    a->accepted=sample;a->policy=policy;a->ready=true;return TV4_TIME_OK;
}
static tv4_time_result revoke(tv4_time_anchor *a,tv4_time_result r) {tv4_time_forget(a);return r;}
tv4_time_result tv4_time_check(tv4_time_anchor *a,const tv4_time_sample *current) {
    if(!a)return TV4_TIME_ARGUMENT;
    if(!a->ready)return TV4_TIME_NOT_READY;
    tv4_time_result r=tv4_time_policy_validate(&a->policy);
    if(r!=TV4_TIME_OK)return revoke(a,r);
    r=sample_valid(&a->policy,current);if(r!=TV4_TIME_OK)return revoke(a,r);
    r=sample_valid(&a->policy,&a->accepted);if(r!=TV4_TIME_OK)return revoke(a,r);
    if(!source_allowed(&a->policy,a->accepted.source))return revoke(a,TV4_TIME_SOURCE);
    if(current->monotonic_us<a->accepted.monotonic_us)return revoke(a,TV4_TIME_CLOCK_CHANGED);
    uint64_t elapsed=current->monotonic_us-a->accepted.monotonic_us;
    if(elapsed>=(uint64_t)a->policy.maximum_age_ms*1000)return revoke(a,TV4_TIME_EXPIRED);
    /* Range and age bounds keep addition/subtraction within signed int64. */
    int64_t expected=a->accepted.epoch_us+(int64_t)elapsed;
    int64_t skew=current->epoch_us-expected,tolerance=(int64_t)a->policy.maximum_skew_ms*1000;
    if(skew>tolerance || skew< -tolerance)return revoke(a,TV4_TIME_CLOCK_CHANGED);
    return TV4_TIME_OK;
}
tv4_time_result tv4_time_clock_read(tv4_time_sample *out) {
    if(!out)return TV4_TIME_ARGUMENT;
    memset(out,0,sizeof(*out));
#ifdef ESP_PLATFORM
    /* Bound sample-pair uncertainty; a preempted read fails closed after 3 tries.
     * No guarantee of hard preemption/deadline inside SDK calls. */
    for(unsigned attempt=0;attempt<3;attempt++) {
        int64_t before=esp_timer_get_time();struct timeval tv;
        if(gettimeofday(&tv,NULL)!=0)return TV4_TIME_INVALID_CLOCK;
        int64_t after=esp_timer_get_time();
        if(before<0 || after<before || tv.tv_sec<=0 || (uint64_t)tv.tv_sec>UINT32_MAX || tv.tv_usec<0 || tv.tv_usec>=1000000)
            return TV4_TIME_INVALID_CLOCK;
        if(after-before>1000)continue;
        out->epoch_us=(int64_t)tv.tv_sec*1000000+tv.tv_usec;
        out->monotonic_us=(uint64_t)(before+(after-before)/2);
        /* UNKNOWN: reading a plausible wall clock does not establish trust. */
        return TV4_TIME_OK;
    }
    return TV4_TIME_INVALID_CLOCK;
#else
    return TV4_TIME_UNAVAILABLE;
#endif
}
