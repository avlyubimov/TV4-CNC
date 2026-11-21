#include "update_metadata_setup.h"
#include <limits.h>
#include <string.h>

_Static_assert(TV4_NEG_REQUEST==1 && TV4_NEG_OFFER==2 && TV4_NEG_ACCEPT==3 && TV4_NEG_CONFIRM==4,"Review explicit MS tokens");
_Static_assert(sizeof(tv4_metadata_setup)<=512u,"Review setup state budget");
_Static_assert(sizeof(tv4_metadata_setup_message)<=176u,"Review setup message budget");
static bool id_equal(tv4_update_id a,tv4_update_id b)
{ return a.boot==b.boot && a.transfer==b.transfer; }
static bool descriptor_equal(const tv4_update_descriptor *a,const tv4_update_descriptor *b)
{ return id_equal(a->id,b->id) && a->image_bytes==b->image_bytes && !memcmp(a->image_sha256,b->image_sha256,32); }
static bool message_valid(const tv4_metadata_setup_message *m)
{
    if(m==NULL || m->kind<TV4_NEG_REQUEST || m->kind>TV4_NEG_CONFIRM ||
        m->request_id.boot==0 || m->request_id.transfer==0 || m->binding.image.id.boot==0 ||
        m->binding.image.id.transfer==0 || m->binding.image.id.transfer==UINT32_MAX ||
        m->request_id.boot==m->binding.image.id.boot || m->binding.image.image_bytes==0 ||
        m->metadata.image_bytes<TV4_META_BUNDLE_MIN || m->metadata.image_bytes>TV4_META_BUNDLE_MAX ||
        m->block_bytes==0 || m->block_bytes>TV4_UPDATE_BLOCK_MAX)return false;
    if(m->kind==TV4_NEG_REQUEST)return m->metadata.id.boot==0 && m->metadata.id.transfer==0;
    return m->metadata.id.boot==m->binding.image.id.boot &&
        m->metadata.id.transfer==m->binding.image.id.transfer+1u;
}
static void put16(uint8_t *p,uint16_t n)
{ p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8); }
static uint16_t get16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8)); }
static void put32(uint8_t *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8u*i)); }
static uint32_t get32(const uint8_t *p)
{ uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)p[i]<<(8u*i);return n; }
static void encode_descriptor(uint8_t *p,const tv4_update_descriptor *d)
{ put32(p,d->id.boot);put32(p+4,d->id.transfer);put32(p+8,d->image_bytes);memcpy(p+12,d->image_sha256,32); }
static void decode_descriptor(const uint8_t *p,tv4_update_descriptor *d)
{ d->id=(tv4_update_id){get32(p),get32(p+4)};d->image_bytes=get32(p+8);memcpy(d->image_sha256,p+12,32); }
tv4_metadata_setup_result tv4_metadata_setup_encode(const tv4_metadata_setup_message *m,
    uint8_t *bytes,size_t capacity,size_t *written)
{
    if(written!=NULL)*written=0;
    if(bytes==NULL || written==NULL || m==NULL)return TV4_SETUP_ARGUMENT;
    if(!message_valid(m))return TV4_SETUP_CONTENT;
    if(capacity<TV4_SETUP_MESSAGE_BYTES)return TV4_SETUP_LIMIT;
    bytes[0]='M';bytes[1]='S';bytes[2]=1;bytes[3]=(uint8_t)m->kind;
    put16(bytes+4,164);bytes[6]=0;bytes[7]=0;
    put32(bytes+8,m->request_id.boot);put32(bytes+12,m->request_id.transfer);
    encode_descriptor(bytes+16,&m->binding.image);memcpy(bytes+60,m->binding.metadata_sha256,32);
    encode_descriptor(bytes+92,&m->metadata);memcpy(bytes+136,m->compatibility_tag,32);
    put16(bytes+168,m->block_bytes);bytes[170]=0;bytes[171]=0;
    *written=TV4_SETUP_MESSAGE_BYTES;return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_decode(const uint8_t *bytes,size_t length,
    tv4_metadata_setup_message *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(bytes==NULL || out==NULL)return TV4_SETUP_ARGUMENT;
    if(length!=TV4_SETUP_MESSAGE_BYTES)return TV4_SETUP_LIMIT;
    if(bytes[0]!='M' || bytes[1]!='S' || bytes[2]!=1 || bytes[3]<1 || bytes[3]>4 ||
        get16(bytes+4)!=164 || bytes[6]!=0 || bytes[7]!=0 || bytes[170]!=0 || bytes[171]!=0)
        return TV4_SETUP_CONTENT;
    out->kind=(tv4_neg_kind)bytes[3];out->request_id=(tv4_update_id){get32(bytes+8),get32(bytes+12)};
    decode_descriptor(bytes+16,&out->binding.image);memcpy(out->binding.metadata_sha256,bytes+60,32);
    decode_descriptor(bytes+92,&out->metadata);memcpy(out->compatibility_tag,bytes+136,32);
    out->block_bytes=get16(bytes+168);
    if(!message_valid(out)){memset(out,0,sizeof(*out));return TV4_SETUP_CONTENT;}
    return TV4_SETUP_OK;
}
static bool terminal(const tv4_metadata_setup *s)
{ return s->state>=TV4_SETUP_CLOSED; }
static bool active(const tv4_metadata_setup *s)
{ return s->state!=TV4_SETUP_IDLE && !terminal(s); }
static void finish(tv4_metadata_setup *s,tv4_metadata_setup_state state,tv4_metadata_setup_error error)
{
    if(s->pool->owner==s)s->pool->owner=NULL;
    if(s->error==TV4_SETUP_NO_ERROR || error==TV4_SETUP_CONTEXT || error==TV4_SETUP_CLOCK)s->error=error;
    s->state=state;s->pending=TV4_NEG_NO_MESSAGE;
}
static tv4_metadata_setup_result fail(tv4_metadata_setup *s,tv4_metadata_setup_error error)
{ finish(s,error==TV4_SETUP_TIMEOUT?TV4_SETUP_EXPIRED:TV4_SETUP_FAILED,error);return TV4_SETUP_ERROR; }
static bool limits_equal(tv4_update_limits a,tv4_update_limits b)
{
    return a.max_image_bytes==b.max_image_bytes && a.ack_timeout_ms==b.ack_timeout_ms &&
        a.progress_timeout_ms==b.progress_timeout_ms && a.total_timeout_ms==b.total_timeout_ms &&
        a.block_bytes==b.block_bytes && a.retry_limit==b.retry_limit;
}
static bool image_matches(const tv4_metadata_setup *s,const tv4_negotiation *n)
{
    if(n==NULL || !n->configured || n->pool!=s->pool || n->role!=s->role ||
        n->state!=TV4_NEG_CLOSED || n->error!=TV4_NEG_ERROR_NONE || n->policy_generation!=s->generation ||
        n->local_boot!=s->local_boot || n->peer_boot!=s->peer_boot ||
        n->request.kind!=TV4_NEG_REQUEST || n->offer.kind!=TV4_NEG_OFFER ||
        !tv4_neg_message_valid(&n->request) || !tv4_neg_message_valid(&n->offer))return false;
    uint32_t requester=s->role==TV4_NEG_REQUESTER?s->local_boot:s->peer_boot;
    uint32_t responder=s->role==TV4_NEG_RESPONDER?s->local_boot:s->peer_boot;
    return n->request.request_id.boot==requester && n->offer.descriptor.id.boot==responder &&
        n->request.responder_boot==responder && n->offer.responder_boot==responder &&
        id_equal(n->request.request_id,n->offer.request_id) &&
        n->request.descriptor.image_bytes==n->offer.descriptor.image_bytes &&
        !memcmp(n->request.descriptor.image_sha256,n->offer.descriptor.image_sha256,32) &&
        !memcmp(n->request.metadata_sha256,n->offer.metadata_sha256,32) &&
        !memcmp(n->compatibility_tag,n->offer.compatibility_tag,32) &&
        !memcmp(n->request.compatibility_tag,n->offer.compatibility_tag,32) &&
        n->offer.block_bytes<=n->request.block_bytes;
}
static bool endpoint(tv4_metadata_setup *s,tv4_update_context *c,uint32_t now)
{
    if(c==NULL || !c->configured || c->error!=TV4_UPDATE_ERROR_NONE ||
        c->local_boot!=s->message.binding.image.id.boot || !descriptor_equal(&c->descriptor,&s->message.binding.image) ||
        !limits_equal(c->limits,s->image_limits) || c->started_ms!=s->image_started ||
        c->progress_ms!=s->image_progress || c->next_offset!=0 || c->next_sequence!=0 ||
        c->state!=(s->role==TV4_NEG_REQUESTER?TV4_UPDATE_NEED_DATA:TV4_UPDATE_RECEIVING))return false;
    return (s->role==TV4_NEG_REQUESTER?tv4_update_tx_poll(s->image_tx,now):tv4_update_rx_poll(s->image_rx,now))==TV4_UPDATE_OK;
}
static tv4_metadata_setup_result observe(tv4_metadata_setup *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured || s->pool==NULL)return TV4_SETUP_ARGUMENT;
    if(generation==0 || generation!=s->generation || !s->pool->configured ||
        s->pool->local_boot!=s->local_boot || s->pool->peer_boot!=s->peer_boot)return fail(s,TV4_SETUP_CONTEXT);
    if(now-s->observed>(uint32_t)INT32_MAX)return fail(s,TV4_SETUP_CLOCK);
    s->observed=now;
    if(terminal(s))return s->error==TV4_SETUP_NO_ERROR?TV4_SETUP_STATE:TV4_SETUP_ERROR;
    if(active(s)) {
        if(s->pool->owner!=s || s->pool->last_issued!=s->issued ||
            s->pool->last_peer_request!=s->peer_request)return fail(s,TV4_SETUP_CONTEXT);
        if(!image_matches(s,s->image) || !id_equal(s->image->request.request_id,s->message.request_id) ||
            !descriptor_equal(&s->image->offer.descriptor,&s->message.binding.image) ||
            memcmp(s->image->offer.metadata_sha256,s->message.binding.metadata_sha256,32) ||
            memcmp(s->image->offer.compatibility_tag,s->message.compatibility_tag,32) ||
            s->image->offer.block_bytes!=s->message.block_bytes)return fail(s,TV4_SETUP_IMAGE);
        tv4_update_context *c=s->role==TV4_NEG_REQUESTER?&s->image_tx->context:&s->image_rx->context;
        if(!endpoint(s,c,now))return fail(s,TV4_SETUP_IMAGE);
        if(now-s->started>=s->limits.total_ms)return fail(s,TV4_SETUP_TIMEOUT);
    }
    return TV4_SETUP_OK;
}
bool tv4_metadata_setup_init(tv4_metadata_setup *s,tv4_neg_pool *p,tv4_neg_role role,
    tv4_metadata_setup_limits limits,uint64_t generation,uint32_t now)
{
    if(s==NULL || s->configured || p==NULL || !p->configured || p->local_boot==0 ||
        p->peer_boot==0 || p->local_boot==p->peer_boot || generation==0 ||
        (role!=TV4_NEG_REQUESTER && role!=TV4_NEG_RESPONDER) || limits.retry_ms==0 ||
        limits.retry_ms>=limits.total_ms || limits.total_ms>INT32_MAX || limits.send_limit==0 ||
        limits.max_bundle_bytes<TV4_META_BUNDLE_MIN || limits.max_bundle_bytes>TV4_META_BUNDLE_MAX)return false;
    memset(s,0,sizeof(*s));s->pool=p;s->role=role;s->limits=limits;s->generation=generation;
    s->local_boot=p->local_boot;s->peer_boot=p->peer_boot;s->observed=now;s->configured=true;return true;
}
static tv4_metadata_setup_result bind(tv4_metadata_setup *s,const tv4_negotiation *n,
    tv4_update_tx *tx,tv4_update_rx *rx,uint64_t generation,uint32_t now)
{
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    if(s->state!=TV4_SETUP_IDLE)return TV4_SETUP_STATE;
    if(!image_matches(s,n) || s->pool->owner!=NULL)return TV4_SETUP_SESSION;
    if(n->offer.descriptor.id.transfer<=s->last_image || n->request.request_id.transfer<=s->last_request ||
        n->offer.descriptor.id.transfer==UINT32_MAX)return TV4_SETUP_SESSION;
    if((s->role==TV4_NEG_REQUESTER && (s->pool->last_issued!=n->request.request_id.transfer || tx==NULL)) ||
        (s->role==TV4_NEG_RESPONDER && (s->pool->last_issued!=n->offer.descriptor.id.transfer ||
          s->pool->last_peer_request!=n->request.request_id.transfer || rx==NULL)))return TV4_SETUP_SESSION;
    s->image=n;s->image_tx=tx;s->image_rx=rx;
    s->message=(tv4_metadata_setup_message){.request_id=n->request.request_id,
        .binding={.image=n->offer.descriptor},.block_bytes=n->offer.block_bytes};
    memcpy(s->message.binding.metadata_sha256,n->offer.metadata_sha256,32);
    memcpy(s->message.compatibility_tag,n->offer.compatibility_tag,32);
    tv4_update_context *c=s->role==TV4_NEG_REQUESTER?&tx->context:&rx->context;
    s->image_limits=c->limits;s->image_started=c->started_ms;s->image_progress=c->progress_ms;
    if(c->limits.block_bytes!=s->message.block_bytes || !endpoint(s,c,now))return fail(s,TV4_SETUP_IMAGE);
    if(s->role==TV4_NEG_RESPONDER) {
        if(tv4_neg_reserve_metadata(n,generation,&s->message.metadata.id)!=TV4_NEG_OK)
            return fail(s,TV4_SETUP_EXHAUSTED);
    }
    s->last_image=n->offer.descriptor.id.transfer;s->last_request=n->request.request_id.transfer;
    s->issued=s->pool->last_issued;s->peer_request=s->pool->last_peer_request;s->pool->owner=s;
    s->started=now;s->sent=now;s->sends=0;return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_request(tv4_metadata_setup *s,const tv4_negotiation *n,
    tv4_update_tx *tx,uint32_t size,const uint8_t hash[32],uint64_t generation,uint32_t now)
{
    if(s==NULL || hash==NULL || tx==NULL)return TV4_SETUP_ARGUMENT;
    if(s->role!=TV4_NEG_REQUESTER)return TV4_SETUP_STATE;
    if(size<TV4_META_BUNDLE_MIN || size>s->limits.max_bundle_bytes)return TV4_SETUP_LIMIT;
    tv4_metadata_setup_result r=bind(s,n,tx,NULL,generation,now);if(r!=TV4_SETUP_OK)return r;
    s->message.metadata.image_bytes=size;memcpy(s->message.metadata.image_sha256,hash,32);
    s->pending=TV4_NEG_REQUEST;s->state=TV4_SETUP_WAIT_OFFER;return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_respond(tv4_metadata_setup *s,const tv4_negotiation *n,
    tv4_update_rx *rx,uint64_t generation,uint32_t now)
{
    if(s==NULL || rx==NULL)return TV4_SETUP_ARGUMENT;
    if(s->role!=TV4_NEG_RESPONDER)return TV4_SETUP_STATE;
    tv4_metadata_setup_result r=bind(s,n,NULL,rx,generation,now);if(r!=TV4_SETUP_OK)return r;
    s->state=TV4_SETUP_WAIT_REQUEST;return TV4_SETUP_OK;
}
static bool content_equal(const tv4_metadata_setup_message *a,const tv4_metadata_setup_message *b)
{
    return id_equal(a->request_id,b->request_id) && tv4_update_binding_equal(&a->binding,&b->binding) &&
        a->block_bytes==b->block_bytes && !memcmp(a->compatibility_tag,b->compatibility_tag,32);
}
static bool metadata_equal(const tv4_metadata_setup_message *a,const tv4_metadata_setup_message *b)
{ return a->metadata.image_bytes==b->metadata.image_bytes && !memcmp(a->metadata.image_sha256,b->metadata.image_sha256,32); }
tv4_metadata_setup_result tv4_metadata_setup_receive(tv4_metadata_setup *s,const tv4_metadata_setup_message *m,
    uint64_t generation,uint32_t now)
{
    if(m==NULL)return TV4_SETUP_ARGUMENT;
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    if(!active(s))return TV4_SETUP_STATE;
    if(!message_valid(m))return TV4_SETUP_CONTENT;
    if(!id_equal(m->request_id,s->message.request_id) ||
        !id_equal(m->binding.image.id,s->message.binding.image.id))return TV4_SETUP_SESSION;
    if(!content_equal(m,&s->message))return fail(s,TV4_SETUP_CONFLICT);
    if(m->metadata.image_bytes>s->limits.max_bundle_bytes)return TV4_SETUP_LIMIT;
    if(s->role==TV4_NEG_RESPONDER && m->kind==TV4_NEG_REQUEST) {
        if(s->state==TV4_SETUP_WAIT_REQUEST) {
            s->message.metadata.image_bytes=m->metadata.image_bytes;
            memcpy(s->message.metadata.image_sha256,m->metadata.image_sha256,32);
            s->state=TV4_SETUP_WAIT_ACCEPT;s->pending=TV4_NEG_OFFER;s->sends=0;return TV4_SETUP_OK;
        }
        if(!metadata_equal(m,&s->message))return fail(s,TV4_SETUP_CONFLICT);
        return TV4_SETUP_DUPLICATE;
    }
    if(!metadata_equal(m,&s->message))return fail(s,TV4_SETUP_CONFLICT);
    if(s->role==TV4_NEG_REQUESTER && m->kind==TV4_NEG_OFFER) {
        if(s->state==TV4_SETUP_WAIT_OFFER) {
            if(s->sends==0)return TV4_SETUP_STATE;
            s->message.metadata.id=m->metadata.id;s->state=TV4_SETUP_WAIT_CONFIRM;
            s->pending=TV4_NEG_ACCEPT;s->sends=0;return TV4_SETUP_OK;
        }
        return TV4_SETUP_DUPLICATE;
    }
    if(s->role==TV4_NEG_RESPONDER && m->kind==TV4_NEG_ACCEPT) {
        if(s->state==TV4_SETUP_WAIT_ACCEPT) {
            if(s->sends==0)return TV4_SETUP_STATE;
            s->state=TV4_SETUP_READY;s->pending=TV4_NEG_CONFIRM;s->sends=0;return TV4_SETUP_OK;
        }
        if(s->state==TV4_SETUP_READY || s->state==TV4_SETUP_TAKEN)return TV4_SETUP_DUPLICATE;
        return TV4_SETUP_STATE;
    }
    if(s->role==TV4_NEG_REQUESTER && m->kind==TV4_NEG_CONFIRM) {
        if(s->state==TV4_SETUP_WAIT_CONFIRM) {
            if(s->sends==0)return TV4_SETUP_STATE;
            s->state=TV4_SETUP_READY;s->pending=TV4_NEG_NO_MESSAGE;return TV4_SETUP_OK;
        }
        if(s->state==TV4_SETUP_READY || s->state==TV4_SETUP_TAKEN)return TV4_SETUP_DUPLICATE;
    }
    return TV4_SETUP_STATE;
}
tv4_metadata_setup_result tv4_metadata_setup_next(tv4_metadata_setup *s,tv4_metadata_setup_message *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(out==NULL)return TV4_SETUP_ARGUMENT;
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    tv4_neg_kind kind=s->pending;
    if(kind==TV4_NEG_NO_MESSAGE) {
        if(now-s->sent<s->limits.retry_ms)return TV4_SETUP_WAIT;
        if(s->state==TV4_SETUP_WAIT_OFFER)kind=TV4_NEG_REQUEST;
        else if(s->state==TV4_SETUP_WAIT_ACCEPT)kind=TV4_NEG_OFFER;
        else if(s->state==TV4_SETUP_WAIT_CONFIRM)kind=TV4_NEG_ACCEPT;
        else if(s->role==TV4_NEG_RESPONDER && (s->state==TV4_SETUP_READY || s->state==TV4_SETUP_TAKEN))kind=TV4_NEG_CONFIRM;
        else return TV4_SETUP_WAIT;
    }
    if(s->sends>=s->limits.send_limit)return fail(s,TV4_SETUP_SENDS);
    *out=s->message;out->kind=kind;
    if(kind==TV4_NEG_REQUEST)out->metadata.id=(tv4_update_id){0,0};
    s->pending=TV4_NEG_NO_MESSAGE;++s->sends;s->sent=now;return TV4_SETUP_SENT;
}
tv4_metadata_setup_result tv4_metadata_setup_poll(tv4_metadata_setup *s,uint64_t generation,uint32_t now)
{ return observe(s,generation,now); }
tv4_metadata_setup_result tv4_metadata_setup_take(tv4_metadata_setup *s,tv4_metadata_parameters *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(out==NULL)return TV4_SETUP_ARGUMENT;
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    if(s->state!=TV4_SETUP_READY)return TV4_SETUP_STATE;
    *out=(tv4_metadata_parameters){.binding=s->message.binding,.metadata=s->message.metadata,.block_bytes=s->message.block_bytes};
    s->state=TV4_SETUP_TAKEN;return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_close_tx(tv4_metadata_setup *s,tv4_update_tx *tx,
    uint64_t generation,uint32_t now)
{
    if(tx==NULL)return TV4_SETUP_ARGUMENT;
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    if(s->role!=TV4_NEG_REQUESTER || s->state!=TV4_SETUP_TAKEN)return TV4_SETUP_STATE;
    if(!tx->context.configured || tx->context.local_boot!=s->message.metadata.id.boot ||
        !descriptor_equal(&tx->context.descriptor,&s->message.metadata) ||
        tx->context.limits.block_bytes!=s->message.block_bytes ||
        tv4_update_tx_poll(tx,now)!=TV4_UPDATE_OK || tx->context.state!=TV4_UPDATE_COMPLETE_UNVERIFIED ||
        tx->context.next_offset!=s->message.metadata.image_bytes)return TV4_SETUP_CONTENT;
    finish(s,TV4_SETUP_CLOSED,TV4_SETUP_NO_ERROR);return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_close_rx(tv4_metadata_setup *s,tv4_metadata_rx *rx,
    uint64_t generation,uint32_t now)
{
    if(rx==NULL)return TV4_SETUP_ARGUMENT;
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    if(s->role!=TV4_NEG_RESPONDER || s->state!=TV4_SETUP_TAKEN)return TV4_SETUP_STATE;
    if(tv4_metadata_rx_poll(rx,generation,now)!=TV4_META_OK || rx->state!=TV4_META_TAKEN ||
        !descriptor_equal(&rx->stream.context.descriptor,&s->message.metadata) ||
        rx->stream.context.limits.block_bytes!=s->message.block_bytes ||
        !tv4_update_binding_equal(&rx->binding,&s->message.binding))return TV4_SETUP_CONTENT;
    finish(s,TV4_SETUP_CLOSED,TV4_SETUP_NO_ERROR);return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_abort(tv4_metadata_setup *s,uint64_t generation,uint32_t now)
{
    tv4_metadata_setup_result r=observe(s,generation,now);if(r!=TV4_SETUP_OK)return r;
    finish(s,TV4_SETUP_ABORTED,TV4_SETUP_NO_ERROR);return TV4_SETUP_OK;
}
tv4_metadata_setup_result tv4_metadata_setup_clear(tv4_metadata_setup *s,uint64_t generation,uint32_t now)
{
    tv4_metadata_setup_result r=observe(s,generation,now);if(r==TV4_SETUP_ARGUMENT)return r;
    if(!terminal(s))return TV4_SETUP_STATE;
    if(s->error==TV4_SETUP_CLOCK || s->error==TV4_SETUP_CONTEXT)return TV4_SETUP_ERROR;
    memset(&s->message,0,sizeof(s->message));s->image=NULL;s->image_tx=NULL;s->image_rx=NULL;
    s->state=TV4_SETUP_IDLE;s->error=TV4_SETUP_NO_ERROR;s->sends=0;return TV4_SETUP_OK;
}
