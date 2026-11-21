#include "frame_ingress.h"
#include "update_channel.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_fragment_frame frame(uint32_t n)
{
    tv4_fragment_frame f={.length=8};
    for(unsigned i=0;i<4;++i){f.bytes[i]=(uint8_t)(n>>(8u*i));f.bytes[4u+i]=(uint8_t)~f.bytes[i];}return f;
}
static void init(tv4_frame_ingress *m,unsigned capacity,uint32_t now)
{assert(tv4_frame_ingress_init(m,capacity,40,7,11,0,now));}
static tv4_ingress_token stage(tv4_frame_ingress *m)
{tv4_ingress_token t;assert(tv4_frame_ingress_stage(m,&t)==TV4_INGRESS_UNCHECKED);return t;}
static void fifo_and_tokens(void)
{
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_frame_ingress m={0};init(&m,capacity,100);tv4_fragment_frame f=frame(1),out;
        for(unsigned n=1;n<=capacity+1u;++n){f=frame(n);assert(tv4_frame_ingress_offer(&m,7,11,&f,100,100)==(n<=capacity?TV4_INGRESS_ADMITTED:TV4_INGRESS_HELD));memset(&f,0,sizeof(f));}
        f=frame(99);assert(tv4_frame_ingress_offer(&m,7,11,&f,101,101)==TV4_INGRESS_BUSY);
        assert(m.issued==capacity+1u && m.producer_pending.captured==100 && tv4_frame_ingress_empty(&m)==TV4_INGRESS_BUSY);
        assert(tv4_frame_ingress_retry(&m,101)==TV4_INGRESS_HELD);
        tv4_ingress_token first=stage(&m),wrong=first;wrong.serial++;
        assert(tv4_frame_ingress_check(&m,wrong,&out,101)==TV4_INGRESS_STALE && zero(&out,sizeof(out)));
        wrong=first;wrong.owner=NULL;assert(tv4_frame_ingress_check(&m,wrong,&out,101)==TV4_INGRESS_STALE);
        tv4_ingress_token blocked;memset(&blocked,0xa5,sizeof(blocked));assert(tv4_frame_ingress_stage(&m,&blocked)==TV4_INGRESS_BUSY && zero(&blocked,sizeof(blocked)));
        assert(tv4_frame_ingress_retry(&m,102)==TV4_INGRESS_ADMITTED && !m.producer_has_pending);
        assert(tv4_frame_ingress_check(&m,first,&out,102)==TV4_INGRESS_FRAME);f=frame(1);assert(memcmp(&f,&out,sizeof(f))==0);
        assert(tv4_frame_ingress_check(&m,first,&out,102)==TV4_INGRESS_STALE && zero(&out,sizeof(out)));
        for(unsigned n=2;n<=capacity+1u;++n){tv4_ingress_token t=stage(&m);assert(t.serial==n && t.captured==100);assert(tv4_frame_ingress_check(&m,t,&out,103)==TV4_INGRESS_FRAME);f=frame(n);assert(memcmp(&f,&out,sizeof(f))==0);}
        assert(tv4_frame_ingress_empty(&m)==TV4_INGRESS_WAIT && tv4_frame_ingress_retry(&m,103)==TV4_INGRESS_WAIT);
        assert(!tv4_frame_ingress_init(&m,capacity,40,7,11,0,103));
    }
    puts("PASS capacity1..4 FIFO/copy-before-release/input wipe/full retains first/original time/exact token/no live reinit");
}
static void faults_and_boundaries(void)
{
    for(unsigned which=0;which<7;++which) {
        uint32_t start=which==6?UINT32_MAX-10u:100u;
        tv4_frame_ingress m={0};init(&m,1,start);tv4_fragment_frame f=frame(1),out;
        assert(tv4_frame_ingress_offer(&m,8,11,&f,start,start)==TV4_INGRESS_IDENTITY);
        assert(tv4_frame_ingress_offer(&m,7,12,&f,start,start)==TV4_INGRESS_IDENTITY);
        f.length=9;assert(tv4_frame_ingress_offer(&m,7,11,&f,start,start)==TV4_INGRESS_ARGUMENT);f=frame(1);
        assert(tv4_frame_ingress_offer(&m,7,11,&f,start,start)==TV4_INGRESS_ADMITTED);
        tv4_ingress_token t=stage(&m);
        if(which==0)assert(tv4_frame_ingress_lost(&m)==TV4_INGRESS_FAULT);
        if(which==1)assert(tv4_frame_ingress_retry(&m,start-1u)==TV4_INGRESS_FAULT);
        if(which==2){m.issued=UINT32_MAX;assert(tv4_frame_ingress_offer(&m,7,11,&f,start,start)==TV4_INGRESS_FAULT);}
        uint32_t now=which==3?start+40u:which==4?start-1u:start+39u;
        bool good=which>=5;
        assert(tv4_frame_ingress_check(&m,t,&out,now)==(good?TV4_INGRESS_FRAME:TV4_INGRESS_FAULT));
        if(good)assert(memcmp(&out,&f,sizeof(f))==0 && tv4_frame_ingress_faults(&m)==0);
        else {
            unsigned reason=which==0?TV4_INGRESS_OVERRUN:which==2?TV4_INGRESS_EXHAUSTED:which==3?TV4_INGRESS_EXPIRED:TV4_INGRESS_CLOCK;
            assert(zero(&out,sizeof(out)) && (tv4_frame_ingress_faults(&m)&reason)!=0);
            assert(tv4_frame_ingress_empty(&m)==TV4_INGRESS_FAULT && tv4_frame_ingress_stage(&m,&t)==TV4_INGRESS_FAULT && zero(&t,sizeof(t)));
            assert(tv4_frame_ingress_offer(&m,7,11,&f,start+40u,start+40u)==TV4_INGRESS_FAULT);
        }
    }
    tv4_frame_ingress m={0};init(&m,1,100);tv4_fragment_frame f=frame(1),out;
    assert(tv4_frame_ingress_offer(&m,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);f=frame(2);
    assert(tv4_frame_ingress_offer(&m,7,11,&f,100,100)==TV4_INGRESS_HELD);
    tv4_ingress_token t=stage(&m);assert(tv4_frame_ingress_check(&m,t,&out,139)==TV4_INGRESS_FRAME);
    assert(tv4_frame_ingress_retry(&m,139)==TV4_INGRESS_ADMITTED);t=stage(&m);
    assert(t.captured==100 && tv4_frame_ingress_check(&m,t,&out,140)==TV4_INGRESS_FAULT && zero(&out,sizeof(out)));
    assert(tv4_frame_ingress_faults(&m)==TV4_INGRESS_EXPIRED);
    tv4_frame_ingress last={0};assert(tv4_frame_ingress_init(&last,1,40,7,11,UINT32_MAX-1u,100));f=frame(3);
    assert(tv4_frame_ingress_offer(&last,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);t=stage(&last);
    assert(t.serial==UINT32_MAX && tv4_frame_ingress_check(&last,t,&out,100)==TV4_INGRESS_FRAME);
    assert(tv4_frame_ingress_offer(&last,7,11,&f,100,100)==TV4_INGRESS_FAULT && last.issued==UINT32_MAX);
    puts("PASS clock wrap/backwards/original age39 accepts40 refuses/retained retry age/overrun/exhaustion sticky; zero refusals");
}
static void arguments_and_original_channel_clock(void)
{
    tv4_frame_ingress invalid={0};assert(tv4_frame_ingress_faults(&invalid)==UINT_MAX && tv4_frame_ingress_faults(NULL)==UINT_MAX);
    assert(!tv4_frame_ingress_init(&invalid,0,40,7,11,0,100) && !tv4_frame_ingress_init(&invalid,5,40,7,11,0,100));
    assert(!tv4_frame_ingress_init(&invalid,1,0,7,11,0,100) && !tv4_frame_ingress_init(&invalid,1,UINT32_C(0x80000000),7,11,0,100));
    for(unsigned n=0;n<=255;++n){tv4_frame_ingress m={0};init(&m,1,100);tv4_fragment_frame f=frame(1);f.length=(uint8_t)n;
        tv4_ingress_result r=tv4_frame_ingress_offer(&m,7,11,&f,100,100);assert(r==(n>0 && n<=8?TV4_INGRESS_ADMITTED:TV4_INGRESS_ARGUMENT));}
    tv4_frame_ingress m={0};init(&m,1,100);tv4_channel_tx tx={0};tv4_channel_rx rx={0};
    tv4_fragment_config config={20,1000,200,TV4_FRAGMENT_VERSION};
    assert(tv4_channel_tx_init(&tx,&config,7,11,0,100) && tv4_channel_rx_init(&rx,&config,7,11,0,100));
    tv4_channel_message msg={.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,.body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};
    assert(tv4_channel_tx_begin(&tx,&msg,11,100)==TV4_CHANNEL_OK);tv4_fragment_frame f,out;
    assert(tv4_channel_tx_peek(&tx,&f,11,100)==TV4_FRAG_FRAME && tv4_channel_rx_receive(&rx,7,&f,11,100)==TV4_FRAG_PENDING);
    assert(tv4_channel_tx_commit(&tx,&f,11,100)==TV4_FRAG_COMMITTED && tv4_channel_tx_peek(&tx,&f,11,110)==TV4_FRAG_FRAME);
    assert(tv4_frame_ingress_offer(&m,7,11,&f,110,110)==TV4_INGRESS_ADMITTED);tv4_ingress_token t=stage(&m),bad=t;
    bad.captured++;assert(tv4_frame_ingress_check(&m,bad,&out,120)==TV4_INGRESS_STALE && zero(&out,sizeof(out)));
    bad=t;bad.route++;assert(tv4_frame_ingress_check(&m,bad,&out,120)==TV4_INGRESS_STALE);
    bad=t;bad.generation++;assert(tv4_frame_ingress_check(&m,bad,&out,120)==TV4_INGRESS_STALE);
    assert(tv4_frame_ingress_check(&m,t,&out,120)==TV4_INGRESS_FRAME);
    assert(tv4_channel_rx_receive(&rx,7,&out,11,120)==TV4_FRAG_ERROR && rx.fragments.storage.error==TV4_FRAG_PROGRESS_TIMEOUT);
    assert(rx.fragments.storage.started==100 && rx.fragments.storage.progress==100);
    puts("PASS256 DLC shapes/config/token binding; ingress FRAME does not revive original channel timeout at current service time");
}
static void external_retention_keeps_capture(void)
{
    tv4_frame_ingress m={0};init(&m,1,100);tv4_fragment_frame f=frame(1),out;
    assert(tv4_frame_ingress_offer(&m,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);f=frame(2);
    assert(tv4_frame_ingress_offer(&m,7,11,&f,101,101)==TV4_INGRESS_HELD);f=frame(3);
    assert(tv4_frame_ingress_offer(&m,7,11,&f,102,102)==TV4_INGRESS_BUSY && m.issued==2);
    tv4_ingress_token t=stage(&m);assert(tv4_frame_ingress_check(&m,t,&out,103)==TV4_INGRESS_FRAME);
    assert(tv4_frame_ingress_retry(&m,103)==TV4_INGRESS_ADMITTED);t=stage(&m);assert(tv4_frame_ingress_check(&m,t,&out,104)==TV4_INGRESS_FRAME);
    assert(tv4_frame_ingress_offer(&m,7,11,&f,102,105)==TV4_INGRESS_ADMITTED);memset(&f,0,sizeof(f));t=stage(&m);
    assert(t.captured==102 && tv4_frame_ingress_check(&m,t,&out,141)==TV4_INGRESS_FRAME);f=frame(3);assert(memcmp(&f,&out,sizeof(f))==0);
    assert(tv4_frame_ingress_offer(&m,7,11,&f,102,142)==TV4_INGRESS_FAULT && tv4_frame_ingress_faults(&m)==TV4_INGRESS_EXPIRED && m.issued==3);
    tv4_frame_ingress future={0};init(&future,1,100);
    assert(tv4_frame_ingress_offer(&future,7,11,&f,101,100)==TV4_INGRESS_FAULT && tv4_frame_ingress_faults(&future)==TV4_INGRESS_CLOCK);
    puts("PASS BUSY caller retains original capture across later offer; fresh producer time cannot refresh external age, future capture refused");
}
typedef struct {tv4_frame_ingress *m;unsigned count;bool channel;uint32_t now;} thread_context;
static void send_frame(thread_context *c,tv4_fragment_frame *f)
{
    tv4_ingress_result r=tv4_frame_ingress_offer(c->m,7,11,f,c->now,c->now);
    assert(r==TV4_INGRESS_ADMITTED || r==TV4_INGRESS_HELD);memset(f,0,sizeof(*f));
    while(r==TV4_INGRESS_HELD){sched_yield();r=tv4_frame_ingress_retry(c->m,c->now);}
    assert(r==TV4_INGRESS_ADMITTED);
}
static tv4_channel_message request(void)
{return (tv4_channel_message){.kind=TV4_CHANNEL_WIRE,.body.wire={.kind=TV4_WIRE_REQUEST,.body.negotiation={.kind=TV4_NEG_REQUEST,.request_id={11,1},.descriptor={.image_bytes=1000},.responder_boot=22,.block_bytes=64}}};}
static void *producer(void *arg)
{
    thread_context *c=arg;
    if(!c->channel){for(unsigned n=1;n<=c->count;++n){tv4_fragment_frame f=frame(n);send_frame(c,&f);}return NULL;}
    tv4_channel_tx tx={0};tv4_fragment_config config={20,1000,200,TV4_FRAGMENT_VERSION};
    assert(tv4_channel_tx_init(&tx,&config,7,11,0,c->now));
    for(unsigned n=0;n<c->count;++n){tv4_channel_message m=request();assert(tv4_channel_tx_begin(&tx,&m,11,c->now)==TV4_CHANNEL_OK);
        while(tx.active){tv4_fragment_frame f;assert(tv4_channel_tx_peek(&tx,&f,11,c->now)==TV4_FRAG_FRAME);tv4_fragment_frame commit=f;send_frame(c,&f);
            tv4_fragment_result r=tv4_channel_tx_commit(&tx,&commit,11,c->now);assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);}}
    return NULL;
}
static void threads(void)
{
    unsigned total=0,frames=0,messages=0;
    for(unsigned capacity=1;capacity<=4;++capacity)for(unsigned channel=0;channel<2;++channel) {
        tv4_frame_ingress m={0};uint32_t now=UINT32_MAX-20u;init(&m,capacity,now);
        thread_context c={&m,channel?64u:200000u,channel!=0,now};pthread_t thread;
        tv4_channel_rx rx={0};tv4_fragment_config config={20,1000,200,TV4_FRAGMENT_VERSION};
        if(channel)assert(tv4_channel_rx_init(&rx,&config,7,11,0,now));
        assert(pthread_create(&thread,NULL,producer,&c)==0);unsigned got=0,nframes=0;
        while(got<c.count) {
            tv4_ingress_token t;tv4_ingress_result r=tv4_frame_ingress_stage(&m,&t);
            if(r==TV4_INGRESS_WAIT){sched_yield();continue;}assert(r==TV4_INGRESS_UNCHECKED);
            tv4_fragment_frame out;assert(tv4_frame_ingress_check(&m,t,&out,now)==TV4_INGRESS_FRAME);++nframes;
            if(!channel){tv4_fragment_frame expected=frame(++got);assert(memcmp(&expected,&out,sizeof(out))==0);}
            else {tv4_fragment_result received=tv4_channel_rx_receive(&rx,7,&out,11,now);
                if(received==TV4_FRAG_READY){tv4_channel_message message;assert(tv4_channel_rx_take(&rx,&message,11,now)==TV4_CHANNEL_TAKEN_UNVERIFIED);assert(message.kind==TV4_CHANNEL_WIRE && message.body.wire.kind==TV4_WIRE_REQUEST && message.body.wire.body.negotiation.request_id.boot==11);++got;}
                else assert(received==TV4_FRAG_PENDING);}
        }
        assert(pthread_join(thread,NULL)==0 && tv4_frame_ingress_empty(&m)==TV4_INGRESS_WAIT && tv4_frame_ingress_faults(&m)==0);
        if(channel){assert(nframes==2112);frames+=nframes;messages+=got;}else total+=got;
    }
    printf("PASS two real actors: %u owned FIFO frames; %u actual channel messages/%u fragments; capacity1..4, input wiped, fixed synthetic near-wrap clock\n",total,messages,frames);
}
typedef struct {tv4_frame_ingress *m;atomic_uint gate;unsigned which;} fault_context;
static void *fault_producer(void *arg)
{
    fault_context *c=arg;tv4_fragment_frame f=frame(1);
    assert(tv4_frame_ingress_offer(c->m,7,11,&f,100,100)==TV4_INGRESS_ADMITTED);atomic_store_explicit(&c->gate,1u,memory_order_release);
    while(atomic_load_explicit(&c->gate,memory_order_acquire)!=2)sched_yield();
    if(c->which==0)assert(tv4_frame_ingress_lost(c->m)==TV4_INGRESS_FAULT);
    if(c->which==1)assert(tv4_frame_ingress_retry(c->m,99)==TV4_INGRESS_FAULT);
    atomic_store_explicit(&c->gate,3u,memory_order_release);return NULL;
}
static void concurrent_faults(void)
{
    for(unsigned which=0;which<3;++which){tv4_frame_ingress m={0};init(&m,1,100);fault_context c={.m=&m,.which=which};atomic_init(&c.gate,0u);
        pthread_t thread;assert(pthread_create(&thread,NULL,fault_producer,&c)==0);
        while(atomic_load_explicit(&c.gate,memory_order_acquire)!=1)sched_yield();
        tv4_ingress_token t=stage(&m);
        atomic_store_explicit(&c.gate,2u,memory_order_release);while(atomic_load_explicit(&c.gate,memory_order_acquire)!=3)sched_yield();
        tv4_fragment_frame out;assert(tv4_frame_ingress_check(&m,t,&out,which==2?140u:100u)==TV4_INGRESS_FAULT && zero(&out,sizeof(out)));
        assert(pthread_join(thread,NULL)==0 && tv4_frame_ingress_faults(&m)!=0);}
    puts("PASS3 controlled two-actor loss/producer-clock/consumer-age refusals after stage, no payload exposure");
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--threads")==0){threads();concurrent_faults();return 0;}
    assert(argc==1);fifo_and_tokens();faults_and_boundaries();arguments_and_original_channel_clock();external_retention_keeps_capture();
    printf("host ingress=%zu token=%zu entry=%zu bytes, not MCU RAM/physical timing approval\n",sizeof(tv4_frame_ingress),sizeof(tv4_ingress_token),sizeof(tv4_ingress_entry));return 0;
}
