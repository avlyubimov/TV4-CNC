#include "completion_mailbox.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config fragments={20,1000,200,TV4_FRAGMENT_VERSION};
static tv4_channel_message request(void)
{return (tv4_channel_message){.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,.body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};}
static void init(tv4_completion_owner *s,unsigned capacity,uint32_t now)
{assert(tv4_completion_owner_init(s,&fragments,(tv4_frame_queue_limits){40,2},capacity,7,11,0,0,now));}
static tv4_frame_delivery first(tv4_completion_owner *s,uint32_t now)
{
    tv4_channel_message m=request();assert(tv4_completion_owner_begin(s,&m,11,now)==TV4_CHANNEL_OK);
    assert(tv4_completion_owner_pump(s,11,now)==TV4_QUEUE_ADMITTED);
    tv4_frame_delivery d;assert(tv4_completion_owner_claim(s,&d,11,now)==TV4_QUEUE_FRAME);return d;
}
typedef struct {tv4_completion_owner *owner;tv4_frame_delivery job;atomic_uint state;unsigned reports,busy;} driver_context;
static void *driver(void *arg)
{
    driver_context *c=arg;
    for(;;) {
        unsigned state=atomic_load_explicit(&c->state,memory_order_acquire);
        if(state==3)return NULL;
        if(state!=1){sched_yield();continue;}
        tv4_frame_delivery d=c->job;
        assert(d.route==7 && d.frame.length>=5 && d.frame.length<=8);
        tv4_driver_event e={d.ticket,TV4_FRAME_SENT_LOCAL};
        unsigned copies=d.ticket.serial%8u==0?2u:1u;
        for(unsigned n=0;n<copies;++n) {
            tv4_frame_queue_result r;
            while((r=tv4_completion_owner_post(c->owner,&e))==TV4_QUEUE_BUSY){++c->busy;sched_yield();}
            assert(r==TV4_QUEUE_ADMITTED);++c->reports;
        }
        memset(&e,0,sizeof(e));atomic_store_explicit(&c->state,2u,memory_order_release);
    }
}
static void actual_concurrent_completions(void)
{
    unsigned frames=0,reports=0,stales=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_completion_owner s={0};uint32_t now=UINT32_MAX-50u;init(&s,capacity,now);
        tv4_channel_rx rx={0};assert(tv4_channel_rx_init(&rx,&fragments,7,22,0,now));
        driver_context c={.owner=&s};atomic_init(&c.state,0u);pthread_t thread;assert(pthread_create(&thread,NULL,driver,&c)==0);
        for(unsigned cycle=0;cycle<64;++cycle) {
            tv4_channel_message m=request();assert(tv4_completion_owner_begin(&s,&m,11,now)==TV4_CHANNEL_OK);
            do {
                assert(tv4_completion_owner_pump(&s,11,now)==TV4_QUEUE_ADMITTED);
                tv4_frame_delivery d;assert(tv4_completion_owner_claim(&s,&d,11,now)==TV4_QUEUE_FRAME);
                tv4_fragment_result r=tv4_channel_rx_receive(&rx,d.route,&d.frame,22,now);assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);
                c.job=d;atomic_store_explicit(&c.state,1u,memory_order_release);
                unsigned completions=0;
                do {
                    tv4_frame_queue_result result=tv4_completion_owner_service(&s,11,now);
                    if(result==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED)++completions;
                    else if(result==TV4_QUEUE_STALE)++stales;
                    else assert(result==TV4_QUEUE_WAIT);
                    sched_yield();
                }while(atomic_load_explicit(&c.state,memory_order_acquire)!=2 || tv4_completion_mailbox_empty(&s.mailbox)!=TV4_QUEUE_OK || s.has_pending || s.events.count!=0);
                assert(completions==1 && s.events.egress.queue.flight==0);
                atomic_store_explicit(&c.state,0u,memory_order_release);++frames;++now;
            }while(s.events.egress.channel.active);
            tv4_channel_message out;assert(tv4_channel_rx_take(&rx,&out,22,now)==TV4_CHANNEL_TAKEN_UNVERIFIED && out.body.wire.kind==TV4_WIRE_REQUEST);
        }
        atomic_store_explicit(&c.state,3u,memory_order_release);assert(pthread_join(thread,NULL)==0);reports+=c.reports;
        assert(tv4_completion_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_OK);
        assert(s.events.egress.queue.last_issued==2112 && s.events.egress.channel.fragments.storage.counter==64);
    }
    assert(reports==frames+stales);printf("PASS concurrent actual channel/claim/local-driver-report/service/RX:256 messages/%u frames/%u reports/%u stale duplicates, capacities1..4/clock wrap\n",frames,reports,stales);
}
static void held_and_late(void)
{
    for(unsigned which=0;which<5;++which) {
        tv4_completion_owner s={0};init(&s,3,100);tv4_frame_delivery d=first(&s,100);
        tv4_driver_event e={d.ticket,TV4_FRAME_SENT_LOCAL};
        if(which==0){e.result=TV4_FRAME_OWNERSHIP_UNCERTAIN;assert(tv4_completion_owner_post(&s,&e)==TV4_QUEUE_ADMITTED);e.result=TV4_FRAME_SENT_LOCAL;}
        assert(tv4_completion_owner_post(&s,&e)==TV4_QUEUE_ADMITTED);memset(&e,0,sizeof(e));
        uint32_t now=which==1?140u:which==2?99u:100u;uint64_t generation=which==3?12u:11u;
        if(which==0)assert(tv4_completion_owner_service(&s,11,now)==TV4_QUEUE_HELD);
        if(which==0 || which==4)assert(tv4_completion_owner_cancel(&s,11,now)==TV4_QUEUE_HELD);
        assert(tv4_completion_owner_clear(&s,generation,now)==TV4_QUEUE_HELD && s.events.egress.queue.flight==d.ticket.serial);
        assert(tv4_completion_owner_service(&s,generation,now)==TV4_QUEUE_RECLAIMED_FAILED);
        unsigned head=atomic_load(&s.mailbox.head),tail=atomic_load(&s.mailbox.tail);assert(head!=0 && head==tail);
        assert(tv4_completion_owner_clear(&s,11,now)==((which==2 || which==3)?TV4_QUEUE_ERROR:TV4_QUEUE_OK));
        assert(atomic_load(&s.mailbox.head)==head && atomic_load(&s.mailbox.tail)==tail && s.events.egress.queue.last_issued==1 && s.events.egress.channel.fragments.storage.counter==1);
    }
    tv4_completion_owner s={0};init(&s,1,0);tv4_frame_delivery d=first(&s,0);
    tv4_driver_event terminal={d.ticket,TV4_FRAME_SENT_LOCAL},stale=terminal;++stale.ticket.serial;
    assert(tv4_completion_owner_post(&s,&stale)==TV4_QUEUE_ADMITTED);
    assert(tv4_completion_owner_post(&s,&terminal)==TV4_QUEUE_BUSY);
    assert(tv4_completion_owner_service(&s,11,0)==TV4_QUEUE_STALE && s.events.egress.queue.flight==d.ticket.serial);
    assert(tv4_completion_owner_post(&s,&terminal)==TV4_QUEUE_ADMITTED);
    assert(tv4_completion_owner_service(&s,11,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    assert(tv4_completion_owner_cancel(&s,11,0)==TV4_QUEUE_OK && tv4_completion_owner_clear(&s,11,0)==TV4_QUEUE_OK);
    tv4_frame_delivery next=first(&s,0);assert(next.ticket.serial==2 && s.events.egress.channel.fragments.storage.counter==2);
    assert(tv4_completion_owner_post(&s,&terminal)==TV4_QUEUE_ADMITTED);
    assert(tv4_completion_owner_service(&s,11,0)==TV4_QUEUE_STALE && s.events.egress.queue.flight==next.ticket.serial);
    terminal.ticket=next.ticket;assert(tv4_completion_owner_post(&s,&terminal)==TV4_QUEUE_ADMITTED);
    assert(tv4_completion_owner_service(&s,11,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    assert(tv4_completion_owner_cancel(&s,11,0)==TV4_QUEUE_OK && tv4_completion_owner_clear(&s,11,0)==TV4_QUEUE_OK);
    puts("PASS full retained terminal retry/current-time timeout/uncertain/cancel/sticky cleanup; atomic positions and protocol IDs retained, no force-forget");
}
static void phase_boundary(void)
{
    tv4_completion_owner s={0};init(&s,2,0);uint32_t now=0;tv4_channel_message m=request();
    assert(tv4_completion_owner_begin(&s,&m,11,now)==TV4_CHANNEL_OK);
    do {
        assert(tv4_completion_owner_pump(&s,11,now)==TV4_QUEUE_ADMITTED);
        tv4_frame_delivery d;assert(tv4_completion_owner_claim(&s,&d,11,now)==TV4_QUEUE_FRAME);
        tv4_driver_event e={d.ticket,TV4_FRAME_SENT_LOCAL};
        assert(tv4_completion_owner_post(&s,&e)==TV4_QUEUE_ADMITTED);
        assert(tv4_completion_owner_post(&s,&e)==TV4_QUEUE_ADMITTED);
        assert(tv4_completion_owner_service(&s,11,now)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
        if(!s.events.egress.channel.active) {
            assert(s.events.count==0 && s.events.egress.queue.flight==0 && s.events.egress.queue.count==0);
            assert(tv4_completion_owner_begin(&s,&m,11,now)==TV4_CHANNEL_BUSY);
            assert(tv4_completion_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_BUSY);
            tv4_frame_delivery blocked;assert(tv4_completion_owner_claim(&s,&blocked,11,now)==TV4_QUEUE_BUSY);
        }
        assert(tv4_completion_owner_service(&s,11,now)==TV4_QUEUE_STALE);++now;
    }while(s.events.egress.channel.active);
    assert(tv4_completion_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_OK);
    puts("PASS actual mailbox blocks phase/begin/claim with inner owner IDLE and ledger empty; stale duplicate consumed explicitly");
}
int main(void)
{
    held_and_late();phase_boundary();actual_concurrent_completions();
    printf("Host storage only: bound completion owner=%zu; no MCU memory/ISR/RTOS/driver truth approval\n",sizeof(tv4_completion_owner));return 0;
}
