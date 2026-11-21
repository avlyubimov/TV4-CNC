#include "update_metadata.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_metadata_document sample(uint16_t n)
{
    tv4_metadata_document m;memset(&m,0xa5,sizeof(m));
    m.binding.image.id=(tv4_update_id){7,9};m.binding.image.image_bytes=4096;
    for(unsigned i=0;i<32;++i){m.binding.image.image_sha256[i]=(uint8_t)i;m.binding.metadata_sha256[i]=(uint8_t)(i+32u);}
    m.document_bytes=n;m.signature_bytes=72;m.key_id_bytes=31;
    for(unsigned i=0;i<n && i<TV4_META_DOCUMENT_MAX;++i)m.document[i]=(uint8_t)(i*7u+3u);
    for(unsigned i=0;i<72;++i)m.signature[i]=(uint8_t)(i+64u);
    for(unsigned i=0;i<31;++i)m.key_id[i]=(uint8_t)('a'+i%26u);
    return m;
}
static size_t encode(const tv4_metadata_document *m,uint8_t bytes[TV4_META_BUNDLE_MAX])
{size_t n=0;assert(tv4_metadata_encode(m,bytes,TV4_META_BUNDLE_MAX,&n)==TV4_META_OK);return n;}
static void same(const tv4_metadata_document *a,const tv4_metadata_document *b)
{
    uint8_t x[TV4_META_BUNDLE_MAX],y[TV4_META_BUNDLE_MAX];size_t nx=encode(a,x),ny=encode(b,y);
    assert(nx==ny && memcmp(x,y,nx)==0);
}
static tv4_update_limits limits(void)
{return (tv4_update_limits){TV4_META_BUNDLE_MAX,20,200,2000,256,4};}
static void init(tv4_metadata_rx *rx,uint32_t last,uint32_t now)
{assert(tv4_metadata_rx_init(rx,limits(),7,11,last,now)==TV4_META_OK);}
static void transfer_raw(tv4_metadata_rx *rx,const uint8_t *raw,size_t n,const tv4_update_binding *binding,uint32_t id,uint32_t *now,bool lose_final)
{
    tv4_update_descriptor stream={.id={7,id},.image_bytes=(uint32_t)n};
    memset(stream.image_sha256,0x55,32); /* opaque bundle expectation, not verified here */
    assert(tv4_metadata_rx_begin(rx,&stream,binding,11,*now)==TV4_META_OK);
    tv4_update_tx tx;assert(tv4_update_tx_init(&tx,limits(),7,*now));
    assert(tv4_update_tx_begin(&tx,&stream,*now)==TV4_UPDATE_OK);
    unsigned rounds=0,writes=0;bool dropped=false;
    while(tx.context.state!=TV4_UPDATE_COMPLETE_UNVERIFIED) {
        assert(++rounds<20);
        if(tx.context.state==TV4_UPDATE_NEED_DATA) {
            size_t count=n-tx.context.next_offset;if(count>256)count=256;
            assert(tv4_update_tx_load(&tx,raw+tx.context.next_offset,count,*now)==TV4_UPDATE_OK);
        }
        tv4_update_block block;assert(tv4_update_tx_next(&tx,&block,*now)==TV4_UPDATE_OK);
        tv4_update_ack ack;uint32_t before=rx->stream.context.next_offset;
        assert(tv4_metadata_rx_receive(rx,&block,&ack,11,*now)==TV4_UPDATE_ACK);
        if(rx->stream.context.next_offset>before)++writes;
        uint32_t progress=rx->stream.context.progress_ms;
        assert(tv4_metadata_rx_receive(rx,&block,&ack,11,*now)==TV4_UPDATE_ACK);
        assert(rx->stream.context.next_offset==before+block.length || rx->stream.context.next_offset==before);
        assert(rx->stream.context.progress_ms==progress);
        if(lose_final && !dropped && rx->state==TV4_META_COMPLETE_UNVERIFIED){dropped=true;*now+=20;continue;}
        assert(tv4_update_tx_ack(&tx,&ack,*now)==TV4_UPDATE_OK);++*now;
    }
    assert(writes==(n+255u)/256u && rx->state==TV4_META_COMPLETE_UNVERIFIED);
    assert(memcmp(raw,rx->bytes,n)==0 && (dropped || !lose_final));
}
static void transfer(tv4_metadata_rx *rx,const tv4_metadata_document *m,uint32_t id,uint32_t *now,bool lose_final)
{
    uint8_t raw[TV4_META_BUNDLE_MAX];size_t n=encode(m,raw);transfer_raw(rx,raw,n,&m->binding,id,now,lose_final);
}
static void vectors(bool emit)
{
    for(unsigned i=0;i<2;++i) {
        tv4_metadata_document m=sample(i?1024:1),out;
        if(i==0){m.signature_bytes=8;m.key_id_bytes=1;}
        uint8_t raw[TV4_META_BUNDLE_MAX];size_t n=encode(&m,raw);
        assert(n==(i?TV4_META_BUNDLE_MAX:TV4_META_BUNDLE_MIN));
        assert(tv4_metadata_decode(raw,n,&m.binding,&out)==TV4_META_OK);same(&m,&out);
        if(emit){printf("%u ",i);for(size_t j=0;j<n;++j)printf("%02x",raw[j]);putchar('\n');}
    }
}
static void lengths_and_identity(void)
{
    uint32_t now=0;tv4_metadata_rx rx={0};init(&rx,9,now);
    for(unsigned n=1;n<=1024;++n) {
        tv4_metadata_document m=sample((uint16_t)n),out,second;
        transfer(&rx,&m,9u+n,&now,n%2u==0);
        assert(tv4_metadata_rx_take(&rx,&out,11,now)==TV4_META_TAKEN_UNVERIFIED);same(&m,&out);
        assert(zero(rx.bytes,sizeof(rx.bytes)) && rx.stream.retained);
        tv4_update_block tail=rx.stream.block;tv4_update_ack ack;uint32_t progress=rx.stream.context.progress_ms;
        assert(tv4_metadata_rx_receive(&rx,&tail,&ack,11,now)==TV4_UPDATE_ACK);
        assert(rx.stream.context.progress_ms==progress && zero(rx.bytes,sizeof(rx.bytes)));same(&m,&out);
        memset(&second,0xa5,sizeof(second));assert(tv4_metadata_rx_take(&rx,&second,11,now)==TV4_META_STATE && zero(&second,sizeof(second)));
        assert(tv4_metadata_rx_discard(&rx,11,now)==TV4_META_OK && rx.last_transfer==9u+n);
        assert(zero(&rx.stream.block,sizeof(rx.stream.block)));
        same(&m,&out); /* caller snapshot survives receiver discard */
    }
    tv4_metadata_document m=sample(1);tv4_update_descriptor d={.id={7,rx.last_transfer},.image_bytes=200};
    assert(tv4_metadata_rx_begin(&rx,&d,&m.binding,11,now)==TV4_META_IDENTITY);
    d.id.transfer=9;assert(tv4_metadata_rx_begin(&rx,&d,&m.binding,11,now)==TV4_META_IDENTITY);
    assert(tv4_metadata_rx_init(&rx,limits(),7,11,0,now)==TV4_META_STATE);
    tv4_metadata_rx end={0};init(&end,UINT32_MAX-1u,0);d.id.transfer=UINT32_MAX;
    assert(tv4_metadata_rx_begin(&end,&d,&m.binding,11,0)==TV4_META_OK);
    assert(tv4_metadata_rx_discard(&end,11,0)==TV4_META_OK && end.last_transfer==UINT32_MAX);
    d.id.transfer=0;assert(tv4_metadata_rx_begin(&end,&d,&m.binding,11,0)==TV4_META_IDENTITY);
    printf("PASS1024 document lengths, duplicate/final-ACK loss/copy-before-ACK/single take, fresh IDs/no reinit/wrap; rx=%zu snapshot=%zu host bytes\n",sizeof(rx),sizeof(m));
}
static void codec_faults(void)
{
    tv4_metadata_document m=sample(1024),out;uint8_t raw[TV4_META_BUNDLE_MAX+1],original[TV4_META_BUNDLE_MAX+1];
    size_t n=encode(&m,original);memcpy(raw,original,n);raw[n]=0;
    for(size_t length=0;length<n;++length) {
        memset(&out,0xa5,sizeof(out));assert(tv4_metadata_decode(raw,length,&m.binding,&out)!=TV4_META_OK && zero(&out,sizeof(out)));
        uint8_t dest[TV4_META_BUNDLE_MAX];memset(dest,0xa5,sizeof(dest));size_t written=99;
        assert(tv4_metadata_encode(&m,dest,length,&written)==TV4_META_LENGTH && written==0);
        for(size_t i=0;i<sizeof(dest);++i)assert(dest[i]==0xa5);
    }
    assert(tv4_metadata_decode(raw,n+1,&m.binding,&out)!=TV4_META_OK && zero(&out,sizeof(out)));
    const unsigned offsets[]={0,1,2,3,4,5,6,7,8,9,10,11};
    for(unsigned i=0;i<12;++i) {
        memcpy(raw,original,n);raw[offsets[i]]^=0x80;memset(&out,0xa5,sizeof(out));
        assert(tv4_metadata_decode(raw,n,&m.binding,&out)!=TV4_META_OK && zero(&out,sizeof(out)));
    }
    for(unsigned i=12;i<88;++i) {
        memcpy(raw,original,n);raw[i]^=1;assert(tv4_metadata_decode(raw,n,&m.binding,&out)==TV4_META_BINDING && zero(&out,sizeof(out)));
    }
    for(unsigned field=0;field<3;++field)for(unsigned value=0;value<=1025;++value) {
        tv4_metadata_document bad=m;
        if(field==0)bad.document_bytes=(uint16_t)value;else if(field==1)bad.signature_bytes=(uint16_t)value;else bad.key_id_bytes=(uint16_t)value;
        bool valid=field==0?value>=1 && value<=1024:field==1?value>=8 && value<=72:value>=1 && value<=31;
        size_t written=0;tv4_metadata_result r=tv4_metadata_encode(&bad,raw,sizeof(raw),&written);
        assert((r==TV4_META_OK)==valid);if(!valid)assert(written==0);
    }
    memcpy(raw,original,n);raw[n-1]^=1;
    assert(tv4_metadata_decode(raw,n,&m.binding,&out)==TV4_META_OK && out.document[1023]!=m.document[1023]);
    puts("PASS exact length/header/reserved/binding/field ranges; modified raw document structurally decodes and still needs independent signature/digest verification");
}
static void roles_crc_time_and_cancel(void)
{
    tv4_metadata_document m=sample(1),out;uint8_t raw[TV4_META_BUNDLE_MAX];size_t n=encode(&m,raw);
    tv4_update_descriptor d={.id={7,10},.image_bytes=(uint32_t)n};tv4_metadata_rx rx={0};init(&rx,0,0);
    assert(tv4_metadata_rx_begin(&rx,&d,&m.binding,11,0)==TV4_META_OK);
    tv4_update_block block={.id=m.binding.image.id,.length=1,.bytes={0x12}};tv4_update_ack ack;
    assert(tv4_update_block_checksum(&block,&block.crc32c));memset(&ack,0xa5,sizeof(ack));
    assert(tv4_metadata_rx_receive(&rx,&block,&ack,11,0)==TV4_UPDATE_REJECT_SESSION && zero(&ack,sizeof(ack)) && rx.stream.context.next_offset==0);
    block.id=d.id;assert(tv4_update_block_checksum(&block,&block.crc32c));block.bytes[0]^=1;
    assert(tv4_metadata_rx_receive(&rx,&block,&ack,11,0)==TV4_UPDATE_REJECT_CHECKSUM && zero(&ack,sizeof(ack)));
    assert(tv4_metadata_rx_poll(&rx,11,199)==TV4_META_OK);
    assert(tv4_metadata_rx_poll(&rx,11,200)==TV4_META_TIMEOUT && zero(rx.bytes,sizeof(rx.bytes)));
    assert(tv4_metadata_rx_discard(&rx,11,200)==TV4_META_OK && rx.last_transfer==10);
    d.id.transfer=11;assert(tv4_metadata_rx_begin(&rx,&d,&m.binding,11,200)==TV4_META_OK);
    assert(tv4_metadata_rx_poll(&rx,12,200)==TV4_META_CONTEXT && tv4_metadata_rx_discard(&rx,11,200)==TV4_META_CONTEXT);
    memset(&out,0xa5,sizeof(out));assert(tv4_metadata_rx_take(&rx,&out,11,200)==TV4_META_CONTEXT && zero(&out,sizeof(out)));
    tv4_metadata_rx clock={0};init(&clock,0,UINT32_MAX-9u);d.id.transfer=10;
    assert(tv4_metadata_rx_begin(&clock,&d,&m.binding,11,UINT32_MAX-9u)==TV4_META_OK);
    assert(tv4_metadata_rx_poll(&clock,11,189)==TV4_META_OK);
    assert(tv4_metadata_rx_poll(&clock,11,190)==TV4_META_TIMEOUT);
    assert(tv4_metadata_rx_discard(&clock,11,189)==TV4_META_CLOCK && tv4_metadata_rx_discard(&clock,11,190)==TV4_META_CLOCK);
    tv4_metadata_rx ready={0};init(&ready,0,0);uint32_t now=0;transfer(&ready,&m,10,&now,false);
    uint32_t progress=ready.stream.context.progress_ms;
    assert(tv4_metadata_rx_poll(&ready,11,progress+199u)==TV4_META_OK);
    memset(&out,0xa5,sizeof(out));assert(tv4_metadata_rx_take(&ready,&out,11,progress+200u)==TV4_META_TIMEOUT && zero(&out,sizeof(out)));
    tv4_metadata_rx cancelled={0};init(&cancelled,0,0);assert(tv4_metadata_rx_begin(&cancelled,&d,&m.binding,11,0)==TV4_META_OK);
    assert(tv4_metadata_rx_discard(&cancelled,11,0)==TV4_META_OK && cancelled.last_transfer==10 && cancelled.stream.context.state==TV4_UPDATE_ABORTED);
    puts("PASS image/metadata role separation, CRC, exact active/ready deadlines, wrap/backward/context/sticky cancellation and cache wipe");
}
static uint32_t random32(uint32_t *state){*state=*state*1664525u+1013904223u;return *state;}
static void invalid_complete_and_total_deadline(void)
{
    tv4_metadata_document m=sample(1024),out;uint8_t raw[TV4_META_BUNDLE_MAX];
    for(unsigned i=0;i<2;++i) {
        tv4_metadata_rx rx={0};init(&rx,0,0);uint32_t now=0;size_t n=encode(&m,raw);raw[i==0?2:12]^=1;
        transfer_raw(&rx,raw,n,&m.binding,10,&now,false);memset(&out,0xa5,sizeof(out));
        assert(tv4_metadata_rx_take(&rx,&out,11,now)==(i==0?TV4_META_FORMAT:TV4_META_BINDING));
        assert(rx.state==TV4_META_FAILED && zero(&out,sizeof(out)) && zero(rx.bytes,sizeof(rx.bytes)) && zero(&rx.stream.block,sizeof(rx.stream.block)));
        assert(tv4_metadata_rx_discard(&rx,11,now)==TV4_META_OK && rx.last_transfer==10);
        transfer(&rx,&m,11,&now,false);assert(tv4_metadata_rx_take(&rx,&out,11,now)==TV4_META_TAKEN_UNVERIFIED);same(&m,&out);
    }
    tv4_metadata_rx slow={0};tv4_update_limits slow_limits=limits();slow_limits.block_bytes=1;
    assert(tv4_metadata_rx_init(&slow,slow_limits,7,11,0,0)==TV4_META_OK);
    tv4_update_descriptor descriptor={.id={7,10},.image_bytes=TV4_META_BUNDLE_MAX};
    assert(tv4_metadata_rx_begin(&slow,&descriptor,&m.binding,11,0)==TV4_META_OK);
    tv4_update_tx tx;assert(tv4_update_tx_init(&tx,slow_limits,7,0) && tv4_update_tx_begin(&tx,&descriptor,0)==TV4_UPDATE_OK);
    for(unsigned i=0;i<=10;++i) {
        uint32_t now=i*199u;assert(tv4_update_tx_load(&tx,raw+i,1,now)==TV4_UPDATE_OK);
        tv4_update_block b;tv4_update_ack ack;assert(tv4_update_tx_next(&tx,&b,now)==TV4_UPDATE_OK);
        assert(tv4_metadata_rx_receive(&slow,&b,&ack,11,now)==TV4_UPDATE_ACK && tv4_update_tx_ack(&tx,&ack,now)==TV4_UPDATE_OK);
    }
    assert(tv4_metadata_rx_poll(&slow,11,1999)==TV4_META_OK);
    assert(tv4_metadata_rx_poll(&slow,11,2000)==TV4_META_TIMEOUT && slow.stream.context.error==TV4_UPDATE_ERROR_TOTAL_TIMEOUT);
    assert(slow.last_transfer==10 && zero(slow.bytes,sizeof(slow.bytes)));
    puts("PASS malformed complete container zeroes/wipes before delivery; fresh-ID recovery and exact total deadline despite slow progress");
}
static void mutation_corpus(void)
{
    tv4_metadata_document m=sample(1024),out;uint8_t original[TV4_META_BUNDLE_MAX],raw[TV4_META_BUNDLE_MAX],back[TV4_META_BUNDLE_MAX];
    size_t n=encode(&m,original);uint32_t state=0x71f407u;unsigned accepted=0,rejected=0;
    for(unsigned i=0;i<100000;++i) {
        memcpy(raw,original,n);size_t length=n;
        if(i%3u==0)length=random32(&state)%(TV4_META_BUNDLE_MAX+2u);
        else raw[random32(&state)%(uint32_t)n]^=(uint8_t)(random32(&state)|1u);
        memset(&out,0xa5,sizeof(out));tv4_metadata_result r=tv4_metadata_decode(raw,length,&m.binding,&out);
        if(r==TV4_META_OK){assert(encode(&out,back)==length && memcmp(raw,back,length)==0);++accepted;}
        else {assert(zero(&out,sizeof(out)));++rejected;}
    }
    assert(accepted>1000 && rejected>1000);
    printf("PASS100000 mutations: structurally accepted=%u rejected/zeroed=%u; no raw SHA/signature authority\n",accepted,rejected);
}
static void arguments(void)
{
    tv4_metadata_document m=sample(1),out;uint8_t raw[TV4_META_BUNDLE_MAX];size_t n=99;
    assert(tv4_metadata_encode(NULL,raw,sizeof(raw),&n)==TV4_META_ARGUMENT && n==0);
    assert(tv4_metadata_encode(&m,NULL,sizeof(raw),&n)==TV4_META_ARGUMENT && tv4_metadata_encode(&m,raw,sizeof(raw),NULL)==TV4_META_ARGUMENT);
    memset(&out,0xa5,sizeof(out));assert(tv4_metadata_decode(NULL,0,&m.binding,&out)==TV4_META_ARGUMENT && zero(&out,sizeof(out)));
    assert(tv4_metadata_decode(raw,0,&m.binding,NULL)==TV4_META_ARGUMENT);
    tv4_metadata_rx rx={0};tv4_update_limits bad=limits();bad.ack_timeout_ms=0;
    assert(tv4_metadata_rx_init(&rx,bad,7,11,0,0)==TV4_META_ARGUMENT);
    bad=limits();bad.max_image_bytes=TV4_META_BUNDLE_MAX+1;
    assert(tv4_metadata_rx_init(&rx,bad,7,11,0,0)==TV4_META_ARGUMENT);
    assert(tv4_metadata_rx_poll(NULL,11,0)==TV4_META_ARGUMENT && tv4_metadata_rx_discard(NULL,11,0)==TV4_META_ARGUMENT);
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--vectors")==0){vectors(true);return 0;}
    assert(argc==1);vectors(false);lengths_and_identity();codec_faults();roles_crc_time_and_cancel();invalid_complete_and_total_deadline();mutation_corpus();arguments();return 0;
}
