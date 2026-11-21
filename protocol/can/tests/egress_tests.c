#include "update_egress.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config fragments={20,1000,200,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static void init(tv4_update_egress *s,unsigned capacity,uint32_t now)
{assert(tv4_update_egress_init(s,&fragments,(tv4_frame_queue_limits){40,(uint8_t)capacity},7,11,0,0,now));}
static tv4_channel_message request(void)
{
    return (tv4_channel_message){.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,
        .body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};
}
static void real_channel_and_pending_driver(void)
{
    unsigned frames=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_update_egress s={0};uint32_t now=0;init(&s,capacity,now);
        tv4_channel_rx rx={0};assert(tv4_channel_rx_init(&rx,&fragments,7,22,0,now));
        tv4_channel_message m=request();assert(tv4_update_egress_begin(&s,&m,11,now)==TV4_CHANNEL_OK);
        while(s.channel.active || s.queue.count!=0) {
            tv4_frame_queue_result p=tv4_update_egress_pump(&s,11,now);assert(p==TV4_QUEUE_ADMITTED || p==TV4_QUEUE_WAIT || p==TV4_QUEUE_BUSY);
            tv4_frame_delivery d;assert(tv4_update_egress_claim(&s,&d,11,now)==TV4_QUEUE_FRAME);
            uint32_t started=s.queue.entries[s.queue.head].admitted;
            ++now;assert(tv4_update_egress_poll(&s,11,now)==TV4_QUEUE_OK);
            assert(s.queue.entries[s.queue.head].admitted==started);
            assert(tv4_update_egress_begin(&s,&m,11,now)==TV4_CHANNEL_BUSY);
            assert(tv4_update_egress_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_BUSY);
            tv4_frame_queue_result second=tv4_update_egress_pump(&s,11,now);
            assert(second==TV4_QUEUE_ADMITTED || second==TV4_QUEUE_BUSY || second==TV4_QUEUE_WAIT);
            tv4_frame_delivery held;memset(&held,0xa5,sizeof(held));
            assert(tv4_update_egress_claim(&s,&held,11,now)==TV4_QUEUE_HELD && zero(&held,sizeof(held)));
            tv4_fragment_result r=tv4_channel_rx_receive(&rx,d.route,&d.frame,22,now);assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);
            assert(tv4_update_egress_complete(&s,d.ticket,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
            assert(tv4_update_egress_complete(&s,d.ticket,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_STALE);++frames;
        }
        tv4_channel_message out;assert(tv4_channel_rx_take(&rx,&out,22,now)==TV4_CHANNEL_TAKEN_UNVERIFIED && out.body.wire.kind==TV4_WIRE_REQUEST);
        assert(s.queue.last_issued>0 && s.channel.fragments.storage.counter==1 && s.queue.count==0);
        assert(tv4_update_egress_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_OK);
        assert(s.channel.policy.phase==TV4_CHANNEL_SETUP && s.channel.fragments.storage.counter==1 && s.queue.last_issued>0);
    }
    printf("PASS actual channel→owned queue→driver claim→correlated return→RX; %u frames, capacities1..4, phase waits for driver return\n",frames);
}
static void final_pending_frame(void)
{
    tv4_update_egress s={0};init(&s,4,0);tv4_channel_message m=request();uint32_t now=0;
    assert(tv4_update_egress_begin(&s,&m,11,now)==TV4_CHANNEL_OK);
    tv4_frame_delivery last={0};
    while(s.channel.active) {
        assert(tv4_update_egress_pump(&s,11,now)==TV4_QUEUE_ADMITTED);
        assert(tv4_update_egress_claim(&s,&last,11,now)==TV4_QUEUE_FRAME);
        if(!s.channel.active)break;
        assert(tv4_update_egress_complete(&s,last.ticket,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);++now;
    }
    assert(!s.channel.active && !s.channel.queued && s.channel.fragments.storage.state==TV4_FRAG_IDLE && s.queue.flight!=0);
    assert(tv4_update_egress_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_BUSY);
    now+=40;assert(tv4_update_egress_poll(&s,11,now)==TV4_QUEUE_ERROR && s.queue.flight==last.ticket.serial);
    assert(tv4_update_egress_clear(&s,11,now)==TV4_QUEUE_HELD);
    assert(tv4_update_egress_complete(&s,last.ticket,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_RECLAIMED_FAILED);
    assert(tv4_update_egress_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_ERROR);
    uint32_t serial=s.queue.last_issued;assert(tv4_update_egress_clear(&s,11,now)==TV4_QUEUE_OK);
    assert(s.channel.policy.phase==TV4_CHANNEL_NEGOTIATION && s.channel.fragments.storage.counter==1 && s.queue.last_issued==serial);
    puts("PASS actual final pending driver frame blocks phase despite inner IDLE; late completion reclaims, explicit clear retains counters/phase");
}
static void abort_and_sticky(void)
{
    for(unsigned which=0;which<5;++which) {
        tv4_update_egress s={0};init(&s,2,100);tv4_channel_message m=request();
        assert(tv4_update_egress_begin(&s,&m,11,100)==TV4_CHANNEL_OK);
        assert(tv4_update_egress_pump(&s,11,100)==TV4_QUEUE_ADMITTED);
        tv4_frame_delivery d;assert(tv4_update_egress_claim(&s,&d,11,100)==TV4_QUEUE_FRAME);
        assert(tv4_update_egress_pump(&s,11,100)==TV4_QUEUE_ADMITTED);
        uint32_t now=100;
        if(which==0)assert(tv4_update_egress_complete(&s,d.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN,now)==TV4_QUEUE_HELD);
        else if(which==1)assert(tv4_update_egress_poll(&s,12,now)==TV4_QUEUE_ERROR);
        else if(which==2){--now;assert(tv4_update_egress_poll(&s,11,now)==TV4_QUEUE_ERROR);}
        else if(which==3){now+=20;assert(tv4_update_egress_poll(&s,11,now)==TV4_QUEUE_ERROR);}
        assert(tv4_update_egress_cancel(&s,11,now)==TV4_QUEUE_HELD && s.queue.count==1);
        assert(tv4_update_egress_clear(&s,11,now)==TV4_QUEUE_HELD);
        assert(tv4_update_egress_complete(&s,d.ticket,TV4_FRAME_CANCELLED_LOCAL,now)==TV4_QUEUE_RECLAIMED_FAILED);
        if(which==1 || which==2)assert(tv4_update_egress_clear(&s,11,now)==TV4_QUEUE_ERROR);
        else assert(tv4_update_egress_clear(&s,11,now)==TV4_QUEUE_OK);
        assert(s.queue.last_issued==2 && s.channel.fragments.storage.counter==1);
    }
    tv4_update_egress s={0};init(&s,1,0);tv4_channel_message m=request();
    assert(tv4_update_egress_begin(&s,&m,11,0)==TV4_CHANNEL_OK);
    assert(tv4_update_egress_poll(&s,11,20)==TV4_QUEUE_ERROR && s.queue.count==0);
    assert(tv4_update_egress_clear(&s,11,20)==TV4_QUEUE_OK && s.channel.fragments.storage.counter==1);
    for(unsigned kind=0;kind<2;++kind) {
        tv4_update_egress exhausted={0};
        assert(tv4_update_egress_init(&exhausted,&fragments,(tv4_frame_queue_limits){40,1},7,11,
            kind==0?UINT16_MAX:0,kind==1?UINT32_MAX:0,0));
        tv4_channel_result begun=tv4_update_egress_begin(&exhausted,&m,11,0);
        if(kind==0)assert(begun==TV4_CHANNEL_ERROR);
        else {assert(begun==TV4_CHANNEL_OK);assert(tv4_update_egress_pump(&exhausted,11,0)==TV4_QUEUE_ERROR);}
        assert(tv4_update_egress_cancel(&exhausted,11,0)==TV4_QUEUE_OK);
        assert(tv4_update_egress_clear(&exhausted,11,0)==TV4_QUEUE_ERROR && exhausted.queue.count==0);
    }
    puts("PASS uncertain/cancel/context/clock/channel-timeout cleanup; matching driver ownership retained, no force-forget/reinit");
}
static void config(void)
{
    tv4_update_egress s={0};tv4_fragment_config f=fragments;f.version=2;
    assert(!tv4_update_egress_init(&s,&f,(tv4_frame_queue_limits){40,1},7,11,0,0,0) && !s.queue.configured);
    assert(!tv4_update_egress_init(&s,&fragments,(tv4_frame_queue_limits){0,1},7,11,0,0,0) && !s.channel.fragments.storage.configured);
    init(&s,1,0);assert(!tv4_update_egress_init(&s,&fragments,(tv4_frame_queue_limits){40,1},7,11,0,0,0));
}
int main(void)
{
    config();real_channel_and_pending_driver();final_pending_frame();abort_and_sticky();
    printf("Host storage only: egress owner=%zu; no actual MCU RAM/driver/physical approval\n",sizeof(tv4_update_egress));return 0;
}
