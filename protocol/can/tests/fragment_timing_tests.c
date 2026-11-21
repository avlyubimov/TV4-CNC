#include "fragment_test_link.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static tv4_test_link_config config(void)
{
    return (tv4_test_link_config){.fragments={8,128,160,TV4_FRAGMENT_VERSION},
        .delivery_ms=1,.held_delivery_ms=3,.hold_every=11,.duplicate_every=17,
        .recover_fragment_timeouts=true};
}
static tv4_wire_message ack_message(void)
{ return (tv4_wire_message){.kind=TV4_WIRE_DATA_ACK,.body.ack={.id={7,9},.next_sequence=1,.next_offset=256}}; }
static unsigned scenario_count,total_frames,total_drops,total_timeouts,total_peeks;
static void data_case(uint16_t block,uint16_t mask,uint16_t drop_index,uint32_t start,
    bool permanent_loss,bool hold_ack,bool expect_success)
{
    uint32_t now=start;tv4_test_link link={0};tv4_test_link_config cfg=config();
    cfg.drop_kinds=mask;cfg.drop_index=drop_index;cfg.drop_every_message=permanent_loss;
    if(hold_ack)cfg.fragments.total_timeout_ms=1000;
    tv4_test_link_init(&link,&now,&cfg);
    tv4_update_limits limits={.max_image_bytes=1024,.ack_timeout_ms=200,.progress_timeout_ms=3000,
        .total_timeout_ms=10000,.block_bytes=block,.retry_limit=4};
    if(hold_ack){limits.ack_timeout_ms=100;limits.progress_timeout_ms=300;limits.total_timeout_ms=300;}
    tv4_update_descriptor image={.id={7,9},.image_bytes=256};
    tv4_update_tx tx;tv4_update_rx rx;
    assert(tv4_update_tx_init(&tx,limits,7,now) && tv4_update_rx_init(&rx,limits,7,now));
    assert(tv4_update_tx_begin(&tx,&image,now)==TV4_UPDATE_OK && tv4_update_rx_begin(&rx,&image,now)==TV4_UPDATE_OK);
    uint8_t source[256],stored[256]={0};for(unsigned i=0;i<256;++i)source[i]=(uint8_t)(i*19u+3u);
    unsigned writes=0;tv4_wire_message pending_ack={0};bool ack_pending=false;
    unsigned ticks;
    for(ticks=0;ticks<12000;++ticks) {
        tv4_update_result tr=tv4_update_tx_poll(&tx,now),rr=tv4_update_rx_poll(&rx,now);
        if(tr==TV4_UPDATE_ERROR || rr==TV4_UPDATE_ERROR)break;
        tv4_wire_message incoming;
        if(tv4_test_link_take(&link,0,&incoming)==TV4_FRAG_TAKEN) {
            assert(incoming.kind==TV4_WIRE_DATA);tv4_update_ack ack;
            tv4_update_result r=tv4_update_rx_receive(&rx,&incoming.body.data,&ack,now);
            if(r==TV4_UPDATE_PENDING) {
                tv4_update_block pending;assert(tv4_update_rx_pending(&rx,&pending,now)==TV4_UPDATE_PENDING);
                memcpy(stored+pending.offset,pending.bytes,pending.length);++writes;
                assert(tv4_update_rx_consume(&rx,pending.id,pending.sequence,true,&ack,now)==TV4_UPDATE_ACK);
            }else assert(r==TV4_UPDATE_ACK);
            pending_ack=(tv4_wire_message){.kind=TV4_WIRE_DATA_ACK,.body.ack=ack};ack_pending=true;
        }
        if(!hold_ack && tv4_test_link_take(&link,1,&incoming)==TV4_FRAG_TAKEN) {
            assert(incoming.kind==TV4_WIRE_DATA_ACK);
            tv4_update_result r=tv4_update_tx_ack(&tx,&incoming.body.ack,now);
            assert(r==TV4_UPDATE_OK || r==TV4_UPDATE_STALE_ACK);
        }
        if(tx.context.state==TV4_UPDATE_COMPLETE_UNVERIFIED)break;
        if(ack_pending && tv4_test_link_can_send(&link,1)) {
            assert(tv4_test_link_send(&link,1,&pending_ack)==TV4_FRAG_OK);ack_pending=false;
        }
        if(tv4_test_link_can_send(&link,0)) {
            if(tx.context.state==TV4_UPDATE_NEED_DATA) {
                size_t n=sizeof(source)-tx.context.next_offset;if(n>block)n=block;
                assert(tv4_update_tx_load(&tx,source+tx.context.next_offset,n,now)==TV4_UPDATE_OK);
            }
            tv4_wire_message outgoing={.kind=TV4_WIRE_DATA};
            tv4_update_result r=tv4_update_tx_next(&tx,&outgoing.body.data,now);
            if(r==TV4_UPDATE_OK)assert(tv4_test_link_send(&link,0,&outgoing)==TV4_FRAG_OK);
            else if(r==TV4_UPDATE_ERROR){assert(!expect_success);break;}
            else assert(r==TV4_UPDATE_WAIT);
        }
        assert(tx.context.started_ms==start && rx.context.started_ms==start);
        tv4_test_link_step(&link);
    }
    if(ticks==12000 || link.sticky_failures!=0 ||
        (expect_success && tx.context.state!=TV4_UPDATE_COMPLETE_UNVERIFIED))
        fprintf(stderr,"case block=%u mask=%u index=%u age=%u tx=%u/%u rx=%u/%u sticky=%u timeouts=%u\n",
            block,mask,drop_index,now-start,(unsigned)tx.context.state,(unsigned)tx.context.error,
            (unsigned)rx.context.state,(unsigned)rx.context.error,link.sticky_failures,link.timeouts);
    assert(ticks<12000 && link.sticky_failures==0);
    if(expect_success) {
        assert(tx.context.state==TV4_UPDATE_COMPLETE_UNVERIFIED && rx.context.state==TV4_UPDATE_COMPLETE_UNVERIFIED);
        assert(memcmp(source,stored,sizeof(source))==0 && writes==(256u+block-1u)/block);
        assert(link.dropped_kinds==mask && link.repeated_peeks>0);
        if(mask!=0){assert(link.attempts[5]>writes);if(drop_index>0)assert(link.timeouts>0);}
    }else {
        assert(tx.context.state==TV4_UPDATE_FAILED);
        if(hold_ack) {
            assert(tx.context.error==TV4_UPDATE_ERROR_TOTAL_TIMEOUT && now-start==300);
            tv4_wire_message late;assert(tv4_test_link_take(&link,1,&late)==TV4_FRAG_TAKEN);
            assert(late.kind==TV4_WIRE_DATA_ACK);
            assert(tv4_update_tx_ack(&tx,&late.body.ack,now)==TV4_UPDATE_ERROR && tx.context.state==TV4_UPDATE_FAILED);
            assert(writes==1); /* delayed ACK does not duplicate consumer work */
        }else assert(tx.context.error==TV4_UPDATE_ERROR_RETRIES && writes==0);
    }
    ++scenario_count;total_frames+=link.frames;total_drops+=link.dropped_frames;
    total_timeouts+=link.timeouts;total_peeks+=link.repeated_peeks;
}
static void loss_retry_and_wrap(void)
{
    for(unsigned block=1;block<=256;block=block==1?31:block==31?256:257) {
        data_case((uint16_t)block,0,0,10,false,false,true);
        for(unsigned index=0;index<3;++index) {
            data_case((uint16_t)block,1u<<5,(uint16_t)index,10,false,false,true);
            data_case((uint16_t)block,1u<<6,(uint16_t)index,10,false,false,true);
            data_case((uint16_t)block,(1u<<5)|(1u<<6),(uint16_t)index,UINT32_MAX-25u,false,false,true);
        }
    }
    data_case(256,1u<<5,71,10,false,false,true); /* last max-DATA fragment */
    data_case(256,1u<<6,5,10,false,false,true);  /* last ACK fragment */
    data_case(256,1u<<5,1,10,true,false,false);
    data_case(256,0,0,UINT32_MAX-100u,false,true,false);
    puts("PASS shared clock: whole-message retries, missing START/middle/final, lost ACK, bounded queue, exact consumer writes, wrap and retry exhaustion");
}
static void ready_and_queue_deadlines(void)
{
    uint32_t now=0;tv4_test_link link={0};tv4_test_link_config cfg=config();cfg.recover_fragment_timeouts=false;
    tv4_test_link_init(&link,&now,&cfg);tv4_wire_message m=ack_message(),out;
    assert(tv4_test_link_send(&link,0,&m)==TV4_FRAG_OK);
    assert(tv4_test_link_send(&link,0,&m)==TV4_FRAG_BUSY && link.direction[0].tx.storage.counter==1);
    while(link.direction[0].rx.storage.state!=TV4_FRAG_READY_BYTES_UNCHECKED){assert(now<30);tv4_test_link_step(&link);}
    uint32_t started=link.direction[0].rx.storage.started;
    while(now-started<127)tv4_test_link_step(&link);
    assert(link.direction[0].rx.storage.state==TV4_FRAG_READY_BYTES_UNCHECKED);
    tv4_test_link_step(&link);
    assert(link.direction[0].rx.storage.error==TV4_FRAG_TOTAL_TIMEOUT);
    memset(&out,0xa5,sizeof(out));assert(tv4_test_link_take(&link,0,&out)==TV4_FRAG_ERROR && out.kind==TV4_WIRE_NONE);
    assert(tv4_fragment_rx_clear(&link.direction[0].rx,100,now)==TV4_FRAG_OK && link.direction[0].rx.storage.counter==1);
    assert(tv4_test_link_send(&link,0,&m)==TV4_FRAG_OK && link.direction[0].tx.storage.counter==2);

    uint32_t stalled_now=0;tv4_test_link stalled={0};cfg.delivery_ms=32;cfg.held_delivery_ms=32;
    tv4_test_link_init(&stalled,&stalled_now,&cfg);assert(tv4_test_link_send(&stalled,0,&m)==TV4_FRAG_OK);
    for(unsigned i=0;i<8;++i)tv4_test_link_step(&stalled);
    assert(stalled.direction[0].tx.storage.state==TV4_FRAG_SENDING);
    tv4_test_link_step(&stalled);
    assert(stalled.direction[0].tx.storage.error==TV4_FRAG_PROGRESS_TIMEOUT && stalled.repeated_peeks>0);
    assert(stalled.direction[0].occupied && stalled.direction[0].tx.storage.counter==1);
    assert(tv4_fragment_tx_clear(&stalled.direction[0].tx,100,stalled_now)==TV4_FRAG_OK);
    assert(tv4_test_link_send(&stalled,0,&m)==TV4_FRAG_BUSY && stalled.direction[0].tx.storage.counter==1);
    puts("PASS READY exact total deadline, queue stall exact progress deadline, peek/busy cannot reset time/counters");
}
static void context_and_replay_boundary(void)
{
    uint32_t now=0;tv4_test_link link={0};tv4_test_link_config cfg=config();
    tv4_test_link_init(&link,&now,&cfg);tv4_wire_message m=ack_message();
    assert(tv4_test_link_send(&link,0,&m)==TV4_FRAG_OK);tv4_test_link_step(&link);
    tv4_fragment_frame old_start=link.direction[0].queued;
    ++link.generation[0];tv4_test_link_poll(&link);
    assert(link.direction[0].tx.storage.error==TV4_FRAG_CONTEXT && link.direction[0].rx.storage.error==TV4_FRAG_CONTEXT);
    assert(tv4_fragment_tx_clear(&link.direction[0].tx,100,now)==TV4_FRAG_ERROR);
    assert(tv4_fragment_rx_clear(&link.direction[0].rx,100,now)==TV4_FRAG_ERROR);
    /* Demonstrate the known external identity blocker: a new local object/
     * generation alone cannot distinguish an old wire START after reboot. */
    tv4_fragment_rx fresh={0};assert(tv4_fragment_rx_init(&fresh,&cfg.fragments,1,999,0,now)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_receive(&fresh,1,&old_start,999,now)==TV4_FRAG_PENDING);
    uint32_t other_now=100;tv4_test_link backward={0};tv4_test_link_init(&backward,&other_now,&cfg);
    assert(tv4_test_link_send(&backward,0,&m)==TV4_FRAG_OK);--other_now;tv4_test_link_poll(&backward);
    assert(backward.direction[0].tx.storage.error==TV4_FRAG_CLOCK && backward.direction[0].rx.storage.error==TV4_FRAG_CLOCK);
    assert(tv4_fragment_rx_clear(&backward.direction[0].rx,100,100)==TV4_FRAG_ERROR);
    puts("PASS sticky context/backward-clock cancellation; old START accepted by fresh generation proves reboot replay isolation is NOT provided");
}
static void negotiation_deadline_composition(void)
{
    uint32_t now=0;tv4_test_link link={0};tv4_test_link_config cfg=config();
    cfg.delivery_ms=3;cfg.held_delivery_ms=3;cfg.fragments.total_timeout_ms=200;
    tv4_test_link_init(&link,&now,&cfg);
    tv4_neg_pool pools[2]={{0},{0}};tv4_negotiation n[2]={{0},{0}};uint8_t tag[32]={1};
    assert(tv4_neg_pool_init(&pools[0],7,8,0) && tv4_neg_pool_init(&pools[1],8,7,0));
    tv4_neg_limits limits={.max_image_bytes=1024,.retry_ms=20,.total_ms=50,.block_ceiling=256,.send_limit=4};
    assert(tv4_neg_init(&n[0],&pools[0],TV4_NEG_REQUESTER,limits,tag,1,now));
    assert(tv4_neg_init(&n[1],&pools[1],TV4_NEG_RESPONDER,limits,tag,2,now));
    tv4_neg_image image={.image_bytes=256};assert(tv4_neg_begin(&n[0],&image,1,now)==TV4_NEG_OK);
    tv4_wire_message message={.kind=TV4_WIRE_REQUEST};
    assert(tv4_neg_next(&n[0],&message.body.negotiation,1,now)==TV4_NEG_SENT);
    assert(tv4_test_link_send(&link,0,&message)==TV4_FRAG_OK);
    while(now<49){tv4_test_link_step(&link);assert(tv4_neg_poll(&n[0],1,now)==TV4_NEG_OK);}
    tv4_test_link_step(&link);assert(now==50 && tv4_neg_poll(&n[0],1,now)==TV4_NEG_ERROR);
    assert(n[0].state==TV4_NEG_EXPIRED && n[0].error==TV4_NEG_ERROR_TIMEOUT && n[0].started_ms==0);
    while(tv4_test_link_take(&link,0,&message)!=TV4_FRAG_TAKEN){assert(now<200);tv4_test_link_step(&link);}
    assert(tv4_neg_receive(&n[1],&message.body.negotiation,2,now)==TV4_NEG_OK);
    message.kind=TV4_WIRE_OFFER;assert(tv4_neg_next(&n[1],&message.body.negotiation,2,now)==TV4_NEG_SENT);
    assert(tv4_test_link_send(&link,1,&message)==TV4_FRAG_OK);
    while(tv4_test_link_take(&link,1,&message)!=TV4_FRAG_TAKEN){assert(now<400);tv4_test_link_step(&link);}
    assert(tv4_neg_receive(&n[0],&message.body.negotiation,1,now)==TV4_NEG_ERROR);
    assert(n[0].state==TV4_NEG_EXPIRED && n[0].started_ms==0 && pools[0].last_issued==1);
    puts("PASS fragment progress/late valid OFFER cannot extend or revive exact50ms negotiation lifetime");
}
int main(void)
{
    loss_retry_and_wrap();ready_and_queue_deadlines();context_and_replay_boundary();negotiation_deadline_composition();
    printf("PASS timing data scenarios=%u frames=%u dropped=%u timeout-clears=%u repeated-peeks=%u; HOST simulation, no bus/CPU/default timing claim\n",
        scenario_count,total_frames,total_drops,total_timeouts,total_peeks);return 0;
}
