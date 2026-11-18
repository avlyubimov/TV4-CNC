#include "tls_probe.h"
#include <assert.h>
#include <stdio.h>
static const tv4_time_policy policy={true,false,1700000000,2300000000,60000,2000};
static const tv4_time_sample sample={INT64_C(2000000000000000),1000000,TV4_TIME_LOCAL_CONFIRMED};
typedef struct { tv4_time_sample time;unsigned opens,heads,closes;int status;bool open_fail,close_fail,cancel,clock_fail,jump_open,jump_close;uint32_t open_delay,head_delay; } fake;
static bool open_io(void *context,uint32_t timeout) {
    fake *f=context;assert(timeout && timeout<=5000);f->opens++;
    f->time.epoch_us+=(int64_t)f->open_delay*1000;f->time.monotonic_us+=(uint64_t)f->open_delay*1000;
    if(f->jump_open)f->time.epoch_us+=3000000;
    return !f->open_fail;
}
static int headers(void *context,uint32_t timeout) {
    fake *f=context;assert(timeout && timeout<=5000);f->heads++;
    f->time.epoch_us+=(int64_t)f->head_delay*1000;f->time.monotonic_us+=(uint64_t)f->head_delay*1000;
    return f->status;
}
static bool close_io(void *context) {fake *f=context;f->closes++;if(f->jump_close)f->time.epoch_us-=3000000;return !f->close_fail;}
static tv4_time_result clock_read(void *context,tv4_time_sample *out) {fake *f=context;*out=f->time;return f->clock_fail?TV4_TIME_INVALID_CLOCK:TV4_TIME_OK;}
static uint32_t now(void *context) {return (uint32_t)(((fake *)context)->time.monotonic_us/1000);}
static bool cancel(void *context) {return ((fake *)context)->cancel;}
static fake healthy(void) {fake f={0};f.time=sample;f.status=200;return f;}
static tv4_tls_result run(fake *f,tv4_time_anchor *a,tv4_tls_report *report) {
    tv4_tls_ops ops={f,open_io,headers,close_io,clock_read,now,cancel};
    tv4_tls_result result=tv4_tls_probe_run(a,15000,&ops,report);
    assert(result==report->result && f->opens==f->closes);return result;
}
static void adopt(tv4_time_anchor *a) {assert(tv4_time_adopt(a,&policy,&sample)==TV4_TIME_OK);}
int main(void) {
    tv4_time_anchor a={0};tv4_tls_report r;fake f=healthy();
    assert(run(&f,&a,&r)==TV4_TLS_TIME && !f.opens);
    adopt(&a);f=healthy();assert(run(&f,&a,&r)==TV4_TLS_OK && f.heads==1 && r.cleanup_ok);
    adopt(&a);f=healthy();f.open_fail=true;assert(run(&f,&a,&r)==TV4_TLS_OPEN && !f.heads && r.cleanup_ok);
    adopt(&a);f=healthy();f.status=-1;assert(run(&f,&a,&r)==TV4_TLS_HEADERS);
    adopt(&a);f=healthy();f.status=302;assert(run(&f,&a,&r)==TV4_TLS_STATUS && f.opens==1); /* No redirects. */
    adopt(&a);f=healthy();f.close_fail=true;assert(run(&f,&a,&r)==TV4_TLS_CLEANUP && !r.cleanup_ok);
    adopt(&a);f=healthy();f.cancel=true;assert(run(&f,&a,&r)==TV4_TLS_CANCELLED && !f.opens);
    adopt(&a);f=healthy();f.clock_fail=true;assert(run(&f,&a,&r)==TV4_TLS_TIME && !a.ready && !f.opens);
    adopt(&a);f=healthy();f.jump_open=true;assert(run(&f,&a,&r)==TV4_TLS_TIME && !a.ready && !f.heads);
    adopt(&a);f=healthy();f.jump_close=true;assert(run(&f,&a,&r)==TV4_TLS_TIME && !a.ready);
    adopt(&a);f=healthy();f.open_delay=15000;assert(run(&f,&a,&r)==TV4_TLS_TIMEOUT && !f.heads);
    adopt(&a);f=healthy();f.head_delay=15000;assert(run(&f,&a,&r)==TV4_TLS_TIMEOUT);
    assert(tv4_tls_probe_run(NULL,1,NULL,&r)==TV4_TLS_ARGUMENT && r.result==TV4_TLS_ARGUMENT);
    assert(!tv4_utc_tls_bench_start());
    puts("HEAD probe time/deadline/cleanup/status/no-redirect lifecycle PASS; mocks only, no CA/TLS/network executed");
}
