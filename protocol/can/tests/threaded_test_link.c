#include "threaded_test_link.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static bool due(uint32_t now,uint32_t deadline)
{return now-deadline<=(uint32_t)INT32_MAX;}
static bool zero(const void *p,size_t n)
{const unsigned char *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static unsigned frame_kind(const tv4_fragment_frame *frame)
{
    assert(frame->length==TV4_FRAGMENT_BYTES);
    const uint8_t *raw=frame->bytes+TV4_FRAGMENT_HEADER;
    assert(raw[2]==1 && ((raw[0]=='T' && raw[1]=='U') || (raw[0]=='M' && raw[1]=='S')));
    unsigned kind=raw[3]+(raw[0]=='M'?8u:0u);assert(kind>=1 && kind<=12);return kind;
}
static void claim(tv4_threaded_link *s,unsigned side,uint32_t now)
{
    tv4_threaded_direction *d=&s->direction[side];
    if(d->occupied)return;
    tv4_dispatch_delivery token,checked;
    tv4_dispatch_status r=tv4_driver_dispatch_take(s->dispatch[side],&token);
    if(r==TV4_DISPATCH_WAIT)return;
    assert(r==TV4_DISPATCH_UNCHECKED && zero(&token.delivery.frame,sizeof(token.delivery.frame)));
    /* now belongs to the frozen current tick; use it AFTER acquiring token. */
    r=tv4_driver_dispatch_check(s->dispatch[side],token.delivery.ticket,&checked,now);
    d->occupied=true;d->job=token;++s->stats.claims;
    if(r!=TV4_DISPATCH_SENDABLE) {
        assert((r==TV4_DISPATCH_REVOKED || r==TV4_DISPATCH_EXPIRED || r==TV4_DISPATCH_CLOCK) && zero(&checked,sizeof(checked)));
        d->due=now;return;
    }
    d->job=checked;
    const tv4_fragment_frame *frame=&checked.delivery.frame;
    uint16_t field=(uint16_t)((uint16_t)frame->bytes[2] | (uint16_t)((uint16_t)frame->bytes[3]<<8));
    if((field&TV4_FRAGMENT_START)!=0) {
        d->kind=frame_kind(frame);d->index=0;
        unsigned bit=1u<<d->kind;
        d->drop_message=(s->config.drop_kinds&bit)!=0 &&
            (s->config.drop_every_message || (s->stats.dropped_kinds&bit)==0);
    }
    assert(d->kind>=1 && d->kind<=12);
    d->drop_current=d->drop_message && d->index==s->config.drop_index;++d->index;
    if(d->drop_current){++s->stats.drops;s->stats.dropped_kinds=(uint16_t)(s->stats.dropped_kinds|(1u<<d->kind));}
    uint32_t delay=s->config.hold_every>0 && s->stats.claims%s->config.hold_every==0?
        s->config.held_delivery_ms:s->config.delivery_ms;
    d->due=now+delay;
}
static void returned(tv4_threaded_link *s,unsigned side,tv4_frame_driver_result result)
{
    tv4_threaded_direction *d=&s->direction[side];
    tv4_driver_event event={d->job.delivery.ticket,result};
    tv4_frame_queue_result r=tv4_driver_dispatch_report(s->dispatch[side],&event);
    assert(r==TV4_QUEUE_ADMITTED || r==TV4_QUEUE_BUSY);++s->stats.posts;
    if(r==TV4_QUEUE_BUSY){d->pending_report=true;return;}
    if(s->stats.claims%17u==0) {
        assert(tv4_driver_dispatch_report(s->dispatch[side],&event)==TV4_QUEUE_STALE);++s->stats.rejected_callbacks;
    }
    /* The fake backend has relinquished this exact frame. Preserve message
     * index/kind, which are derived from actual published bytes, not the owner. */
    d->occupied=false;d->repeated=false;d->drop_current=false;
    memset(&d->job,0,sizeof(d->job));memset(&event,0,sizeof(event));
}
static void deliver(tv4_threaded_link *s,unsigned side,uint32_t now,bool hold)
{
    tv4_threaded_direction *d=&s->direction[side];
    if(!d->occupied)return;
    ++s->stats.held_ticks;
    if(d->pending_report) {
        tv4_frame_queue_result r=tv4_driver_dispatch_retry(s->dispatch[side]);++s->stats.report_retries;
        assert(r==TV4_QUEUE_ADMITTED || r==TV4_QUEUE_BUSY);
        if(r==TV4_QUEUE_ADMITTED){d->pending_report=false;d->occupied=false;d->repeated=false;d->drop_current=false;memset(&d->job,0,sizeof(d->job));}
        return;
    }
    if(hold || !due(now,d->due))return;
    tv4_dispatch_delivery checked;
    tv4_dispatch_status r=tv4_driver_dispatch_check(s->dispatch[side],d->job.delivery.ticket,&checked,now);
    if(r!=TV4_DISPATCH_SENDABLE) {
        assert((r==TV4_DISPATCH_REVOKED || r==TV4_DISPATCH_EXPIRED || r==TV4_DISPATCH_CLOCK) && zero(&checked,sizeof(checked)));
        /* A retained input cannot silently disappear on cancellation. It is
         * kept for explicit reconciliation; no positive tests use this path. */
        assert(!d->has_capture);returned(s,side,TV4_FRAME_CANCELLED_LOCAL);return;
    }
    assert(checked.delivery.frame.length==d->job.delivery.frame.length &&
        memcmp(checked.delivery.frame.bytes,d->job.delivery.frame.bytes,TV4_FRAGMENT_BYTES)==0);
    if(!d->drop_current) {
        if(!d->has_capture) {
            tv4_ingress_gate_result gate=tv4_ingress_owner_permit(s->owner[side],&d->permit);
            if(gate!=TV4_GATE_OPEN){assert(gate==TV4_GATE_REQUESTED || gate==TV4_GATE_PAUSED);return;}
            /* New simulated arrival AFTER permit. Backend can retain a TX job
             * until this arrival; that is not a physical CAN flow-control claim. */
            d->captured=checked.delivery.frame;d->captured_at=now;d->has_capture=true;++s->stats.captures;
        }
        tv4_ingress_result in=tv4_ingress_owner_offer(s->owner[side],d->permit,side+1u,
            s->generation[side],&d->captured,d->captured_at,now);
        if(in==TV4_INGRESS_BUSY){++s->stats.retained_captures;return;}
        assert(in==TV4_INGRESS_ADMITTED || in==TV4_INGRESS_HELD);
        d->has_capture=false;memset(&d->captured,0,sizeof(d->captured));++s->stats.ingress_posts;
    }
    if(!d->drop_current && !d->repeated && s->config.duplicate_every>0 &&
        s->stats.ingress_posts%s->config.duplicate_every==0) {
        d->repeated=true;d->due=now+s->config.delivery_ms;return;
    }
    returned(s,side,TV4_FRAME_SENT_LOCAL);
}
static void tick(tv4_threaded_link *s,uint32_t now,bool hold)
{
    for(unsigned side=0;side<2;++side) {
        tv4_ingress_pause pause;
        tv4_ingress_gate_result gate=tv4_ingress_owner_pause_request(s->owner[side],&pause);
        if(gate==TV4_GATE_REQUESTED) {
            /* External retained input is not covered by inner pending ACK.
             * Do not falsely acknowledge it, discard it or retag it. */
            if(!s->direction[side].has_capture) {
                gate=tv4_ingress_owner_ack(s->owner[side],pause,now);
                assert(gate==TV4_GATE_PAUSED || gate==TV4_GATE_BUSY);if(gate==TV4_GATE_PAUSED)++s->stats.pause_acks;
            }
        }else if(gate==TV4_GATE_OPEN) {
            tv4_ingress_result retry=tv4_ingress_owner_retry(s->owner[side],now);
            assert(retry==TV4_INGRESS_WAIT || retry==TV4_INGRESS_ADMITTED || retry==TV4_INGRESS_HELD);
        }else assert(gate==TV4_GATE_PAUSED);
        deliver(s,side,now,hold);claim(s,side,now);
    }
}
static void *driver(void *opaque)
{
    tv4_threaded_link *s=opaque;
    assert(pthread_mutex_lock(&s->lock)==0);
    for(;;) {
        while(!s->requested && !s->stopping)assert(pthread_cond_wait(&s->work,&s->lock)==0);
        if(s->stopping)break;
        uint32_t now=s->now;bool hold=s->hold_backend;s->requested=false;
        assert(pthread_mutex_unlock(&s->lock)==0);
        tick(s,now,hold);
        assert(pthread_mutex_lock(&s->lock)==0);s->finished=true;assert(pthread_cond_signal(&s->done)==0);
    }
    assert(pthread_mutex_unlock(&s->lock)==0);return NULL;
}
void tv4_threaded_link_start(tv4_threaded_link *s,tv4_test_link *link)
{
    assert(s!=NULL && link!=NULL && link->configured && link->config.ingress_owner &&
        !s->started && link->threaded==NULL);
    s->config=link->config;s->finished=true;s->now=*link->clock;
    for(unsigned side=0;side<2;++side) {
        s->dispatch[side]=&link->direction[side].dispatch;s->owner[side]=&link->direction[side].owner;
        s->generation[side]=link->generation[side];
    }
    assert(pthread_mutex_init(&s->lock,NULL)==0 && pthread_cond_init(&s->work,NULL)==0 && pthread_cond_init(&s->done,NULL)==0);
    s->started=true;s->active=true;link->threaded=s;assert(pthread_create(&s->thread,NULL,driver,s)==0);
}
void tv4_threaded_link_begin(tv4_threaded_link *s,uint32_t now)
{
    assert(s!=NULL && s->active && pthread_mutex_lock(&s->lock)==0 && s->finished);
    s->now=now;s->finished=false;s->requested=true;assert(pthread_cond_signal(&s->work)==0 && pthread_mutex_unlock(&s->lock)==0);
}
void tv4_threaded_link_wait(tv4_threaded_link *s,tv4_test_link *link)
{
    assert(s!=NULL && s->active && link->threaded==s && pthread_mutex_lock(&s->lock)==0);
    while(!s->finished)assert(pthread_cond_wait(&s->done,&s->lock)==0);
    /* Diagnostic snapshot after completed driver turn; NOT drain authority. */
    link->driver_claims=s->stats.claims;link->event_posts=s->stats.posts;link->ingress_posts=s->stats.ingress_posts;
    link->dispatch_rejected_callbacks=s->stats.rejected_callbacks;link->dropped_frames=s->stats.drops;
    link->dropped_kinds=s->stats.dropped_kinds;link->driver_held_ticks=s->stats.held_ticks;
    if(s->stats.posts>0)link->event_max_pending=1;
    assert(pthread_mutex_unlock(&s->lock)==0);
}
void tv4_threaded_link_pulse(tv4_threaded_link *s,tv4_test_link *link)
{tv4_threaded_link_wait(s,link);tv4_threaded_link_begin(s,*link->clock);tv4_threaded_link_wait(s,link);}
void tv4_threaded_link_hold(tv4_threaded_link *s,tv4_test_link *link,bool hold)
{
    tv4_threaded_link_wait(s,link);assert(pthread_mutex_lock(&s->lock)==0);s->hold_backend=hold;
    assert(pthread_mutex_unlock(&s->lock)==0);
}
void tv4_threaded_link_stop(tv4_threaded_link *s,tv4_test_link *link)
{
    tv4_threaded_link_wait(s,link);assert(pthread_mutex_lock(&s->lock)==0);s->stopping=true;
    assert(pthread_cond_signal(&s->work)==0 && pthread_mutex_unlock(&s->lock)==0 && pthread_join(s->thread,NULL)==0);
    assert(pthread_cond_destroy(&s->done)==0 && pthread_cond_destroy(&s->work)==0 && pthread_mutex_destroy(&s->lock)==0);
    s->active=false;link->threaded=NULL;
}
