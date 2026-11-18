#include "sntp_source.h"
static tv4_sntp_result finish(tv4_sntp_report *r,tv4_sntp_result result,const tv4_sntp_ops *o,uint32_t started) {
    r->result=result;
    if(o && o->now_ms)r->elapsed_ms=o->now_ms(o->context)-started;
    return result;
}
static tv4_sntp_result budget(const tv4_sntp_ops *o,uint32_t started,uint32_t timeout) {
    if(o->cancelled && o->cancelled(o->context))return TV4_SNTP_CANCELLED;
    if((uint32_t)(o->now_ms(o->context)-started)>=timeout)return TV4_SNTP_TIMEOUT;
    if(!o->online(o->context))return TV4_SNTP_NO_NETWORK;
    return TV4_SNTP_OK;
}
tv4_sntp_result tv4_sntp_run(tv4_time_anchor *a,const tv4_time_policy *p,uint32_t timeout,
    const tv4_sntp_ops *o,tv4_sntp_report *report) {
    if(report)*report=(tv4_sntp_report){0};
    if(!a || !report) {if(report)report->result=TV4_SNTP_ARGUMENT;return TV4_SNTP_ARGUMENT;}
    tv4_time_policy policy={0};if(p)policy=*p;
    tv4_time_forget(a);
    if(!p || !o || !o->online || !o->start || !o->poll || !o->stop || !o->clock || !o->now_ms ||
       !timeout || timeout>30000) {report->result=TV4_SNTP_ARGUMENT;return report->result;}
    p=&policy;report->time_result=tv4_time_policy_validate(p);
    if(report->time_result==TV4_TIME_OK && !p->allow_unauthenticated_sntp)report->time_result=TV4_TIME_SOURCE;
    if(report->time_result!=TV4_TIME_OK) {
        report->result=TV4_SNTP_POLICY;return report->result;
    }
    uint32_t started=o->now_ms(o->context);
    tv4_sntp_result result=budget(o,started,timeout);
    if(result!=TV4_SNTP_OK)return finish(report,result,o,started);
    result=o->start(o->context);
    if(result==TV4_SNTP_OK) {
        for(;;) {
            result=budget(o,started,timeout);if(result!=TV4_SNTP_OK)break;
            /* Fail closed if a broken backend returns WAIT without clock progress. */
            if(report->polls>=(timeout+249)/250+2) {result=TV4_SNTP_IO;break;}
            uint32_t elapsed=o->now_ms(o->context)-started;
            if(elapsed>=timeout) {result=TV4_SNTP_TIMEOUT;break;}
            uint32_t slice=timeout-elapsed;if(slice>250)slice=250;
            tv4_sntp_poll poll=o->poll(o->context,slice);report->polls++;
            result=budget(o,started,timeout);if(result!=TV4_SNTP_OK)break;
            if(poll==TV4_SNTP_SYNCED)break;
            if(poll!=TV4_SNTP_WAIT) {result=TV4_SNTP_IO;break;}
        }
    } else if(result!=TV4_SNTP_BUSY && result!=TV4_SNTP_NO_NETWORK)result=TV4_SNTP_START_FAILED;
    /* Backend owns any partial initialization; foreign client is never owned. */
    report->cleanup_ok=o->stop(o->context);
    if(!report->cleanup_ok)return finish(report,TV4_SNTP_CLEANUP,o,started);
    if(result!=TV4_SNTP_OK)return finish(report,result,o,started);
    result=budget(o,started,timeout);if(result!=TV4_SNTP_OK)return finish(report,result,o,started);
    tv4_time_sample sample={0};report->time_result=o->clock(o->context,&sample);
    if(report->time_result!=TV4_TIME_OK)return finish(report,TV4_SNTP_TIME_REJECTED,o,started);
    result=budget(o,started,timeout);if(result!=TV4_SNTP_OK)return finish(report,result,o,started);
    sample.source=TV4_TIME_SNTP_UNAUTHENTICATED;
    report->time_result=tv4_time_adopt(a,p,&sample);
    if(report->time_result!=TV4_TIME_OK)return finish(report,TV4_SNTP_TIME_REJECTED,o,started);
    result=budget(o,started,timeout);
    report->elapsed_ms=o->now_ms(o->context)-started;
    if(result==TV4_SNTP_OK && report->elapsed_ms>=timeout)result=TV4_SNTP_TIMEOUT;
    if(result!=TV4_SNTP_OK)tv4_time_forget(a);
    report->result=result;return result;
}
