#include "update_wire.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool zero(const void *p,size_t n)
{ const uint8_t *v=p;for(size_t i=0;i<n;++i)if(v[i]!=0)return false;return true; }
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
static size_t encode(const tv4_wire_message *m,uint8_t out[TV4_WIRE_MESSAGE_MAX])
{ size_t n=0;assert(tv4_update_wire_encode(m,out,TV4_WIRE_MESSAGE_MAX,&n)==TV4_WIRE_OK);return n; }
static void rejected(const uint8_t *raw,size_t n)
{
    tv4_wire_message out;memset(&out,0xa5,sizeof(out));
    assert(tv4_update_wire_decode(raw,n,&out)!=TV4_WIRE_OK && zero(&out,sizeof(out)));
}
static void encode_rejected(const tv4_wire_message *m)
{
    uint8_t out[TV4_WIRE_MESSAGE_MAX],before[TV4_WIRE_MESSAGE_MAX];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));size_t n=77;
    assert(tv4_update_wire_encode(m,out,sizeof(out),&n)!=TV4_WIRE_OK && n==0 && !memcmp(out,before,sizeof(out)));
}
static void vectors(bool emit)
{
    for(unsigned k=1;k<=8;++k) {
        tv4_wire_message m=sample((tv4_wire_kind)k,17),before=m,out;
        uint8_t region[TV4_WIRE_MESSAGE_MAX+2],again[TV4_WIRE_MESSAGE_MAX];memset(region,0xcc,sizeof(region));
        size_t n=encode(&m,region+1);assert(!memcmp(&before,&m,sizeof(m)));
        assert(region[0]==0xcc && region[n+1]==0xcc);
        assert(tv4_update_wire_decode(region+1,n,&out)==TV4_WIRE_OK && out.kind==m.kind);
        size_t other=encode(&out,again);assert(other==n && !memcmp(region+1,again,n));
        assert(region[1]=='T' && region[2]=='U' && region[3]==1 && region[4]==k && region[7]==0 && region[8]==0);
        if(emit){printf("%u ",k);for(size_t i=0;i<n;++i)printf("%02x",region[i+1]);putchar('\n');}
    }
    if(emit) {
        const tv4_outcome_code codes[]={TV4_OUTCOME_REJECTED_CLOSED,TV4_OUTCOME_CANCELLED_CLOSED,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_QUARANTINED};
        const tv4_outcome_reason reasons[]={TV4_OUTCOME_ROLLBACK,TV4_OUTCOME_CANCEL,TV4_OUTCOME_INTERNAL,TV4_OUTCOME_RELEASE};
        for(unsigned i=0;i<4;++i) {
            tv4_wire_message m=sample(TV4_WIRE_OUTCOME,1);m.body.outcome.code=codes[i];m.body.outcome.reason=reasons[i];
            uint8_t raw[TV4_WIRE_MESSAGE_MAX];size_t n=encode(&m,raw);
            printf("%u ",i+9u);for(size_t j=0;j<n;++j)printf("%02x",raw[j]);putchar('\n');
        }
    }
}
static void lengths_headers_and_capacities(void)
{
    for(unsigned k=1;k<=8;++k) {
        tv4_wire_message m=sample((tv4_wire_kind)k,256);
        uint8_t raw[TV4_WIRE_MESSAGE_MAX+1],copy[TV4_WIRE_MESSAGE_MAX+1],out[TV4_WIRE_MESSAGE_MAX];
        memset(raw,0,sizeof(raw));size_t n=encode(&m,raw);memcpy(copy,raw,sizeof(raw));
        for(size_t i=0;i<n;++i){rejected(raw,i);size_t written=99;memset(out,0xa5,sizeof(out));
            assert(tv4_update_wire_encode(&m,out,i,&written)==TV4_WIRE_CAPACITY && written==0);
            for(size_t j=0;j<sizeof(out);++j)assert(out[j]==0xa5);}
        rejected(raw,n+1);
        const unsigned indexes[]={0,1,2,4,5,6,7};
        for(size_t i=0;i<sizeof(indexes)/sizeof(indexes[0]);++i){memcpy(raw,copy,sizeof(raw));raw[indexes[i]]^=0x80;rejected(raw,n);}
        for(unsigned value=0;value<=255;++value)if(value==0 || value>8){memcpy(raw,copy,sizeof(raw));raw[3]=(uint8_t)value;rejected(raw,n);}
    }
}
static void structural_content_and_tokens(void)
{
    assert(!tv4_neg_message_valid(NULL));
    for(unsigned k=1;k<=4;++k)for(unsigned f=0;f<9;++f) {
        tv4_wire_message m=sample((tv4_wire_kind)k,1);tv4_neg_message *v=&m.body.negotiation;
        switch(f){case 0:v->request_id.boot=0;break;case 1:v->request_id.transfer=0;break;case 2:v->responder_boot=0;break;
        case 3:v->responder_boot=v->request_id.boot;break;case 4:v->descriptor.image_bytes=0;break;case 5:v->block_bytes=0;break;
        case 6:v->block_bytes=257;break;case 7:v->descriptor.id.boot^=1;break;
        case 8:v->kind=TV4_NEG_NO_MESSAGE;break;default:assert(false);}
        encode_rejected(&m);
    }
    for(unsigned code=0;code<=255;++code)for(unsigned reason=0;reason<=255;++reason) {
        tv4_wire_message m=sample(TV4_WIRE_OUTCOME,1);uint8_t raw[TV4_WIRE_MESSAGE_MAX];size_t n=encode(&m,raw);
        raw[8+76]=(uint8_t)code;raw[8+77]=(uint8_t)reason;
        tv4_wire_message out;tv4_wire_result r=tv4_update_wire_decode(raw,n,&out);
        if(r==TV4_WIRE_OK){assert(tv4_update_outcome_valid(&out.body.outcome));uint8_t back[TV4_WIRE_MESSAGE_MAX];assert(encode(&out,back)==n && !memcmp(back,raw,n));}
        else assert(zero(&out,sizeof(out)));
        if(code==0 || code>5 || reason>17)assert(r!=TV4_WIRE_OK);
    }
    tv4_wire_message invalid=sample(TV4_WIRE_OUTCOME,1);invalid.body.outcome.reason=TV4_OUTCOME_SIGNATURE;encode_rejected(&invalid);
    invalid.kind=(tv4_wire_kind)-1;encode_rejected(&invalid);invalid.kind=TV4_WIRE_NONE;encode_rejected(&invalid);
    puts("PASS 65536 outcome code/reason byte combinations; unknown and incompatible tokens reject with zero output");
}
static void malformed_body_fields(void)
{
    for(unsigned kind=1;kind<=8;++kind) {
        tv4_wire_message m=sample((tv4_wire_kind)kind,1);uint8_t raw[TV4_WIRE_MESSAGE_MAX],original[TV4_WIRE_MESSAGE_MAX];size_t n=encode(&m,original);
        memcpy(raw,original,n);memset(raw+8,0,4);rejected(raw,n);
        if(kind<=4) {
            const unsigned offsets[]={12,24,60,128};const size_t widths[]={4,4,4,2};
            for(unsigned i=0;i<4;++i){memcpy(raw,original,n);memset(raw+offsets[i],0,widths[i]);rejected(raw,n);}
            memcpy(raw,original,n);memcpy(raw+60,raw+8,4);rejected(raw,n);
            memcpy(raw,original,n);if(kind==1)raw[16]=1;else memset(raw+16,0,4);rejected(raw,n);
        }else if(kind==5) {
            memcpy(raw,original,n);memset(raw+20,0xff,4);rejected(raw,n);
            memcpy(raw,original,n);raw[24]=0;rejected(raw,n);
            memcpy(raw,original,n);raw[24]=1;raw[25]=1;rejected(raw,n);
        }
    }
}
static void all_data_lengths_and_opaque_tail(void)
{
    for(unsigned length=1;length<=256;++length) {
        tv4_wire_message m=sample(TV4_WIRE_DATA,(uint16_t)length);m.body.data.offset=UINT32_MAX-length;
        uint8_t raw[TV4_WIRE_MESSAGE_MAX],back[TV4_WIRE_MESSAGE_MAX];size_t n=encode(&m,raw);assert(n==30u+length);
        memset(m.body.data.bytes+length,0x17,256u-length);assert(encode(&m,back)==n && !memcmp(raw,back,n));
        tv4_wire_message out;assert(tv4_update_wire_decode(raw,n,&out)==TV4_WIRE_OK);
        assert(zero(out.body.data.bytes+length,256u-length));
        ++m.body.data.offset;encode_rejected(&m);
    }
    tv4_wire_message m=sample(TV4_WIRE_DATA,1);m.body.data.id.boot=0;encode_rejected(&m);
    m=sample(TV4_WIRE_DATA,1);m.body.data.length=0;encode_rejected(&m);m.body.data.length=257;encode_rejected(&m);
    m=sample(TV4_WIRE_DATA_ACK,1);m.body.ack.id.transfer=0;encode_rejected(&m);
    puts("PASS all256 DATA lengths; uint32 offset overflow, tail zeroing and no padding/tail emission");
}
static uint32_t random32(uint32_t *state)
{ *state^=*state<<13;*state^=*state>>17;*state^=*state<<5;return *state; }
static void malformed_corpus(void)
{
    uint32_t state=0x713ba51du;unsigned accepted=0,rejects=0;
    for(unsigned iteration=0;iteration<200000;++iteration) {
        tv4_wire_message m=sample((tv4_wire_kind)(iteration%8u+1u),(uint16_t)(iteration%256u+1u)),out;
        uint8_t raw[TV4_WIRE_MESSAGE_MAX+1],back[TV4_WIRE_MESSAGE_MAX];memset(raw,0,sizeof(raw));size_t n=encode(&m,raw);
        if(iteration%3u==0)n=random32(&state)%(TV4_WIRE_MESSAGE_MAX+2u);
        else raw[random32(&state)%(uint32_t)n]^=(uint8_t)(random32(&state)|1u);
        if(n>sizeof(raw)){rejected(raw,n);++rejects;continue;}
        uint8_t before[sizeof(raw)];memcpy(before,raw,sizeof(raw));
        tv4_wire_result r=tv4_update_wire_decode(raw,n,&out);assert(!memcmp(raw,before,sizeof(raw)));
        if(r==TV4_WIRE_OK){assert(encode(&out,back)==n && !memcmp(raw,back,n));++accepted;}
        else{assert(zero(&out,sizeof(out)));++rejects;}
    }
    assert(accepted>1000 && rejects>1000);printf("PASS 200000 deterministic malformed/mutation cases: accepted=%u canonical, rejected=%u zeroed; structural only\n",accepted,rejects);
}
static void crc_remains_receiver_work(void)
{
    tv4_update_limits limits={.max_image_bytes=1024,.ack_timeout_ms=10,.progress_timeout_ms=100,.total_timeout_ms=500,.block_bytes=256,.retry_limit=3};
    tv4_update_descriptor d={.id={7,9},.image_bytes=1};tv4_update_rx rx;tv4_update_tx tx;
    assert(tv4_update_rx_init(&rx,limits,7,0) && tv4_update_tx_init(&tx,limits,7,0));
    assert(tv4_update_rx_begin(&rx,&d,0)==TV4_UPDATE_OK && tv4_update_tx_begin(&tx,&d,0)==TV4_UPDATE_OK);
    uint8_t value=0x42;assert(tv4_update_tx_load(&tx,&value,1,0)==TV4_UPDATE_OK);
    tv4_wire_message m={.kind=TV4_WIRE_DATA},out;assert(tv4_update_tx_next(&tx,&m.body.data,0)==TV4_UPDATE_OK);
    uint8_t raw[TV4_WIRE_MESSAGE_MAX];size_t n=encode(&m,raw);raw[n-1]^=1;
    assert(tv4_update_wire_decode(raw,n,&out)==TV4_WIRE_OK);tv4_update_ack ack;
    assert(tv4_update_rx_receive(&rx,&out.body.data,&ack,0)==TV4_UPDATE_REJECT_CHECKSUM && rx.context.next_offset==0);
    raw[n-1]^=1;assert(tv4_update_wire_decode(raw,n,&out)==TV4_WIRE_OK);
    assert(tv4_update_rx_receive(&rx,&out.body.data,&ack,0)==TV4_UPDATE_PENDING);
    assert(tv4_update_rx_receive(&rx,&out.body.data,&ack,0)==TV4_UPDATE_PENDING);
    assert(tv4_update_rx_consume(&rx,d.id,0,true,&ack,0)==TV4_UPDATE_ACK);
    m.kind=TV4_WIRE_DATA_ACK;m.body.ack=ack;n=encode(&m,raw);assert(tv4_update_wire_decode(raw,n,&out)==TV4_WIRE_OK);
    assert(tv4_update_tx_ack(&tx,&out.body.ack,0)==TV4_UPDATE_OK);
    puts("PASS corrupted DATA structurally decodes but existing RX rejects CRC before progress; consumer ACK still mandatory");
}
static void arguments(void)
{
    tv4_wire_message m=sample(TV4_WIRE_DATA,1),out;uint8_t raw[TV4_WIRE_MESSAGE_MAX];size_t n=99;
    assert(tv4_update_wire_encode(NULL,raw,sizeof(raw),&n)==TV4_WIRE_ARGUMENT && n==0);
    assert(tv4_update_wire_encode(&m,NULL,sizeof(raw),&n)==TV4_WIRE_ARGUMENT && n==0);
    assert(tv4_update_wire_encode(&m,raw,sizeof(raw),NULL)==TV4_WIRE_ARGUMENT);
    assert(tv4_update_wire_decode(raw,0,NULL)==TV4_WIRE_ARGUMENT);
    memset(&out,0xa5,sizeof(out));assert(tv4_update_wire_decode(NULL,0,&out)==TV4_WIRE_ARGUMENT && zero(&out,sizeof(out)));
    rejected(raw,SIZE_MAX);
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--vectors")){vectors(true);return 0;}assert(argc==1);
    vectors(false);lengths_headers_and_capacities();structural_content_and_tokens();malformed_body_fields();all_data_lengths_and_opaque_tail();
    malformed_corpus();crc_remains_receiver_work();arguments();
    printf("PASS wire draft:8 message kinds, max=%u caller_message=%zu; no CAN/device/installation\n",TV4_WIRE_MESSAGE_MAX,sizeof(tv4_wire_message));return 0;
}
