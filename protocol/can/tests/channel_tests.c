#include "update_channel.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config cfg={20,1000,200,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_channel_message sample(unsigned token,tv4_update_id id)
{
    tv4_channel_message m={0};
    if(token>=9 && token<=12) {
        m.kind=TV4_CHANNEL_MS;m.body.setup=(tv4_metadata_setup_message){.kind=(tv4_neg_kind)(token-8u),
            .request_id={11,1},.binding={.image={.id={22,1},.image_bytes=1000}},
            .metadata={.id={22,2},.image_bytes=98},.block_bytes=64};
        if(token==9)m.body.setup.metadata.id=(tv4_update_id){0,0};
    }else {
        m.kind=TV4_CHANNEL_WIRE;m.body.wire.kind=(tv4_wire_kind)token;
        if(token<=4) {
            m.body.wire.body.negotiation=(tv4_neg_message){.kind=(tv4_neg_kind)token,.request_id={11,1},
                .descriptor={.id={22,1},.image_bytes=1000},.responder_boot=22,.block_bytes=64};
            if(token==1)m.body.wire.body.negotiation.descriptor.id=(tv4_update_id){0,0};
        }else if(token==5) {
            m.body.wire.body.data=(tv4_update_block){.id=id,.length=64,.bytes={0x55}};
            assert(tv4_update_block_checksum(&m.body.wire.body.data,&m.body.wire.body.data.crc32c));
        }else if(token==6)m.body.wire.body.ack=(tv4_update_ack){.id=id,.next_sequence=1,.next_offset=64};
        else m.body.wire.body.outcome=(tv4_update_outcome){.binding={.image={.id=id,.image_bytes=1000}},
            .code=TV4_OUTCOME_CHECKED_CLOSED,.reason=TV4_OUTCOME_REASON_NONE};
    }
    return m;
}
static void advance(tv4_channel_tx *tx,tv4_channel_rx *rx,tv4_channel_phase phase,tv4_update_id id,uint32_t now)
{
    assert(tv4_channel_tx_advance(tx,phase,id,11,now)==TV4_CHANNEL_OK);
    assert(tv4_channel_rx_advance(rx,phase,id,22,now)==TV4_CHANNEL_OK);
}
static void same(const tv4_channel_message *a,const tv4_channel_message *b)
{
    uint8_t x[286],y[286];size_t nx,ny;assert(a->kind==b->kind);
    if(a->kind==TV4_CHANNEL_WIRE) {
        assert(tv4_update_wire_encode(&a->body.wire,x,sizeof(x),&nx)==TV4_WIRE_OK);
        assert(tv4_update_wire_encode(&b->body.wire,y,sizeof(y),&ny)==TV4_WIRE_OK);
    }else {
        assert(tv4_metadata_setup_encode(&a->body.setup,x,sizeof(x),&nx)==TV4_SETUP_OK);
        assert(tv4_metadata_setup_encode(&b->body.setup,y,sizeof(y),&ny)==TV4_SETUP_OK);
    }
    assert(nx==ny && !memcmp(x,y,nx));
}
static void transfer(tv4_channel_tx *tx,tv4_channel_rx *rx,const tv4_channel_message *m,uint32_t *now)
{
    assert(tv4_channel_tx_begin(tx,m,11,*now)==TV4_CHANNEL_OK);
    assert(zero(tx->scratch,sizeof(tx->scratch)));
    unsigned frames=0;
    while(tx->active) {
        tv4_fragment_frame f,again;
        assert(tv4_channel_tx_peek(tx,&f,11,*now)==TV4_FRAG_FRAME);
        uint32_t progress=tx->progress;
        assert(tv4_channel_tx_peek(tx,&again,11,*now)==TV4_FRAG_FRAME && !memcmp(&f,&again,sizeof(f)) && tx->progress==progress);
        assert(tv4_channel_tx_begin(tx,m,11,*now)==TV4_CHANNEL_BUSY);
        assert(tv4_channel_tx_advance(tx,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,*now)==TV4_CHANNEL_BUSY);
        tv4_fragment_frame bad=f;bad.bytes[4]^=1;
        assert(tv4_channel_tx_commit(tx,&bad,11,*now)==TV4_FRAG_ORDER && tx->progress==progress);
        tv4_fragment_result r=tv4_channel_rx_receive(rx,7,&f,22,*now);
        assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);
        assert(tv4_channel_rx_advance(rx,TV4_CHANNEL_SETUP,(tv4_update_id){0},22,*now)==TV4_CHANNEL_BUSY);
        r=tv4_channel_tx_commit(tx,&f,11,*now);assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);
        assert(tv4_channel_tx_commit(tx,&f,11,*now)==TV4_FRAG_STATE);++frames;++*now;
    }
    assert(frames>1 && frames<=72 && !tx->queued && zero(&tx->outbox,sizeof(tx->outbox)));
    tv4_channel_message out;
    assert(tv4_channel_rx_take(rx,&out,22,*now)==TV4_CHANNEL_TAKEN_UNVERIFIED);same(m,&out);
    assert(zero(rx->scratch,sizeof(rx->scratch)) && zero(rx->fragments.storage.bytes,sizeof(rx->fragments.storage.bytes)));
    assert(tv4_channel_rx_take(rx,&out,22,*now)==TV4_CHANNEL_STATE && zero(&out,sizeof(out)));
}
static void sequential(void)
{
    tv4_channel_tx tx={0};tv4_channel_rx rx={0};uint32_t now=UINT32_MAX-2u;
    assert(tv4_channel_tx_init(&tx,&cfg,7,11,7,now));assert(tv4_channel_rx_init(&rx,&cfg,7,22,7,now));
    assert(!tv4_channel_tx_init(&tx,&cfg,7,11,0,now) && !tv4_channel_rx_init(&rx,&cfg,7,22,0,now));
    unsigned messages=0;
    for(unsigned round=0;round<512;++round) {
        for(unsigned k=1;k<=4;++k){tv4_channel_message m=sample(k,(tv4_update_id){0});transfer(&tx,&rx,&m,&now);++messages;}
        advance(&tx,&rx,TV4_CHANNEL_SETUP,(tv4_update_id){0},now);
        for(unsigned k=9;k<=12;++k){tv4_channel_message m=sample(k,(tv4_update_id){0});transfer(&tx,&rx,&m,&now);++messages;}
        tv4_update_id metadata={22,2u*round+2u},image={22,2u*round+1u};
        advance(&tx,&rx,TV4_CHANNEL_METADATA,metadata,now);
        for(unsigned k=5;k<=6;++k){tv4_channel_message m=sample(k,metadata);transfer(&tx,&rx,&m,&now);++messages;}
        advance(&tx,&rx,TV4_CHANNEL_IMAGE,image,now);
        for(unsigned k=5;k<=6;++k){tv4_channel_message m=sample(k,image);transfer(&tx,&rx,&m,&now);++messages;}
        advance(&tx,&rx,TV4_CHANNEL_OUTCOME,(tv4_update_id){0},now);
        for(unsigned k=7;k<=8;++k){tv4_channel_message m=sample(k,image);transfer(&tx,&rx,&m,&now);++messages;}
        advance(&tx,&rx,TV4_CHANNEL_NEGOTIATION,(tv4_update_id){0},now);
        assert(tx.fragments.storage.counter==messages+7u && rx.fragments.storage.counter==messages+7u);
    }
    printf("PASS512 phase cycles, %u mixed TU/MS messages, shared retained counters, exact queue commit/single owned take/wrap clock\n",messages);
}
static void inject(tv4_channel_rx *rx,const uint8_t *bytes,size_t n,uint16_t last,uint32_t now)
{
    tv4_fragment_tx raw={0};assert(tv4_fragment_tx_init_raw(&raw,&cfg,7,11,last,now)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin_raw(&raw,bytes,n,11,now)==TV4_FRAG_OK);
    while(raw.storage.state==TV4_FRAG_SENDING) {
        tv4_fragment_frame f;assert(tv4_fragment_tx_peek(&raw,&f,11,now)==TV4_FRAG_FRAME);
        tv4_fragment_result r=tv4_channel_rx_receive(rx,7,&f,22,now);assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);
        r=tv4_fragment_tx_commit(&raw,&f,11,now);assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);
    }
}
static size_t encode(const tv4_channel_message *m,uint8_t bytes[286])
{
    size_t n;if(m->kind==TV4_CHANNEL_WIRE)assert(tv4_update_wire_encode(&m->body.wire,bytes,286,&n)==TV4_WIRE_OK);
    else assert(tv4_metadata_setup_encode(&m->body.setup,bytes,286,&n)==TV4_SETUP_OK);
    return n;
}
static void phases_and_prefixes(void)
{
    tv4_channel_tx tx={0};tv4_channel_rx rx={0};assert(tv4_channel_tx_init(&tx,&cfg,7,11,0,0));assert(tv4_channel_rx_init(&rx,&cfg,7,22,0,0));
    assert(tv4_channel_tx_advance(&tx,TV4_CHANNEL_IMAGE,(tv4_update_id){22,1},11,0)==TV4_CHANNEL_STATE);
    assert(tv4_channel_rx_advance(&rx,(tv4_channel_phase)99,(tv4_update_id){0},22,0)==TV4_CHANNEL_ARGUMENT);
    tv4_channel_message m=sample(9,(tv4_update_id){0});uint8_t bytes[286];size_t n=encode(&m,bytes);
    assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_STATE && tx.fragments.storage.counter==0);
    inject(&rx,bytes,n,0,0);tv4_channel_message out;
    assert(tv4_channel_rx_take(&rx,&out,22,0)==TV4_CHANNEL_STATE && zero(&out,sizeof(out)) && rx.fragments.storage.counter==1);
    advance(&tx,&rx,TV4_CHANNEL_SETUP,(tv4_update_id){0},0);
    assert(tv4_channel_tx_advance(&tx,TV4_CHANNEL_METADATA,(tv4_update_id){22,1},11,0)==TV4_CHANNEL_IDENTITY);
    advance(&tx,&rx,TV4_CHANNEL_METADATA,(tv4_update_id){22,2},0);
    m=sample(5,(tv4_update_id){22,1});assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_IDENTITY);
    n=encode(&m,bytes);inject(&rx,bytes,n,1,0);
    assert(tv4_channel_rx_take(&rx,&out,22,0)==TV4_CHANNEL_IDENTITY && zero(&out,sizeof(out)));
    assert(tv4_channel_rx_advance(&rx,TV4_CHANNEL_IMAGE,(tv4_update_id){23,1},22,0)==TV4_CHANNEL_IDENTITY);
    assert(tv4_channel_rx_advance(&rx,TV4_CHANNEL_IMAGE,(tv4_update_id){22,2},22,0)==TV4_CHANNEL_IDENTITY);
    advance(&tx,&rx,TV4_CHANNEL_IMAGE,(tv4_update_id){22,1},0);
    m=sample(6,(tv4_update_id){22,2});assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_IDENTITY);
    n=encode(&m,bytes);inject(&rx,bytes,n,2,0);
    assert(tv4_channel_rx_take(&rx,&out,22,0)==TV4_CHANNEL_IDENTITY && zero(&out,sizeof(out)));
    m=sample(12,(tv4_update_id){0});assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_STATE);
    n=encode(&m,bytes);inject(&rx,bytes,n,3,0);
    assert(tv4_channel_rx_take(&rx,&out,22,0)==TV4_CHANNEL_STATE && zero(&out,sizeof(out)) && rx.fragments.storage.counter==4);
    advance(&tx,&rx,TV4_CHANNEL_OUTCOME,(tv4_update_id){0},0);
    m=sample(7,(tv4_update_id){22,2});assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_IDENTITY);
    for(unsigned i=0;i<4;++i) {
        tv4_channel_rx bad={0};assert(tv4_channel_rx_init(&bad,&cfg,7,22,0,0));
        m=sample(9,(tv4_update_id){0});n=encode(&m,bytes);
        if(i==0)bytes[0]='X';else if(i==1)bytes[2]=2;else if(i==2){bytes[0]='T';bytes[1]='U';}else bytes[7]=1;
        inject(&bad,bytes,n,0,0);memset(&out,0xa5,sizeof(out));
        assert(tv4_channel_rx_take(&bad,&out,22,0)==TV4_CHANNEL_ERROR && zero(&out,sizeof(out)) && bad.error==TV4_CHANNEL_CODEC);
        assert(tv4_channel_rx_poll(&bad,22,0)==TV4_FRAG_ERROR);
        assert(tv4_channel_rx_clear(&bad,22,0)==TV4_FRAG_OK && bad.fragments.storage.counter==1 && bad.policy.phase==TV4_CHANNEL_NEGOTIATION);
    }
    puts("PASS phase order/pair IDs/late control/old metadata ACK filtering; strict prefix/version/reserved refusal, no decoder fallback");
}
static void queue_faults(void)
{
    for(unsigned total=0;total<2;++total) {
        tv4_channel_tx tx={0};tv4_fragment_config l={8,10,200,TV4_FRAGMENT_VERSION};
        assert(tv4_channel_tx_init(&tx,&l,7,11,0,0));
        assert(tv4_channel_tx_advance(&tx,TV4_CHANNEL_SETUP,(tv4_update_id){0},11,0)==TV4_CHANNEL_OK);
        assert(tv4_channel_tx_advance(&tx,TV4_CHANNEL_METADATA,(tv4_update_id){22,2},11,0)==TV4_CHANNEL_OK);
        tv4_channel_message m=sample(6,(tv4_update_id){22,2});assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_OK);
        uint32_t now=0;tv4_fragment_frame f;
        do {
            assert(tv4_channel_tx_peek(&tx,&f,11,now)==TV4_FRAG_FRAME);
            if(tx.fragments.storage.state==TV4_FRAG_IDLE)break;
            assert(tv4_channel_tx_commit(&tx,&f,11,now)==TV4_FRAG_COMMITTED);if(total)++now;
        }while(true);
        assert(tx.active && tx.queued && tx.fragments.storage.state==TV4_FRAG_IDLE);
        assert(tv4_channel_tx_advance(&tx,TV4_CHANNEL_IMAGE,(tv4_update_id){22,1},11,now)==TV4_CHANNEL_BUSY);
        now=total?10u:8u;assert(tv4_channel_tx_peek(&tx,&f,11,now)==TV4_FRAG_ERROR && zero(&f,sizeof(f)));
        assert(tx.error==(total?TV4_CHANNEL_QUEUE_TOTAL:TV4_CHANNEL_QUEUE_PROGRESS));
        assert(!tx.queued && zero(&tx.outbox,sizeof(tx.outbox)) && tx.fragments.storage.counter==1);
        assert(tv4_channel_tx_clear(&tx,11,now)==TV4_FRAG_OK && tx.policy.phase==TV4_CHANNEL_METADATA && tx.fragments.storage.counter==1);
    }
    for(unsigned which=0;which<3;++which) {
        tv4_channel_tx tx={0};tv4_channel_rx rx={0};assert(tv4_channel_tx_init(&tx,&cfg,7,11,which==2?UINT16_MAX:0,100));
        assert(tv4_channel_rx_init(&rx,&cfg,7,22,0,100));tv4_channel_message m=sample(1,(tv4_update_id){0});
        if(which==0) {assert(tv4_channel_tx_poll(&tx,12,100)==TV4_FRAG_ERROR);assert(tv4_channel_rx_poll(&rx,23,100)==TV4_FRAG_ERROR);}
        else if(which==1) {assert(tv4_channel_tx_poll(&tx,11,99)==TV4_FRAG_ERROR);assert(tv4_channel_rx_poll(&rx,22,99)==TV4_FRAG_ERROR);}
        else assert(tv4_channel_tx_begin(&tx,&m,11,100)==TV4_CHANNEL_ERROR);
        assert(tv4_channel_tx_clear(&tx,11,100)==TV4_FRAG_ERROR);
        if(which<2)assert(tv4_channel_rx_clear(&rx,22,100)==TV4_FRAG_ERROR);
    }
    tv4_channel_tx tx={0};assert(tv4_channel_tx_init(&tx,&cfg,7,11,4,0));tv4_channel_message m=sample(1,(tv4_update_id){0});
    assert(tv4_channel_tx_begin(&tx,&m,11,0)==TV4_CHANNEL_OK);tv4_fragment_frame f;
    assert(tv4_channel_tx_peek(&tx,&f,11,0)==TV4_FRAG_FRAME);
    assert(tv4_channel_tx_cancel(&tx,11,0)==TV4_FRAG_OK && !tx.queued);
    assert(tv4_channel_tx_clear(&tx,11,0)==TV4_FRAG_OK && tx.fragments.storage.counter==5);
    puts("PASS final queued frame progress/total timeout, exact backpressure, cancel/clear retained policy/counters; sticky context/clock/exhaustion");
}
int main(void)
{
    sequential();phases_and_prefixes();queue_faults();
    printf("Host storage only: channel TX=%zu RX=%zu snapshot=%zu; no MCU RAM/stack approval\n",sizeof(tv4_channel_tx),sizeof(tv4_channel_rx),sizeof(tv4_channel_message));return 0;
}
