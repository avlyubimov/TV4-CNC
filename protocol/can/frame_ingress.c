#include "frame_ingress.h"
#include <limits.h>
#include <string.h>
_Static_assert(UINT_MAX==UINT32_MAX && ATOMIC_INT_LOCK_FREE==2,
    "ingress requires always-native 32-bit unsigned atomics");
static bool valid(const tv4_frame_ingress *m){return m!=NULL && m->configured;}
static unsigned next(const tv4_frame_ingress *m,unsigned p){return p+1u==m->slots?0u:p+1u;}
static void fault(tv4_frame_ingress *m,unsigned reason)
{(void)atomic_fetch_or_explicit(&m->fault,reason,memory_order_acq_rel);}
static bool observe(tv4_frame_ingress *m,uint32_t *last,uint32_t now)
{
    if((uint32_t)(now-*last)>=UINT32_C(0x80000000)){fault(m,TV4_INGRESS_CLOCK);return false;}
    *last=now;return atomic_load_explicit(&m->fault,memory_order_acquire)==0;
}
static bool push(tv4_frame_ingress *m,const tv4_ingress_entry *entry)
{
    unsigned head=atomic_load_explicit(&m->head,memory_order_relaxed),n=next(m,head);
    if(n==atomic_load_explicit(&m->tail,memory_order_acquire))return false;
    m->entries[head]=*entry;atomic_store_explicit(&m->head,n,memory_order_release);return true;
}
bool tv4_frame_ingress_init(tv4_frame_ingress *m,unsigned capacity,uint32_t residency_ms,
    uint32_t route,uint64_t generation,uint32_t last_serial,uint32_t now)
{
    if(m==NULL || m->configured || capacity==0 || capacity>TV4_FRAME_INGRESS_MAX ||
        residency_ms==0 || residency_ms>=UINT32_C(0x80000000) || route==0 || generation==0)return false;
    memset(m,0,sizeof(*m));atomic_init(&m->head,0u);atomic_init(&m->tail,0u);
    atomic_init(&m->held,0u);atomic_init(&m->fault,0u);m->slots=capacity+1u;
    m->route=route;m->generation=generation;m->residency_ms=residency_ms;
    m->issued=last_serial;m->consumed=last_serial;
    m->producer_observed=now;m->consumer_observed=now;m->configured=true;return true;
}
tv4_ingress_result tv4_frame_ingress_offer(tv4_frame_ingress *m,uint32_t route,
    uint64_t generation,const tv4_fragment_frame *frame,uint32_t captured,uint32_t now)
{
    if(!valid(m) || frame==NULL || frame->length==0 || frame->length>TV4_FRAGMENT_BYTES)return TV4_INGRESS_ARGUMENT;
    if(route!=m->route || generation!=m->generation)return TV4_INGRESS_IDENTITY;
    if(!observe(m,&m->producer_observed,now))return TV4_INGRESS_FAULT;
    uint32_t age=now-captured;
    if(age>=UINT32_C(0x80000000)){fault(m,TV4_INGRESS_CLOCK);return TV4_INGRESS_FAULT;}
    if(age>=m->residency_ms){fault(m,TV4_INGRESS_EXPIRED);return TV4_INGRESS_FAULT;}
    if(m->producer_has_pending)return TV4_INGRESS_BUSY;
    if(m->issued==UINT32_MAX){fault(m,TV4_INGRESS_EXHAUSTED);return TV4_INGRESS_FAULT;}
    tv4_ingress_entry e={.captured=captured,.serial=++m->issued};
    e.frame.length=frame->length;memcpy(e.frame.bytes,frame->bytes,frame->length);
    if(push(m,&e))return TV4_INGRESS_ADMITTED;
    m->producer_pending=e;m->producer_has_pending=true;
    atomic_store_explicit(&m->held,1u,memory_order_release);return TV4_INGRESS_HELD;
}
tv4_ingress_result tv4_frame_ingress_retry(tv4_frame_ingress *m,uint32_t now)
{
    if(!valid(m))return TV4_INGRESS_ARGUMENT;
    if(!observe(m,&m->producer_observed,now))return TV4_INGRESS_FAULT;
    if(!m->producer_has_pending)return TV4_INGRESS_WAIT;
    if(!push(m,&m->producer_pending))return TV4_INGRESS_HELD;
    memset(&m->producer_pending,0,sizeof(m->producer_pending));m->producer_has_pending=false;
    atomic_store_explicit(&m->held,0u,memory_order_release);return TV4_INGRESS_ADMITTED;
}
tv4_ingress_result tv4_frame_ingress_lost(tv4_frame_ingress *m)
{if(!valid(m))return TV4_INGRESS_ARGUMENT;fault(m,TV4_INGRESS_OVERRUN);return TV4_INGRESS_FAULT;}
tv4_ingress_result tv4_frame_ingress_stage(tv4_frame_ingress *m,tv4_ingress_token *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(!valid(m) || out==NULL)return TV4_INGRESS_ARGUMENT;
    if(atomic_load_explicit(&m->fault,memory_order_acquire)!=0)return TV4_INGRESS_FAULT;
    if(m->consumer_has_staged)return TV4_INGRESS_BUSY;
    unsigned tail=atomic_load_explicit(&m->tail,memory_order_relaxed);
    if(tail==atomic_load_explicit(&m->head,memory_order_acquire))return TV4_INGRESS_WAIT;
    m->consumer_staged=m->entries[tail];memset(&m->entries[tail],0,sizeof(m->entries[tail]));
    atomic_store_explicit(&m->tail,next(m,tail),memory_order_release);m->consumer_has_staged=true;
    *out=(tv4_ingress_token){.owner=m,.generation=m->generation,.route=m->route,
        .serial=m->consumer_staged.serial,.captured=m->consumer_staged.captured};return TV4_INGRESS_UNCHECKED;
}
tv4_ingress_result tv4_frame_ingress_check(tv4_frame_ingress *m,
    tv4_ingress_token token,tv4_fragment_frame *out,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(!valid(m) || out==NULL)return TV4_INGRESS_ARGUMENT;
    if(!m->consumer_has_staged || token.owner!=m || token.route!=m->route || token.generation!=m->generation ||
        token.serial!=m->consumer_staged.serial || token.captured!=m->consumer_staged.captured)return TV4_INGRESS_STALE;
    bool ok=observe(m,&m->consumer_observed,now);
    uint32_t age=now-m->consumer_staged.captured;
    if(age>=UINT32_C(0x80000000)){fault(m,TV4_INGRESS_CLOCK);ok=false;}
    else if(age>=m->residency_ms){fault(m,TV4_INGRESS_EXPIRED);ok=false;}
    if(m->consumed==UINT32_MAX || m->consumer_staged.serial!=m->consumed+1u){fault(m,TV4_INGRESS_EXHAUSTED);ok=false;}
    m->consumed=m->consumer_staged.serial;
    if(atomic_load_explicit(&m->fault,memory_order_acquire)!=0)ok=false;
    if(ok)*out=m->consumer_staged.frame;
    memset(&m->consumer_staged,0,sizeof(m->consumer_staged));m->consumer_has_staged=false;
    return ok?TV4_INGRESS_FRAME:TV4_INGRESS_FAULT;
}
tv4_ingress_result tv4_frame_ingress_empty(const tv4_frame_ingress *m)
{
    if(!valid(m))return TV4_INGRESS_ARGUMENT;
    if(atomic_load_explicit(&m->fault,memory_order_acquire)!=0)return TV4_INGRESS_FAULT;
    if(m->consumer_has_staged || atomic_load_explicit(&m->held,memory_order_acquire)!=0)return TV4_INGRESS_BUSY;
    unsigned head=atomic_load_explicit(&m->head,memory_order_acquire);
    return head==atomic_load_explicit(&m->tail,memory_order_relaxed)?TV4_INGRESS_WAIT:TV4_INGRESS_BUSY;
}
unsigned tv4_frame_ingress_faults(const tv4_frame_ingress *m)
{return valid(m)?atomic_load_explicit(&m->fault,memory_order_acquire):UINT_MAX;}
