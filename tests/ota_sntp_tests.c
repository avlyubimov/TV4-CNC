#include "sntp_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const tv4_time_policy policy={true,true,1700000000u,2300000000u,60000,2000};
typedef struct {
    uint32_t now,step,start_delay,stop_delay,clock_delay;
    unsigned starts,stops,polls,reads;
    bool frozen_clock,online,cancel,cancel_poll,lose_network,stop_fail,clock_fail,bad_epoch,never_sync;
    tv4_sntp_result start_result;
    tv4_sntp_poll poll_result;
} fake;
static fake healthy(void) {fake f={0};f.online=true;f.step=10;f.poll_result=TV4_SNTP_SYNCED;return f;}
static bool online(void *context) {return ((fake *)context)->online;}
static bool cancelled(void *context) {return ((fake *)context)->cancel;}
static uint32_t now(void *context) {return ((fake *)context)->now;}
static tv4_sntp_result start(void *context) {fake *f=context;f->starts++;f->now+=f->start_delay;return f->start_result;}
static tv4_sntp_poll poll(void *context,uint32_t wait) {
    fake *f=context;assert(wait && wait<=250);f->polls++;if(!f->frozen_clock)f->now+=f->step>wait?f->step:wait;
    if(f->cancel_poll)f->cancel=true;
    if(f->lose_network)f->online=false;
    return f->never_sync?TV4_SNTP_WAIT:f->poll_result;
}
static bool stop(void *context) {fake *f=context;f->stops++;f->now+=f->stop_delay;return !f->stop_fail;}
static tv4_time_result clock_read(void *context,tv4_time_sample *out) {
    fake *f=context;assert(f->stops==1);f->reads++;f->now+=f->clock_delay;
    *out=(tv4_time_sample){f->bad_epoch?0:INT64_C(2000000000000000),f->now*UINT64_C(1000),TV4_TIME_LOCAL_CONFIRMED};
    return f->clock_fail?TV4_TIME_INVALID_CLOCK:TV4_TIME_OK;
}
static tv4_sntp_result run(fake *f,tv4_time_anchor *a,tv4_time_policy p,uint32_t timeout,tv4_sntp_report *report) {
    tv4_sntp_ops ops={f,online,start,poll,stop,clock_read,now,cancelled};
    tv4_sntp_result result=tv4_sntp_run(a,&p,timeout,&ops,report);
    assert(result==report->result && f->starts==f->stops);
    assert(a->ready==(result==TV4_SNTP_OK));
    if(a->ready)assert(a->accepted.source==TV4_TIME_SNTP_UNAUTHENTICATED);
    return result;
}
int main(void) {
    tv4_time_anchor a={0};tv4_sntp_report report;fake f=healthy();
    assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_OK && report.polls==1 && report.cleanup_ok);
    tv4_time_sample current=a.accepted;current.source=TV4_TIME_UNKNOWN;
    assert(tv4_time_check(&a,&current)==TV4_TIME_OK);
    f=healthy();f.online=false;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_NO_NETWORK && !f.starts);
    f=healthy();f.cancel=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_CANCELLED && !f.starts);
    f=healthy();f.start_result=TV4_SNTP_BUSY;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_BUSY && !f.reads);
    f=healthy();f.start_result=TV4_SNTP_START_FAILED;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_START_FAILED);
    f=healthy();f.start_result=TV4_SNTP_NO_NETWORK;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_NO_NETWORK);
    f=healthy();f.poll_result=TV4_SNTP_POLL_FAILED;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_IO && !f.reads);
    f=healthy();f.never_sync=true;assert(run(&f,&a,policy,501,&report)==TV4_SNTP_TIMEOUT && f.polls==3);
    f=healthy();f.frozen_clock=true;f.never_sync=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_IO && f.polls==6);
    f=healthy();f.step=1001;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_TIMEOUT && !f.reads);
    f=healthy();f.start_delay=1000;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_TIMEOUT && !f.polls);
    f=healthy();f.stop_delay=1000;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_TIMEOUT && !f.reads);
    f=healthy();f.clock_delay=1000;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_TIMEOUT);
    f=healthy();f.cancel_poll=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_CANCELLED && !f.reads);
    f=healthy();f.lose_network=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_NO_NETWORK && !f.reads);
    f=healthy();f.stop_fail=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_CLEANUP && !f.reads && !report.cleanup_ok);
    f=healthy();f.clock_fail=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_TIME_REJECTED && report.time_result==TV4_TIME_INVALID_CLOCK);
    f=healthy();f.bad_epoch=true;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_TIME_REJECTED && report.time_result==TV4_TIME_RANGE);
    f=healthy();f.now=UINT32_MAX-100;assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_OK && report.elapsed_ms==250);
    tv4_time_policy p=policy;p.allow_unauthenticated_sntp=false;
    f=healthy();assert(run(&f,&a,p,1000,&report)==TV4_SNTP_POLICY && !f.starts);
    p=policy;p.reviewed=false;f=healthy();assert(run(&f,&a,p,1000,&report)==TV4_SNTP_POLICY && !f.starts);
    p=policy;p.maximum_age_ms=0;f=healthy();assert(run(&f,&a,p,1000,&report)==TV4_SNTP_POLICY && !f.starts);
    f=healthy();assert(run(&f,&a,policy,30001,&report)==TV4_SNTP_ARGUMENT && !f.starts);
    f=healthy();assert(run(&f,&a,policy,0,&report)==TV4_SNTP_ARGUMENT && !f.starts);
    assert(tv4_sntp_run(NULL,&policy,1000,NULL,&report)==TV4_SNTP_ARGUMENT && report.result==TV4_SNTP_ARGUMENT);
    assert(tv4_sntp_sync(&a,&policy,1000,NULL,NULL,&report)==TV4_SNTP_UNAVAILABLE && !a.ready);
    for(unsigned i=0;i<100;i++) {f=healthy();assert(run(&f,&a,policy,1000,&report)==TV4_SNTP_OK);}
    puts("SNTP lifecycle/policy/provenance/deadline/cancel/network/cleanup/retry PASS; mock only, no UTC/network changed");
}
