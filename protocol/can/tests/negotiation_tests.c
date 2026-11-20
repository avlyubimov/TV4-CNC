#include "update_negotiation.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    tv4_neg_pool a,b;
    tv4_negotiation tx,rx;
    tv4_neg_image image;
    uint8_t tag[32];
    uint32_t now;
} pair;
static tv4_neg_limits limits(void)
{
    return (tv4_neg_limits){.max_image_bytes=4096,.retry_ms=10,.total_ms=1000,.block_ceiling=256,.send_limit=8};
}
static void setup(pair *p,uint32_t start,uint32_t last_a,uint32_t last_b)
{
    memset(p,0,sizeof(*p));p->now=start;p->tag[0]=9;p->image.image_bytes=777;
    memset(p->image.image_sha256,0x35,32);memset(p->image.metadata_sha256,0x51,32);
    assert(tv4_neg_pool_init(&p->a,11,22,last_a));assert(tv4_neg_pool_init(&p->b,22,11,last_b));
    tv4_neg_limits r=limits();r.block_ceiling=64;
    assert(tv4_neg_init(&p->tx,&p->a,TV4_NEG_REQUESTER,limits(),p->tag,101,start));
    assert(tv4_neg_init(&p->rx,&p->b,TV4_NEG_RESPONDER,r,p->tag,202,start));
}
static tv4_neg_result receive(pair *p,tv4_negotiation *s,const tv4_neg_message *m)
{
    return tv4_neg_receive(s,m,s==&p->tx?101:202,p->now);
}
static tv4_neg_message next(pair *p,tv4_negotiation *s)
{
    tv4_neg_message m;
    assert(tv4_neg_next(s,&m,s==&p->tx?101:202,p->now)==TV4_NEG_SENT);return m;
}
static void request_offer(pair *p,tv4_neg_message *req,tv4_neg_message *off)
{
    assert(tv4_neg_begin(&p->tx,&p->image,101,p->now)==TV4_NEG_OK);
    *req=next(p,&p->tx);assert(req->kind==TV4_NEG_REQUEST);
    assert(receive(p,&p->rx,req)==TV4_NEG_OK);
    *off=next(p,&p->rx);assert(off->kind==TV4_NEG_OFFER);
    assert(receive(p,&p->tx,off)==TV4_NEG_OK);
}
static void ready(pair *p)
{
    tv4_neg_message req,off;request_offer(p,&req,&off);
    tv4_neg_message ack=next(p,&p->tx);assert(ack.kind==TV4_NEG_ACCEPT);
    assert(receive(p,&p->rx,&ack)==TV4_NEG_OK);
    tv4_neg_message confirm=next(p,&p->rx);assert(confirm.kind==TV4_NEG_CONFIRM);
    assert(receive(p,&p->tx,&confirm)==TV4_NEG_OK);
}
static tv4_negotiated take_close(pair *p)
{
    tv4_negotiated a,b,unchanged;memset(&unchanged,0xa5,sizeof(unchanged));
    assert(tv4_neg_take(&p->tx,&a,101,p->now)==TV4_NEG_OK);
    assert(tv4_neg_take(&p->rx,&b,202,p->now)==TV4_NEG_OK);
    assert(a.descriptor.id.boot==22 && a.descriptor.id.transfer==b.descriptor.id.transfer);
    assert(a.descriptor.image_bytes==p->image.image_bytes && a.block_bytes==64 && b.block_bytes==64);
    assert(!memcmp(a.descriptor.image_sha256,p->image.image_sha256,32));
    assert(!memcmp(a.metadata_sha256,p->image.metadata_sha256,32));
    assert(!memcmp(a.metadata_sha256,b.metadata_sha256,32));
    assert(tv4_neg_take(&p->tx,&unchanged,101,p->now)==TV4_NEG_REJECT_STATE);
    for(size_t i=0;i<sizeof(unchanged);i++)assert(((const uint8_t *)&unchanged)[i]==0xa5);
    assert(tv4_neg_close(&p->tx,101,p->now)==TV4_NEG_OK);
    assert(tv4_neg_close(&p->rx,202,p->now)==TV4_NEG_OK);
    assert(p->a.owner==NULL && p->b.owner==NULL);
    assert(tv4_neg_clear(&p->tx) && tv4_neg_clear(&p->rx));return a;
}
static void configuration_and_ownership(void)
{
    tv4_neg_pool pool={0};tv4_negotiation s={0};uint8_t tag[32]={1};
    assert(!tv4_neg_pool_init(NULL,11,22,0));assert(!tv4_neg_pool_init(&pool,0,22,0));
    assert(!tv4_neg_pool_init(&pool,11,11,0));assert(tv4_neg_pool_init(&pool,11,22,7));
    assert(!tv4_neg_pool_init(&pool,12,23,0) && pool.local_boot==11 && pool.last_issued==7);
    for(unsigned which=0;which<8;which++) {
        tv4_neg_limits l=limits();
        switch(which) {
        case 0:l.retry_ms=0;break;case 1:l.total_ms=l.retry_ms;break;
        case 2:l.total_ms=UINT32_MAX;break;case 3:l.block_ceiling=0;break;
        case 4:l.block_ceiling=257;break;case 5:l.send_limit=0;break;
        case 6:l.max_image_bytes=0;break;default:l.retry_ms=UINT32_MAX;break;
        }
        assert(!tv4_neg_init(&s,&pool,TV4_NEG_REQUESTER,l,tag,1,0) && !s.configured);
    }
    assert(!tv4_neg_init(&s,&pool,(tv4_neg_role)99,limits(),tag,1,0));
    assert(!tv4_neg_init(&s,&pool,TV4_NEG_REQUESTER,limits(),NULL,1,0));
    tag[0]=0;assert(!tv4_neg_init(&s,&pool,TV4_NEG_REQUESTER,limits(),tag,1,0));tag[0]=1;
    assert(!tv4_neg_init(&s,&pool,TV4_NEG_REQUESTER,limits(),tag,0,0));
    assert(tv4_neg_init(&s,&pool,TV4_NEG_REQUESTER,limits(),tag,1,0));
    assert(!tv4_neg_init(&s,&pool,TV4_NEG_REQUESTER,limits(),tag,1,0));
    assert(!tv4_neg_clear(&s));assert(tv4_neg_begin(&s,NULL,1,0)==TV4_NEG_REJECT_ARGUMENT);
    tv4_neg_image image={.image_bytes=1};assert(tv4_neg_begin(&s,&image,1,0)==TV4_NEG_OK);
    tv4_negotiation other={0};assert(tv4_neg_init(&other,&pool,TV4_NEG_REQUESTER,limits(),tag,1,0));
    assert(tv4_neg_begin(&other,&image,1,0)==TV4_NEG_BUSY && pool.last_issued==8 && pool.owner==&s);
    assert(!tv4_neg_clear(&s));assert(tv4_neg_close(&s,1,0)==TV4_NEG_REJECT_STATE);
    assert(tv4_neg_abort(&s,1,0)==TV4_NEG_OK && pool.owner==NULL && tv4_neg_clear(&s));
    assert(tv4_neg_begin(&other,&image,1,0)==TV4_NEG_OK && pool.last_issued==9);
    assert(tv4_neg_abort(&other,1,0)==TV4_NEG_OK);
}
static void fresh_ids_replay_and_snapshots(void)
{
    pair p;setup(&p,0,0,0);
    tv4_neg_message req,off;request_offer(&p,&req,&off);
    uint32_t start=p.tx.started_ms;
    p.image.image_sha256[0]^=1;
    assert(p.tx.request.descriptor.image_sha256[0]==0x35);p.image.image_sha256[0]^=1;
    off.block_bytes=31;assert(p.tx.offer.block_bytes==64);
    assert(receive(&p,&p.rx,&req)==TV4_NEG_DUPLICATE && p.b.last_issued==1);
    off=next(&p,&p.rx);assert(receive(&p,&p.tx,&off)==TV4_NEG_DUPLICATE);
    tv4_neg_message accept=next(&p,&p.tx);
    assert(receive(&p,&p.rx,&accept)==TV4_NEG_OK);
    tv4_neg_message confirm=next(&p,&p.rx);
    assert(receive(&p,&p.tx,&confirm)==TV4_NEG_OK);
    assert(receive(&p,&p.tx,&confirm)==TV4_NEG_DUPLICATE && p.tx.started_ms==start);
    tv4_negotiated old=take_close(&p);assert(old.descriptor.id.transfer==1);
    assert(receive(&p,&p.rx,&req)==TV4_NEG_REJECT_SESSION && p.b.last_issued==1);
    assert(receive(&p,&p.rx,&accept)==TV4_NEG_REJECT_STATE);
    ready(&p);assert(receive(&p,&p.tx,&confirm)==TV4_NEG_REJECT_SESSION);
    tv4_negotiated fresh=take_close(&p);assert(fresh.descriptor.id.transfer==2 && p.a.last_issued==2);
    tv4_negotiation new_receiver={0};assert(tv4_neg_init(&new_receiver,&p.b,TV4_NEG_RESPONDER,limits(),p.tag,202,p.now));
    assert(tv4_neg_receive(&new_receiver,&req,202,p.now)==TV4_NEG_REJECT_SESSION && p.b.last_peer_request==2);
}
static void dropped_packets_and_repeated_sessions(void)
{
    pair p;setup(&p,0,0,0);uint32_t last=0;
    for(unsigned session=0;session<1024;session++) {
        assert(tv4_neg_begin(&p.tx,&p.image,101,p.now)==TV4_NEG_OK);
        unsigned dropped=0,mask=(session%16u)<<1;
        for(unsigned step=0;step<40 && (p.tx.state!=TV4_NEG_READY || p.rx.state!=TV4_NEG_READY);step++) {
            for(unsigned direction=0;direction<2;direction++) {
                tv4_negotiation *from=direction?&p.rx:&p.tx,*to=direction?&p.tx:&p.rx;
                tv4_neg_message message;
                tv4_neg_result result=tv4_neg_next(from,&message,direction?202:101,p.now);
                if(result==TV4_NEG_WAIT)continue;
                assert(result==TV4_NEG_SENT);
                unsigned bit=1u<<(unsigned)message.kind;
                if((mask&bit)!=0 && (dropped&bit)==0) { dropped|=bit;continue; }
                result=receive(&p,to,&message);assert(result==TV4_NEG_OK || result==TV4_NEG_DUPLICATE);
            }
            p.now+=10;
        }
        assert(p.tx.state==TV4_NEG_READY && p.rx.state==TV4_NEG_READY && dropped==mask);
        tv4_negotiated n=take_close(&p);assert(n.descriptor.id.transfer>last);last=n.descriptor.id.transfer;
    }
    assert(p.a.last_issued==1024 && p.b.last_issued==1024 && p.b.last_peer_request==1024);
}
static void conflicts_and_canonical_rejections(void)
{
    for(unsigned field=0;field<4;field++) {
        pair p;setup(&p,0,0,0);tv4_neg_message req,off;request_offer(&p,&req,&off);
        switch(field) {
        case 0:req.descriptor.image_sha256[0]^=1;break;
        case 1:req.metadata_sha256[31]^=1;break;
        case 2:++req.descriptor.image_bytes;break;
        default:--req.block_bytes;break;
        }
        assert(receive(&p,&p.rx,&req)==TV4_NEG_ERROR && p.rx.error==TV4_NEG_ERROR_CONFLICT);
        assert(p.b.owner==NULL && p.b.last_issued==1 && p.b.last_peer_request==1);
        assert(tv4_neg_abort(&p.tx,101,0)==TV4_NEG_OK);
    }
    for(unsigned field=0;field<9;field++) {
        pair p;setup(&p,0,0,0);assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_OK);
        tv4_neg_message m=next(&p,&p.tx);
        switch(field) {
        case 0:m.kind=(tv4_neg_kind)99;break;case 1:m.block_bytes=0;break;
        case 2:m.block_bytes=257;break;case 3:m.request_id.transfer=0;break;
        case 4:m.descriptor.id.transfer=1;break;case 5:m.descriptor.image_bytes=0;break;
        case 6:m.compatibility_tag[0]^=1;break;case 7:m.request_id.boot=33;break;
        default:m.descriptor.image_bytes=4097;break;
        }
        tv4_neg_result r=receive(&p,&p.rx,&m);
        assert(r==TV4_NEG_REJECT_CONTENT || r==TV4_NEG_REJECT_SESSION);
        assert(p.rx.state==TV4_NEG_IDLE && p.b.last_issued==0 && p.b.last_peer_request==0);
    }
    pair p;setup(&p,0,0,0);tv4_neg_message req,off;request_offer(&p,&req,&off);
    ++off.descriptor.id.transfer;assert(receive(&p,&p.tx,&off)==TV4_NEG_ERROR && p.tx.error==TV4_NEG_ERROR_CONFLICT);
}
static void deadlines_clocks_generations_and_sends(void)
{
    pair p;setup(&p,UINT32_MAX-5u,0,0);
    assert(tv4_neg_begin(&p.tx,&p.image,101,p.now)==TV4_NEG_OK);(void)next(&p,&p.tx);
    p.now+=10;(void)next(&p,&p.tx);assert(p.tx.state==TV4_NEG_WAIT_OFFER);
    assert(tv4_neg_poll(&p.tx,101,p.now-1)==TV4_NEG_ERROR && p.tx.error==TV4_NEG_ERROR_CLOCK && p.a.owner==NULL);
    setup(&p,0,0,0);ready(&p);tv4_negotiated output;memset(&output,0xa5,sizeof(output));
    assert(tv4_neg_take(&p.tx,&output,101,1000)==TV4_NEG_ERROR && p.tx.error==TV4_NEG_ERROR_TIMEOUT);
    for(size_t i=0;i<sizeof(output);i++)assert(((const uint8_t *)&output)[i]==0xa5);
    setup(&p,0,0,0);tv4_neg_message req,off;request_offer(&p,&req,&off);
    assert(tv4_neg_poll(&p.rx,203,0)==TV4_NEG_ERROR && p.rx.error==TV4_NEG_ERROR_CONTEXT);
    assert(tv4_neg_poll(&p.rx,202,1)==TV4_NEG_ERROR && p.rx.error==TV4_NEG_ERROR_CONTEXT);
    assert(p.b.owner==NULL && p.b.last_peer_request==1);
    setup(&p,0,0,0);p.tx.limits.send_limit=2; /* test-only pre-begin fault/config fixture */
    assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_OK);(void)next(&p,&p.tx);
    p.now=10;(void)next(&p,&p.tx);tv4_neg_message m;
    assert(tv4_neg_next(&p.tx,&m,101,20)==TV4_NEG_ERROR && p.tx.error==TV4_NEG_ERROR_SENDS);
    setup(&p,0,0,0);request_offer(&p,&req,&off);
    for(unsigned i=1;i<100;i++)assert(tv4_neg_receive(&p.rx,&req,202,i*10u)==TV4_NEG_DUPLICATE);
    assert(tv4_neg_receive(&p.rx,&req,202,1000)==TV4_NEG_ERROR && p.rx.error==TV4_NEG_ERROR_TIMEOUT);
    setup(&p,0,0,0);request_offer(&p,&req,&off);p.a.peer_boot=23;
    assert(tv4_neg_poll(&p.tx,101,0)==TV4_NEG_ERROR && p.tx.error==TV4_NEG_ERROR_OWNER);
    setup(&p,0,0,0);request_offer(&p,&req,&off);p.a.owner=&p.rx;
    assert(tv4_neg_poll(&p.tx,101,0)==TV4_NEG_ERROR && p.a.owner==&p.rx);
    assert(tv4_neg_clear(&p.tx) && p.a.owner==&p.rx); /* never forget/steal foreign ownership */
}
static void exhaustion_reboot_and_no_authentication(void)
{
    pair p;setup(&p,0,UINT32_MAX,0);
    assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_ERROR && p.tx.error==TV4_NEG_ERROR_EXHAUSTED);
    setup(&p,0,0,UINT32_MAX);assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_OK);
    tv4_neg_message exhausted=next(&p,&p.tx);
    assert(receive(&p,&p.rx,&exhausted)==TV4_NEG_ERROR && p.rx.error==TV4_NEG_ERROR_EXHAUSTED);
    assert(p.b.last_issued==UINT32_MAX && p.b.last_peer_request==0 && p.b.owner==NULL);
    setup(&p,0,UINT32_MAX-1u,UINT32_MAX-1u);ready(&p);
    tv4_neg_message old_request=p.tx.request;tv4_negotiated last=take_close(&p);
    assert(last.descriptor.id.transfer==UINT32_MAX && p.b.last_peer_request==UINT32_MAX);
    assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_ERROR && p.a.last_issued==UINT32_MAX);
    pair reboot;setup(&reboot,0,0,0);
    /* New boot epochs come from an external provider. Configure a fresh pair;
     * stale packets don't bootstrap peer identities from received data. */
    tv4_neg_pool a={0},b={0};tv4_negotiation tx={0},rx={0};
    assert(tv4_neg_pool_init(&a,111,222,0) && tv4_neg_pool_init(&b,222,111,0));
    assert(tv4_neg_init(&tx,&a,TV4_NEG_REQUESTER,limits(),reboot.tag,101,0));
    assert(tv4_neg_init(&rx,&b,TV4_NEG_RESPONDER,limits(),reboot.tag,202,0));
    assert(tv4_neg_receive(&rx,&old_request,202,0)==TV4_NEG_REJECT_SESSION && b.last_issued==0);
    /* Peer-only reboot may keep the local epoch only if its issued high-water
     * is carried forward by the trusted owner. No silent counter reset. */
    tv4_neg_pool stable={0};tv4_negotiation stable_tx={0};
    assert(tv4_neg_pool_init(&stable,11,222,91));
    assert(tv4_neg_init(&stable_tx,&stable,TV4_NEG_REQUESTER,limits(),reboot.tag,101,0));
    assert(tv4_neg_begin(&stable_tx,&reboot.image,101,0)==TV4_NEG_OK);
    assert(stable_tx.request.request_id.transfer==92 && stable_tx.request.responder_boot==222);
    setup(&p,0,0,0);assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_OK);
    tv4_neg_message forged=next(&p,&p.tx);forged.kind=TV4_NEG_OFFER;
    forged.descriptor.id=(tv4_update_id){22,999};forged.block_bytes=32;
    assert(receive(&p,&p.tx,&forged)==TV4_NEG_OK);(void)next(&p,&p.tx);
    forged.kind=TV4_NEG_CONFIRM;assert(receive(&p,&p.tx,&forged)==TV4_NEG_OK);
    tv4_negotiated result;assert(tv4_neg_take(&p.tx,&result,101,0)==TV4_NEG_OK);
    assert(result.descriptor.id.transfer==999); /* matching forged handshake CAN pass; not authentication */
}
static void ordering_and_live_deadline_after_take(void)
{
    pair p;setup(&p,0,0,0);assert(tv4_neg_begin(&p.tx,&p.image,101,0)==TV4_NEG_OK);
    tv4_neg_message req=next(&p,&p.tx);assert(receive(&p,&p.rx,&req)==TV4_NEG_OK);
    tv4_neg_message early=p.rx.offer;early.kind=TV4_NEG_ACCEPT;
    assert(receive(&p,&p.rx,&early)==TV4_NEG_REJECT_STATE);
    tv4_neg_message off=next(&p,&p.rx);early.kind=TV4_NEG_CONFIRM;
    assert(receive(&p,&p.tx,&early)==TV4_NEG_REJECT_STATE);
    assert(receive(&p,&p.tx,&off)==TV4_NEG_OK);
    assert(receive(&p,&p.tx,&early)==TV4_NEG_REJECT_STATE);
    tv4_neg_message ack=next(&p,&p.tx);assert(receive(&p,&p.rx,&ack)==TV4_NEG_OK);
    tv4_negotiated receiver;assert(tv4_neg_take(&p.rx,&receiver,202,0)==TV4_NEG_OK);
    tv4_neg_message confirmation=next(&p,&p.rx); /* emulate lost final CONFIRM */
    p.now=10;ack=next(&p,&p.tx);assert(receive(&p,&p.rx,&ack)==TV4_NEG_DUPLICATE);
    confirmation=next(&p,&p.rx);assert(receive(&p,&p.tx,&confirmation)==TV4_NEG_OK);
    assert(tv4_neg_take(&p.rx,&receiver,202,10)==TV4_NEG_REJECT_STATE);
    assert(tv4_neg_poll(&p.rx,202,1000)==TV4_NEG_ERROR && p.rx.state==TV4_NEG_EXPIRED);
    assert(p.b.owner==NULL && p.b.last_peer_request==1);
}
int main(void)
{
    configuration_and_ownership();fresh_ids_replay_and_snapshots();
    dropped_packets_and_repeated_sessions();conflicts_and_canonical_rejections();
    deadlines_clocks_generations_and_sends();exhaustion_reboot_and_no_authentication();
    ordering_and_live_deadline_after_take();
    printf("PASS 7 negotiation groups/1024 sessions; state=%zu pool=%zu message=%zu. "
        "Opaque bindings only; no CAN/authentication/install/device access.\n",
        sizeof(tv4_negotiation),sizeof(tv4_neg_pool),sizeof(tv4_neg_message));
}
