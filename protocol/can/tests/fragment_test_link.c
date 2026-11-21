#include "fragment_test_link.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

void tv4_test_link_init(tv4_test_link *link, uint32_t *clock, const tv4_test_link_config *config)
{
    assert(link!=NULL && clock!=NULL && config!=NULL && !link->configured);
    assert(config->delivery_ms>0 && config->delivery_ms<=(uint32_t)INT32_MAX);
    assert(config->held_delivery_ms>=config->delivery_ms && config->held_delivery_ms<=(uint32_t)INT32_MAX);
    memset(link,0,sizeof(*link));link->config=*config;link->clock=clock;
    for(unsigned i=0;i<2;++i) {
        link->generation[i]=100u+i;
        assert(tv4_fragment_tx_init(&link->direction[i].tx,&config->fragments,i+1u,link->generation[i],0,*clock)==TV4_FRAG_OK);
        assert(tv4_fragment_rx_init(&link->direction[i].rx,&config->fragments,i+1u,link->generation[i],0,*clock)==TV4_FRAG_OK);
    }
    link->configured=true;
}
bool tv4_test_link_can_send(const tv4_test_link *link, unsigned direction)
{
    assert(link!=NULL && link->configured && direction<2);
    const tv4_test_link_direction *d=&link->direction[direction];
    return !d->occupied && d->tx.storage.state==TV4_FRAG_IDLE;
}
tv4_fragment_result tv4_test_link_send(tv4_test_link *link, unsigned direction, const tv4_wire_message *message)
{
    assert(link!=NULL && link->configured && direction<2 && message!=NULL);
    if(!tv4_test_link_can_send(link,direction)){++link->queue_busy;return TV4_FRAG_BUSY;}
    tv4_test_link_direction *d=&link->direction[direction];
    tv4_fragment_result r=tv4_fragment_tx_begin(&d->tx,message,link->generation[direction],*link->clock);
    if(r!=TV4_FRAG_OK)return r;
    assert(message->kind>=TV4_WIRE_REQUEST && message->kind<=TV4_WIRE_OUTCOME_ACK);
    d->kind=message->kind;d->index=0;
    unsigned bit=1u<<(unsigned)message->kind;
    d->drop_message=(link->config.drop_kinds&bit)!=0 &&
        (link->config.drop_every_message || (link->dropped_kinds&bit)==0);
    ++link->attempts[(unsigned)message->kind];return r;
}
static void timeout_recovery(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    uint64_t generation=link->generation[side];uint32_t now=*link->clock;
    tv4_fragment_result tx=tv4_fragment_tx_poll(&d->tx,generation,now);
    tv4_fragment_result rx=tv4_fragment_rx_poll(&d->rx,generation,now);
    if(tx==TV4_FRAG_ERROR) {
        if(link->config.recover_fragment_timeouts &&
            (d->tx.storage.error==TV4_FRAG_PROGRESS_TIMEOUT || d->tx.storage.error==TV4_FRAG_TOTAL_TIMEOUT)) {
            ++link->timeouts;assert(tv4_fragment_tx_clear(&d->tx,generation,now)==TV4_FRAG_OK);
        }else ++link->sticky_failures;
    }
    if(rx==TV4_FRAG_ERROR) {
        if(link->config.recover_fragment_timeouts &&
            (d->rx.storage.error==TV4_FRAG_PROGRESS_TIMEOUT || d->rx.storage.error==TV4_FRAG_TOTAL_TIMEOUT)) {
            ++link->timeouts;assert(tv4_fragment_rx_clear(&d->rx,generation,now)==TV4_FRAG_OK);
        }else ++link->sticky_failures;
    }
}
void tv4_test_link_poll(tv4_test_link *link)
{ assert(link!=NULL && link->configured);for(unsigned side=0;side<2;++side)timeout_recovery(link,side); }
static void deliver(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    if(!d->occupied || *link->clock-d->due>(uint32_t)INT32_MAX)return;
    tv4_fragment_result r=tv4_fragment_rx_receive(&d->rx,side+1u,&d->queued,link->generation[side],*link->clock);
    ++link->delivered_frames;
    if(r==TV4_FRAG_DUPLICATE)++link->duplicate_frames;
    if(r==TV4_FRAG_ORDER)++link->order_rejects;
    if(r==TV4_FRAG_STALE)++link->stale_rejects;
    if(r==TV4_FRAG_BUSY)++link->ready_busy;
    if(!d->repeated && link->config.duplicate_every>0 && link->delivered_frames%link->config.duplicate_every==0) {
        d->repeated=true;d->due=*link->clock+link->config.delivery_ms;
    }else { d->occupied=false;d->repeated=false;memset(&d->queued,0,sizeof(d->queued)); }
}
static void queue_next(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    if(d->tx.storage.state!=TV4_FRAG_SENDING)return;
    tv4_fragment_frame frame;
    assert(tv4_fragment_tx_peek(&d->tx,&frame,link->generation[side],*link->clock)==TV4_FRAG_FRAME);
    if(d->occupied) {
        tv4_fragment_frame again;
        assert(tv4_fragment_tx_peek(&d->tx,&again,link->generation[side],*link->clock)==TV4_FRAG_FRAME);
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
    tv4_fragment_result r=tv4_fragment_tx_commit(&d->tx,&frame,link->generation[side],*link->clock);
    assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);++d->index;
}
void tv4_test_link_step(tv4_test_link *link)
{
    assert(link!=NULL && link->configured);++*link->clock;
    tv4_test_link_poll(link);
    for(unsigned side=0;side<2;++side){deliver(link,side);queue_next(link,side);}
}
tv4_fragment_result tv4_test_link_take(tv4_test_link *link, unsigned direction, tv4_wire_message *out)
{
    assert(link!=NULL && link->configured && direction<2 && out!=NULL);
    tv4_fragment_result r=tv4_fragment_rx_take(&link->direction[direction].rx,out,link->generation[direction],*link->clock);
    if(r==TV4_FRAG_TAKEN){++link->takes;++link->deliveries[(unsigned)out->kind];}
    return r;
}
