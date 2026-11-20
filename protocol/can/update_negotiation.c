#include "update_negotiation.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_negotiation)<=384,"Negotiation state budget changed");
_Static_assert(sizeof(tv4_neg_message)<=128,"Logical message budget changed");
static bool tag_known(const uint8_t tag[32])
{
    if(tag==NULL)return false;
    uint8_t any=0;
    for(size_t i=0;i<32;i++)any|=tag[i];
    return any!=0;
}
static bool limits_valid(tv4_neg_limits l)
{
    return l.max_image_bytes!=0 && l.retry_ms!=0 && l.retry_ms<l.total_ms &&
        l.total_ms<=INT32_MAX && l.block_ceiling!=0 &&
        l.block_ceiling<=TV4_UPDATE_BLOCK_MAX && l.send_limit!=0;
}
bool tv4_neg_pool_init(tv4_neg_pool *p,uint32_t local,uint32_t peer,uint32_t last)
{
    if(p==NULL || p->configured || local==0 || peer==0 || local==peer)return false;
    *p=(tv4_neg_pool){.local_boot=local,.peer_boot=peer,.last_issued=last,.configured=true};
    return true;
}
bool tv4_neg_init(tv4_negotiation *s,tv4_neg_pool *p,tv4_neg_role role,
    tv4_neg_limits limits,const uint8_t tag[32],uint64_t generation,uint32_t now)
{
    if(s==NULL || s->configured || p==NULL || !p->configured || p->local_boot==0 ||
        p->peer_boot==0 || p->local_boot==p->peer_boot ||
        (role!=TV4_NEG_REQUESTER && role!=TV4_NEG_RESPONDER) || !limits_valid(limits) ||
        !tag_known(tag) || generation==0)return false;
    memset(s,0,sizeof(*s));
    s->pool=p;s->limits=limits;s->role=role;s->policy_generation=generation;
    s->local_boot=p->local_boot;s->peer_boot=p->peer_boot;s->observed_ms=now;
    memcpy(s->compatibility_tag,tag,32);s->configured=true;
    return true;
}
static bool terminal(const tv4_negotiation *s)
{
    return s->state==TV4_NEG_CLOSED || s->state==TV4_NEG_ABORTED ||
        s->state==TV4_NEG_EXPIRED || s->state==TV4_NEG_FAILED;
}
static void finish(tv4_negotiation *s,tv4_neg_state state,tv4_neg_error error)
{
    if(s->error==TV4_NEG_ERROR_NONE)s->error=error;
    if(s->pool->owner==s)s->pool->owner=NULL;
    memset(&s->request,0,sizeof(s->request));memset(&s->offer,0,sizeof(s->offer));
    s->pending=TV4_NEG_NO_MESSAGE;s->state=state;
}
static tv4_neg_result fail(tv4_negotiation *s,tv4_neg_error error)
{
    finish(s,error==TV4_NEG_ERROR_TIMEOUT?TV4_NEG_EXPIRED:TV4_NEG_FAILED,error);
    return TV4_NEG_ERROR;
}
static tv4_neg_result observe(tv4_negotiation *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured || s->pool==NULL)return TV4_NEG_REJECT_ARGUMENT;
    if(terminal(s))return s->error==TV4_NEG_ERROR_NONE?TV4_NEG_REJECT_STATE:TV4_NEG_ERROR;
    if(generation==0 || generation!=s->policy_generation)return fail(s,TV4_NEG_ERROR_CONTEXT);
    if(!s->pool->configured || s->pool->local_boot!=s->local_boot ||
        s->pool->peer_boot!=s->peer_boot)return fail(s,TV4_NEG_ERROR_OWNER);
    if((uint32_t)(now-s->observed_ms)>INT32_MAX)return fail(s,TV4_NEG_ERROR_CLOCK);
    s->observed_ms=now;
    if(s->state!=TV4_NEG_IDLE) {
        uint32_t issued=s->role==TV4_NEG_REQUESTER?s->request.request_id.transfer:s->offer.descriptor.id.transfer;
        if(s->pool->owner!=s || s->pool->last_issued<issued)return fail(s,TV4_NEG_ERROR_OWNER);
        if((uint32_t)(now-s->started_ms)>=s->limits.total_ms)return fail(s,TV4_NEG_ERROR_TIMEOUT);
    }
    return TV4_NEG_OK;
}
static bool same_id(tv4_update_id a,tv4_update_id b)
{
    return a.boot==b.boot && a.transfer==b.transfer;
}
static bool same(const tv4_neg_message *a,const tv4_neg_message *b)
{
    return same_id(a->request_id,b->request_id) && same_id(a->descriptor.id,b->descriptor.id) &&
        a->descriptor.image_bytes==b->descriptor.image_bytes && a->responder_boot==b->responder_boot &&
        a->block_bytes==b->block_bytes && !memcmp(a->descriptor.image_sha256,b->descriptor.image_sha256,32) &&
        !memcmp(a->metadata_sha256,b->metadata_sha256,32) && !memcmp(a->compatibility_tag,b->compatibility_tag,32);
}
static bool canonical(const tv4_neg_message *m)
{
    if(m->kind<TV4_NEG_REQUEST || m->kind>TV4_NEG_CONFIRM || m->request_id.boot==0 ||
        m->request_id.transfer==0 || m->responder_boot==0 || m->request_id.boot==m->responder_boot ||
        m->descriptor.image_bytes==0 || m->block_bytes==0 || m->block_bytes>TV4_UPDATE_BLOCK_MAX)return false;
    if(m->kind==TV4_NEG_REQUEST)return m->descriptor.id.boot==0 && m->descriptor.id.transfer==0;
    return m->descriptor.id.boot==m->responder_boot && m->descriptor.id.transfer!=0;
}
static bool reserve(tv4_neg_pool *pool,uint32_t *number)
{
    if(pool->last_issued==UINT32_MAX)return false;
    *number=++pool->last_issued;return true;
}
tv4_neg_result tv4_neg_begin(tv4_negotiation *s,const tv4_neg_image *image,uint64_t generation,uint32_t now)
{
    if(image==NULL)return TV4_NEG_REJECT_ARGUMENT;
    tv4_neg_result r=observe(s,generation,now);if(r!=TV4_NEG_OK)return r;
    if(s->role!=TV4_NEG_REQUESTER || s->state!=TV4_NEG_IDLE)return TV4_NEG_REJECT_STATE;
    if(image->image_bytes==0 || image->image_bytes>s->limits.max_image_bytes)return TV4_NEG_REJECT_CONTENT;
    if(s->pool->owner!=NULL)return TV4_NEG_BUSY;
    if(!reserve(s->pool,&s->request.request_id.transfer))return fail(s,TV4_NEG_ERROR_EXHAUSTED);
    s->pool->owner=s;s->request.kind=TV4_NEG_REQUEST;
    s->request.request_id.boot=s->local_boot;s->request.responder_boot=s->peer_boot;
    s->request.descriptor.image_bytes=image->image_bytes;
    memcpy(s->request.descriptor.image_sha256,image->image_sha256,32);
    memcpy(s->request.metadata_sha256,image->metadata_sha256,32);
    memcpy(s->request.compatibility_tag,s->compatibility_tag,32);
    s->request.block_bytes=s->limits.block_ceiling;
    s->started_ms=now;s->sends=0;s->pending=TV4_NEG_REQUEST;s->state=TV4_NEG_WAIT_OFFER;
    return TV4_NEG_OK;
}
static tv4_neg_result request(tv4_negotiation *s,const tv4_neg_message *m,uint32_t now)
{
    if(s->role!=TV4_NEG_RESPONDER)return TV4_NEG_REJECT_STATE;
    if(s->state!=TV4_NEG_IDLE && same_id(m->request_id,s->request.request_id)) {
        if(!same(m,&s->request))return fail(s,TV4_NEG_ERROR_CONFLICT);
        s->pending=TV4_NEG_OFFER;return TV4_NEG_DUPLICATE;
    }
    if(m->request_id.transfer<=s->pool->last_peer_request)return TV4_NEG_REJECT_SESSION;
    if(s->state!=TV4_NEG_IDLE || s->pool->owner!=NULL)return TV4_NEG_BUSY;
    if(m->descriptor.image_bytes>s->limits.max_image_bytes)return TV4_NEG_REJECT_CONTENT;
    uint32_t number;
    if(!reserve(s->pool,&number))return fail(s,TV4_NEG_ERROR_EXHAUSTED);
    s->pool->owner=s;s->pool->last_peer_request=m->request_id.transfer;
    s->request=*m;s->offer=*m;s->offer.kind=TV4_NEG_OFFER;
    s->offer.descriptor.id=(tv4_update_id){s->local_boot,number};
    if(s->offer.block_bytes>s->limits.block_ceiling)s->offer.block_bytes=s->limits.block_ceiling;
    s->started_ms=now;s->sends=0;s->pending=TV4_NEG_OFFER;s->state=TV4_NEG_WAIT_ACCEPT;
    return TV4_NEG_OK;
}
static bool offer_matches_request(const tv4_neg_message *m,const tv4_neg_message *r)
{
    return same_id(m->request_id,r->request_id) && m->responder_boot==r->responder_boot &&
        m->descriptor.image_bytes==r->descriptor.image_bytes && m->block_bytes<=r->block_bytes &&
        !memcmp(m->descriptor.image_sha256,r->descriptor.image_sha256,32) &&
        !memcmp(m->metadata_sha256,r->metadata_sha256,32) && !memcmp(m->compatibility_tag,r->compatibility_tag,32);
}
static tv4_neg_result offer(tv4_negotiation *s,const tv4_neg_message *m)
{
    if(s->role!=TV4_NEG_REQUESTER || s->state==TV4_NEG_IDLE)return TV4_NEG_REJECT_STATE;
    if(!same_id(m->request_id,s->request.request_id))return TV4_NEG_REJECT_SESSION;
    if(s->state==TV4_NEG_WAIT_OFFER) {
        if(s->sends==0)return TV4_NEG_REJECT_STATE;
        if(!offer_matches_request(m,&s->request))return TV4_NEG_REJECT_CONTENT;
        s->offer=*m;s->sends=0;s->pending=TV4_NEG_ACCEPT;s->state=TV4_NEG_WAIT_CONFIRM;
        return TV4_NEG_OK;
    }
    if(!same(m,&s->offer))return fail(s,TV4_NEG_ERROR_CONFLICT);
    if(s->state==TV4_NEG_WAIT_CONFIRM)s->pending=TV4_NEG_ACCEPT;
    return TV4_NEG_DUPLICATE;
}
static tv4_neg_result accept(tv4_negotiation *s,const tv4_neg_message *m)
{
    if(s->role!=TV4_NEG_RESPONDER || s->state==TV4_NEG_IDLE)return TV4_NEG_REJECT_STATE;
    if(!same_id(m->request_id,s->request.request_id))return TV4_NEG_REJECT_SESSION;
    if(!same(m,&s->offer))return fail(s,TV4_NEG_ERROR_CONFLICT);
    if(s->state==TV4_NEG_WAIT_ACCEPT) {
        if(s->sends==0)return TV4_NEG_REJECT_STATE;
        s->sends=0;s->pending=TV4_NEG_CONFIRM;s->state=TV4_NEG_READY;
        return TV4_NEG_OK;
    }
    s->pending=TV4_NEG_CONFIRM;return TV4_NEG_DUPLICATE;
}
static tv4_neg_result confirm(tv4_negotiation *s,const tv4_neg_message *m)
{
    if(s->role!=TV4_NEG_REQUESTER || s->state==TV4_NEG_IDLE || s->state==TV4_NEG_WAIT_OFFER)
        return TV4_NEG_REJECT_STATE;
    if(!same_id(m->request_id,s->request.request_id))return TV4_NEG_REJECT_SESSION;
    if(!same(m,&s->offer))return fail(s,TV4_NEG_ERROR_CONFLICT);
    if(s->state==TV4_NEG_WAIT_CONFIRM) {
        if(s->sends==0)return TV4_NEG_REJECT_STATE;
        s->pending=TV4_NEG_NO_MESSAGE;s->state=TV4_NEG_READY;return TV4_NEG_OK;
    }
    return TV4_NEG_DUPLICATE;
}
tv4_neg_result tv4_neg_receive(tv4_negotiation *s,const tv4_neg_message *m,uint64_t generation,uint32_t now)
{
    if(m==NULL)return TV4_NEG_REJECT_ARGUMENT;
    tv4_neg_result r=observe(s,generation,now);if(r!=TV4_NEG_OK)return r;
    if(!canonical(m))return TV4_NEG_REJECT_CONTENT;
    uint32_t requester=s->role==TV4_NEG_REQUESTER?s->local_boot:s->peer_boot;
    uint32_t responder=s->role==TV4_NEG_REQUESTER?s->peer_boot:s->local_boot;
    if(m->request_id.boot!=requester || m->responder_boot!=responder)return TV4_NEG_REJECT_SESSION;
    if(memcmp(m->compatibility_tag,s->compatibility_tag,32))return TV4_NEG_REJECT_CONTENT;
    switch(m->kind) {
    case TV4_NEG_REQUEST:return request(s,m,now);
    case TV4_NEG_OFFER:return offer(s,m);
    case TV4_NEG_ACCEPT:return accept(s,m);
    case TV4_NEG_CONFIRM:return confirm(s,m);
    default:return TV4_NEG_REJECT_CONTENT;
    }
}
tv4_neg_result tv4_neg_poll(tv4_negotiation *s,uint64_t generation,uint32_t now)
{
    return observe(s,generation,now);
}
tv4_neg_result tv4_neg_next(tv4_negotiation *s,tv4_neg_message *out,uint64_t generation,uint32_t now)
{
    if(out==NULL)return TV4_NEG_REJECT_ARGUMENT;
    tv4_neg_result r=observe(s,generation,now);if(r!=TV4_NEG_OK)return r;
    tv4_neg_kind kind=s->pending;
    if(kind==TV4_NEG_NO_MESSAGE) {
        if((uint32_t)(now-s->sent_ms)<s->limits.retry_ms)return TV4_NEG_WAIT;
        if(s->state==TV4_NEG_WAIT_OFFER)kind=TV4_NEG_REQUEST;
        else if(s->state==TV4_NEG_WAIT_ACCEPT)kind=TV4_NEG_OFFER;
        else if(s->state==TV4_NEG_WAIT_CONFIRM)kind=TV4_NEG_ACCEPT;
        else return TV4_NEG_WAIT;
    }
    if(s->sends>=s->limits.send_limit)return fail(s,TV4_NEG_ERROR_SENDS);
    if(kind==TV4_NEG_REQUEST)*out=s->request;
    else *out=s->offer;
    out->kind=kind;
    s->pending=TV4_NEG_NO_MESSAGE;++s->sends;s->sent_ms=now;
    return TV4_NEG_SENT;
}
tv4_neg_result tv4_neg_take(tv4_negotiation *s,tv4_negotiated *out,uint64_t generation,uint32_t now)
{
    if(out==NULL)return TV4_NEG_REJECT_ARGUMENT;
    tv4_neg_result r=observe(s,generation,now);if(r!=TV4_NEG_OK)return r;
    if(s->state!=TV4_NEG_READY)return TV4_NEG_REJECT_STATE;
    out->descriptor=s->offer.descriptor;memcpy(out->metadata_sha256,s->offer.metadata_sha256,32);
    out->block_bytes=s->offer.block_bytes;s->state=TV4_NEG_TAKEN;
    return TV4_NEG_OK;
}
tv4_neg_result tv4_neg_close(tv4_negotiation *s,uint64_t generation,uint32_t now)
{
    tv4_neg_result r=observe(s,generation,now);if(r!=TV4_NEG_OK)return r;
    if(s->state!=TV4_NEG_TAKEN)return TV4_NEG_REJECT_STATE;
    finish(s,TV4_NEG_CLOSED,TV4_NEG_ERROR_NONE);return TV4_NEG_OK;
}
tv4_neg_result tv4_neg_abort(tv4_negotiation *s,uint64_t generation,uint32_t now)
{
    tv4_neg_result r=observe(s,generation,now);if(r!=TV4_NEG_OK)return r;
    finish(s,TV4_NEG_ABORTED,TV4_NEG_ERROR_NONE);return TV4_NEG_OK;
}
bool tv4_neg_clear(tv4_negotiation *s)
{
    if(s==NULL || !s->configured || s->pool==NULL || !terminal(s))return false;
    if(s->pool->owner==s)s->pool->owner=NULL;
    s->pending=TV4_NEG_NO_MESSAGE;s->sends=0;s->started_ms=s->sent_ms=0;
    s->error=TV4_NEG_ERROR_NONE;s->state=TV4_NEG_IDLE;
    return true;
}
