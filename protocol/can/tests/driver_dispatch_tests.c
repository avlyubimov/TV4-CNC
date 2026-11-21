#include "driver_dispatch.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config fragments={20,1000,200,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_channel_message request(void)
{return (tv4_channel_message){.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,.body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};}
static void init(tv4_driver_dispatch *s,unsigned capacity,uint32_t now)
{assert(tv4_driver_dispatch_init(s,&fragments,(tv4_frame_queue_limits){40,2},capacity,7,11,0,0,now));}
static void first(tv4_driver_dispatch *s,uint32_t now)
{
    tv4_channel_message m=request();assert(tv4_driver_dispatch_begin(s,&m,11,now)==TV4_CHANNEL_OK);
    assert(tv4_driver_dispatch_pump(s,11,now)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_dispatch_publish(s,11,now)==TV4_QUEUE_ADMITTED);
}
static void report(tv4_driver_dispatch *s,tv4_frame_ticket ticket,tv4_frame_driver_result result)
{tv4_driver_event e={ticket,result};assert(tv4_driver_dispatch_report(s,&e)==TV4_QUEUE_ADMITTED);}
static void expiry_and_revocation(void)
{
    for(unsigned which=0;which<5;++which) {
        tv4_driver_dispatch s={0};init(&s,2,100);first(&s,100);
        assert(tv4_driver_dispatch_publish(&s,11,100)==TV4_QUEUE_BUSY);
        assert(tv4_driver_dispatch_clear(&s,11,100)==TV4_QUEUE_HELD);
        if(which==0)assert(tv4_driver_dispatch_cancel(&s,11,100)==TV4_QUEUE_HELD);
        if(which==1)assert(tv4_driver_dispatch_poll(&s,12,100)==TV4_QUEUE_ERROR);
        tv4_dispatch_delivery token,out;assert(tv4_driver_dispatch_take(&s,&token)==TV4_DISPATCH_UNCHECKED && zero(&token.delivery.frame,sizeof(token.delivery.frame)));
        tv4_dispatch_status expected=which<2?TV4_DISPATCH_REVOKED:which==2?TV4_DISPATCH_EXPIRED:which==3?TV4_DISPATCH_CLOCK:TV4_DISPATCH_SENDABLE;
        uint32_t now=which==2?140u:which==3?99u:100u;
        assert(tv4_driver_dispatch_check(&s,token.delivery.ticket,&out,now)==expected);
        if(which!=4) {
            assert(zero(&out,sizeof(out)));tv4_driver_event e={token.delivery.ticket,TV4_FRAME_SENT_LOCAL};
            assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_ARGUMENT);
        }else {
            assert(out.delivery.frame.length>0);
            assert(tv4_driver_dispatch_check(&s,token.delivery.ticket,&out,140)==TV4_DISPATCH_EXPIRED && zero(&out,sizeof(out)));now=140;
        }
        report(&s,token.delivery.ticket,which==4?TV4_FRAME_SENT_LOCAL:TV4_FRAME_CANCELLED_LOCAL);
        assert(atomic_load(&s.state)==TV4_DISPATCH_RETURNED && s.completion.events.egress.queue.flight==token.delivery.ticket.serial);
        uint32_t owner_now=now==99?100:now;
        assert(tv4_driver_dispatch_service(&s,11,owner_now)==TV4_QUEUE_RECLAIMED_FAILED && atomic_load(&s.state)==TV4_DISPATCH_EMPTY);
        assert(tv4_driver_dispatch_clear(&s,11,owner_now)==(which==1?TV4_QUEUE_ERROR:TV4_QUEUE_OK));
        assert(s.completion.events.egress.queue.last_issued==1 && s.completion.events.egress.channel.fragments.storage.counter==1);
        if(which==3) {
            first(&s,100);assert(tv4_driver_dispatch_take(&s,&token)==TV4_DISPATCH_UNCHECKED);
            assert(tv4_driver_dispatch_check(&s,token.delivery.ticket,&out,100)==TV4_DISPATCH_CLOCK);
            report(&s,token.delivery.ticket,TV4_FRAME_CANCELLED_LOCAL);
            assert(tv4_driver_dispatch_service(&s,11,100)==TV4_QUEUE_RECLAIMED_FAILED);
            assert(tv4_driver_dispatch_clear(&s,11,100)==TV4_QUEUE_OK);
        }
    }
    tv4_driver_dispatch s={0};init(&s,1,100);tv4_channel_message m=request();
    assert(tv4_driver_dispatch_begin(&s,&m,11,100)==TV4_CHANNEL_OK);
    assert(tv4_driver_dispatch_pump(&s,11,100)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_dispatch_publish(&s,11,105)==TV4_QUEUE_ADMITTED);
    tv4_dispatch_delivery token,out;assert(tv4_driver_dispatch_take(&s,&token)==TV4_DISPATCH_UNCHECKED && token.admitted==100 && token.residency_ms==40);
    assert(tv4_driver_dispatch_check(&s,token.delivery.ticket,&out,140)==TV4_DISPATCH_EXPIRED);
    report(&s,token.delivery.ticket,TV4_FRAME_CANCELLED_LOCAL);
    assert(tv4_driver_dispatch_service(&s,11,140)==TV4_QUEUE_RECLAIMED_FAILED);
    assert(tv4_driver_dispatch_clear(&s,11,140)==TV4_QUEUE_OK);
    puts("PASS published untaken/taken claims survive cancel/expiry/context/driver-clock fault; fresh check required, original admission retained, late SENT cleanup only");
}
static void full_report_and_stale_callback(void)
{
    tv4_driver_dispatch s={0};init(&s,1,0);first(&s,0);tv4_dispatch_delivery token,out;
    assert(tv4_driver_dispatch_take(&s,&token)==TV4_DISPATCH_UNCHECKED);
    assert(tv4_driver_dispatch_check(&s,token.delivery.ticket,&out,0)==TV4_DISPATCH_SENDABLE);
    tv4_driver_event e={token.delivery.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN};assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_ADMITTED);
    e.result=TV4_FRAME_SENT_LOCAL;assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_BUSY && s.driver_has_pending);
    e.result=TV4_FRAME_FAILED_LOCAL;assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_BUSY && s.driver_pending.result==TV4_FRAME_SENT_LOCAL);
    assert(tv4_driver_dispatch_service(&s,11,0)==TV4_QUEUE_HELD && atomic_load(&s.state)==TV4_DISPATCH_TAKEN);
    assert(tv4_driver_dispatch_clear(&s,11,0)==TV4_QUEUE_HELD);
    assert(tv4_driver_dispatch_retry(&s)==TV4_QUEUE_ADMITTED && atomic_load(&s.state)==TV4_DISPATCH_RETURNED);
    assert(tv4_driver_dispatch_service(&s,11,0)==TV4_QUEUE_RECLAIMED_FAILED && atomic_load(&s.state)==TV4_DISPATCH_EMPTY);
    assert(tv4_driver_dispatch_clear(&s,11,0)==TV4_QUEUE_OK);
    first(&s,0);tv4_dispatch_delivery next;assert(tv4_driver_dispatch_take(&s,&next)==TV4_DISPATCH_UNCHECKED && next.delivery.ticket.serial==2);
    e.ticket=token.delivery.ticket;assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_STALE);
    e.ticket=next.delivery.ticket;e.result=TV4_FRAME_SENT_LOCAL;assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_ARGUMENT);
    assert(tv4_driver_dispatch_check(&s,next.delivery.ticket,&out,0)==TV4_DISPATCH_SENDABLE);
    assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_ADMITTED);
    assert(tv4_driver_dispatch_report(&s,&e)==TV4_QUEUE_STALE);
    assert(tv4_driver_dispatch_service(&s,11,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    assert(tv4_driver_dispatch_cancel(&s,11,0)==TV4_QUEUE_OK && tv4_driver_dispatch_clear(&s,11,0)==TV4_QUEUE_OK);
    puts("PASS full completion retains first terminal for retry; uncertain never overwritten, old callback cannot free next claim after clear, no implicit-current callback");
}
static void exhaustion_and_arguments(void)
{
    tv4_driver_dispatch s={0};assert(tv4_driver_dispatch_init(&s,&fragments,(tv4_frame_queue_limits){40,2},1,7,11,0,UINT32_MAX-1u,0));
    first(&s,0);assert(tv4_driver_dispatch_pump(&s,11,0)==TV4_QUEUE_ERROR && atomic_load(&s.revoked)==1);
    tv4_dispatch_delivery token,out;assert(tv4_driver_dispatch_take(&s,&token)==TV4_DISPATCH_UNCHECKED);
    assert(tv4_driver_dispatch_check(&s,token.delivery.ticket,&out,0)==TV4_DISPATCH_REVOKED);
    report(&s,token.delivery.ticket,TV4_FRAME_CANCELLED_LOCAL);
    assert(tv4_driver_dispatch_service(&s,11,0)==TV4_QUEUE_RECLAIMED_FAILED && tv4_driver_dispatch_clear(&s,11,0)==TV4_QUEUE_ERROR);
    memset(&out,0xa5,sizeof(out));assert(tv4_driver_dispatch_take(NULL,&out)==TV4_DISPATCH_ARGUMENT && zero(&out,sizeof(out)));
    assert(tv4_driver_dispatch_report(&s,NULL)==TV4_QUEUE_ARGUMENT && tv4_driver_dispatch_retry(&s)==TV4_QUEUE_WAIT);
    assert(!tv4_driver_dispatch_init(&s,&fragments,(tv4_frame_queue_limits){40,2},1,7,11,0,0,0));
    tv4_driver_dispatch invalid={0};assert(!tv4_driver_dispatch_init(&invalid,&fragments,(tv4_frame_queue_limits){40,2},0,7,11,0,0,0) && !invalid.configured);
}
typedef struct {tv4_driver_dispatch *s;tv4_channel_rx peer;atomic_uint clock,stop;unsigned frames,messages,stales;} threaded_driver;
static void *driver_actor(void *arg)
{
    threaded_driver *c=arg;
    while(atomic_load_explicit(&c->stop,memory_order_acquire)==0) {
        tv4_dispatch_delivery token,out;tv4_dispatch_status got=tv4_driver_dispatch_take(c->s,&token);
        if(got==TV4_DISPATCH_WAIT){sched_yield();continue;}
        assert(got==TV4_DISPATCH_UNCHECKED && zero(&token.delivery.frame,sizeof(token.delivery.frame)));
        uint32_t now=atomic_load_explicit(&c->clock,memory_order_acquire);
        assert(tv4_driver_dispatch_check(c->s,token.delivery.ticket,&out,now)==TV4_DISPATCH_SENDABLE);
        tv4_fragment_result received=tv4_channel_rx_receive(&c->peer,out.delivery.route,&out.delivery.frame,22,now);
        assert(received==TV4_FRAG_PENDING || received==TV4_FRAG_READY);
        if(received==TV4_FRAG_READY) {
            tv4_channel_message m;assert(tv4_channel_rx_take(&c->peer,&m,22,now)==TV4_CHANNEL_TAKEN_UNVERIFIED && m.body.wire.kind==TV4_WIRE_REQUEST);++c->messages;
        }
        tv4_driver_event e={token.delivery.ticket,TV4_FRAME_SENT_LOCAL};tv4_frame_queue_result result=tv4_driver_dispatch_report(c->s,&e);
        while(result==TV4_QUEUE_BUSY){sched_yield();result=tv4_driver_dispatch_retry(c->s);}
        assert(result==TV4_QUEUE_ADMITTED);++c->frames;
        if(e.ticket.serial%8u==0){assert(tv4_driver_dispatch_report(c->s,&e)==TV4_QUEUE_STALE);++c->stales;}
    }
    return NULL;
}
static void two_actor_roundtrip(void)
{
    unsigned frames=0,messages=0,stales=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_driver_dispatch s={0};uint32_t now=UINT32_MAX-50u;init(&s,capacity,now);
        threaded_driver c={.s=&s};atomic_init(&c.clock,now);atomic_init(&c.stop,0u);
        assert(tv4_channel_rx_init(&c.peer,&fragments,7,22,0,now));pthread_t thread;assert(pthread_create(&thread,NULL,driver_actor,&c)==0);
        for(unsigned n=0;n<64;++n) {
            tv4_channel_message m=request();assert(tv4_driver_dispatch_begin(&s,&m,11,now)==TV4_CHANNEL_OK);
            do {
                assert(tv4_driver_dispatch_pump(&s,11,now)==TV4_QUEUE_ADMITTED);
                assert(tv4_driver_dispatch_publish(&s,11,now)==TV4_QUEUE_ADMITTED);unsigned completed=0;
                do {
                    tv4_frame_queue_result result=tv4_driver_dispatch_service(&s,11,now);
                    if(result==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED)++completed;else assert(result==TV4_QUEUE_WAIT);
                    sched_yield();
                }while(atomic_load_explicit(&s.state,memory_order_acquire)!=TV4_DISPATCH_EMPTY);
                assert(completed==1 && s.completion.events.egress.queue.flight==0);++now;
                atomic_store_explicit(&c.clock,now,memory_order_release);
            }while(s.completion.events.egress.channel.active);
        }
        atomic_store_explicit(&c.stop,1u,memory_order_release);assert(pthread_join(thread,NULL)==0);
        assert(c.frames==2112 && c.messages==64 && s.completion.events.egress.queue.last_issued==2112);
        assert(tv4_driver_dispatch_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==TV4_CHANNEL_OK);
        frames+=c.frames;messages+=c.messages;stales+=c.stales;
    }
    printf("PASS two real actors bidirectional publication/take/fresh-check/peer-RX/exact-return/service: %u messages/%u frames/%u stale callbacks, capacities1..4/clock wrap\n",messages,frames,stales);
}
typedef struct {tv4_driver_dispatch *s;atomic_uint gate;unsigned which;} fault_context;
static void *fault_actor(void *arg)
{
    fault_context *c=arg;while(atomic_load_explicit(&c->gate,memory_order_acquire)!=1)sched_yield();
    tv4_dispatch_delivery token,out;assert(tv4_driver_dispatch_take(c->s,&token)==TV4_DISPATCH_UNCHECKED);
    if(c->which==3) {
        assert(tv4_driver_dispatch_check(c->s,token.delivery.ticket,&out,100)==TV4_DISPATCH_SENDABLE);
        tv4_driver_event e={token.delivery.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN};
        assert(tv4_driver_dispatch_report(c->s,&e)==TV4_QUEUE_ADMITTED);e.result=TV4_FRAME_SENT_LOCAL;
        assert(tv4_driver_dispatch_report(c->s,&e)==TV4_QUEUE_BUSY);
        atomic_store_explicit(&c->gate,2u,memory_order_release);
        while(atomic_load_explicit(&c->gate,memory_order_acquire)!=3)sched_yield();
        assert(tv4_driver_dispatch_retry(c->s)==TV4_QUEUE_ADMITTED);
    }else {
        atomic_store_explicit(&c->gate,2u,memory_order_release);
        while(atomic_load_explicit(&c->gate,memory_order_acquire)!=3)sched_yield();
        assert(tv4_driver_dispatch_check(c->s,token.delivery.ticket,&out,c->which==2?140u:100u)==(c->which==2?TV4_DISPATCH_EXPIRED:TV4_DISPATCH_REVOKED));
        assert(zero(&out,sizeof(out)));report(c->s,token.delivery.ticket,TV4_FRAME_CANCELLED_LOCAL);
    }
    atomic_store_explicit(&c->gate,4u,memory_order_release);return NULL;
}
static void concurrent_cancellation_and_retry(void)
{
    for(unsigned which=0;which<4;++which) {
        tv4_driver_dispatch s={0};init(&s,1,100);first(&s,100);fault_context c={.s=&s,.which=which};atomic_init(&c.gate,0u);
        if(which==0)assert(tv4_driver_dispatch_cancel(&s,11,100)==TV4_QUEUE_HELD);
        pthread_t thread;assert(pthread_create(&thread,NULL,fault_actor,&c)==0);atomic_store_explicit(&c.gate,1u,memory_order_release);
        while(atomic_load_explicit(&c.gate,memory_order_acquire)!=2)sched_yield();
        if(which==1)assert(tv4_driver_dispatch_cancel(&s,11,100)==TV4_QUEUE_HELD);
        if(which==3)assert(tv4_driver_dispatch_service(&s,11,100)==TV4_QUEUE_HELD);
        assert(tv4_driver_dispatch_clear(&s,11,100)==TV4_QUEUE_HELD);
        atomic_store_explicit(&c.gate,3u,memory_order_release);unsigned reclaimed=0;
        /* Isolate DRIVER expiry from owner revocation: the owner must not poll
         * at140 until the driver's independent check/report has completed. */
        if(which==2)while(atomic_load_explicit(&c.gate,memory_order_acquire)!=4)sched_yield();
        do {
            tv4_frame_queue_result r=tv4_driver_dispatch_service(&s,11,which==2?140u:100u);
            if(r==TV4_QUEUE_RECLAIMED_FAILED)++reclaimed;else assert(r==TV4_QUEUE_WAIT || r==TV4_QUEUE_ERROR);
            sched_yield();
        }while(atomic_load_explicit(&c.gate,memory_order_acquire)!=4 || atomic_load_explicit(&s.state,memory_order_acquire)!=TV4_DISPATCH_EMPTY);
        assert(pthread_join(thread,NULL)==0 && reclaimed==1);
        assert(tv4_driver_dispatch_clear(&s,11,which==2?140u:100u)==TV4_QUEUE_OK);
    }
    puts("PASS4 controlled two-actor cancellations before/after take, driver expiry and full uncertain→retained terminal retry; exact claim reclaimed only");
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--threads")==0){two_actor_roundtrip();concurrent_cancellation_and_retry();return 0;}
    expiry_and_revocation();full_report_and_stale_callback();exhaustion_and_arguments();
    printf("Host storage only: dispatch=%zu delivery=%zu; no target RAM/ISR/RTOS/backend or physical authority\n",sizeof(tv4_driver_dispatch),sizeof(tv4_dispatch_delivery));return 0;
}
