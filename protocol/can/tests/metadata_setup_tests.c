#include "update_metadata_setup.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    tv4_neg_pool pool[2];tv4_negotiation image[2];tv4_metadata_setup setup[2];
    tv4_update_tx tx;tv4_update_rx rx;uint32_t now;
} pair;
static bool zero(const void *p,size_t n)
{ const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true; }
static uint64_t gen(unsigned i){return i==0?101u:202u;}
static tv4_metadata_setup_limits setup_limits(void)
{ return (tv4_metadata_setup_limits){10,1000,TV4_META_BUNDLE_MAX,8}; }
static tv4_update_limits data_limits(void)
{ return (tv4_update_limits){4096,20,1500,2000,64,4}; }
static void image_session(pair *p)
{
    tv4_neg_image image={.image_bytes=777,.image_sha256={0x35},.metadata_sha256={0x51}};
    assert(tv4_neg_begin(&p->image[0],&image,gen(0),p->now)==TV4_NEG_OK);
    for(unsigned k=1;k<=4;++k) {
        unsigned from=(k%2u)==1u?0u:1u;tv4_neg_message m;
        assert(tv4_neg_next(&p->image[from],&m,gen(from),p->now)==TV4_NEG_SENT);
        assert((unsigned)m.kind==k);
        assert(tv4_neg_receive(&p->image[1u-from],&m,gen(1u-from),p->now)==TV4_NEG_OK);
    }
    tv4_negotiated params[2];
    for(unsigned i=0;i<2;++i) {
        assert(tv4_neg_take(&p->image[i],&params[i],gen(i),p->now)==TV4_NEG_OK);
        assert(tv4_neg_close(&p->image[i],gen(i),p->now)==TV4_NEG_OK);
        assert(p->image[i].offer.descriptor.id.transfer!=0); /* retained CLOSED binding */
    }
    assert(tv4_update_tx_init(&p->tx,data_limits(),22,p->now));
    assert(tv4_update_rx_init(&p->rx,data_limits(),22,p->now));
    assert(tv4_update_tx_begin(&p->tx,&params[0].descriptor,p->now)==TV4_UPDATE_OK);
    assert(tv4_update_rx_begin(&p->rx,&params[1].descriptor,p->now)==TV4_UPDATE_OK);
}
static void init(pair *p,uint32_t now,uint32_t last)
{
    memset(p,0,sizeof(*p));p->now=now;uint8_t tag[32]={9};
    tv4_neg_limits l={4096,10,1000,64,8};
    assert(tv4_neg_pool_init(&p->pool[0],11,22,0));assert(tv4_neg_pool_init(&p->pool[1],22,11,last));
    for(unsigned i=0;i<2;++i) {
        assert(tv4_neg_init(&p->image[i],&p->pool[i],i?TV4_NEG_RESPONDER:TV4_NEG_REQUESTER,l,tag,gen(i),now));
        assert(tv4_metadata_setup_init(&p->setup[i],&p->pool[i],i?TV4_NEG_RESPONDER:TV4_NEG_REQUESTER,setup_limits(),gen(i),now));
    }
    image_session(p);
}
static void begin(pair *p)
{
    const uint8_t hash[32]={0x93};
    assert(tv4_metadata_setup_request(&p->setup[0],&p->image[0],&p->tx,TV4_META_BUNDLE_MIN,hash,gen(0),p->now)==TV4_SETUP_OK);
    assert(tv4_metadata_setup_respond(&p->setup[1],&p->image[1],&p->rx,gen(1),p->now)==TV4_SETUP_OK);
    assert(p->pool[0].owner==&p->setup[0] && p->pool[1].owner==&p->setup[1]);
    assert(p->pool[1].last_issued==p->rx.context.descriptor.id.transfer+1u);
}
static tv4_metadata_setup_message next(pair *p,unsigned i)
{
    tv4_metadata_setup_message m;
    assert(tv4_metadata_setup_next(&p->setup[i],&m,gen(i),p->now)==TV4_SETUP_SENT);return m;
}
static void receive(pair *p,unsigned i,const tv4_metadata_setup_message *m)
{ assert(tv4_metadata_setup_receive(&p->setup[i],m,gen(i),p->now)==TV4_SETUP_OK); }
static void exchange(pair *p)
{
    for(unsigned k=1;k<=4;++k) {
        unsigned from=(k%2u)==1u?0u:1u;tv4_metadata_setup_message m=next(p,from);
        uint8_t raw[TV4_SETUP_MESSAGE_BYTES];size_t n;tv4_metadata_setup_message decoded;
        assert(tv4_metadata_setup_encode(&m,raw,sizeof(raw),&n)==TV4_SETUP_OK);
        assert(tv4_metadata_setup_decode(raw,n,&decoded)==TV4_SETUP_OK);
        assert((unsigned)decoded.kind==k);receive(p,1u-from,&decoded);
    }
}
static void complete(pair *p)
{
    tv4_metadata_parameters params[2];
    for(unsigned i=0;i<2;++i)assert(tv4_metadata_setup_take(&p->setup[i],&params[i],gen(i),p->now)==TV4_SETUP_OK);
    assert(tv4_update_binding_equal(&params[0].binding,&params[1].binding));
    assert(params[0].metadata.id.transfer==p->rx.context.descriptor.id.transfer+1u);
    tv4_metadata_parameters twice;memset(&twice,0xa5,sizeof(twice));
    assert(tv4_metadata_setup_take(&p->setup[0],&twice,gen(0),p->now)==TV4_SETUP_STATE && zero(&twice,sizeof(twice)));
    tv4_metadata_document document={.binding=params[0].binding,.document_bytes=1,.signature_bytes=8,.key_id_bytes=1};
    uint8_t raw[TV4_META_BUNDLE_MAX];size_t n;
    assert(tv4_metadata_encode(&document,raw,sizeof(raw),&n)==TV4_META_OK);
    assert(n==params[0].metadata.image_bytes);
    tv4_update_limits l=data_limits();l.max_image_bytes=TV4_META_BUNDLE_MAX;
    tv4_update_tx tx;tv4_metadata_rx rx={0};
    assert(tv4_update_tx_init(&tx,l,22,p->now));
    assert(tv4_update_tx_begin(&tx,&params[0].metadata,p->now)==TV4_UPDATE_OK);
    assert(tv4_metadata_rx_init(&rx,l,22,gen(1),0,p->now)==TV4_META_OK);
    assert(tv4_metadata_rx_begin(&rx,&params[1].metadata,&params[1].binding,gen(1),p->now)==TV4_META_OK);
    assert(tv4_metadata_setup_close_tx(&p->setup[0],&tx,gen(0),p->now)==TV4_SETUP_CONTENT);
    assert(tv4_metadata_setup_close_rx(&p->setup[1],&rx,gen(1),p->now)==TV4_SETUP_CONTENT);
    while(tx.context.state!=TV4_UPDATE_COMPLETE_UNVERIFIED) {
        size_t count=n-tx.context.next_offset;if(count>l.block_bytes)count=l.block_bytes;
        assert(tv4_update_tx_load(&tx,raw+tx.context.next_offset,count,p->now)==TV4_UPDATE_OK);
        tv4_update_block b;tv4_update_ack ack;
        assert(tv4_update_tx_next(&tx,&b,p->now)==TV4_UPDATE_OK);
        assert(tv4_metadata_rx_receive(&rx,&b,&ack,gen(1),p->now)==TV4_UPDATE_ACK);
        assert(tv4_update_tx_ack(&tx,&ack,p->now)==TV4_UPDATE_OK);++p->now;
        for(unsigned i=0;i<2;++i)assert(tv4_metadata_setup_poll(&p->setup[i],gen(i),p->now)==TV4_SETUP_OK);
    }
    assert(tv4_metadata_setup_close_rx(&p->setup[1],&rx,gen(1),p->now)==TV4_SETUP_CONTENT); /* take required */
    tv4_metadata_document owned;
    assert(tv4_metadata_rx_take(&rx,&owned,gen(1),p->now)==TV4_META_TAKEN_UNVERIFIED);
    assert(tv4_metadata_setup_close_tx(&p->setup[0],&tx,gen(0),p->now)==TV4_SETUP_OK);
    assert(tv4_metadata_setup_close_rx(&p->setup[1],&rx,gen(1),p->now)==TV4_SETUP_OK);
    assert(p->pool[0].owner==NULL && p->pool[1].owner==NULL);
}
static void namespace_lifetime(void)
{
    pair p;init(&p,UINT32_MAX-2u,0);
    for(unsigned round=0;round<8;++round) {
        assert(p.rx.context.descriptor.id.transfer==2u*round+1u);
        begin(&p);tv4_update_id id={99,99};
        assert(tv4_neg_reserve_metadata(&p.image[1],gen(1),&id)==TV4_NEG_REJECT_SESSION && zero(&id,sizeof(id)));
        assert(!tv4_metadata_setup_init(&p.setup[0],&p.pool[0],TV4_NEG_REQUESTER,setup_limits(),gen(0),p.now));
        tv4_negotiation other={0};uint8_t tag[32]={9};tv4_neg_limits l={4096,10,1000,64,8};
        assert(tv4_neg_init(&other,&p.pool[0],TV4_NEG_REQUESTER,l,tag,gen(0),p.now));
        tv4_neg_image image={.image_bytes=1};
        assert(tv4_neg_begin(&other,&image,gen(0),p.now)==TV4_NEG_BUSY);
        exchange(&p);complete(&p);
        for(unsigned i=0;i<2;++i)assert(tv4_metadata_setup_clear(&p.setup[i],gen(i),p.now)==TV4_SETUP_OK);
        uint8_t hash[32]={1};
        assert(tv4_metadata_setup_request(&p.setup[0],&p.image[0],&p.tx,98,hash,gen(0),p.now)==TV4_SETUP_SESSION);
        assert(tv4_metadata_setup_respond(&p.setup[1],&p.image[1],&p.rx,gen(1),p.now)==TV4_SETUP_SESSION);
        assert(p.pool[1].last_issued==2u*round+2u);
        for(unsigned i=0;i<2;++i)assert(tv4_neg_clear(&p.image[i]));
        assert(zero(&p.image[0].offer,sizeof(p.image[0].offer)));
        image_session(&p);
    }
    init(&p,0,UINT32_MAX-1u);tv4_update_id out={1,2};
    assert(tv4_neg_reserve_metadata(&p.image[1],gen(1),&out)==TV4_NEG_ERROR);
    assert(p.pool[1].last_issued==UINT32_MAX && zero(&out,sizeof(out)));
    puts("PASS8 sequential image/metadata pairs, shared namespace ownership, no wrap/reuse; concrete close");
}
static void loss_and_take(void)
{
    for(unsigned lost=1;lost<=4;++lost) {
        pair p;init(&p,0,0);begin(&p);bool dropped=false,taken=false;
        tv4_metadata_parameters params;
        for(unsigned tick=0;tick<200;++tick) {
            for(unsigned side=0;side<2;++side) {
                tv4_metadata_setup_message m;tv4_metadata_setup_result r=tv4_metadata_setup_next(&p.setup[side],&m,gen(side),p.now);
                assert(r==TV4_SETUP_SENT || r==TV4_SETUP_WAIT);
                if(r==TV4_SETUP_SENT) {
                    if((unsigned)m.kind==lost && !dropped)dropped=true;
                    else {
                        r=tv4_metadata_setup_receive(&p.setup[1u-side],&m,gen(1u-side),p.now);
                        assert(r==TV4_SETUP_OK || r==TV4_SETUP_DUPLICATE);
                    }
                }
            }
            if(p.setup[1].state==TV4_SETUP_READY && !taken) {
                assert(tv4_metadata_setup_take(&p.setup[1],&params,gen(1),p.now)==TV4_SETUP_OK);taken=true;
            }
            if(p.setup[0].state==TV4_SETUP_READY)break;
            ++p.now;assert(p.now<199);
        }
        assert(dropped && taken && p.setup[1].state==TV4_SETUP_TAKEN);
        assert(tv4_metadata_setup_take(&p.setup[1],&params,gen(1),p.now)==TV4_SETUP_STATE);
        assert(p.setup[0].started==0 && p.setup[1].started==0 && p.rx.context.started_ms==0);
    }
    puts("PASS4 lost setup phases, responder early take + lost CONFIRM recovery; no deadline restart");
}
static void faults(void)
{
    for(unsigned which=0;which<13;++which) {
        pair p;init(&p,100,0);begin(&p);uint32_t issued=p.pool[1].last_issued;
        tv4_metadata_setup_message m=next(&p,0);
        switch(which) {
        case 0:assert(tv4_metadata_setup_poll(&p.setup[1],999,p.now)==TV4_SETUP_ERROR);break;
        case 1:--p.now;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 2:p.pool[1].peer_boot=99;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 3:p.now+=1000;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR && p.setup[1].error==TV4_SETUP_TIMEOUT);break;
        case 4:assert(tv4_update_rx_abort(&p.rx,p.rx.context.descriptor.id,p.now)==TV4_UPDATE_OK);assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 5:assert(tv4_neg_clear(&p.image[1]));assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 6:m.binding.metadata_sha256[0]^=1;assert(tv4_metadata_setup_receive(&p.setup[1],&m,gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 7:receive(&p,1,&m);m.metadata.image_sha256[0]^=1;assert(tv4_metadata_setup_receive(&p.setup[1],&m,gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 8:++p.pool[1].last_peer_request;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 9:p.pool[1].owner=&p.rx;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);assert(p.pool[1].owner==&p.rx);break;
        case 10:++p.rx.context.limits.total_timeout_ms;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        case 11:++p.rx.context.progress_ms;assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        default: {
            tv4_update_block b={.id=p.rx.context.descriptor.id,.length=64,.bytes={4}};tv4_update_ack ack;
            assert(tv4_update_block_checksum(&b,&b.crc32c));assert(tv4_update_rx_receive(&p.rx,&b,&ack,p.now)==TV4_UPDATE_PENDING);
            assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);break;
        }}
        assert(p.pool[1].last_issued==issued);
        if(which==0 || which==1 || which==2 || which==8 || which==9)
            assert(tv4_metadata_setup_clear(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR);
    }
    pair p;init(&p,0,0);p.rx.context.limits.progress_timeout_ms=50;begin(&p);p.now=50;
    assert(tv4_metadata_setup_poll(&p.setup[1],gen(1),p.now)==TV4_SETUP_ERROR && p.setup[1].error==TV4_SETUP_IMAGE);
    init(&p,0,0);begin(&p);
    for(unsigned i=0;i<8;++i){tv4_metadata_setup_message m=next(&p,0);(void)m;p.now+=10;}
    tv4_metadata_setup_message m;assert(tv4_metadata_setup_next(&p.setup[0],&m,gen(0),p.now)==TV4_SETUP_ERROR && zero(&m,sizeof(m)));
    assert(p.setup[0].error==TV4_SETUP_SENDS && p.pool[0].last_issued==1);
    init(&p,0,0);begin(&p);m=next(&p,0);receive(&p,1,&m);
    for(unsigned i=0;i<100;++i)assert(tv4_metadata_setup_receive(&p.setup[1],&m,gen(1),p.now)==TV4_SETUP_DUPLICATE);
    assert(p.setup[1].sends==0 && p.setup[1].started==0 && p.pool[1].last_issued==2);
    assert(tv4_metadata_setup_abort(&p.setup[1],gen(1),p.now)==TV4_SETUP_OK);
    assert(tv4_metadata_setup_clear(&p.setup[1],gen(1),p.now)==TV4_SETUP_OK);
    assert(tv4_metadata_setup_respond(&p.setup[1],&p.image[1],&p.rx,gen(1),p.now)==TV4_SETUP_SESSION);
    puts("PASS context/clock/owner/image/progress/content/timeout/send faults, ID retained, sticky guards");
}
static void configuration(void)
{
    tv4_neg_pool pool={0};assert(tv4_neg_pool_init(&pool,11,22,0));
    for(unsigned which=0;which<7;++which) {
        tv4_metadata_setup a={0};tv4_metadata_setup_limits limits=setup_limits();
        switch(which) {
        case 0:limits.retry_ms=0;break;case 1:limits.total_ms=limits.retry_ms;break;
        case 2:limits.total_ms=UINT32_MAX;break;case 3:limits.send_limit=0;break;
        case 4:limits.max_bundle_bytes=97;break;case 5:limits.max_bundle_bytes=1216;break;
        default:limits.retry_ms=UINT32_MAX;break;
        }
        assert(!tv4_metadata_setup_init(&a,&pool,TV4_NEG_REQUESTER,limits,1,0) && !a.configured);
    }
    tv4_metadata_setup a={0};
    assert(!tv4_metadata_setup_init(&a,&pool,(tv4_neg_role)99,setup_limits(),1,0));
    assert(!tv4_metadata_setup_init(&a,&pool,TV4_NEG_REQUESTER,setup_limits(),0,0));
    assert(!tv4_metadata_setup_init(NULL,&pool,TV4_NEG_REQUESTER,setup_limits(),1,0));
    pair p;init(&p,0,0);uint8_t hash[32]={1};
    assert(tv4_metadata_setup_request(&p.setup[0],&p.image[0],&p.tx,97,hash,gen(0),0)==TV4_SETUP_LIMIT);
    assert(tv4_metadata_setup_request(&p.setup[0],&p.image[0],&p.tx,1216,hash,gen(0),0)==TV4_SETUP_LIMIT);
    assert(p.pool[0].owner==NULL && p.pool[1].last_issued==1);
    tv4_update_id id={9,9};
    assert(tv4_neg_reserve_metadata(&p.image[0],gen(0),&id)==TV4_NEG_REJECT_STATE && zero(&id,sizeof(id)));
    assert(tv4_neg_reserve_metadata(&p.image[1],999,&id)==TV4_NEG_REJECT_SESSION && zero(&id,sizeof(id)));
    begin(&p);tv4_metadata_setup_message req=next(&p,0),stale=req;
    ++stale.request_id.transfer;
    assert(tv4_metadata_setup_receive(&p.setup[1],&stale,gen(1),0)==TV4_SETUP_SESSION && p.setup[1].state==TV4_SETUP_WAIT_REQUEST);
    req.block_bytes=0;
    assert(tv4_metadata_setup_receive(&p.setup[1],&req,gen(1),0)==TV4_SETUP_CONTENT && p.setup[1].started==0);
    puts("PASS invalid config/bounds/stale session/role/generation; rejection does not allocate or restart");
}
static tv4_metadata_setup_message sample(unsigned kind)
{
    tv4_metadata_setup_message m={.kind=(tv4_neg_kind)kind,.request_id={11,5},
        .binding={.image={.id={22,9},.image_bytes=4096}},
        .metadata={.id={22,10},.image_bytes=1215},.block_bytes=256};
    if(kind==1)m.metadata.id=(tv4_update_id){0,0};
    for(unsigned i=0;i<32;++i) {
        m.binding.image.image_sha256[i]=(uint8_t)i;m.binding.metadata_sha256[i]=(uint8_t)(i+32u);
        m.metadata.image_sha256[i]=(uint8_t)(i+64u);m.compatibility_tag[i]=(uint8_t)(i+96u);
    }
    return m;
}
static void codec(void)
{
    uint8_t bytes[TV4_SETUP_MESSAGE_BYTES+1],again[TV4_SETUP_MESSAGE_BYTES];size_t n;
    for(unsigned k=1;k<=4;++k) {
        tv4_metadata_setup_message m=sample(k),out;
        assert(tv4_metadata_setup_encode(&m,bytes,sizeof(bytes),&n)==TV4_SETUP_OK && n==172);
        for(size_t cap=0;cap<172;++cap) {
            memset(again,0xa5,sizeof(again));n=99;
            assert(tv4_metadata_setup_encode(&m,again,cap,&n)==TV4_SETUP_LIMIT && n==0);
            for(size_t i=0;i<sizeof(again);++i)assert(again[i]==0xa5);
        }
        for(size_t len=0;len<=173;++len)if(len!=172) {
            memset(&out,0xa5,sizeof(out));assert(tv4_metadata_setup_decode(bytes,len,&out)!=TV4_SETUP_OK && zero(&out,sizeof(out)));
        }
        assert(tv4_metadata_setup_decode(bytes,172,&out)==TV4_SETUP_OK);
        assert(tv4_metadata_setup_encode(&out,again,sizeof(again),&n)==TV4_SETUP_OK && !memcmp(bytes,again,n));
    }
    uint32_t random=39;unsigned accepted=0,rejected=0;tv4_metadata_setup_message m=sample(2);
    assert(tv4_metadata_setup_encode(&m,bytes,sizeof(bytes),&n)==TV4_SETUP_OK);
    for(unsigned i=0;i<100000;++i) {
        memcpy(again,bytes,172);random=random*1664525u+1013904223u;size_t at=random%172u;
        random=random*1664525u+1013904223u;again[at]^=(uint8_t)(1u<<(random%8u));
        tv4_metadata_setup_message out;memset(&out,0xa5,sizeof(out));
        if(tv4_metadata_setup_decode(again,172,&out)==TV4_SETUP_OK) {
            uint8_t roundtrip[172];assert(tv4_metadata_setup_encode(&out,roundtrip,sizeof(roundtrip),&n)==TV4_SETUP_OK);
            assert(!memcmp(roundtrip,again,n));++accepted;
        }else {assert(zero(&out,sizeof(out)));++rejected;}
    }
    printf("PASS4 setup kinds; capacities/truncation/tails; 100000 mutations: %u structural, %u zero-output refusals\n",accepted,rejected);
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--vectors")) {
        for(unsigned k=1;k<=4;++k) {
            tv4_metadata_setup_message m=sample(k);uint8_t bytes[172];size_t n;
            assert(tv4_metadata_setup_encode(&m,bytes,sizeof(bytes),&n)==TV4_SETUP_OK);
            printf("%u ",k);for(size_t i=0;i<n;++i)printf("%02x",bytes[i]);puts("");
        }return 0;
    }
    assert(argc==1);codec();configuration();namespace_lifetime();loss_and_take();faults();
    printf("Host storage only: setup=%zu message=%zu; no MCU RAM/stack approval\n",sizeof(tv4_metadata_setup),sizeof(tv4_metadata_setup_message));
    return 0;
}
