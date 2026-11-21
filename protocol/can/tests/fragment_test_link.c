#include "fragment_test_link.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

void tv4_test_link_init(tv4_test_link *link, uint32_t *clock, const tv4_test_link_config *config)
{
    assert(link!=NULL && clock!=NULL && config!=NULL && !link->configured);
    assert(!(config->raw_setup && config->multiplexed));
    assert(config->delivery_ms>0 && config->delivery_ms<=(uint32_t)INT32_MAX);
    assert(config->held_delivery_ms>=config->delivery_ms && config->held_delivery_ms<=(uint32_t)INT32_MAX);
    memset(link,0,sizeof(*link));link->config=*config;link->clock=clock;
    for(unsigned i=0;i<2;++i) {
        link->generation[i]=100u+i;
        if(config->multiplexed) {
            assert(tv4_channel_tx_init(&link->direction[i].channel_tx,&config->fragments,i+1u,link->generation[i],0,*clock));
            assert(tv4_channel_rx_init(&link->direction[i].channel_rx,&config->fragments,i+1u,link->generation[i],0,*clock));
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
            if(d->channel_rx.fragments.storage.state!=TV4_FRAG_IDLE ||
                d->channel_tx.error!=TV4_CHANNEL_NO_ERROR || d->channel_rx.error!=TV4_CHANNEL_NO_ERROR)return false;
        }else if(d->rx.storage.state!=TV4_FRAG_IDLE)return false;
    }
    return true;
}
void tv4_test_link_advance(tv4_test_link *link,tv4_channel_phase phase,tv4_update_id id)
{
    assert(link!=NULL && link->configured && link->config.multiplexed && tv4_test_link_idle(link));
    for(unsigned i=0;i<2;++i) {
        assert(tv4_channel_tx_advance(&link->direction[i].channel_tx,phase,id,link->generation[i],*link->clock)==TV4_CHANNEL_OK);
        assert(tv4_channel_rx_advance(&link->direction[i].channel_rx,phase,id,link->generation[i],*link->clock)==TV4_CHANNEL_OK);
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
        r=channel_result(tv4_channel_tx_begin(&d->channel_tx,&m,link->generation[direction],*link->clock));
    }else r=tv4_fragment_tx_begin(&d->tx,message,link->generation[direction],*link->clock);
    if(r!=TV4_FRAG_OK)return r;
    assert(message->kind>=TV4_WIRE_REQUEST && message->kind<=TV4_WIRE_OUTCOME_ACK);
    d->kind=(unsigned)message->kind;d->index=0;
    unsigned bit=1u<<(unsigned)message->kind;
    d->drop_message=(link->config.drop_kinds&bit)!=0 &&
        (link->config.drop_every_message || (link->dropped_kinds&bit)==0);
    ++link->attempts[(unsigned)message->kind];return r;
}
static void timeout_recovery(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    uint64_t generation=link->generation[side];uint32_t now=*link->clock;
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
{ assert(link!=NULL && link->configured);for(unsigned side=0;side<2;++side)timeout_recovery(link,side); }
static void deliver(tv4_test_link *link, unsigned side)
{
    tv4_test_link_direction *d=&link->direction[side];
    if(!d->occupied || *link->clock-d->due>(uint32_t)INT32_MAX)return;
    tv4_fragment_result r=link->config.multiplexed?
        tv4_channel_rx_receive(&d->channel_rx,side+1u,&d->queued,link->generation[side],*link->clock):
        tv4_fragment_rx_receive(&d->rx,side+1u,&d->queued,link->generation[side],*link->clock);
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
    for(unsigned side=0;side<2;++side){deliver(link,side);queue_next(link,side);}
}
tv4_fragment_result tv4_test_link_take(tv4_test_link *link, unsigned direction, tv4_wire_message *out)
{
    assert(link!=NULL && link->configured && direction<2 && out!=NULL && !link->config.raw_setup);
    tv4_fragment_result r;
    if(link->config.multiplexed) {
        memset(out,0,sizeof(*out));tv4_channel_message m;
        tv4_channel_result c=tv4_channel_rx_take(&link->direction[direction].channel_rx,&m,link->generation[direction],*link->clock);
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
        r=channel_result(tv4_channel_tx_begin(&d->channel_tx,&m,link->generation[direction],*link->clock));
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
        tv4_channel_result r=tv4_channel_rx_take(&link->direction[direction].channel_rx,&m,link->generation[direction],*link->clock);
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
