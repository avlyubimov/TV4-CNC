#include "update_fragments.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Same typed fixtures as the independently checked PR-07j codec vectors. */
static tv4_wire_message sample(tv4_wire_kind kind,uint16_t length)
{
    tv4_wire_message m;memset(&m,0xa5,sizeof(m));m.kind=kind;
    tv4_update_descriptor d={.id={0x11223344u,0xa1b2c3d4u},.image_bytes=0x10203040u};
    for(unsigned i=0;i<32;++i)d.image_sha256[i]=(uint8_t)i;
    if(kind>=TV4_WIRE_REQUEST && kind<=TV4_WIRE_CONFIRM) {
        tv4_neg_message *v=&m.body.negotiation;
        v->kind=kind==TV4_WIRE_REQUEST?TV4_NEG_REQUEST:kind==TV4_WIRE_OFFER?TV4_NEG_OFFER:
            kind==TV4_WIRE_ACCEPT?TV4_NEG_ACCEPT:TV4_NEG_CONFIRM;
        v->request_id=d.id;v->descriptor=d;v->responder_boot=0x55667788u;v->block_bytes=256;
        v->descriptor.id=kind==TV4_WIRE_REQUEST?(tv4_update_id){0,0}:(tv4_update_id){0x55667788u,0x01020304u};
        for(unsigned i=0;i<32;++i){v->metadata_sha256[i]=(uint8_t)(i+32u);v->compatibility_tag[i]=(uint8_t)(i+64u);}
    }else if(kind==TV4_WIRE_DATA) {
        tv4_update_block *v=&m.body.data;v->id=d.id;v->sequence=0x01020304u;v->offset=0x05060708u;
        v->length=length;v->crc32c=0xdeadbeefu;
        for(unsigned i=0;i<length && i<TV4_UPDATE_BLOCK_MAX;++i)v->bytes[i]=(uint8_t)(i*7u+3u);
    }else if(kind==TV4_WIRE_DATA_ACK) {
        m.body.ack=(tv4_update_ack){.id=d.id,.next_sequence=0x01020305u,.next_offset=0x05060808u};
    }else {
        m.body.outcome.binding.image=d;m.body.outcome.code=TV4_OUTCOME_CHECKED_CLOSED;m.body.outcome.reason=TV4_OUTCOME_REASON_NONE;
        for(unsigned i=0;i<32;++i)m.body.outcome.binding.metadata_sha256[i]=(uint8_t)(i+32u);
    }
    return m;
}

static const tv4_fragment_config config={100,500,160,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{ const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true; }
static void rx_init(tv4_fragment_rx *rx,uint16_t counter,uint32_t now)
{ assert(tv4_fragment_rx_init(rx,&config,7,9,counter,now)==TV4_FRAG_OK); }
static size_t collect(const tv4_wire_message *m,tv4_fragment_frame frames[72],uint16_t seed)
{
    tv4_fragment_tx tx={0};assert(tv4_fragment_tx_init(&tx,&config,7,9,seed,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&tx,m,9,0)==TV4_FRAG_OK);
    size_t n=0;
    while(tx.storage.state==TV4_FRAG_SENDING) {
        assert(n<72 && tv4_fragment_tx_peek(&tx,&frames[n],9,0)==TV4_FRAG_FRAME);
        tv4_fragment_result r=tv4_fragment_tx_commit(&tx,&frames[n],9,0);
        assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);++n;
    }
    assert(tx.storage.counter==(uint16_t)(seed+1u) && zero(tx.storage.bytes,sizeof(tx.storage.bytes)));
    return n;
}
static void same_message(const tv4_wire_message *a,const tv4_wire_message *b)
{
    uint8_t x[TV4_WIRE_MESSAGE_MAX],y[TV4_WIRE_MESSAGE_MAX];size_t nx=0,ny=0;
    assert(tv4_update_wire_encode(a,x,sizeof(x),&nx)==TV4_WIRE_OK);
    assert(tv4_update_wire_encode(b,y,sizeof(y),&ny)==TV4_WIRE_OK && nx==ny && memcmp(x,y,nx)==0);
}
static void all_kinds_and_lengths(void)
{
    tv4_fragment_rx rx={0};rx_init(&rx,0,0);unsigned cases=0;
    for(unsigned kind=1;kind<=8;++kind)for(unsigned length=1;length<=(kind==5?256u:1u);++length) {
        tv4_wire_message m=sample((tv4_wire_kind)kind,(uint16_t)length),out;
        tv4_fragment_frame frames[72];size_t count=collect(&m,frames,(uint16_t)cases);
        for(size_t i=0;i<count;++i) {
            assert(frames[i].length>=5 && frames[i].length<=8);
            assert(tv4_fragment_rx_receive(&rx,7,&frames[i],9,0)==(i+1==count?TV4_FRAG_READY:TV4_FRAG_PENDING));
        }
        assert(tv4_fragment_rx_take(&rx,&out,9,0)==TV4_FRAG_TAKEN);same_message(&m,&out);
        assert(zero(rx.storage.bytes,sizeof(rx.storage.bytes)) && rx.storage.state==TV4_FRAG_IDLE);
        memset(&out,0xa5,sizeof(out));assert(tv4_fragment_rx_take(&rx,&out,9,0)==TV4_FRAG_STATE && zero(&out,sizeof(out)));
        assert(tv4_fragment_rx_receive(&rx,7,&frames[0],9,0)==TV4_FRAG_STALE);++cases;
    }
    assert(cases==263);printf("PASS263 all 8 kinds / DATA lengths1..256 / <=72 frames / single delivery; tx=%zu rx=%zu bytes (host only)\n",sizeof(tv4_fragment_tx),sizeof(tv4_fragment_rx));
}
static void malformed_envelopes(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA_ACK,1);tv4_fragment_frame f[72];size_t n=collect(&m,f,0);assert(n==6);
    /* Every DLC, all 32768 START lengths, every 16-bit continuation field. */
    for(unsigned length=0;length<=255;++length) {
        tv4_fragment_rx rx={0};rx_init(&rx,0,0);tv4_fragment_frame v=f[0];v.length=(uint8_t)length;
        tv4_fragment_result r=tv4_fragment_rx_receive(&rx,7,&v,9,0);
        assert(r==(length==8?TV4_FRAG_PENDING:TV4_FRAG_BAD_FRAME));
    }
    for(unsigned total=0;total<32768;++total) {
        tv4_fragment_rx rx={0};rx_init(&rx,0,0);tv4_fragment_frame v=f[0];unsigned field=0x8000u|total;
        v.bytes[2]=(uint8_t)field;v.bytes[3]=(uint8_t)(field>>8);
        tv4_fragment_result r=tv4_fragment_rx_receive(&rx,7,&v,9,0);
        assert(r==(total>=8 && total<=286?TV4_FRAG_PENDING:TV4_FRAG_BAD_FRAME));
    }
    for(unsigned offset=0;offset<=65535;++offset) {
        tv4_fragment_rx rx={0};rx_init(&rx,0,0);assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,0)==TV4_FRAG_PENDING);
        tv4_fragment_frame v=f[1];v.bytes[2]=(uint8_t)offset;v.bytes[3]=(uint8_t)(offset>>8);
        tv4_fragment_result r=tv4_fragment_rx_receive(&rx,7,&v,9,0);
        if(offset==4)assert(r==TV4_FRAG_PENDING);
        else assert(r!=TV4_FRAG_PENDING && r!=TV4_FRAG_READY);
    }
    tv4_fragment_rx rx={0};rx_init(&rx,0,0);tv4_fragment_frame v=f[0];v.bytes[0]=0;v.bytes[1]=0;
    assert(tv4_fragment_rx_receive(&rx,7,&v,9,0)==TV4_FRAG_BAD_FRAME);
    assert(tv4_fragment_rx_receive(&rx,8,&f[0],9,0)==TV4_FRAG_ROUTE && rx.storage.counter==0);
    assert(tv4_fragment_rx_receive(&rx,7,&f[1],9,0)==TV4_FRAG_ORDER && rx.storage.counter==0);
    puts("PASS all DLC256, START32768, continuation65536 fields; no padding or guessed order");
}
static void duplicates_order_and_pressure(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA,256),out;tv4_fragment_frame f[72];size_t n=collect(&m,f,36);assert(n==72 && f[71].length==6);
    tv4_fragment_rx rx={0};rx_init(&rx,36,0);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,0)==TV4_FRAG_PENDING);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,1)==TV4_FRAG_DUPLICATE && rx.storage.progress==0);
    assert(tv4_fragment_rx_receive(&rx,7,&f[2],9,2)==TV4_FRAG_ORDER && rx.storage.cursor==4);
    tv4_fragment_frame other=f[0];other.bytes[0]++;
    assert(tv4_fragment_rx_receive(&rx,7,&other,9,3)==TV4_FRAG_BUSY && rx.storage.counter==37);
    for(size_t i=1;i<n;++i) {
        assert(tv4_fragment_rx_receive(&rx,7,&f[i],9,4)==(i+1==n?TV4_FRAG_READY:TV4_FRAG_PENDING));
        if(i+1<n)assert(tv4_fragment_rx_receive(&rx,7,&f[i],9,4)==TV4_FRAG_DUPLICATE);
    }
    uint8_t before[TV4_WIRE_MESSAGE_MAX];memcpy(before,rx.storage.bytes,sizeof(before));uint16_t attempts=rx.storage.attempts;
    for(unsigned i=0;i<1000;++i)assert(tv4_fragment_rx_receive(&rx,7,&other,9,6)==TV4_FRAG_BUSY);
    assert(attempts==rx.storage.attempts && memcmp(before,rx.storage.bytes,sizeof(before))==0 && rx.storage.progress==4);
    assert(tv4_fragment_rx_take(&rx,&out,9,6)==TV4_FRAG_TAKEN);same_message(&m,&out);
    assert(tv4_fragment_rx_receive(&rx,7,&f[n-1],9,6)==TV4_FRAG_STALE);
    /* A different START cannot replace the active message; conflicting exact-ID
     * duplicate invalidates it, retaining high water across explicit clear. */
    assert(tv4_fragment_rx_receive(&rx,7,&other,9,6)==TV4_FRAG_PENDING);
    other.bytes[7]^=1;assert(tv4_fragment_rx_receive(&rx,7,&other,9,7)==TV4_FRAG_ERROR && rx.storage.error==TV4_FRAG_CONFLICT);
    assert(zero(rx.storage.bytes,sizeof(rx.storage.bytes)));
    assert(tv4_fragment_rx_clear(&rx,9,7)==TV4_FRAG_OK && rx.storage.counter==38);
    assert(tv4_fragment_rx_receive(&rx,7,&other,9,7)==TV4_FRAG_STALE);
    /* Conflicting old continuation, not just START. */
    tv4_fragment_rx c={0};rx_init(&c,0,0);
    assert(tv4_fragment_rx_receive(&c,7,&f[0],9,0)==TV4_FRAG_PENDING);
    assert(tv4_fragment_rx_receive(&c,7,&f[1],9,0)==TV4_FRAG_PENDING);
    other=f[1];other.bytes[4]^=1;
    assert(tv4_fragment_rx_receive(&c,7,&other,9,0)==TV4_FRAG_ERROR && c.storage.error==TV4_FRAG_CONFLICT);
    puts("PASS loss/order/retry/duplicate conflicts, route isolation, READY backpressure and exactly-once take");
}
static void tx_queue_and_snapshot(void)
{
    tv4_fragment_tx tx={0};tv4_wire_message m=sample(TV4_WIRE_DATA_ACK,1);tv4_fragment_frame a,b;
    assert(tv4_fragment_tx_init(&tx,&config,7,9,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&tx,&m,9,0)==TV4_FRAG_OK);m.body.ack.id.boot=0; /* owned snapshot */
    assert(tv4_fragment_tx_begin(&tx,&m,9,0)==TV4_FRAG_BUSY);
    assert(tv4_fragment_tx_commit(&tx,&a,9,0)==TV4_FRAG_STATE);
    assert(tv4_fragment_tx_peek(&tx,&a,9,1)==TV4_FRAG_FRAME);
    for(unsigned i=0;i<1000;++i) {
        assert(tv4_fragment_tx_peek(&tx,&b,9,2)==TV4_FRAG_FRAME && memcmp(&a,&b,sizeof(a))==0);
        assert(tx.storage.cursor==0 && tx.storage.progress==0);
    }
    b=a;b.bytes[4]^=1;assert(tv4_fragment_tx_commit(&tx,&b,9,2)==TV4_FRAG_ORDER);
    b=a;b.length=255;assert(tv4_fragment_tx_commit(&tx,&b,9,2)==TV4_FRAG_ORDER);
    assert(tv4_fragment_tx_commit(&tx,&a,9,3)==TV4_FRAG_COMMITTED && tx.storage.cursor==4);
    assert(tv4_fragment_tx_commit(&tx,&a,9,3)==TV4_FRAG_STATE);
    assert(tv4_fragment_tx_peek(&tx,&b,9,3)==TV4_FRAG_FRAME);
    assert(tv4_fragment_tx_commit(&tx,&a,9,3)==TV4_FRAG_ORDER && tx.storage.cursor==4);
    assert(tv4_fragment_tx_cancel(&tx,9,3)==TV4_FRAG_OK && zero(tx.storage.bytes,sizeof(tx.storage.bytes)));
    assert(tv4_fragment_tx_clear(&tx,9,3)==TV4_FRAG_OK && tx.storage.counter==1);
    assert(tv4_fragment_tx_begin(&tx,&m,9,3)==TV4_FRAG_BAD_FRAME && tx.storage.counter==1);
    puts("PASS owned TX snapshot, deterministic repeated peek, exact queue commit, stale commit and cancellation");
}
static void lost_message_retry(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA_ACK,1),out;
    tv4_fragment_frame f[72];size_t n=collect(&m,f,0);
    tv4_fragment_rx rx={0};rx_init(&rx,0,0);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,0)==TV4_FRAG_PENDING);
    /* Local TX commits do not prove remote delivery. Drop continuation1;
     * subsequent offsets reject. A new message cannot silently replace it. */
    for(size_t i=2;i<n;++i)assert(tv4_fragment_rx_receive(&rx,7,&f[i],9,1)==TV4_FRAG_ORDER);
    n=collect(&m,f,1);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,2)==TV4_FRAG_BUSY && rx.storage.counter==1);
    assert(tv4_fragment_rx_poll(&rx,9,100)==TV4_FRAG_ERROR);
    assert(tv4_fragment_rx_clear(&rx,9,100)==TV4_FRAG_OK && rx.storage.counter==1);
    for(size_t i=0;i<n;++i)assert(tv4_fragment_rx_receive(&rx,7,&f[i],9,100)==(i+1==n?TV4_FRAG_READY:TV4_FRAG_PENDING));
    assert(tv4_fragment_rx_take(&rx,&out,9,100)==TV4_FRAG_TAKEN);same_message(&m,&out);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,100)==TV4_FRAG_STALE);
    puts("PASS whole-message loss requires explicit timeout/clear and fresh counter; no implicit restart or remote ACK claim");
}
static void deadlines_and_context(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA_ACK,1),out;tv4_fragment_frame f[72];size_t n=collect(&m,f,0);
    tv4_fragment_rx rx={0};rx_init(&rx,0,0);assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,0)==TV4_FRAG_PENDING);
    for(uint32_t t=1;t<100;++t)assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,t)==TV4_FRAG_DUPLICATE);
    assert(tv4_fragment_rx_receive(&rx,7,&f[1],9,100)==TV4_FRAG_ERROR && rx.storage.error==TV4_FRAG_PROGRESS_TIMEOUT);
    assert(zero(rx.storage.bytes,sizeof(rx.storage.bytes)) && tv4_fragment_rx_clear(&rx,9,100)==TV4_FRAG_OK && rx.storage.counter==1);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,100)==TV4_FRAG_STALE);
    tv4_fragment_rx slow={0};rx_init(&slow,0,0);
    for(size_t i=0;i<n;++i)assert(tv4_fragment_rx_receive(&slow,7,&f[i],9,(uint32_t)i*90u)==(i+1==n?TV4_FRAG_READY:TV4_FRAG_PENDING));
    assert(tv4_fragment_rx_poll(&slow,9,499)==TV4_FRAG_OK); /* READY ignores progress only */
    memset(&out,0xa5,sizeof(out));assert(tv4_fragment_rx_take(&slow,&out,9,500)==TV4_FRAG_ERROR && zero(&out,sizeof(out)));
    assert(slow.storage.error==TV4_FRAG_TOTAL_TIMEOUT && zero(slow.storage.bytes,sizeof(slow.storage.bytes)));
    tv4_fragment_rx wrap={0};rx_init(&wrap,0,UINT32_MAX-9u);
    assert(tv4_fragment_rx_receive(&wrap,7,&f[0],9,UINT32_MAX-9u)==TV4_FRAG_PENDING);
    assert(tv4_fragment_rx_poll(&wrap,9,89)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_poll(&wrap,9,90)==TV4_FRAG_ERROR && wrap.storage.error==TV4_FRAG_PROGRESS_TIMEOUT);
    assert(tv4_fragment_rx_clear(&wrap,9,89)==TV4_FRAG_ERROR && wrap.storage.error==TV4_FRAG_CLOCK);
    assert(tv4_fragment_rx_clear(&wrap,9,90)==TV4_FRAG_ERROR);
    tv4_fragment_tx tx={0};assert(tv4_fragment_tx_init(&tx,&config,7,9,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&tx,&m,9,0)==TV4_FRAG_OK);
    tv4_fragment_frame a;assert(tv4_fragment_tx_peek(&tx,&a,9,99)==TV4_FRAG_FRAME);
    memset(&a,0xa5,sizeof(a));assert(tv4_fragment_tx_peek(&tx,&a,9,100)==TV4_FRAG_ERROR && zero(&a,sizeof(a)));
    assert(tx.storage.error==TV4_FRAG_PROGRESS_TIMEOUT && tv4_fragment_tx_clear(&tx,9,100)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&tx,&m,9,100)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_poll(&tx,10,100)==TV4_FRAG_ERROR && tx.storage.error==TV4_FRAG_CONTEXT);
    assert(tv4_fragment_tx_clear(&tx,9,100)==TV4_FRAG_ERROR && zero(tx.storage.bytes,sizeof(tx.storage.bytes)));
    tv4_fragment_rx revoked={0};rx_init(&revoked,0,0);
    for(size_t i=0;i<n;++i)(void)tv4_fragment_rx_receive(&revoked,7,&f[i],9,0);
    assert(tv4_fragment_rx_take(&revoked,&out,10,0)==TV4_FRAG_ERROR && revoked.storage.error==TV4_FRAG_CONTEXT && zero(&out,sizeof(out)));
    assert(tv4_fragment_rx_clear(&revoked,9,0)==TV4_FRAG_ERROR);
    tv4_fragment_tx drip={0};
    assert(tv4_fragment_tx_init(&drip,&config,7,9,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&drip,&m,9,0)==TV4_FRAG_OK);
    for(unsigned i=0;i<5;++i) {
        assert(tv4_fragment_tx_peek(&drip,&a,9,i*90u)==TV4_FRAG_FRAME);
        assert(tv4_fragment_tx_commit(&drip,&a,9,i*90u)==TV4_FRAG_COMMITTED);
    }
    assert(tv4_fragment_tx_peek(&drip,&a,9,450)==TV4_FRAG_FRAME);
    assert(tv4_fragment_tx_commit(&drip,&a,9,500)==TV4_FRAG_ERROR && drip.storage.error==TV4_FRAG_TOTAL_TIMEOUT);
    puts("PASS exact progress/total/READY deadlines, drip duplicates, wrap, backward clock, generation revocation and sticky clear");
}
static void budgets_counters_and_arguments(void)
{
    tv4_fragment_tx tx={0};tv4_fragment_rx rx={0};tv4_fragment_config bad=config;
    tv4_wire_message m=sample(TV4_WIRE_DATA_ACK,1),out;tv4_fragment_frame f[72],a;size_t n=collect(&m,f,0);assert(n==6);
    assert(tv4_fragment_tx_init(NULL,&config,7,9,0,0)==TV4_FRAG_ARGUMENT);
    assert(tv4_fragment_rx_init(NULL,&config,7,9,0,0)==TV4_FRAG_ARGUMENT);
    for(unsigned i=0;i<6;++i) {
        bad=config;
        switch(i){case 0:bad.version=0;break;case 1:bad.progress_timeout_ms=0;break;
        case 2:bad.total_timeout_ms=99;break;case 3:bad.total_timeout_ms=UINT32_MAX;break;
        case 4:bad.max_frames=0;break;default:bad.version=2;break;}
        assert(tv4_fragment_tx_init(&tx,&bad,7,9,0,0)==TV4_FRAG_ARGUMENT);
        assert(tv4_fragment_rx_init(&rx,&bad,7,9,0,0)==TV4_FRAG_ARGUMENT);
    }
    assert(tv4_fragment_tx_init(&tx,&config,0,9,0,0)==TV4_FRAG_ARGUMENT);
    assert(tv4_fragment_tx_init(&tx,&config,7,0,0,0)==TV4_FRAG_ARGUMENT);
    assert(tv4_fragment_tx_poll(&tx,9,0)==TV4_FRAG_ARGUMENT);
    bad=config;bad.max_frames=5;
    assert(tv4_fragment_tx_init(&tx,&bad,7,9,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&tx,&m,9,0)==TV4_FRAG_LIMIT && tx.storage.counter==0 && zero(tx.storage.bytes,sizeof(tx.storage.bytes)));
    assert(tv4_fragment_rx_init(&rx,&bad,7,9,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_receive(&rx,7,&f[0],9,0)==TV4_FRAG_LIMIT && rx.storage.counter==0);
    assert(tv4_fragment_rx_init(&rx,&config,7,9,0,0)==TV4_FRAG_STATE);
    tv4_fragment_rx quota={0};bad.max_frames=6;
    assert(tv4_fragment_rx_init(&quota,&bad,7,9,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_receive(&quota,7,&f[0],9,0)==TV4_FRAG_PENDING);
    for(unsigned i=1;i<6;++i)assert(tv4_fragment_rx_receive(&quota,7,&f[0],9,0)==TV4_FRAG_DUPLICATE);
    assert(tv4_fragment_rx_receive(&quota,7,&f[1],9,0)==TV4_FRAG_ERROR && quota.storage.error==TV4_FRAG_FRAME_BUDGET);
    assert(tv4_fragment_rx_clear(&quota,9,0)==TV4_FRAG_OK && quota.storage.counter==1);
    tv4_fragment_tx last={0};assert(tv4_fragment_tx_init(&last,&config,7,9,65534,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin(&last,&m,9,0)==TV4_FRAG_OK);
    for(unsigned i=0;i<6;++i){assert(tv4_fragment_tx_peek(&last,&a,9,0)==TV4_FRAG_FRAME && a.bytes[0]==255 && a.bytes[1]==255);
        assert(tv4_fragment_tx_commit(&last,&a,9,0)==(i==5?TV4_FRAG_FINISHED:TV4_FRAG_COMMITTED));}
    assert(tv4_fragment_tx_begin(&last,&m,9,0)==TV4_FRAG_ERROR && last.storage.error==TV4_FRAG_EXHAUSTED);
    assert(tv4_fragment_tx_clear(&last,9,0)==TV4_FRAG_ERROR && tv4_fragment_tx_init(&last,&config,7,9,0,0)==TV4_FRAG_STATE);
    memset(&out,0xa5,sizeof(out));assert(tv4_fragment_rx_take(NULL,&out,9,0)==TV4_FRAG_ARGUMENT && zero(&out,sizeof(out)));
    memset(&a,0xa5,sizeof(a));assert(tv4_fragment_tx_peek(NULL,&a,9,0)==TV4_FRAG_ARGUMENT && zero(&a,sizeof(a)));
    assert(tv4_fragment_rx_receive(&rx,7,NULL,9,0)==TV4_FRAG_ARGUMENT);
    assert(tv4_fragment_rx_cancel(&rx,9,0)==TV4_FRAG_OK && tv4_fragment_rx_clear(&rx,9,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_clear(&tx,9,0)==TV4_FRAG_STATE);
    puts("PASS explicit config, frame quotas, exhaustion without wrap, no reinit/reset bypass, failure output zeroing");
}
static void invalid_inner_and_crc(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA_ACK,1),out;tv4_fragment_frame f[72];size_t n=collect(&m,f,0);
    tv4_fragment_rx rx={0};rx_init(&rx,0,0);f[0].bytes[4]='X';
    for(size_t i=0;i<n;++i)assert(tv4_fragment_rx_receive(&rx,7,&f[i],9,0)==(i+1==n?TV4_FRAG_READY:TV4_FRAG_PENDING));
    memset(&out,0xa5,sizeof(out));assert(tv4_fragment_rx_take(&rx,&out,9,0)==TV4_FRAG_ERROR && rx.storage.error==TV4_FRAG_CODEC && zero(&out,sizeof(out)));
    assert(zero(rx.storage.bytes,sizeof(rx.storage.bytes)) && rx.storage.counter==1);
    tv4_update_limits limits={.max_image_bytes=1024,.ack_timeout_ms=10,.progress_timeout_ms=100,.total_timeout_ms=500,.block_bytes=256,.retry_limit=3};
    tv4_update_descriptor d={.id={7,9},.image_bytes=1};tv4_update_rx logical_rx;tv4_update_tx logical_tx;
    assert(tv4_update_rx_init(&logical_rx,limits,7,0) && tv4_update_tx_init(&logical_tx,limits,7,0));
    assert(tv4_update_rx_begin(&logical_rx,&d,0)==TV4_UPDATE_OK && tv4_update_tx_begin(&logical_tx,&d,0)==TV4_UPDATE_OK);
    uint8_t value=0x42;assert(tv4_update_tx_load(&logical_tx,&value,1,0)==TV4_UPDATE_OK);
    m.kind=TV4_WIRE_DATA;assert(tv4_update_tx_next(&logical_tx,&m.body.data,0)==TV4_UPDATE_OK);
    n=collect(&m,f,1);f[n-1].bytes[f[n-1].length-1u]^=1;
    assert(tv4_fragment_rx_clear(&rx,9,0)==TV4_FRAG_OK);
    for(size_t i=0;i<n;++i)(void)tv4_fragment_rx_receive(&rx,7,&f[i],9,0);
    assert(tv4_fragment_rx_take(&rx,&out,9,0)==TV4_FRAG_TAKEN);
    tv4_update_ack ack;assert(tv4_update_rx_receive(&logical_rx,&out.body.data,&ack,0)==TV4_UPDATE_REJECT_CHECKSUM && logical_rx.context.next_offset==0);
    puts("PASS READY is unchecked, malformed inner zeroes output, corrupted DATA still requires unchanged logical RX CRC");
}
static uint32_t random32(uint32_t *s) { *s=*s*1664525u+1013904223u;return *s; }
static void fault_corpus(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA,17);tv4_fragment_frame f[72];size_t n=collect(&m,f,0);uint32_t random=0x7432u;
    unsigned taken=0,rejected=0;
    for(unsigned iteration=0;iteration<100000;++iteration) {
        tv4_fragment_rx rx={0};rx_init(&rx,0,0);size_t changed=random32(&random)%n;
        for(size_t i=0;i<n;++i) {
            tv4_fragment_frame v=f[i];
            if(i==changed) {
                unsigned mode=iteration%4u;
                if(mode==0)v.length=(uint8_t)random32(&random);
                else if(mode==1)v.bytes[random32(&random)%8u]^=(uint8_t)(random32(&random)|1u);
                else if(mode==2)continue; /* missing frame, later offsets reject */
                else { (void)tv4_fragment_rx_receive(&rx,7,&v,9,0); } /* exact duplicate */
            }
            (void)tv4_fragment_rx_receive(&rx,7,&v,9,0);
            assert(rx.storage.cursor<=rx.storage.total && rx.storage.total<=TV4_WIRE_MESSAGE_MAX);
        }
        tv4_wire_message out;memset(&out,0xa5,sizeof(out));
        if(tv4_fragment_rx_take(&rx,&out,9,0)==TV4_FRAG_TAKEN)++taken;
        else { ++rejected;assert(zero(&out,sizeof(out))); }
        (void)tv4_fragment_rx_poll(&rx,9,500);
        assert(zero(rx.storage.bytes,sizeof(rx.storage.bytes)));
    }
    assert(taken>1000 && rejected>1000);
    printf("PASS100000 deterministic drop/mutation/DLC/duplicate chains: taken=%u structural only, rejected=%u zero output, bounded cleanup\n",taken,rejected);
}
static void vectors(void)
{
    const tv4_wire_kind kinds[]={TV4_WIRE_DATA_ACK,TV4_WIRE_DATA,TV4_WIRE_DATA};const uint16_t lengths[]={1,1,256};
    for(unsigned i=0;i<3;++i) {
        tv4_wire_message m=sample(kinds[i],lengths[i]);tv4_fragment_frame f[72];size_t n=collect(&m,f,37);
        printf("%u ",i);
        for(size_t j=0;j<n;++j){if(j)putchar(',');for(unsigned k=0;k<f[j].length;++k)printf("%02x",f[j].bytes[k]);}putchar('\n');
    }
}
#define RUN(name) do{name();puts("PASS " #name);}while(0)
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--vectors")==0){vectors();return 0;}
    assert(argc==1);RUN(all_kinds_and_lengths);RUN(malformed_envelopes);RUN(duplicates_order_and_pressure);
    RUN(tx_queue_and_snapshot);RUN(lost_message_retry);RUN(deadlines_and_context);RUN(budgets_counters_and_arguments);
    RUN(invalid_inner_and_crc);RUN(fault_corpus);return 0;
}
