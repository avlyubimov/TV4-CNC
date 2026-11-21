#include "driver_dispatch.h"
#include <limits.h>
#include <string.h>

_Static_assert(ATOMIC_INT_LOCK_FREE==2 && sizeof(unsigned)==4u,"Native lock-free uint32 atomics required");
_Static_assert(sizeof(tv4_driver_dispatch)<=1792u,"Review bidirectional dispatch storage");
static bool same_ticket(tv4_frame_ticket a,tv4_frame_ticket b)
{return a.owner==b.owner && a.route==b.route && a.generation==b.generation && a.serial==b.serial;}
/* Protocol actor only. Driver-private fields are deliberately not read here. */
static void reconcile(tv4_driver_dispatch *s)
{
    if(s->completion.events.egress.failed)atomic_store_explicit(&s->revoked,1u,memory_order_release);
    if(atomic_load_explicit(&s->state,memory_order_acquire)==TV4_DISPATCH_RETURNED &&
        s->completion.events.egress.queue.flight==0 && s->completion.events.count==0 &&
        !s->completion.has_pending && tv4_completion_mailbox_empty(&s->completion.mailbox)==TV4_QUEUE_OK) {
        memset(&s->published,0,sizeof(s->published));
        atomic_store_explicit(&s->state,TV4_DISPATCH_EMPTY,memory_order_release);
    }
}
bool tv4_driver_dispatch_init(tv4_driver_dispatch *s,const tv4_fragment_config *f,
    tv4_frame_queue_limits custody,unsigned capacity,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now)
{
    if(s==NULL || s->configured)return false;
    if(!tv4_completion_owner_init(&s->completion,f,custody,capacity,route,generation,last_fragment,last_ticket,now))return false;
    atomic_init(&s->state,TV4_DISPATCH_EMPTY);atomic_init(&s->revoked,0u);
    s->driver_observed=now;s->configured=true;return true;
}
tv4_frame_queue_result tv4_driver_dispatch_poll(tv4_driver_dispatch *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result r=tv4_completion_owner_poll(&s->completion,generation,now);reconcile(s);return r;
}
tv4_frame_queue_result tv4_driver_dispatch_service(tv4_driver_dispatch *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result r=tv4_completion_owner_service(&s->completion,generation,now);reconcile(s);return r;
}
tv4_channel_result tv4_driver_dispatch_begin(tv4_driver_dispatch *s,const tv4_channel_message *message,uint64_t generation,uint32_t now)
{
    if(message==NULL)return TV4_CHANNEL_ARGUMENT;
    tv4_frame_queue_result r=tv4_driver_dispatch_poll(s,generation,now);
    if(r!=TV4_QUEUE_OK)return r==TV4_QUEUE_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    if(atomic_load_explicit(&s->state,memory_order_acquire)!=TV4_DISPATCH_EMPTY)return TV4_CHANNEL_BUSY;
    tv4_channel_result result=tv4_completion_owner_begin(&s->completion,message,generation,now);
    if(result==TV4_CHANNEL_ERROR)(void)tv4_driver_dispatch_poll(s,generation,now);
    return result;
}
tv4_frame_queue_result tv4_driver_dispatch_pump(tv4_driver_dispatch *s,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_driver_dispatch_poll(s,generation,now);if(r!=TV4_QUEUE_OK)return r;
    r=tv4_completion_owner_pump(&s->completion,generation,now);reconcile(s);return r;
}
tv4_frame_queue_result tv4_driver_dispatch_publish(tv4_driver_dispatch *s,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_driver_dispatch_poll(s,generation,now);if(r!=TV4_QUEUE_OK)return r;
    if(atomic_load_explicit(&s->state,memory_order_acquire)!=TV4_DISPATCH_EMPTY)return TV4_QUEUE_BUSY;
    tv4_frame_delivery delivery;r=tv4_completion_owner_claim(&s->completion,&delivery,generation,now);
    if(r!=TV4_QUEUE_FRAME)return r;
    const tv4_frame_queue *q=&s->completion.events.egress.queue;
    s->published=(tv4_dispatch_delivery){delivery,q->entries[q->head].admitted,q->limits.residency_ms};
    atomic_store_explicit(&s->state,TV4_DISPATCH_READY,memory_order_release);return TV4_QUEUE_ADMITTED;
}
tv4_channel_result tv4_driver_dispatch_advance(tv4_driver_dispatch *s,tv4_channel_phase phase,tv4_update_id id,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_driver_dispatch_poll(s,generation,now);
    if(r!=TV4_QUEUE_OK)return r==TV4_QUEUE_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    if(atomic_load_explicit(&s->state,memory_order_acquire)!=TV4_DISPATCH_EMPTY)return TV4_CHANNEL_BUSY;
    return tv4_completion_owner_advance(&s->completion,phase,id,generation,now);
}
tv4_frame_queue_result tv4_driver_dispatch_cancel(tv4_driver_dispatch *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    atomic_store_explicit(&s->revoked,1u,memory_order_release);
    tv4_frame_queue_result r=tv4_completion_owner_cancel(&s->completion,generation,now);reconcile(s);return r;
}
tv4_frame_queue_result tv4_driver_dispatch_clear(tv4_driver_dispatch *s,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_driver_dispatch_poll(s,generation,now);if(r==TV4_QUEUE_ARGUMENT)return r;
    if(atomic_load_explicit(&s->state,memory_order_acquire)!=TV4_DISPATCH_EMPTY)return TV4_QUEUE_HELD;
    r=tv4_completion_owner_clear(&s->completion,generation,now);
    if(r==TV4_QUEUE_OK)atomic_store_explicit(&s->revoked,0u,memory_order_release);
    return r;
}
tv4_dispatch_status tv4_driver_dispatch_check(tv4_driver_dispatch *s,tv4_frame_ticket ticket,tv4_dispatch_delivery *out,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(s==NULL || !s->configured || out==NULL)return TV4_DISPATCH_ARGUMENT;
    if(!s->driver_has_current || !same_ticket(ticket,s->driver_current.delivery.ticket))return TV4_DISPATCH_STALE;
    if(s->driver_clock_failed || now-s->driver_observed>(uint32_t)INT32_MAX) {
        s->driver_clock_failed=true;return TV4_DISPATCH_CLOCK;
    }
    s->driver_observed=now;
    if(atomic_load_explicit(&s->revoked,memory_order_acquire)!=0)return TV4_DISPATCH_REVOKED;
    uint32_t age=now-s->driver_current.admitted;
    if(age>(uint32_t)INT32_MAX){s->driver_clock_failed=true;return TV4_DISPATCH_CLOCK;}
    if(age>=s->driver_current.residency_ms)return TV4_DISPATCH_EXPIRED;
    s->driver_exposed=true;*out=s->driver_current;return TV4_DISPATCH_SENDABLE;
}
tv4_dispatch_status tv4_driver_dispatch_take(tv4_driver_dispatch *s,tv4_dispatch_delivery *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(s==NULL || !s->configured || out==NULL)return TV4_DISPATCH_ARGUMENT;
    if(s->driver_has_current)return TV4_DISPATCH_HELD;
    if(atomic_load_explicit(&s->state,memory_order_acquire)!=TV4_DISPATCH_READY)return TV4_DISPATCH_WAIT;
    s->driver_current=s->published;s->driver_has_current=true;
    atomic_store_explicit(&s->state,TV4_DISPATCH_TAKEN,memory_order_release);
    s->driver_exposed=false;*out=s->driver_current;
    memset(&out->delivery.frame,0,sizeof(out->delivery.frame));return TV4_DISPATCH_UNCHECKED;
}
/* Driver actor only. RETURNED is published AFTER all driver-private writes;
 * protocol may then reuse the shared slot, once actual ledger cleanup is done. */
tv4_frame_queue_result tv4_driver_dispatch_retry(tv4_driver_dispatch *s)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    if(!s->driver_has_pending)return TV4_QUEUE_WAIT;
    tv4_frame_queue_result r=tv4_completion_owner_post(&s->completion,&s->driver_pending);
    if(r!=TV4_QUEUE_ADMITTED)return r;
    bool terminal=s->driver_pending.result!=TV4_FRAME_OWNERSHIP_UNCERTAIN;
    memset(&s->driver_pending,0,sizeof(s->driver_pending));s->driver_has_pending=false;
    if(terminal) {
        memset(&s->driver_current,0,sizeof(s->driver_current));s->driver_has_current=false;s->driver_exposed=false;
        atomic_store_explicit(&s->state,TV4_DISPATCH_RETURNED,memory_order_release);
    }
    return r;
}
tv4_frame_queue_result tv4_driver_dispatch_report(tv4_driver_dispatch *s,const tv4_driver_event *event)
{
    if(s==NULL || !s->configured || event==NULL || (unsigned)event->result<(unsigned)TV4_FRAME_SENT_LOCAL ||
        (unsigned)event->result>(unsigned)TV4_FRAME_OWNERSHIP_UNCERTAIN)return TV4_QUEUE_ARGUMENT;
    if(!s->driver_has_current || !same_ticket(event->ticket,s->driver_current.delivery.ticket))return TV4_QUEUE_STALE;
    if(s->driver_has_pending)return TV4_QUEUE_BUSY;
    if(!s->driver_exposed && event->result==TV4_FRAME_SENT_LOCAL)return TV4_QUEUE_ARGUMENT;
    s->driver_pending=*event;s->driver_has_pending=true;return tv4_driver_dispatch_retry(s);
}
