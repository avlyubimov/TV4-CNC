#include "fragment_test_link.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static tv4_driver_events *events(tv4_test_link *link,unsigned side)
{return link->config.driver_dispatch?&link->direction[side].dispatch.completion.events:link->config.completion_mailbox?&link->direction[side].completion.events:&link->direction[side].events;}
static tv4_update_egress *egress(tv4_test_link *link,unsigned side)
{return link->config.driver_events?&events(link,side)->egress:&link->direction[side].egress;}
static const tv4_channel_rx *rx_view(const tv4_test_link *link,unsigned side)
{return link->config.ingress_owner?&link->direction[side].owner.channel:&link->direction[side].channel_rx;}
static const tv4_frame_ingress *ingress_view(const tv4_test_link *link,unsigned side)
{return link->config.ingress_owner?&link->direction[side].owner.ingress:&link->direction[side].ingress;}
void tv4_test_link_init(tv4_test_link *link, uint32_t *clock, const tv4_test_link_config *config)
{
    assert(link!=NULL && clock!=NULL && config!=NULL && !link->configured);
    assert(!(config->raw_setup && config->multiplexed));
    assert(!config->egress || config->multiplexed);
    assert(!config->driver_events || (config->egress && config->event_service_ms>0 && config->event_service_ms<=(uint32_t)INT32_MAX));
    assert(!config->completion_mailbox || config->driver_events);
    assert(!config->driver_dispatch || config->completion_mailbox);
    assert(!config->frame_ingress || (config->driver_dispatch && config->ingress_service_ms>0 && config->ingress_service_ms<=(uint32_t)INT32_MAX));
    assert(!config->ingress_owner || config->frame_ingress);
    assert(config->delivery_ms>0 && config->delivery_ms<=(uint32_t)INT32_MAX);
    assert(config->held_delivery_ms>=config->delivery_ms && config->held_delivery_ms<=(uint32_t)INT32_MAX);
    memset(link,0,sizeof(*link));link->config=*config;link->clock=clock;
    for(unsigned i=0;i<2;++i) {
        link->generation[i]=100u+i;
        if(config->multiplexed) {
            if(config->driver_dispatch)assert(tv4_driver_dispatch_init(&link->direction[i].dispatch,&config->fragments,config->custody,config->event_capacity,i+1u,link->generation[i],0,0,*clock));
            else if(config->completion_mailbox)assert(tv4_completion_owner_init(&link->direction[i].completion,&config->fragments,config->custody,config->event_capacity,i+1u,link->generation[i],0,0,*clock));
            else if(config->driver_events)assert(tv4_driver_events_init(&link->direction[i].events,&config->fragments,config->custody,config->event_capacity,i+1u,link->generation[i],0,0,*clock));
            else if(config->egress)assert(tv4_update_egress_init(&link->direction[i].egress,&config->fragments,config->custody,i+1u,link->generation[i],0,0,*clock));
            else assert(tv4_channel_tx_init(&link->direction[i].channel_tx,&config->fragments,i+1u,link->generation[i],0,*clock));
            if(config->ingress_owner)assert(tv4_ingress_owner_init(&link->direction[i].owner,&config->fragments,config->ingress_capacity,config->ingress_residency_ms,i+1u,link->generation[i],0,0,0,*clock));
            else {
                assert(tv4_channel_rx_init(&link->direction[i].channel_rx,&config->fragments,i+1u,link->generation[i],0,*clock));
                if(config->frame_ingress)assert(tv4_frame_ingress_init(&link->direction[i].ingress,config->ingress_capacity,config->ingress_residency_ms,i+1u,link->generation[i],0,*clock));
            }
            continue;
        }
        tv4_fragment_result tx=config->raw_setup?
            tv4_fragment_tx_init_raw(&link->direction[i].tx,&config->fragments,i+1u,link->generation[i],0,*clock):
            tv4_fragment_tx_init(&link->direction[i].tx,&config->fragments,i+1u,link->generation[i],0,*clock);
        assert(tx==TV4_FRAG_OK);
        tv4_fragment_result rx=config->raw_setup?
            tv4_fragment_rx_init_raw(&link->direction[i].rx,&config->fragments,i+1u,link->generation[i],0,*clock):
            tv4_fragment_rx_init(&link->direction[i].rx,&config->fragments,i+1u,link->generation[i],0,*clock);
        assert(rx==TV4_FRAG_OK);
    }
    link->configured=true;
}
bool tv4_test_link_can_send(const tv4_test_link *link, unsigned direction)
{
    assert(link!=NULL && link->configured && direction<2);
    const tv4_test_link_direction *d=&link->direction[direction];
    if(link->config.frame_ingress && tv4_frame_ingress_empty(ingress_view(link,direction))!=TV4_INGRESS_WAIT)return false;
    const tv4_completion_owner *completion=link->config.driver_dispatch?&d->dispatch.completion:&d->completion;
    const tv4_driver_events *ev=link->config.completion_mailbox?&completion->events:&d->events;
    const tv4_update_egress *tx=link->config.driver_events?&ev->egress:&d->egress;
    if(link->config.egress)return !d->occupied && !tx->channel.active && !tx->channel.queued &&
        tx->channel.fragments.storage.state==TV4_FRAG_IDLE && tx->queue.count==0 &&
        tx->queue.flight==0 && !tx->failed && !tx->queue.stopped && (!link->config.driver_events || ev->count==0) &&
        (!link->config.completion_mailbox || (!completion->has_pending && tv4_completion_mailbox_empty(&completion->mailbox)==TV4_QUEUE_OK)) &&
        (!link->config.driver_dispatch || atomic_load_explicit(&d->dispatch.state,memory_order_acquire)==TV4_DISPATCH_EMPTY);
    return !d->occupied && (link->config.multiplexed?
        (!d->channel_tx.active && !d->channel_tx.queued && d->channel_tx.fragments.storage.state==TV4_FRAG_IDLE):
        d->tx.storage.state==TV4_FRAG_IDLE);
}
bool tv4_test_link_idle(const tv4_test_link *link)
{
    for(unsigned side=0;side<2;++side) {
        const tv4_test_link_direction *d=&link->direction[side];
        if(!tv4_test_link_can_send(link,side))return false;
        if(link->config.multiplexed) {
            const tv4_completion_owner *completion=link->config.driver_dispatch?&d->dispatch.completion:&d->completion;
            const tv4_driver_events *ev=link->config.completion_mailbox?&completion->events:&d->events;
            const tv4_channel_tx *tx=link->config.egress?(link->config.driver_events?&ev->egress.channel:&d->egress.channel):&d->channel_tx;
            const tv4_channel_rx *rx=rx_view(link,side);
            if(rx->fragments.storage.state!=TV4_FRAG_IDLE ||
                tx->error!=TV4_CHANNEL_NO_ERROR || rx->error!=TV4_CHANNEL_NO_ERROR)return false;
        }else if(d->rx.storage.state!=TV4_FRAG_IDLE)return false;
    }
    return true;
}
void tv4_test_link_advance(tv4_test_link *link,tv4_channel_phase phase,tv4_update_id id)
{
    assert(link!=NULL && link->configured && link->config.multiplexed && tv4_test_link_idle(link));
    for(unsigned i=0;i<2;++i) {
        assert((link->config.driver_dispatch?
            tv4_driver_dispatch_advance(&link->direction[i].dispatch,phase,id,link->generation[i],*link->clock):link->config.completion_mailbox?
            tv4_completion_owner_advance(&link->direction[i].completion,phase,id,link->generation[i],*link->clock):link->config.driver_events?
            tv4_driver_events_advance(&link->direction[i].events,phase,id,link->generation[i],*link->clock):link->config.egress?
            tv4_update_egress_advance(&link->direction[i].egress,phase,id,link->generation[i],*link->clock):
            tv4_channel_tx_advance(&link->direction[i].channel_tx,phase,id,link->generation[i],*link->clock))==TV4_CHANNEL_OK);
        if(link->config.ingress_owner) {
            tv4_ingress_owner *owner=&link->direction[i].owner;
            assert(tv4_ingress_owner_pause(owner,link->generation[i],*link->clock)==TV4_GATE_REQUESTED);tv4_ingress_pause p;
            assert(tv4_ingress_owner_pause_request(owner,&p)==TV4_GATE_REQUESTED && tv4_ingress_owner_ack(owner,p,*link->clock)==TV4_GATE_PAUSED);++link->owner_pauses;
            assert(tv4_ingress_owner_quiescent(owner,link->generation[i],*link->clock)==TV4_GATE_PAUSED);
            assert(tv4_ingress_owner_advance(owner,phase,id,link->generation[i],*link->clock)==TV4_CHANNEL_OK);++link->owner_advances;
            assert(tv4_ingress_owner_resume(owner,link->generation[i],*link->clock)==TV4_GATE_OPEN);
        }else assert(tv4_channel_rx_advance(&link->direction[i].channel_rx,phase,id,link->generation[i],*link->clock)==TV4_CHANNEL_OK);
    }
}
static tv4_fragment_result channel_result(tv4_channel_result r)
{
    switch(r) {
    case TV4_CHANNEL_OK:return TV4_FRAG_OK;
    case TV4_CHANNEL_BUSY:return TV4_FRAG_BUSY;
    case TV4_CHANNEL_STATE:return TV4_FRAG_STATE;
    case TV4_CHANNEL_IDENTITY:return TV4_FRAG_STALE;
    case TV4_CHANNEL_ARGUMENT:return TV4_FRAG_ARGUMENT;
    case TV4_CHANNEL_CONTENT:return TV4_FRAG_BAD_FRAME;
    default:return TV4_FRAG_ERROR;
    }
}
tv4_fragment_result tv4_test_link_send(tv4_test_link *link, unsigned direction, const tv4_wire_message *message)
{
    assert(link!=NULL && link->configured && direction<2 && message!=NULL && !link->config.raw_setup);
    if(!tv4_test_link_can_send(link,direction)){++link->queue_busy;return TV4_FRAG_BUSY;}
    tv4_test_link_direction *d=&link->direction[direction];
    tv4_fragment_result r;
    if(link->config.multiplexed) {
        tv4_channel_message m={.kind=TV4_CHANNEL_WIRE,.body.wire=*message};
        r=channel_result(link->config.driver_dispatch?
            tv4_driver_dispatch_begin(&d->dispatch,&m,link->generation[direction],*link->clock):link->config.completion_mailbox?
            tv4_completion_owner_begin(&d->completion,&m,link->generation[direction],*link->clock):link->config.driver_events?
            tv4_driver_events_begin(&d->events,&m,link->generation[direction],*link->clock):link->config.egress?
            tv4_update_egress_begin(&d->egress,&m,link->generation[direction],*link->clock):
            tv4_channel_tx_begin(&d->channel_tx,&m,link->generation[direction],*link->clock));
    }else r=tv4_fragment_tx_begin(&d->tx,message,link->generation[direction],*link->clock);
    if(r!=TV4_FRAG_OK)return r;
    assert(message->kind>=TV4_WIRE_REQUEST && message->kind<=TV4_WIRE_OUTCOME_ACK);
    d->kind=(unsigned)message->kind;d->index=0;
    unsigned bit=1u<<(unsigned)message->kind;
    d->drop_message=(link->config.drop_kinds&bit)!=0 &&
        (link->config.drop_every_message || (link->dropped_kinds&bit)==0);
    ++link->attempts[(unsigned)message->kind];return r;
}
static void recover_owner(tv4_test_link *link,unsigned side)
{
    tv4_ingress_owner *owner=&link->direction[side].owner;uint64_t generation=link->generation[side];uint32_t now=*link->clock;
    tv4_ingress_pause p;assert(tv4_ingress_owner_pause_request(owner,&p)==TV4_GATE_REQUESTED);
    tv4_ingress_gate_result ack=tv4_ingress_owner_ack(owner,p,now);
    for(unsigned n=0;ack==TV4_GATE_BUSY && n<TV4_FRAME_INGRESS_MAX+2u;++n) {
        tv4_ingress_token t;assert(tv4_ingress_owner_stage(owner,&t)==TV4_INGRESS_UNCHECKED);
        assert(tv4_ingress_owner_receive(owner,t,generation,now)==TV4_FRAG_STATE);++link->ingress_checks;++link->owner_retirements;
        ack=tv4_ingress_owner_ack(owner,p,now);
    }
    assert(ack==TV4_GATE_PAUSED);++link->owner_pauses;
    for(unsigned n=0;n<TV4_FRAME_INGRESS_MAX+2u;++n) {
        tv4_ingress_token t;tv4_ingress_result r=tv4_ingress_owner_stage(owner,&t);if(r==TV4_INGRESS_WAIT)break;
        assert(r==TV4_INGRESS_UNCHECKED && tv4_ingress_owner_receive(owner,t,generation,now)==TV4_FRAG_STATE);++link->ingress_checks;++link->owner_retirements;
    }
    assert(tv4_ingress_owner_recover(owner,generation,now)==TV4_FRAG_OK && tv4_ingress_owner_resume(owner,generation,now)==TV4_GATE_OPEN);++link->owner_recovers;++link->timeouts;
}
static void timeout_recovery(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    uint64_t generation=link->generation[side];uint32_t now=*link->clock;
    if(link->config.egress) {
        tv4_update_egress *owner=egress(link,side);
        tv4_frame_queue_result tx=link->config.driver_dispatch?
            tv4_driver_dispatch_poll(&d->dispatch,generation,now):link->config.completion_mailbox?
            tv4_completion_owner_poll(&d->completion,generation,now):link->config.driver_events?
            tv4_driver_events_poll(&d->events,generation,now):tv4_update_egress_poll(owner,generation,now);
        tv4_fragment_result rx=link->config.ingress_owner?tv4_ingress_owner_poll(&d->owner,generation,now):tv4_channel_rx_poll(&d->channel_rx,generation,now);
        if(tx==TV4_QUEUE_ERROR) {
            tv4_fragment_error low=owner->channel.fragments.storage.error;
            bool recover=link->config.recover_fragment_timeouts &&
                (owner->queue.error==TV4_QUEUE_TIMEOUT || low==TV4_FRAG_PROGRESS_TIMEOUT || low==TV4_FRAG_TOTAL_TIMEOUT ||
                 owner->channel.error==TV4_CHANNEL_QUEUE_PROGRESS || owner->channel.error==TV4_CHANNEL_QUEUE_TOTAL);
            if(recover) {
                tv4_frame_queue_result stopped=link->config.driver_dispatch?
                    tv4_driver_dispatch_cancel(&d->dispatch,generation,now):link->config.completion_mailbox?
                    tv4_completion_owner_cancel(&d->completion,generation,now):link->config.driver_events?
                    tv4_driver_events_cancel(&d->events,generation,now):tv4_update_egress_cancel(owner,generation,now);
                if(stopped!=TV4_QUEUE_HELD) {
                    tv4_frame_queue_result cleared=link->config.driver_dispatch?
                        tv4_driver_dispatch_clear(&d->dispatch,generation,now):link->config.completion_mailbox?
                        tv4_completion_owner_clear(&d->completion,generation,now):link->config.driver_events?
                        tv4_driver_events_clear(&d->events,generation,now):tv4_update_egress_clear(owner,generation,now);
                    assert(cleared==TV4_QUEUE_OK || (link->config.driver_events && cleared==TV4_QUEUE_HELD));
                    if(cleared==TV4_QUEUE_OK)++link->timeouts;
                }
            }else ++link->sticky_failures;
        }
        if(rx==TV4_FRAG_ERROR) {
            tv4_fragment_error low=rx_view(link,side)->fragments.storage.error;
            if(link->config.recover_fragment_timeouts && (low==TV4_FRAG_PROGRESS_TIMEOUT || low==TV4_FRAG_TOTAL_TIMEOUT)) {
                if(link->config.ingress_owner) {
                    if(tv4_frame_ingress_faults(&d->owner.ingress)==0)recover_owner(link,side);else ++link->sticky_failures;
                }else {assert(tv4_channel_rx_clear(&d->channel_rx,generation,now)==TV4_FRAG_OK);++link->timeouts;}
            }else ++link->sticky_failures;
        }
        return;
    }
    tv4_fragment_result tx=link->config.multiplexed?tv4_channel_tx_poll(&d->channel_tx,generation,now):tv4_fragment_tx_poll(&d->tx,generation,now);
    tv4_fragment_result rx=link->config.multiplexed?tv4_channel_rx_poll(&d->channel_rx,generation,now):tv4_fragment_rx_poll(&d->rx,generation,now);
    tv4_fragment_error tx_error=link->config.multiplexed?d->channel_tx.fragments.storage.error:d->tx.storage.error;
    tv4_fragment_error rx_error=link->config.multiplexed?d->channel_rx.fragments.storage.error:d->rx.storage.error;
    if(tx==TV4_FRAG_ERROR) {
        if(link->config.recover_fragment_timeouts &&
            (tx_error==TV4_FRAG_PROGRESS_TIMEOUT || tx_error==TV4_FRAG_TOTAL_TIMEOUT ||
             (link->config.multiplexed && (d->channel_tx.error==TV4_CHANNEL_QUEUE_PROGRESS || d->channel_tx.error==TV4_CHANNEL_QUEUE_TOTAL)))) {
            ++link->timeouts;tv4_fragment_result r=link->config.multiplexed?
                tv4_channel_tx_clear(&d->channel_tx,generation,now):tv4_fragment_tx_clear(&d->tx,generation,now);
            assert(r==TV4_FRAG_OK);
        }else ++link->sticky_failures;
    }
    if(rx==TV4_FRAG_ERROR) {
        if(link->config.recover_fragment_timeouts &&
            (rx_error==TV4_FRAG_PROGRESS_TIMEOUT || rx_error==TV4_FRAG_TOTAL_TIMEOUT)) {
            ++link->timeouts;tv4_fragment_result r=link->config.multiplexed?
                tv4_channel_rx_clear(&d->channel_rx,generation,now):tv4_fragment_rx_clear(&d->rx,generation,now);
            assert(r==TV4_FRAG_OK);
        }else ++link->sticky_failures;
    }
}
void tv4_test_link_poll(tv4_test_link *link)
{
    assert(link!=NULL && link->configured);
    for(unsigned side=0;side<2;++side) {
        tv4_test_link_direction *d=&link->direction[side];
        const tv4_completion_owner *completion=link->config.driver_dispatch?&d->dispatch.completion:&d->completion;
        bool pending=events(link,side)->count!=0 || (link->config.completion_mailbox &&
            (completion->has_pending || tv4_completion_mailbox_empty(&completion->mailbox)!=TV4_QUEUE_OK));
        if(link->config.driver_events && (!pending || *link->clock-d->event_due<=(uint32_t)INT32_MAX)) {
            tv4_frame_queue_result r=link->config.driver_dispatch?
                tv4_driver_dispatch_service(&d->dispatch,link->generation[side],*link->clock):link->config.completion_mailbox?
                tv4_completion_owner_service(&d->completion,link->generation[side],*link->clock):tv4_driver_events_service(&d->events,link->generation[side],*link->clock);
            if(pending) {
                ++link->event_services;
                if(r==TV4_QUEUE_STALE)++link->event_stales;
                else {assert(r==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED || r==TV4_QUEUE_RECLAIMED_FAILED);++link->driver_returns;}
                d->event_due=*link->clock+link->config.event_service_ms;
            }
        }
        timeout_recovery(link,side);
    }
}
static void receive_result(tv4_test_link *link,tv4_fragment_result r)
{
    ++link->delivered_frames;
    if(r==TV4_FRAG_DUPLICATE)++link->duplicate_frames;
    if(r==TV4_FRAG_ORDER)++link->order_rejects;
    if(r==TV4_FRAG_STALE)++link->stale_rejects;
    if(r==TV4_FRAG_BUSY)++link->ready_busy;
}
static void receive_frame(tv4_test_link *link,unsigned side,const tv4_fragment_frame *frame)
{
    tv4_test_link_direction *d=&link->direction[side];
    assert(!link->config.ingress_owner);
    tv4_fragment_result r=link->config.multiplexed?
        tv4_channel_rx_receive(&d->channel_rx,side+1u,frame,link->generation[side],*link->clock):
        tv4_fragment_rx_receive(&d->rx,side+1u,frame,link->generation[side],*link->clock);
    receive_result(link,r);
}
static void service_ingress(tv4_test_link *link,unsigned side)
{
    if(!link->config.frame_ingress)return;
    tv4_test_link_direction *d=&link->direction[side];
    if(*link->clock-d->ingress_due>(uint32_t)INT32_MAX)return;
    tv4_ingress_token token;tv4_ingress_result staged=link->config.ingress_owner?
        tv4_ingress_owner_stage(&d->owner,&token):tv4_frame_ingress_stage(&d->ingress,&token);
    if(staged==TV4_INGRESS_WAIT)return;
    assert(staged==TV4_INGRESS_UNCHECKED);
    /* Fresh clock read AFTER acquire/stage; never dispatch at captured time. */
    if(link->config.ingress_owner) {
        tv4_fragment_result r=tv4_ingress_owner_receive(&d->owner,token,link->generation[side],*link->clock);
        assert(r!=TV4_FRAG_ERROR && r!=TV4_FRAG_STATE);receive_result(link,r);
    }else {
        tv4_fragment_frame frame;assert(tv4_frame_ingress_check(&d->ingress,token,&frame,*link->clock)==TV4_INGRESS_FRAME);
        receive_frame(link,side,&frame);memset(&frame,0,sizeof(frame));
    }
    ++link->ingress_checks;d->ingress_due=*link->clock+link->config.ingress_service_ms;
}
static void deliver(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    if(!d->occupied || *link->clock-d->due>(uint32_t)INT32_MAX)return;
    bool cancelled=false;
    if(link->config.driver_dispatch) {
        tv4_dispatch_delivery checked;tv4_dispatch_status status=tv4_driver_dispatch_check(&d->dispatch,d->ticket,&checked,*link->clock);
        if(status!=TV4_DISPATCH_SENDABLE){assert(status==TV4_DISPATCH_REVOKED || status==TV4_DISPATCH_EXPIRED || status==TV4_DISPATCH_CLOCK);cancelled=true;}
        else assert(checked.delivery.frame.length==d->queued.length && memcmp(checked.delivery.frame.bytes,d->queued.bytes,TV4_FRAGMENT_BYTES)==0);
    }
    if(!d->drop_current && !cancelled) {
        if(link->config.frame_ingress) {
            bool empty=tv4_frame_ingress_empty(ingress_view(link,side))==TV4_INGRESS_WAIT;
            tv4_ingress_permit p={0};if(link->config.ingress_owner)assert(tv4_ingress_owner_permit(&d->owner,&p)==TV4_GATE_OPEN);
            tv4_fragment_frame captured=d->queued; /* new simulated arrival AFTER permit */
            assert((link->config.ingress_owner?
                tv4_ingress_owner_offer(&d->owner,p,side+1u,link->generation[side],&captured,*link->clock,*link->clock):
                tv4_frame_ingress_offer(&d->ingress,side+1u,link->generation[side],&captured,*link->clock,*link->clock))==TV4_INGRESS_ADMITTED);
            memset(&captured,0,sizeof(captured));++link->ingress_posts;
            if(empty)d->ingress_due=*link->clock+link->config.ingress_service_ms;
        }else receive_frame(link,side,&d->queued);
    }
    unsigned delivered=link->config.frame_ingress?link->ingress_posts:link->delivered_frames;
    if(!d->drop_current && !cancelled && !d->repeated && link->config.duplicate_every>0 && delivered%link->config.duplicate_every==0) {
        d->repeated=true;d->due=*link->clock+link->config.delivery_ms;
    }else {
        if(link->config.egress) {
            if(link->config.driver_events) {
                tv4_driver_event event={d->ticket,TV4_FRAME_SENT_LOCAL};
                if(cancelled)event.result=TV4_FRAME_CANCELLED_LOCAL;
                unsigned copies=link->frames%17u==0?2u:1u;
                if(link->config.driver_dispatch) {
                    assert(tv4_driver_dispatch_report(&d->dispatch,&event)==TV4_QUEUE_ADMITTED);++link->event_posts;
                    if(copies==2){assert(tv4_driver_dispatch_report(&d->dispatch,&event)==TV4_QUEUE_STALE);++link->dispatch_rejected_callbacks;}
                    copies=1;
                }else for(unsigned n=0;n<copies;++n) {
                    assert((link->config.completion_mailbox?tv4_completion_owner_post(&d->completion,&event):tv4_driver_events_post(&d->events,&event))==TV4_QUEUE_ADMITTED);++link->event_posts;
                }
                memset(&event,0,sizeof(event));d->event_due=*link->clock+link->config.event_service_ms;
                /* HOST serialized fixture: service cannot interleave these
                 * posts, actual capacity2 admissions prove pending copies. */
                if(copies>link->event_max_pending)link->event_max_pending=copies;
            }else {
                tv4_frame_queue_result returned=tv4_update_egress_complete(&d->egress,d->ticket,TV4_FRAME_SENT_LOCAL,*link->clock);
                assert(returned==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED || returned==TV4_QUEUE_RECLAIMED_FAILED);++link->driver_returns;
            }
        }
        d->occupied=false;d->repeated=false;d->drop_current=false;
        memset(&d->queued,0,sizeof(d->queued));memset(&d->ticket,0,sizeof(d->ticket));
    }
}
static void queue_next(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    if(link->config.egress) {
        tv4_update_egress *owner=egress(link,side);
        tv4_frame_queue_result pumped=link->config.driver_dispatch?
            tv4_driver_dispatch_pump(&d->dispatch,link->generation[side],*link->clock):link->config.completion_mailbox?
            tv4_completion_owner_pump(&d->completion,link->generation[side],*link->clock):link->config.driver_events?
            tv4_driver_events_pump(&d->events,link->generation[side],*link->clock):tv4_update_egress_pump(owner,link->generation[side],*link->clock);
        if(pumped==TV4_QUEUE_ADMITTED)++link->driver_admissions;
        else if(pumped==TV4_QUEUE_BUSY)++link->queue_busy;
        else if(pumped==TV4_QUEUE_ERROR)return;
        else assert(pumped==TV4_QUEUE_WAIT);
        if(owner->queue.count>link->driver_max_pending)link->driver_max_pending=owner->queue.count;
        if(d->occupied){++link->driver_held_ticks;return;}
        tv4_frame_delivery out;
        tv4_frame_queue_result claimed;
        if(link->config.driver_dispatch) {
            tv4_frame_queue_result published=tv4_driver_dispatch_publish(&d->dispatch,link->generation[side],*link->clock);
            if(published==TV4_QUEUE_BUSY){++link->event_held_ticks;return;}
            if(published==TV4_QUEUE_WAIT)return;
            assert(published==TV4_QUEUE_ADMITTED);++link->dispatch_publications;
            tv4_dispatch_delivery token,checked;
            assert(tv4_driver_dispatch_take(&d->dispatch,&token)==TV4_DISPATCH_UNCHECKED);
            assert(tv4_driver_dispatch_check(&d->dispatch,token.delivery.ticket,&checked,*link->clock)==TV4_DISPATCH_SENDABLE);
            out=checked.delivery;claimed=TV4_QUEUE_FRAME;
        }else claimed=link->config.completion_mailbox?
            tv4_completion_owner_claim(&d->completion,&out,link->generation[side],*link->clock):link->config.driver_events?
            tv4_driver_events_claim(&d->events,&out,link->generation[side],*link->clock):tv4_update_egress_claim(owner,&out,link->generation[side],*link->clock);
        if(link->config.driver_events && claimed==TV4_QUEUE_BUSY){++link->event_held_ticks;return;}
        if(claimed==TV4_QUEUE_WAIT)return;
        assert(claimed==TV4_QUEUE_FRAME && out.route==side+1u);
        d->queued=out.frame;d->ticket=out.ticket;d->occupied=true;d->repeated=false;
        ++link->frames;++link->driver_claims;
        d->drop_current=d->drop_message && d->index==link->config.drop_index;
        if(d->drop_current) {
            ++link->dropped_frames;link->dropped_kinds=(uint16_t)(link->dropped_kinds | (1u<<d->kind));
        }
        ++d->index;
        uint32_t delay=link->config.hold_every>0 && link->frames%link->config.hold_every==0?
            link->config.held_delivery_ms:link->config.delivery_ms;
        d->due=*link->clock+delay;return;
    }
    if(link->config.multiplexed?!d->channel_tx.active:d->tx.storage.state!=TV4_FRAG_SENDING)return;
    tv4_fragment_frame frame;
    assert((link->config.multiplexed?tv4_channel_tx_peek(&d->channel_tx,&frame,link->generation[side],*link->clock):
        tv4_fragment_tx_peek(&d->tx,&frame,link->generation[side],*link->clock))==TV4_FRAG_FRAME);
    if(d->occupied) {
        tv4_fragment_frame again;
        assert((link->config.multiplexed?tv4_channel_tx_peek(&d->channel_tx,&again,link->generation[side],*link->clock):
            tv4_fragment_tx_peek(&d->tx,&again,link->generation[side],*link->clock))==TV4_FRAG_FRAME);
        assert(memcmp(&frame,&again,sizeof(frame))==0);++link->repeated_peeks;return;
    }
    ++link->frames;
    if(d->drop_message && d->index==link->config.drop_index) {
        ++link->dropped_frames;link->dropped_kinds=(uint16_t)(link->dropped_kinds | (1u<<(unsigned)d->kind));
    }else {
        d->queued=frame;d->occupied=true;d->repeated=false;
        uint32_t delay=link->config.hold_every>0 && link->frames%link->config.hold_every==0?
            link->config.held_delivery_ms:link->config.delivery_ms;
        d->due=*link->clock+delay;
    }
    tv4_fragment_result r=link->config.multiplexed?
        tv4_channel_tx_commit(&d->channel_tx,&frame,link->generation[side],*link->clock):
        tv4_fragment_tx_commit(&d->tx,&frame,link->generation[side],*link->clock);
    assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);++d->index;
}
void tv4_test_link_step(tv4_test_link *link)
{
    assert(link!=NULL && link->configured);++*link->clock;
    tv4_test_link_poll(link);
    for(unsigned side=0;side<2;++side){service_ingress(link,side);deliver(link,side);queue_next(link,side);}
}
tv4_fragment_result tv4_test_link_take(tv4_test_link *link, unsigned direction, tv4_wire_message *out)
{
    assert(link!=NULL && link->configured && direction<2 && out!=NULL && !link->config.raw_setup);
    tv4_fragment_result r;
    if(link->config.multiplexed) {
        memset(out,0,sizeof(*out));tv4_channel_message m;
        tv4_channel_result c=link->config.ingress_owner?tv4_ingress_owner_take(&link->direction[direction].owner,&m,link->generation[direction],*link->clock):tv4_channel_rx_take(&link->direction[direction].channel_rx,&m,link->generation[direction],*link->clock);
        r=channel_result(c);
        if(c==TV4_CHANNEL_TAKEN_UNVERIFIED){assert(m.kind==TV4_CHANNEL_WIRE);*out=m.body.wire;r=TV4_FRAG_TAKEN;}
    }else r=tv4_fragment_rx_take(&link->direction[direction].rx,out,link->generation[direction],*link->clock);
    if(r==TV4_FRAG_TAKEN){++link->takes;++link->deliveries[(unsigned)out->kind];}
    return r;
}

tv4_fragment_result tv4_test_link_send_setup(tv4_test_link *link,unsigned direction,const tv4_metadata_setup_message *message)
{
    assert(link!=NULL && link->configured && direction<2 && message!=NULL && (link->config.raw_setup || link->config.multiplexed));
    if(!tv4_test_link_can_send(link,direction)){++link->queue_busy;return TV4_FRAG_BUSY;}
    tv4_test_link_direction *d=&link->direction[direction];tv4_fragment_result r;
    if(link->config.multiplexed) {
        tv4_channel_message m={.kind=TV4_CHANNEL_MS,.body.setup=*message};
        r=channel_result(link->config.driver_dispatch?
            tv4_driver_dispatch_begin(&d->dispatch,&m,link->generation[direction],*link->clock):link->config.completion_mailbox?
            tv4_completion_owner_begin(&d->completion,&m,link->generation[direction],*link->clock):link->config.driver_events?
            tv4_driver_events_begin(&d->events,&m,link->generation[direction],*link->clock):link->config.egress?
            tv4_update_egress_begin(&d->egress,&m,link->generation[direction],*link->clock):
            tv4_channel_tx_begin(&d->channel_tx,&m,link->generation[direction],*link->clock));
    }else {
        uint8_t raw[TV4_SETUP_MESSAGE_BYTES];size_t n=0;
        assert(tv4_metadata_setup_encode(message,raw,sizeof(raw),&n)==TV4_SETUP_OK);
        r=tv4_fragment_tx_begin_raw(&d->tx,raw,n,link->generation[direction],*link->clock);
    }
    if(r!=TV4_FRAG_OK)return r;
    d->kind=(unsigned)message->kind+(link->config.multiplexed?8u:0u);d->index=0;unsigned bit=1u<<d->kind;
    d->drop_message=(link->config.drop_kinds&bit)!=0 &&
        (link->config.drop_every_message || (link->dropped_kinds&bit)==0);
    ++link->attempts[d->kind];return r;
}
tv4_fragment_result tv4_test_link_take_setup(tv4_test_link *link,unsigned direction,tv4_metadata_setup_message *out)
{
    assert(link!=NULL && link->configured && direction<2 && out!=NULL && (link->config.raw_setup || link->config.multiplexed));
    if(link->config.multiplexed) {
        memset(out,0,sizeof(*out));tv4_channel_message m;
        tv4_channel_result r=link->config.ingress_owner?tv4_ingress_owner_take(&link->direction[direction].owner,&m,link->generation[direction],*link->clock):tv4_channel_rx_take(&link->direction[direction].channel_rx,&m,link->generation[direction],*link->clock);
        if(r!=TV4_CHANNEL_TAKEN_UNVERIFIED)return channel_result(r);
        assert(m.kind==TV4_CHANNEL_MS);*out=m.body.setup;
        ++link->takes;++link->deliveries[8u+(unsigned)out->kind];return TV4_FRAG_RAW_TAKEN_UNCHECKED;
    }
    uint8_t raw[TV4_WIRE_MESSAGE_MAX];size_t n=0;
    tv4_fragment_result r=tv4_fragment_rx_take_raw(&link->direction[direction].rx,raw,sizeof(raw),&n,link->generation[direction],*link->clock);
    if(r==TV4_FRAG_RAW_TAKEN_UNCHECKED) {
        assert(tv4_metadata_setup_decode(raw,n,out)==TV4_SETUP_OK);
        ++link->takes;++link->deliveries[(unsigned)out->kind];
    }else memset(out,0,sizeof(*out));
    return r;
}
