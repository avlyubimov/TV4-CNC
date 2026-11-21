#include "update_wire.h"
#include <limits.h>
#include <string.h>

_Static_assert(TV4_WIRE_MESSAGE_MAX == 286u, "Wire draft bound changed");
_Static_assert(sizeof(tv4_wire_message) <= 288u, "Caller message storage budget changed");

static void put16(uint8_t *p, uint16_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v)
{ for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8u)); }
static uint16_t get16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void put_id(uint8_t *p, tv4_update_id id)
{ put32(p,id.boot);put32(p+4,id.transfer); }
static tv4_update_id get_id(const uint8_t *p)
{ return (tv4_update_id){.boot=get32(p),.transfer=get32(p+4)}; }
static bool valid_id(tv4_update_id id) { return id.boot!=0 && id.transfer!=0; }
static void put_descriptor(uint8_t *p,const tv4_update_descriptor *d)
{ put_id(p,d->id);put32(p+8,d->image_bytes);memcpy(p+12,d->image_sha256,32); }
static void get_descriptor(const uint8_t *p,tv4_update_descriptor *d)
{ d->id=get_id(p);d->image_bytes=get32(p+8);memcpy(d->image_sha256,p+12,32); }
static tv4_neg_kind neg_kind(tv4_wire_kind kind)
{
    switch(kind) {
    case TV4_WIRE_REQUEST:return TV4_NEG_REQUEST;
    case TV4_WIRE_OFFER:return TV4_NEG_OFFER;
    case TV4_WIRE_ACCEPT:return TV4_NEG_ACCEPT;
    case TV4_WIRE_CONFIRM:return TV4_NEG_CONFIRM;
    default:return TV4_NEG_NO_MESSAGE;
    }
}
/* Explicit draft token tables: never serialize a C enum's representation. */
static const tv4_outcome_code codes[]={TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REJECTED_CLOSED,
    TV4_OUTCOME_CANCELLED_CLOSED,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_QUARANTINED};
static const tv4_outcome_reason reasons[]={TV4_OUTCOME_REASON_NONE,TV4_OUTCOME_SIGNATURE,
    TV4_OUTCOME_KEY,TV4_OUTCOME_METADATA,TV4_OUTCOME_COMPATIBILITY,TV4_OUTCOME_ROLLBACK,
    TV4_OUTCOME_LENGTH,TV4_OUTCOME_HASH,TV4_OUTCOME_PROFILE,TV4_OUTCOME_VECTORS,
    TV4_OUTCOME_SOURCE,TV4_OUTCOME_CONTEXT,TV4_OUTCOME_CLOCK,TV4_OUTCOME_TIMEOUT,
    TV4_OUTCOME_CANCEL,TV4_OUTCOME_RELEASE,TV4_OUTCOME_BINDING,TV4_OUTCOME_INTERNAL};
static uint8_t code_token(tv4_outcome_code code)
{
    for(size_t i=0;i<sizeof(codes)/sizeof(codes[0]);++i)if(codes[i]==code)return (uint8_t)(i+1u);
    return 0;
}
static uint8_t reason_token(tv4_outcome_reason reason)
{
    for(size_t i=0;i<sizeof(reasons)/sizeof(reasons[0]);++i)if(reasons[i]==reason)return (uint8_t)i;
    return UINT8_MAX;
}
static size_t body_size(const tv4_wire_message *m)
{
    tv4_neg_kind kind=neg_kind(m->kind);
    if(kind!=TV4_NEG_NO_MESSAGE)
        return m->body.negotiation.kind==kind && tv4_neg_message_valid(&m->body.negotiation)?122u:0u;
    switch(m->kind) {
    case TV4_WIRE_DATA: {
        const tv4_update_block *d=&m->body.data;
        return valid_id(d->id) && d->length>0 && d->length<=TV4_UPDATE_BLOCK_MAX &&
            d->offset<=UINT32_MAX-d->length ? 22u+d->length : 0u;
    }
    case TV4_WIRE_DATA_ACK:return valid_id(m->body.ack.id)?16u:0u;
    case TV4_WIRE_OUTCOME:case TV4_WIRE_OUTCOME_ACK:
        return tv4_update_outcome_valid(&m->body.outcome) && code_token(m->body.outcome.code)!=0 &&
            reason_token(m->body.outcome.reason)!=UINT8_MAX?78u:0u;
    default:return 0;
    }
}
tv4_wire_result tv4_update_wire_encode(const tv4_wire_message *m,uint8_t *out,size_t capacity,size_t *written)
{
    if(written!=NULL)*written=0;
    if(m==NULL || out==NULL || written==NULL)return TV4_WIRE_ARGUMENT;
    size_t n=body_size(m);
    if(n==0)return TV4_WIRE_CONTENT;
    if(capacity<n+TV4_WIRE_HEADER_BYTES)return TV4_WIRE_CAPACITY;
    out[0]='T';out[1]='U';out[2]=1;out[3]=(uint8_t)m->kind;put16(out+4,(uint16_t)n);out[6]=0;out[7]=0;
    uint8_t *p=out+TV4_WIRE_HEADER_BYTES;
    if(neg_kind(m->kind)!=TV4_NEG_NO_MESSAGE) {
        const tv4_neg_message *v=&m->body.negotiation;
        put_id(p,v->request_id);put_descriptor(p+8,&v->descriptor);put32(p+52,v->responder_boot);
        memcpy(p+56,v->metadata_sha256,32);memcpy(p+88,v->compatibility_tag,32);put16(p+120,v->block_bytes);
    }else if(m->kind==TV4_WIRE_DATA) {
        const tv4_update_block *v=&m->body.data;
        put_id(p,v->id);put32(p+8,v->sequence);put32(p+12,v->offset);put16(p+16,v->length);
        put32(p+18,v->crc32c);memcpy(p+22,v->bytes,v->length);
    }else if(m->kind==TV4_WIRE_DATA_ACK) {
        const tv4_update_ack *v=&m->body.ack;
        put_id(p,v->id);put32(p+8,v->next_sequence);put32(p+12,v->next_offset);
    }else {
        const tv4_update_outcome *v=&m->body.outcome;
        put_descriptor(p,&v->binding.image);memcpy(p+44,v->binding.metadata_sha256,32);
        p[76]=code_token(v->code);p[77]=reason_token(v->reason);
    }
    *written=n+TV4_WIRE_HEADER_BYTES;return TV4_WIRE_OK;
}
static tv4_wire_result decode_body(const uint8_t *p,size_t n,tv4_wire_message *m)
{
    tv4_neg_kind kind=neg_kind(m->kind);
    if(kind!=TV4_NEG_NO_MESSAGE) {
        if(n!=122)return TV4_WIRE_LENGTH;
        tv4_neg_message *v=&m->body.negotiation;v->kind=kind;v->request_id=get_id(p);
        get_descriptor(p+8,&v->descriptor);v->responder_boot=get32(p+52);
        memcpy(v->metadata_sha256,p+56,32);memcpy(v->compatibility_tag,p+88,32);v->block_bytes=get16(p+120);
    }else if(m->kind==TV4_WIRE_DATA) {
        if(n<22 || get16(p+16)==0 || get16(p+16)>TV4_UPDATE_BLOCK_MAX || n!=22u+get16(p+16))return TV4_WIRE_LENGTH;
        tv4_update_block *v=&m->body.data;v->id=get_id(p);v->sequence=get32(p+8);v->offset=get32(p+12);
        v->length=get16(p+16);v->crc32c=get32(p+18);memcpy(v->bytes,p+22,v->length);
    }else if(m->kind==TV4_WIRE_DATA_ACK) {
        if(n!=16)return TV4_WIRE_LENGTH;
        tv4_update_ack *v=&m->body.ack;v->id=get_id(p);v->next_sequence=get32(p+8);v->next_offset=get32(p+12);
    }else if(m->kind==TV4_WIRE_OUTCOME || m->kind==TV4_WIRE_OUTCOME_ACK) {
        if(n!=78)return TV4_WIRE_LENGTH;
        if(p[76]==0 || p[76]>sizeof(codes)/sizeof(codes[0]) || p[77]>=sizeof(reasons)/sizeof(reasons[0]))return TV4_WIRE_CONTENT;
        tv4_update_outcome *v=&m->body.outcome;get_descriptor(p,&v->binding.image);
        memcpy(v->binding.metadata_sha256,p+44,32);v->code=codes[p[76]-1u];v->reason=reasons[p[77]];
    }else return TV4_WIRE_HEADER;
    return body_size(m)==n?TV4_WIRE_OK:TV4_WIRE_CONTENT;
}
tv4_wire_result tv4_update_wire_decode(const uint8_t *bytes,size_t length,tv4_wire_message *m)
{
    if(m==NULL)return TV4_WIRE_ARGUMENT;
    memset(m,0,sizeof(*m));
    if(bytes==NULL)return TV4_WIRE_ARGUMENT;
    if(length<TV4_WIRE_HEADER_BYTES || length>TV4_WIRE_MESSAGE_MAX)return TV4_WIRE_LENGTH;
    if(bytes[0]!='T' || bytes[1]!='U' || bytes[2]!=1 || bytes[3]<1 || bytes[3]>8 || bytes[6]!=0 || bytes[7]!=0)
        return TV4_WIRE_HEADER;
    size_t n=get16(bytes+4);
    if(n!=length-TV4_WIRE_HEADER_BYTES)return TV4_WIRE_LENGTH;
    m->kind=(tv4_wire_kind)bytes[3];
    tv4_wire_result result=decode_body(bytes+TV4_WIRE_HEADER_BYTES,n,m);
    if(result!=TV4_WIRE_OK)memset(m,0,sizeof(*m));
    return result;
}
