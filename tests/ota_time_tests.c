#include "time_gate.h"
#include "timed_download.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
/* Synthetic local policy/sample only, not production dates or NTP authentication. */
static const tv4_time_policy policy={true,false,1700000000u,2300000000u,60000,2000};
static const tv4_time_sample sample={INT64_C(2000000000000000),1000000,TV4_TIME_LOCAL_CONFIRMED};
static void accept(tv4_time_anchor *a) {assert(tv4_time_adopt(a,&policy,&sample)==TV4_TIME_OK && a->ready);}
static void reject(tv4_time_anchor *a,tv4_time_sample now,tv4_time_result expected) {
    assert(tv4_time_check(a,&now)==expected && !a->ready);
    assert(tv4_time_check(a,&sample)==TV4_TIME_NOT_READY); /* no revival */
}
typedef struct {
    tv4_time_sample clock;
    unsigned opened,closed,received,read_count;
    bool bad_clock,change_open,change_read,change_consumer,change_close,expire;
} server;
static tv4_time_result clock_read(void *context,tv4_time_sample *out) {
    server *s=context;*out=s->clock;return s->bad_clock?TV4_TIME_UNAVAILABLE:TV4_TIME_OK;
}
static bool open_server(void *context,const char *url,uint32_t timeout,tv4_http_response *h) {
    (void)url;(void)timeout;server *s=context;s->opened++;
    if(s->change_open)s->clock.epoch_us+=3000000;
    if(s->expire) {s->clock.epoch_us+=60000000;s->clock.monotonic_us+=60000000;}
    h->status=200;h->identity_encoding=true;h->length_known=true;h->content_length=4;return true;
}
static int read_server(void *context,uint8_t *bytes,size_t length) {
    server *s=context;assert(length>=2);
    if(s->change_read)s->clock.epoch_us-=3000000;
    if(s->read_count++==2)return 0;
    bytes[0]='a';bytes[1]='b';return 2;
}
static bool complete_server(void *context) {(void)context;return true;}
static void close_server(void *context) {server *s=context;s->closed++;if(s->change_close)s->clock.epoch_us+=3000000;}
static uint32_t mono(void *context) {return (uint32_t)(((server *)context)->clock.monotonic_us/1000);}
static bool consume(void *context,const uint8_t *bytes,size_t length) {
    server *s=context;assert(bytes);s->received+=(unsigned)length;
    if(s->change_consumer)s->clock.epoch_us+=3000000;
    return true;
}
static void transport_checks(void) {
    static tv4_http_workspace workspace;
    const tv4_http_request request={"https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json",16384,0,1000,10000,100,3,true};
    for(unsigned scenario=0;scenario<8;scenario++) {
        server s={0};s.clock=sample;tv4_time_anchor anchor={0};accept(&anchor);
        if(scenario==1)tv4_time_forget(&anchor); /* true hint alone is insufficient */
        if(scenario==2)s.change_open=true;
        if(scenario==3)s.change_read=true;
        if(scenario==4)s.change_consumer=true;
        if(scenario==5)s.change_close=true;
        if(scenario==6)s.bad_clock=true;
        if(scenario==7)s.expire=true;
        tv4_http_ops ops={&s,open_server,read_server,complete_server,close_server,mono,NULL};tv4_http_report report;
        tv4_http_result result=tv4_http_fetch_timed(&request,&ops,&anchor,clock_read,&s,&workspace,consume,&s,&report);
        assert(s.opened==s.closed);
        if(!scenario)assert(result==TV4_HTTP_OK && s.received==4 && anchor.ready);
        else {
            assert(result==TV4_HTTP_UNTRUSTED_TIME && !anchor.ready);
            if(scenario==1 || scenario==6)assert(!s.opened);
            if(scenario==2 || scenario==3 || scenario==7)assert(!s.received);
            if(scenario==4)assert(s.received==2);
            if(scenario==5)assert(s.received==4); /* late failure still refuses success */
        }
    }
}
int main(void) {
    tv4_time_anchor a={0};assert(tv4_time_check(&a,&sample)==TV4_TIME_NOT_READY);
    accept(&a);tv4_time_sample now=sample;
    now.monotonic_us+=59999999;now.epoch_us+=59999999;
    assert(tv4_time_check(&a,&now)==TV4_TIME_OK);
    now.monotonic_us++;now.epoch_us++;reject(&a,now,TV4_TIME_EXPIRED);
    accept(&a);now=sample;now.epoch_us+=2000000;assert(tv4_time_check(&a,&now)==TV4_TIME_OK);
    now.epoch_us++;reject(&a,now,TV4_TIME_CLOCK_CHANGED);
    accept(&a);now=sample;now.epoch_us-=2000000;assert(tv4_time_check(&a,&now)==TV4_TIME_OK);
    now.epoch_us--;reject(&a,now,TV4_TIME_CLOCK_CHANGED);
    accept(&a);now=sample;now.monotonic_us--;reject(&a,now,TV4_TIME_CLOCK_CHANGED);
    accept(&a);now=sample;now.epoch_us=0;reject(&a,now,TV4_TIME_RANGE);
    accept(&a);now=sample;now.epoch_us=-1;reject(&a,now,TV4_TIME_INVALID_CLOCK);
    accept(&a);now=sample;now.monotonic_us=UINT64_MAX;reject(&a,now,TV4_TIME_INVALID_CLOCK);
    accept(&a);assert(tv4_time_check(&a,NULL)==TV4_TIME_ARGUMENT && !a.ready);
    assert(tv4_time_check(NULL,&sample)==TV4_TIME_ARGUMENT);
    assert(tv4_time_adopt(NULL,&policy,&sample)==TV4_TIME_ARGUMENT);
    accept(&a);assert(tv4_time_adopt(&a,NULL,&sample)==TV4_TIME_ARGUMENT && !a.ready);
    accept(&a);assert(tv4_time_adopt(&a,&policy,NULL)==TV4_TIME_ARGUMENT && !a.ready);
    tv4_time_policy p=policy;p.reviewed=false;
    accept(&a);assert(tv4_time_adopt(&a,&p,&sample)==TV4_TIME_UNREVIEWED && !a.ready);
    for(unsigned field=0;field<6;field++) {
        p=policy;
        if(field==0)p.earliest_epoch=0;
        if(field==1)p.latest_epoch=p.earliest_epoch;
        if(field==2)p.maximum_age_ms=0;
        if(field==3)p.maximum_age_ms=86400001;
        if(field==4)p.maximum_skew_ms=0;
        if(field==5)p.maximum_skew_ms=60001;
        assert(tv4_time_adopt(&a,&p,&sample)==TV4_TIME_POLICY && !a.ready);
    }
    p=policy;now=sample;now.source=TV4_TIME_UNKNOWN;
    assert(tv4_time_adopt(&a,&p,&now)==TV4_TIME_SOURCE && !a.ready);
    now.source=TV4_TIME_SNTP_UNAUTHENTICATED;
    assert(tv4_time_adopt(&a,&p,&now)==TV4_TIME_SOURCE && !a.ready);
    p.allow_unauthenticated_sntp=true;
    assert(tv4_time_adopt(&a,&p,&now)==TV4_TIME_OK && a.accepted.source==TV4_TIME_SNTP_UNAUTHENTICATED);
    now.source=TV4_TIME_UNKNOWN; /* SDK current sample does not establish provenance. */
    assert(tv4_time_check(&a,&now)==TV4_TIME_OK);
    now=sample;now.epoch_us=(int64_t)policy.earliest_epoch*1000000-1;
    assert(tv4_time_adopt(&a,&policy,&now)==TV4_TIME_RANGE);
    now.epoch_us=(int64_t)policy.latest_epoch*1000000+999999;
    assert(tv4_time_adopt(&a,&policy,&now)==TV4_TIME_OK);
    now.epoch_us++;reject(&a,now,TV4_TIME_RANGE);
    now=sample;now.monotonic_us=INT64_MAX-100000000;
    assert(tv4_time_adopt(&a,&policy,&now)==TV4_TIME_OK);
    now.monotonic_us+=1000000;now.epoch_us+=1000000;
    assert(tv4_time_check(&a,&now)==TV4_TIME_OK);
    now=sample;now.epoch_us=INT64_C(2208988800000000); /* 2040, beyond signed 32-bit epoch */
    assert(tv4_time_adopt(&a,&policy,&now)==TV4_TIME_OK);
    assert(tv4_time_check(&a,&now)==TV4_TIME_OK);
    tv4_time_forget(&a);assert(!a.ready);tv4_time_forget(NULL);
    memset(&now,0xa5,sizeof(now));assert(tv4_time_clock_read(&now)==TV4_TIME_UNAVAILABLE);
    assert(now.epoch_us==0 && now.monotonic_us==0 && now.source==TV4_TIME_UNKNOWN);
    assert(tv4_time_clock_read(NULL)==TV4_TIME_ARGUMENT);
    transport_checks();
    puts("Time provenance/policy/age/skew/revocation/range/64-bit-clock PASS; no system clock or network changed");
}
