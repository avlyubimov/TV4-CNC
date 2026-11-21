#include "frame_queue.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_frame_queue)<=192u,"Review custody ledger budget");
_Static_assert(sizeof(tv4_frame_delivery)<=40u,"Review driver snapshot budget");
static bool sticky(tv4_frame_queue_error e)
{return e==TV4_QUEUE_CONTEXT || e==TV4_QUEUE_CLOCK || e==TV4_QUEUE_EXHAUSTED;}
static tv4_frame_queue_result fail(tv4_frame_queue *q,tv4_frame_queue_error e)
{
    if(q->error==TV4_QUEUE_NO_ERROR || (!sticky(q->error) && sticky(e)))q->error=e;
    q->stopped=true;return TV4_QUEUE_ERROR;
}
static tv4_frame_queue_result observe(tv4_frame_queue *q,uint64_t generation,uint32_t now)
{
    if(q==NULL || !q->configured)return TV4_QUEUE_ARGUMENT;
    if(generation!=q->generation)return fail(q,TV4_QUEUE_CONTEXT);
    if(now-q->observed>(uint32_t)INT32_MAX)return fail(q,TV4_QUEUE_CLOCK);
    q->observed=now;
    if(q->stopped)return TV4_QUEUE_ERROR;
    if(q->count!=0 && now-q->entries[q->head].admitted>=q->limits.residency_ms)
        return fail(q,TV4_QUEUE_TIMEOUT);
    return TV4_QUEUE_OK;
}
bool tv4_frame_queue_init(tv4_frame_queue *q,tv4_frame_queue_limits limits,
    uint32_t route,uint64_t generation,uint32_t last,uint32_t now)
{
    if(q==NULL || q->configured || limits.capacity==0 || limits.capacity>TV4_FRAME_QUEUE_MAX ||
        limits.residency_ms==0 || limits.residency_ms>INT32_MAX || route==0 || generation==0)return false;
    memset(q,0,sizeof(*q));q->limits=limits;q->route=route;q->generation=generation;
    q->last_issued=last;q->observed=now;q->configured=true;return true;
}
tv4_frame_queue_result tv4_frame_queue_poll(tv4_frame_queue *q,uint64_t generation,uint32_t now)
{return observe(q,generation,now);}
tv4_frame_queue_result tv4_frame_queue_admit(tv4_frame_queue *q,const tv4_fragment_frame *frame,
    tv4_frame_ticket *ticket,uint64_t generation,uint32_t now)
{
    if(ticket!=NULL)memset(ticket,0,sizeof(*ticket));
    if(frame==NULL || ticket==NULL)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result r=observe(q,generation,now);if(r!=TV4_QUEUE_OK)return r;
    if(frame->length<=TV4_FRAGMENT_HEADER || frame->length>TV4_FRAGMENT_BYTES)return TV4_QUEUE_ARGUMENT;
    if(q->count==q->limits.capacity)return TV4_QUEUE_BUSY;
    if(q->last_issued==UINT32_MAX)return fail(q,TV4_QUEUE_EXHAUSTED);
    unsigned tail=((unsigned)q->head+q->count)%q->limits.capacity;
    tv4_frame_queue_entry *entry=&q->entries[tail];memset(entry,0,sizeof(*entry));
    entry->frame.length=frame->length;memcpy(entry->frame.bytes,frame->bytes,frame->length);
    entry->serial=++q->last_issued;entry->admitted=now;++q->count;
    *ticket=(tv4_frame_ticket){.generation=q->generation,.serial=entry->serial,.route=q->route,.owner=q};return TV4_QUEUE_ADMITTED;
}
tv4_frame_queue_result tv4_frame_queue_claim(tv4_frame_queue *q,tv4_frame_delivery *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(out==NULL)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result r=observe(q,generation,now);if(r!=TV4_QUEUE_OK)return r;
    if(q->flight!=0)return TV4_QUEUE_HELD;
    if(q->count==0)return TV4_QUEUE_WAIT;
    const tv4_frame_queue_entry *entry=&q->entries[q->head];q->flight=entry->serial;
    out->ticket=(tv4_frame_ticket){.generation=q->generation,.serial=entry->serial,.route=q->route,.owner=q};out->route=q->route;
    out->frame=entry->frame;return TV4_QUEUE_FRAME;
}
tv4_frame_queue_result tv4_frame_queue_complete(tv4_frame_queue *q,tv4_frame_ticket ticket,
    tv4_frame_driver_result result,uint32_t now)
{
    if(q==NULL || !q->configured)return TV4_QUEUE_ARGUMENT;
    if((unsigned)result<(unsigned)TV4_FRAME_SENT_LOCAL || (unsigned)result>(unsigned)TV4_FRAME_OWNERSHIP_UNCERTAIN)
        return TV4_QUEUE_ARGUMENT;
    /* Observe time even for stale callbacks, but never let it prevent reclaim
     * of the exact original terminal claim after a sticky context/clock fault. */
    (void)observe(q,q->generation,now);
    if(ticket.owner!=q || ticket.route!=q->route || ticket.generation!=q->generation || ticket.serial==0 || ticket.serial!=q->flight ||
        q->count==0 || q->entries[q->head].serial!=ticket.serial)return TV4_QUEUE_STALE;
    if(result==TV4_FRAME_OWNERSHIP_UNCERTAIN){(void)fail(q,TV4_QUEUE_UNCERTAIN);return TV4_QUEUE_HELD;}
    if(result!=TV4_FRAME_SENT_LOCAL)(void)fail(q,result==TV4_FRAME_FAILED_LOCAL?TV4_QUEUE_DRIVER:TV4_QUEUE_CANCELLED);
    memset(&q->entries[q->head],0,sizeof(q->entries[q->head]));
    q->head=(uint8_t)(((unsigned)q->head+1u)%q->limits.capacity);--q->count;q->flight=0;
    return q->stopped?TV4_QUEUE_RECLAIMED_FAILED:TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED;
}
tv4_frame_queue_result tv4_frame_queue_cancel(tv4_frame_queue *q,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=observe(q,generation,now);if(r==TV4_QUEUE_ARGUMENT)return r;
    if(!q->stopped)(void)fail(q,TV4_QUEUE_CANCELLED);
    unsigned keep=q->flight!=0?1u:0u;
    for(unsigned i=keep;i<q->count;++i) {
        unsigned at=((unsigned)q->head+i)%q->limits.capacity;memset(&q->entries[at],0,sizeof(q->entries[at]));
    }
    q->count=(uint8_t)keep;
    return keep!=0?TV4_QUEUE_HELD:TV4_QUEUE_OK;
}
tv4_frame_queue_result tv4_frame_queue_clear(tv4_frame_queue *q,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=observe(q,generation,now);if(r==TV4_QUEUE_ARGUMENT)return r;
    if(sticky(q->error))return TV4_QUEUE_ERROR;
    if(!q->stopped)return TV4_QUEUE_STATE;
    if(q->count!=0 || q->flight!=0)return TV4_QUEUE_HELD;
    q->head=0;q->error=TV4_QUEUE_NO_ERROR;q->stopped=false;return TV4_QUEUE_OK;
}
tv4_frame_queue_result tv4_frame_queue_quiescent(tv4_frame_queue *q,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=observe(q,generation,now);if(r!=TV4_QUEUE_OK)return r;
    return q->count==0 && q->flight==0?TV4_QUEUE_OK:TV4_QUEUE_BUSY;
}
