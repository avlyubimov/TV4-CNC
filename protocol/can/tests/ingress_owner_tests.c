#include "ingress_owner.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config config={20,1000,200,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_channel_message request(void)
{return (tv4_channel_message){.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,.body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};}
static void init(tv4_ingress_owner *s,unsigned capacity,uint32_t now)
{assert(tv4_ingress_owner_init(s,&config,capacity,40,7,11,0,0,0,now));}
static tv4_ingress_permit permit(tv4_ingress_owner *s)
{tv4_ingress_permit p;assert(tv4_ingress_owner_permit(s,&p)==TV4_GATE_OPEN);return p;}
static tv4_ingress_pause pause_request(tv4_ingress_owner *s)
{tv4_ingress_pause p;assert(tv4_ingress_owner_pause_request(s,&p)==TV4_GATE_REQUESTED);return p;}
static tv4_ingress_token stage(tv4_ingress_owner *s)
{tv4_ingress_token t;assert(tv4_ingress_owner_stage(s,&t)==TV4_INGRESS_UNCHECKED);return t;}
static tv4_fragment_frame first(tv4_channel_tx *tx,uint32_t now)
{
    assert(tv4_channel_tx_init(tx,&config,7,11,0,now));tv4_channel_message m=request();
    assert(tv4_channel_tx_begin(tx,&m,11,now)==TV4_CHANNEL_OK);tv4_fragment_frame f;
    assert(tv4_channel_tx_peek(tx,&f,11,now)==TV4_FRAG_FRAME);return f;
}
static void concrete_drain_and_epoch(void)
{
    tv4_ingress_owner s={0};init(&s,1,100);tv4_channel_tx tx={0};tv4_fragment_frame f=first(&tx,100);tv4_ingress_permit old=permit(&s);
    assert(tv4_ingress_owner_offer(&s,old,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);
    assert(tv4_channel_tx_commit(&tx,&f,11,100)==TV4_FRAG_COMMITTED && tv4_channel_tx_peek(&tx,&f,11,100)==TV4_FRAG_FRAME);
    assert(tv4_ingress_owner_offer(&s,old,7,11,&f,100,100)==TV4_INGRESS_HELD);
    assert(tv4_channel_tx_commit(&tx,&f,11,100)==TV4_FRAG_COMMITTED);
    assert(tv4_ingress_owner_pause(&s,11,100)==TV4_GATE_REQUESTED);tv4_ingress_pause previous=pause_request(&s);
    assert(tv4_ingress_owner_ack(&s,previous,100)==TV4_GATE_BUSY && tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_BUSY);
    tv4_ingress_token one=stage(&s);
    assert(tv4_ingress_owner_ack(&s,previous,100)==TV4_GATE_PAUSED);
    assert(tv4_ingress_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,100)==TV4_CHANNEL_BUSY);
    assert(tv4_ingress_owner_resume(&s,11,100)==TV4_GATE_BUSY);
    tv4_ingress_permit refused;assert(tv4_ingress_owner_permit(&s,&refused)==TV4_GATE_PAUSED && zero(&refused,sizeof(refused)));
    assert(tv4_ingress_owner_offer(&s,old,7,11,&f,100,100)==TV4_INGRESS_BUSY && tv4_ingress_owner_retry(&s,100)==TV4_INGRESS_BUSY);
    assert(tv4_ingress_owner_receive(&s,one,11,100)==TV4_FRAG_PENDING);
    tv4_ingress_token two=stage(&s);assert(tv4_ingress_owner_receive(&s,two,11,100)==TV4_FRAG_PENDING);
    assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_BUSY); /* actual partial RX, not external boolean */
    assert(tv4_ingress_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,100)==TV4_CHANNEL_BUSY);
    assert(tv4_ingress_owner_resume(&s,11,100)==TV4_GATE_OPEN);tv4_ingress_permit fresh=permit(&s);assert(fresh.epoch==old.epoch+1u);
    assert(tv4_ingress_owner_offer(&s,old,7,11,&f,100,100)==TV4_INGRESS_STALE && tv4_ingress_owner_ack(&s,previous,100)==TV4_GATE_STALE);
    while(tx.active){assert(tv4_channel_tx_peek(&tx,&f,11,100)==TV4_FRAG_FRAME);
        /* New simulated capture AFTER fresh permit; never retag retained input. */
        assert(tv4_ingress_owner_offer(&s,fresh,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);
        tv4_fragment_result sent=tv4_channel_tx_commit(&tx,&f,11,100);assert(sent==TV4_FRAG_COMMITTED || sent==TV4_FRAG_FINISHED);
        tv4_fragment_result received=tv4_ingress_owner_receive(&s,stage(&s),11,100);assert(received==(tx.active?TV4_FRAG_PENDING:TV4_FRAG_READY));}
    assert(tv4_ingress_owner_pause(&s,11,100)==TV4_GATE_REQUESTED);tv4_ingress_pause p=pause_request(&s);
    assert(tv4_ingress_owner_ack(&s,previous,100)==TV4_GATE_STALE && tv4_ingress_owner_ack(&s,p,100)==TV4_GATE_PAUSED);
    assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_BUSY); /* completed but not yet taken */
    tv4_channel_message out;assert(tv4_ingress_owner_take(&s,&out,11,100)==TV4_CHANNEL_TAKEN_UNVERIFIED && out.body.wire.kind==TV4_WIRE_REQUEST);
    assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_PAUSED);
    uint32_t serial=s.ingress.issued;uint16_t counter=s.channel.fragments.storage.counter;
    assert(serial==33 && counter==1 && tv4_ingress_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,100)==TV4_CHANNEL_OK);
    assert(tv4_ingress_owner_advance(&s,TV4_CHANNEL_METADATA,(tv4_update_id){11,2},11,100)==TV4_CHANNEL_STATE);
    assert(tv4_ingress_owner_resume(&s,11,100)==TV4_GATE_OPEN && s.ingress.issued==serial && s.channel.fragments.storage.counter==counter);
    assert(!tv4_ingress_owner_init(&s,&config,1,40,7,11,0,0,0,100));
    puts("PASS actual pending/staged/partial/READY drain blocks advance; same-phase resume retains clocks/IDs, exact old permits/ACK stale, one advance per pause");
}
static void faults_and_arguments(void)
{
    for(unsigned which=0;which<5;++which) {
        tv4_ingress_owner s={0};init(&s,1,100);tv4_ingress_permit p=permit(&s);tv4_channel_tx tx={0};tv4_fragment_frame f=first(&tx,100);
        assert(tv4_ingress_owner_offer(&s,p,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);tv4_ingress_token t=stage(&s),bad=t;bad.serial++;
        assert(tv4_ingress_owner_receive(&s,bad,11,100)==TV4_FRAG_STALE);
        if(which==0)assert(tv4_ingress_owner_lost(&s)==TV4_INGRESS_FAULT);
        if(which==1)assert(tv4_ingress_owner_retry(&s,99)==TV4_INGRESS_FAULT);
        if(which==2)assert(tv4_ingress_owner_poll(&s,12,100)==TV4_FRAG_ERROR);
        uint32_t now=which==3?140u:which==4?99u:100u;
        assert(tv4_ingress_owner_receive(&s,t,11,now)==(which==2?TV4_FRAG_STATE:TV4_FRAG_ERROR));
        uint32_t current=now==99?100:now;assert(tv4_ingress_owner_resume(&s,11,current)==TV4_GATE_FAULT && tv4_ingress_owner_quiescent(&s,11,current)==TV4_GATE_FAULT);
        tv4_channel_message out;memset(&out,0xa5,sizeof(out));assert(tv4_ingress_owner_take(&s,&out,11,current)==TV4_CHANNEL_ERROR && zero(&out,sizeof(out)));
        assert(s.channel.fragments.storage.counter==0 && s.failed);
        assert(tv4_ingress_owner_recover(&s,11,current)==TV4_FRAG_ERROR);
    }
    tv4_ingress_owner s={0};assert(!tv4_ingress_owner_init(&s,&config,1,40,7,11,0,0,UINT32_MAX,100));
    assert(tv4_ingress_owner_init(&s,&config,1,40,7,11,0,0,UINT32_MAX-1u,100));tv4_ingress_permit p=permit(&s);assert(p.epoch==UINT32_MAX);
    assert(tv4_ingress_owner_pause(&s,11,100)==TV4_GATE_REQUESTED);tv4_ingress_pause r=pause_request(&s),bad=r;bad.owner=NULL;
    assert(tv4_ingress_owner_ack(&s,bad,100)==TV4_GATE_STALE && tv4_ingress_owner_ack(&s,r,100)==TV4_GATE_PAUSED);
    assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_PAUSED && tv4_ingress_owner_resume(&s,11,100)==TV4_GATE_FAULT);
    assert(atomic_load(&s.epoch)==UINT32_MAX && tv4_ingress_owner_resume(&s,11,100)==TV4_GATE_FAULT);
    tv4_ingress_owner lost={0};init(&lost,1,100);assert(tv4_ingress_owner_pause(&lost,11,100)==TV4_GATE_REQUESTED);
    r=pause_request(&lost);assert(tv4_ingress_owner_ack(&lost,r,100)==TV4_GATE_PAUSED && tv4_ingress_owner_lost(&lost)==TV4_INGRESS_FAULT);
    assert(tv4_ingress_owner_advance(&lost,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,100)==TV4_CHANNEL_ERROR);
    puts("PASS loss/producer clock/owner context/consumer expiry/clock/exhaustion fail closed; lost after ACK blocks advance, zero take output, no live recovery");
}
static void bounded_timeout_recovery(void)
{
    tv4_ingress_owner s={0};init(&s,1,100);tv4_ingress_permit p=permit(&s);tv4_channel_tx tx={0};tv4_fragment_frame f=first(&tx,100);
    assert(tv4_ingress_owner_offer(&s,p,7,11,&f,100,100)==TV4_INGRESS_ADMITTED && tv4_ingress_owner_receive(&s,stage(&s),11,100)==TV4_FRAG_PENDING);
    assert(tv4_channel_tx_commit(&tx,&f,11,100)==TV4_FRAG_COMMITTED && tv4_channel_tx_peek(&tx,&f,11,119)==TV4_FRAG_FRAME);
    assert(tv4_ingress_owner_offer(&s,p,7,11,&f,119,119)==TV4_INGRESS_ADMITTED);
    assert(tv4_channel_tx_commit(&tx,&f,11,119)==TV4_FRAG_COMMITTED && tv4_channel_tx_peek(&tx,&f,11,119)==TV4_FRAG_FRAME);
    assert(tv4_ingress_owner_offer(&s,p,7,11,&f,119,119)==TV4_INGRESS_HELD);
    assert(tv4_ingress_owner_poll(&s,11,120)==TV4_FRAG_ERROR && s.channel.fragments.storage.error==TV4_FRAG_PROGRESS_TIMEOUT);
    assert(tv4_ingress_owner_recover(&s,11,120)==TV4_FRAG_BUSY);tv4_ingress_pause paused=pause_request(&s);
    assert(tv4_ingress_owner_ack(&s,paused,120)==TV4_GATE_BUSY);tv4_ingress_token t=stage(&s);
    assert(tv4_ingress_owner_ack(&s,paused,120)==TV4_GATE_PAUSED && tv4_ingress_owner_recover(&s,11,120)==TV4_FRAG_BUSY);
    assert(tv4_ingress_owner_receive(&s,t,11,120)==TV4_FRAG_STATE && s.channel.fragments.storage.progress==100);
    assert(tv4_ingress_owner_recover(&s,11,120)==TV4_FRAG_BUSY && tv4_ingress_owner_receive(&s,stage(&s),11,120)==TV4_FRAG_STATE);
    uint32_t issued=s.ingress.issued,head=atomic_load(&s.ingress.head),tail=atomic_load(&s.ingress.tail);
    assert(tv4_ingress_owner_recover(&s,11,120)==TV4_FRAG_OK && s.channel.fragments.storage.counter==1 && s.channel.fragments.storage.observed==120);
    assert(s.ingress.issued==issued && issued==3 && atomic_load(&s.ingress.head)==head && atomic_load(&s.ingress.tail)==tail && s.channel.policy.phase==TV4_CHANNEL_NEGOTIATION);
    assert(atomic_load(&s.gate)==TV4_GATE_PAUSED && tv4_ingress_owner_recover(&s,11,120)==TV4_FRAG_ERROR);
    assert(tv4_ingress_owner_resume(&s,11,120)==TV4_GATE_OPEN && tv4_ingress_owner_offer(&s,p,7,11,&f,119,120)==TV4_INGRESS_STALE);
    assert(tv4_channel_tx_cancel(&tx,11,120)==TV4_FRAG_OK && tv4_channel_tx_clear(&tx,11,120)==TV4_FRAG_OK);
    tv4_channel_message m=request();assert(tv4_channel_tx_begin(&tx,&m,11,120)==TV4_CHANNEL_OK);p=permit(&s);
    while(tx.active){assert(tv4_channel_tx_peek(&tx,&f,11,120)==TV4_FRAG_FRAME);assert(tv4_ingress_owner_offer(&s,p,7,11,&f,120,120)==TV4_INGRESS_ADMITTED);
        tv4_fragment_result commit=tv4_channel_tx_commit(&tx,&f,11,120);assert(commit==TV4_FRAG_COMMITTED || commit==TV4_FRAG_FINISHED);
        tv4_fragment_result received=tv4_ingress_owner_receive(&s,stage(&s),11,120);assert(received==(tx.active?TV4_FRAG_PENDING:TV4_FRAG_READY));}
    assert(tv4_ingress_owner_take(&s,&m,11,120)==TV4_CHANNEL_TAKEN_UNVERIFIED && s.channel.fragments.storage.counter==2);
    puts("PASS recover only actual fragment timeout after ACK+pending/staged/ring cleanup; explicit STATE retirement never feeds failed RX; clocks/counters/phase/atomic positions retained, gate stays paused until fresh epoch resume");
}
typedef struct {tv4_ingress_owner *s;unsigned count,frames,acks,stale_acks,stale_offers;uint32_t now;} thread_context;
static void *driver(void *arg)
{
    thread_context *c=arg;tv4_channel_tx tx={0};assert(tv4_channel_tx_init(&tx,&config,7,11,0,c->now));tv4_ingress_pause previous={0};
    for(unsigned n=0;n<c->count;++n) {
        tv4_ingress_permit p;while(tv4_ingress_owner_permit(c->s,&p)!=TV4_GATE_OPEN)sched_yield();
        tv4_channel_message m=request();assert(tv4_channel_tx_begin(&tx,&m,11,c->now)==TV4_CHANNEL_OK);
        while(tx.active){tv4_fragment_frame f;assert(tv4_channel_tx_peek(&tx,&f,11,c->now)==TV4_FRAG_FRAME);tv4_fragment_frame commit=f;
            tv4_ingress_result result=tv4_ingress_owner_offer(c->s,p,7,11,&f,c->now,c->now);assert(result==TV4_INGRESS_ADMITTED || result==TV4_INGRESS_HELD);memset(&f,0,sizeof(f));
            while(result==TV4_INGRESS_HELD){sched_yield();result=tv4_ingress_owner_retry(c->s,c->now);}assert(result==TV4_INGRESS_ADMITTED);
            tv4_fragment_result sent=tv4_channel_tx_commit(&tx,&commit,11,c->now);assert(sent==TV4_FRAG_COMMITTED || sent==TV4_FRAG_FINISHED);++c->frames;}
        for(unsigned phase=0;phase<5;++phase) {
            tv4_ingress_pause paused;while(tv4_ingress_owner_pause_request(c->s,&paused)!=TV4_GATE_REQUESTED)sched_yield();
            if(previous.owner!=NULL){assert(tv4_ingress_owner_ack(c->s,previous,c->now)==TV4_GATE_STALE);++c->stale_acks;}
            if(phase>0){tv4_fragment_frame old={.length=1};assert(tv4_ingress_owner_offer(c->s,p,7,11,&old,c->now,c->now)==TV4_INGRESS_STALE);++c->stale_offers;}
            tv4_ingress_gate_result ack=tv4_ingress_owner_ack(c->s,paused,c->now);
            while(ack==TV4_GATE_BUSY){sched_yield();ack=tv4_ingress_owner_ack(c->s,paused,c->now);}assert(ack==TV4_GATE_PAUSED);++c->acks;previous=paused;
        }
    }
    return NULL;
}
static void threaded_cycles(void)
{
    unsigned frames=0,acks=0,stale_acks=0,stale_offers=0;
    for(unsigned capacity=1;capacity<=4;++capacity){tv4_ingress_owner s={0};uint32_t now=UINT32_MAX-10u;init(&s,capacity,now);
        thread_context c={.s=&s,.count=64,.now=now};pthread_t thread;assert(pthread_create(&thread,NULL,driver,&c)==0);
        for(unsigned n=0;n<c.count;++n){bool taken=false;
            while(!taken){tv4_ingress_token t;tv4_ingress_result staged=tv4_ingress_owner_stage(&s,&t);
                if(staged==TV4_INGRESS_UNCHECKED){tv4_fragment_result r=tv4_ingress_owner_receive(&s,t,11,now);assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);}
                else assert(staged==TV4_INGRESS_WAIT);
                tv4_channel_message m;tv4_channel_result take=tv4_ingress_owner_take(&s,&m,11,now);
                if(take==TV4_CHANNEL_TAKEN_UNVERIFIED){assert(m.kind==TV4_CHANNEL_WIRE && m.body.wire.kind==TV4_WIRE_REQUEST);taken=true;}else assert(take==TV4_CHANNEL_BUSY || take==TV4_CHANNEL_STATE);
                sched_yield();}
            for(unsigned phase=0;phase<5;++phase){tv4_ingress_gate_result requested=tv4_ingress_owner_pause(&s,11,now);
                assert(requested==TV4_GATE_REQUESTED || requested==TV4_GATE_PAUSED); /* driver ACK may win before pause returns */
                tv4_ingress_gate_result r=tv4_ingress_owner_quiescent(&s,11,now);while(r==TV4_GATE_BUSY){sched_yield();r=tv4_ingress_owner_quiescent(&s,11,now);}assert(r==TV4_GATE_PAUSED);
                tv4_channel_phase next=phase==4?TV4_CHANNEL_NEGOTIATION:(tv4_channel_phase)(phase+1u);
                tv4_update_id id=phase==1?(tv4_update_id){11,2}:phase==2?(tv4_update_id){11,1}:(tv4_update_id){0};
                assert(tv4_ingress_owner_advance(&s,next,id,11,now)==TV4_CHANNEL_OK && tv4_ingress_owner_resume(&s,11,now)==TV4_GATE_OPEN);}
        }
        assert(pthread_join(thread,NULL)==0 && c.frames==2112 && c.acks==320 && c.stale_acks==319 && c.stale_offers==256);
        assert(s.ingress.issued==c.frames && s.channel.fragments.storage.counter==64 && atomic_load(&s.epoch)==321);
        frames+=c.frames;acks+=c.acks;stale_acks+=c.stale_acks;stale_offers+=c.stale_offers;
    }
    printf("PASS256 actual two-actor channel messages/%u frames/%u acknowledged phase cutovers/%u stale ACKs/%u stale offers; capacity1..4, fixed synthetic near-wrap clock, counters retained\n",frames,acks,stale_acks,stale_offers);
}
typedef struct {tv4_ingress_owner *s;atomic_uint gate;unsigned which;} fault_context;
static void wait_gate(fault_context *c,unsigned n){while(atomic_load_explicit(&c->gate,memory_order_acquire)!=n)sched_yield();}
static void set_gate(fault_context *c,unsigned n){atomic_store_explicit(&c->gate,n,memory_order_release);}
static void *controlled_driver(void *arg)
{
    fault_context *c=arg;tv4_ingress_permit p=permit(c->s);set_gate(c,1);wait_gate(c,2);tv4_fragment_frame f={.length=1};
    uint32_t captured=c->which==4?119u:100u,now=c->which==4?120u:100u;
    if(c->which>0){assert(tv4_ingress_owner_offer(c->s,p,7,11,&f,captured,captured)==TV4_INGRESS_ADMITTED);
        if(c->which==2 || c->which==4)assert(tv4_ingress_owner_offer(c->s,p,7,11,&f,captured,captured)==TV4_INGRESS_HELD);}
    set_gate(c,3);wait_gate(c,4);tv4_ingress_pause request_token=pause_request(c->s);
    if(c->which==0)assert(tv4_ingress_owner_offer(c->s,p,7,11,&f,100,100)==TV4_INGRESS_BUSY);
    tv4_ingress_gate_result r=tv4_ingress_owner_ack(c->s,request_token,now);
    if(c->which==2 || c->which==4){assert(r==TV4_GATE_BUSY);set_gate(c,5);wait_gate(c,6);r=tv4_ingress_owner_ack(c->s,request_token,now);}
    assert(r==TV4_GATE_PAUSED);
    if(c->which==3)assert(tv4_ingress_owner_lost(c->s)==TV4_INGRESS_FAULT);
    set_gate(c,7);return NULL;
}
static void controlled_races(void)
{
    for(unsigned which=0;which<5;++which){tv4_ingress_owner s={0};init(&s,1,100);
        if(which==4){tv4_channel_tx tx={0};tv4_fragment_frame f=first(&tx,100);tv4_ingress_permit p=permit(&s);
            assert(tv4_ingress_owner_offer(&s,p,7,11,&f,100,100)==TV4_INGRESS_ADMITTED && tv4_ingress_owner_receive(&s,stage(&s),11,100)==TV4_FRAG_PENDING);}
        fault_context c={.s=&s,.which=which};atomic_init(&c.gate,0u);
        pthread_t thread;assert(pthread_create(&thread,NULL,controlled_driver,&c)==0);wait_gate(&c,1);
        assert(tv4_frame_ingress_empty(&s.ingress)==TV4_INGRESS_WAIT && tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_BUSY);
        set_gate(&c,2);wait_gate(&c,3);uint32_t now=which==4?120u:100u;
        assert(tv4_ingress_owner_pause(&s,11,now)==(which==4?TV4_GATE_FAULT:TV4_GATE_REQUESTED));
        assert(tv4_ingress_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,now)==(which==4?TV4_CHANNEL_ERROR:TV4_CHANNEL_BUSY));set_gate(&c,4);
        tv4_ingress_token held={0};if(which==2 || which==4){wait_gate(&c,5);held=stage(&s);set_gate(&c,6);}wait_gate(&c,7);
        if(which==3)assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_FAULT && tv4_ingress_owner_resume(&s,11,100)==TV4_GATE_FAULT);
        else if(which==4) {
            assert(tv4_ingress_owner_recover(&s,11,now)==TV4_FRAG_BUSY && tv4_ingress_owner_receive(&s,held,11,now)==TV4_FRAG_STATE);
            assert(tv4_ingress_owner_recover(&s,11,now)==TV4_FRAG_BUSY && tv4_ingress_owner_receive(&s,stage(&s),11,now)==TV4_FRAG_STATE);
            assert(s.channel.fragments.storage.progress==100 && tv4_ingress_owner_recover(&s,11,now)==TV4_FRAG_OK && s.channel.fragments.storage.counter==1);
            assert(s.channel.fragments.storage.observed==now && tv4_ingress_owner_resume(&s,11,now)==TV4_GATE_OPEN);
        }else {
            if(which>0)assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_BUSY);
            if(which==2)assert(tv4_ingress_owner_receive(&s,held,11,100)==TV4_FRAG_BAD_FRAME);
            if(which>0)assert(tv4_ingress_owner_receive(&s,stage(&s),11,100)==TV4_FRAG_BAD_FRAME);
            assert(tv4_ingress_owner_quiescent(&s,11,100)==TV4_GATE_PAUSED && tv4_ingress_owner_advance(&s,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,100)==TV4_CHANNEL_OK);
        }
        assert(pthread_join(thread,NULL)==0);
    }
    puts("PASS5 controlled races: early empty/in-flight permit, published frame after empty, full pending+staged ACK barrier, loss after ACK, lower-timeout ACK+retirement+recovery; no external stop/drain boolean");
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--threads")==0){threaded_cycles();controlled_races();return 0;}
    assert(argc==1);concrete_drain_and_epoch();faults_and_arguments();bounded_timeout_recovery();
    printf("HOST ingress owner=%zu permit=%zu pause=%zu bytes; not MCU RAM/full stack/physical drain approval\n",sizeof(tv4_ingress_owner),sizeof(tv4_ingress_permit),sizeof(tv4_ingress_pause));return 0;
}
