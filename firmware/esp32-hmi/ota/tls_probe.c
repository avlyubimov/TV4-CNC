#include "tls_probe.h"
static tv4_tls_result check(tv4_time_anchor *a,const tv4_tls_ops *o,uint32_t start,uint32_t timeout) {
    if(o->cancelled && o->cancelled(o->context))return TV4_TLS_CANCELLED;
    if((uint32_t)(o->now_ms(o->context)-start)>=timeout)return TV4_TLS_TIMEOUT;
    tv4_time_sample sample={0};
    if(o->clock(o->context,&sample)!=TV4_TIME_OK || tv4_time_check(a,&sample)!=TV4_TIME_OK) {
        tv4_time_forget(a);return TV4_TLS_TIME;
    }
    return TV4_TLS_OK;
}
static uint32_t remaining(const tv4_tls_ops *o,uint32_t start,uint32_t timeout) {
    uint32_t elapsed=o->now_ms(o->context)-start;if(elapsed>=timeout)return 0;
    uint32_t value=timeout-elapsed;return value>5000?5000:value;
}
tv4_tls_result tv4_tls_probe_run(tv4_time_anchor *a,uint32_t timeout,const tv4_tls_ops *o,tv4_tls_report *report) {
    if(report)*report=(tv4_tls_report){.result=TV4_TLS_ARGUMENT};
    if(!a || !o || !report || !o->open || !o->headers || !o->close || !o->clock || !o->now_ms || !timeout || timeout>15000)
        return TV4_TLS_ARGUMENT;
    uint32_t start=o->now_ms(o->context);tv4_tls_result result=check(a,o,start,timeout);
    if(result!=TV4_TLS_OK) {report->result=result;report->elapsed_ms=o->now_ms(o->context)-start;return result;}
    uint32_t budget=remaining(o,start,timeout);
    if(!budget) {report->result=TV4_TLS_TIMEOUT;report->elapsed_ms=o->now_ms(o->context)-start;return report->result;}
    bool opened=o->open(o->context,budget);
    result=check(a,o,start,timeout);
    if(result==TV4_TLS_OK && !opened)result=TV4_TLS_OPEN;
    if(result==TV4_TLS_OK) {
        budget=remaining(o,start,timeout);
        if(!budget)result=TV4_TLS_TIMEOUT;
        else {
            report->http_status=o->headers(o->context,budget);
            result=check(a,o,start,timeout);
            if(result==TV4_TLS_OK && report->http_status<0)result=TV4_TLS_HEADERS;
            else if(result==TV4_TLS_OK && report->http_status!=200)result=TV4_TLS_STATUS;
        }
    }
    report->cleanup_ok=o->close(o->context);
    tv4_tls_result final=check(a,o,start,timeout);
    if(final!=TV4_TLS_OK)result=final;
    if(!report->cleanup_ok)result=TV4_TLS_CLEANUP;
    report->elapsed_ms=o->now_ms(o->context)-start;
    if(result==TV4_TLS_OK && report->elapsed_ms>=timeout)result=TV4_TLS_TIMEOUT;
    report->result=result;return result;
}
