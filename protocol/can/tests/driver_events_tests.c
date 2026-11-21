#include "update_driver_events.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config fragments={20,1000,200,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static void init(tv4_driver_events *s,unsigned capacity,uint32_t now)
{assert(tv4_driver_events_init(s,&fragments,(tv4_frame_queue_limits){40,2},(uint8_t)capacity,7,11,0,0,now));}
static tv4_channel_message request(void)
{
    return (tv4_channel_message){.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,
        .body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};
}
static tv4_frame_delivery first(tv4_driver_events *s,uint32_t now)
{
    tv4_channel_message m=request();assert(tv4_driver_events_begin(s,&m,11,now)==TV4_CHANNEL_OK);
    assert(tv4_driver_events_pump(s,11,now)==TV4_QUEUE_ADMITTED);
    tv4_frame_delivery out;assert(tv4_driver_events_claim(s,&out,11,now)==TV4_QUEUE_FRAME);return out;
}
static void real_link_backpressure(void)
{
    unsigned frames=0,events=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_driver_events s={0};uint32_t now=UINT32_MAX-50u;init(&s,capacity,now);
        tv4_channel_rx rx={0};assert(tv4_channel_rx_init(&rx,&fragments,7,22,0,now));
        for(unsigned cycle=0;cycle<64;++cycle) {
            tv4_channel_message m=request();assert(tv4_driver_events_begin(&s,&m,11,now)==TV4_CHANNEL_OK);
            do {
                assert(tv4_driver_events_pump(&s,11,now)==TV4_QUEUE_ADMITTED);
                tv4_frame_delivery d;assert(tv4_driver_events_claim(&s,&d,11,now)==TV4_QUEUE_FRAME);
                tv4_fragment_result got=tv4_channel_rx_receive(&rx,d.route,&d.frame,22,now);
                assert(got==TV4_FRAG_PENDING || got==TV4_FRAG_READY);
                tv4_driver_event event={d.ticket,TV4_FRAME_SENT_LOCAL};
                for(unsigned n=0;n<capacity;++n){assert(tv4_driver_events_post(&s,&event)==TV4_QUEUE_ADMITTED);++events;}
                uint8_t snapshot[sizeof(s)];memcpy(snapshot,&s,sizeof(s));
                assert(tv4_driver_events_post(&s,&event)==TV4_QUEUE_BUSY && memcmp(snapshot,&s,sizeof(s))==0);
                memset(&event,0,sizeof(event)); /* actual owned event copy */
                assert(s.egress.queue.flight==d.ticket.serial && s.count==capacity);
                assert(tv4_driver_events_begin(&s,&m,11,now)==TV4_CHANNEL_BUSY);
                assert(tv4_driver_events_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_BUSY);
                tv4_frame_delivery blocked;memset(&blocked,0xa5,sizeof(blocked));
                assert(tv4_driver_events_claim(&s,&blocked,11,now)==TV4_QUEUE_BUSY && zero(&blocked,sizeof(blocked)));
                ++now;assert(tv4_driver_events_service(&s,11,now)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
                assert(s.count==capacity-1u && s.egress.queue.flight==0);
                while(s.count!=0) {
                    assert(tv4_driver_events_begin(&s,&m,11,now)==TV4_CHANNEL_BUSY);
                    assert(tv4_driver_events_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_BUSY);
                    ++now;assert(tv4_driver_events_service(&s,11,now)==TV4_QUEUE_STALE);
                }
                ++frames;
            }while(s.egress.channel.active);
            tv4_channel_message out;assert(tv4_channel_rx_take(&rx,&out,22,now)==TV4_CHANNEL_TAKEN_UNVERIFIED);
            assert(out.body.wire.kind==TV4_WIRE_REQUEST && s.egress.channel.fragments.storage.counter==cycle+1u);
            assert(tv4_driver_events_service(&s,11,now)==TV4_QUEUE_WAIT);
        }
        assert(s.egress.queue.last_issued==64u*33u && s.count==0);
        assert(tv4_driver_events_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_OK);
    }
    printf("PASS256 actual channel/RX messages, %u frames/%u owned reports; capacities1..4/FIFO wrap/clock wrap, full retry, one-event service, stale/duplicate phase guards\n",frames,events);
}
static void full_report_retry(void)
{
    tv4_driver_events s={0};init(&s,1,10);tv4_frame_delivery d=first(&s,10);
    tv4_driver_event stale={d.ticket,TV4_FRAME_SENT_LOCAL},terminal=stale;++stale.ticket.serial;
    assert(tv4_driver_events_post(&s,&stale)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_events_post(&s,&terminal)==TV4_QUEUE_BUSY && s.egress.queue.flight==d.ticket.serial);
    assert(tv4_driver_events_service(&s,11,11)==TV4_QUEUE_STALE && s.egress.queue.flight==d.ticket.serial);
    assert(tv4_driver_events_post(&s,&terminal)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_events_service(&s,11,12)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED && s.egress.queue.flight==0);
    assert(tv4_driver_events_cancel(&s,11,12)==TV4_QUEUE_OK);
    assert(tv4_driver_events_clear(&s,11,12)==TV4_QUEUE_OK && s.egress.queue.last_issued==1);
    puts("PASS full inbox preserves actual claim; external terminal report retained and retried after stale event service");
}
static void delayed_and_faulted_cleanup(void)
{
    for(unsigned fault=0;fault<5;++fault) {
        tv4_driver_events s={0};init(&s,2,100);tv4_frame_delivery d=first(&s,100);
        tv4_driver_event terminal={d.ticket,TV4_FRAME_SENT_LOCAL};
        if(fault==0) {
            tv4_driver_event unknown={d.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN};
            assert(tv4_driver_events_post(&s,&unknown)==TV4_QUEUE_ADMITTED);
        }
        assert(tv4_driver_events_post(&s,&terminal)==TV4_QUEUE_ADMITTED);
        uint32_t now=fault==1?140u:fault==2?99u:100u;uint64_t generation=fault==3?12u:11u;
        if(fault==0)assert(tv4_driver_events_service(&s,11,now)==TV4_QUEUE_HELD && s.egress.queue.flight==d.ticket.serial);
        if(fault==0 || fault==4)assert(tv4_driver_events_cancel(&s,11,now)==TV4_QUEUE_HELD);
        assert(tv4_driver_events_clear(&s,generation,now)==TV4_QUEUE_HELD && s.count==1 && s.egress.queue.flight==d.ticket.serial);
        assert(tv4_driver_events_service(&s,generation,now)==TV4_QUEUE_RECLAIMED_FAILED);
        assert(s.count==0 && s.egress.queue.count==0 && s.egress.failed);
        assert(tv4_driver_events_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_ERROR);
        tv4_frame_queue_result cleared=tv4_driver_events_clear(&s,11,now);
        assert(cleared==((fault==2 || fault==3)?TV4_QUEUE_ERROR:TV4_QUEUE_OK));
        assert(s.egress.queue.last_issued==1 && s.egress.channel.fragments.storage.counter==1 && s.egress.channel.policy.phase==TV4_CHANNEL_NEGOTIATION);
    }
    puts("PASS queued terminal ages at current service time, not report time; uncertainty/cancel/context/clock retain ownership, late cleanup never revives phase");
}
static void invalid_and_foreign(void)
{
    tv4_driver_events s={0},other={0};init(&s,2,0);init(&other,2,0);
    tv4_frame_delivery d=first(&s,0),foreign=first(&other,0);
    tv4_driver_event event={foreign.ticket,TV4_FRAME_SENT_LOCAL};
    assert(tv4_driver_events_post(&s,&event)==TV4_QUEUE_STALE && s.count==0);
    event.ticket=d.ticket;event.result=(tv4_frame_driver_result)256;
    assert(tv4_driver_events_post(&s,&event)==TV4_QUEUE_ARGUMENT && s.count==0);
    event.result=TV4_FRAME_SENT_LOCAL;
    for(unsigned which=0;which<3;++which) {
        event.ticket=d.ticket;if(which==0)++event.ticket.generation;else if(which==1)++event.ticket.route;else event.ticket.serial=0;
        assert(tv4_driver_events_post(&s,&event)==TV4_QUEUE_STALE && s.count==0);
    }
    event.ticket=d.ticket;assert(tv4_driver_events_post(&s,&event)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_events_service(&s,11,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    event.ticket=foreign.ticket;assert(tv4_driver_events_post(&other,&event)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_events_service(&other,11,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    assert(tv4_driver_events_cancel(&s,11,0)==TV4_QUEUE_OK && tv4_driver_events_clear(&s,11,0)==TV4_QUEUE_OK);
    assert(tv4_driver_events_cancel(&other,11,0)==TV4_QUEUE_OK && tv4_driver_events_clear(&other,11,0)==TV4_QUEUE_OK);
    tv4_driver_events unconfigured={0};
    assert(!tv4_driver_events_init(&unconfigured,&fragments,(tv4_frame_queue_limits){40,2},0,7,11,0,0,0) && !unconfigured.egress.configured);
    assert(!tv4_driver_events_init(&unconfigured,&fragments,(tv4_frame_queue_limits){40,2},5,7,11,0,0,0));
    assert(tv4_driver_events_service(&unconfigured,11,0)==TV4_QUEUE_ARGUMENT);
    assert(tv4_driver_events_post(&s,NULL)==TV4_QUEUE_ARGUMENT);
    assert(!tv4_driver_events_init(&s,&fragments,(tv4_frame_queue_limits){40,2},2,7,11,0,0,0));
}
int main(void)
{
    invalid_and_foreign();real_link_backpressure();full_report_retry();delayed_and_faulted_cleanup();
    printf("Host storage only: driver event=%zu bound event owner=%zu; no MCU RAM/RTOS/ISR/driver approval\n",sizeof(tv4_driver_event),sizeof(tv4_driver_events));return 0;
}
