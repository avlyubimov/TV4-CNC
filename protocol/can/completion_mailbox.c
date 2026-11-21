#include "completion_mailbox.h"
#include <string.h>

_Static_assert(ATOMIC_INT_LOCK_FREE==2 && sizeof(unsigned)==4u,"Native lock-free 32-bit atomic unsigned required");
_Static_assert(sizeof(tv4_completion_mailbox)<=256u,"Review atomic mailbox storage");
_Static_assert(sizeof(tv4_completion_owner)<=1536u,"Review bound completion owner storage");
bool tv4_completion_mailbox_init(tv4_completion_mailbox *m,unsigned capacity,const tv4_frame_queue *q)
{
    if(m==NULL || m->configured || capacity==0 || capacity>TV4_COMPLETION_MAILBOX_MAX || q==NULL || !q->configured)return false;
    memset(m->entries,0,sizeof(m->entries));atomic_init(&m->head,0u);atomic_init(&m->tail,0u);
    m->queue=q;m->generation=q->generation;m->route=q->route;m->slots=capacity+1u;m->configured=true;return true;
}
tv4_frame_queue_result tv4_completion_mailbox_post(tv4_completion_mailbox *m,const tv4_driver_event *event)
{
    if(m==NULL || !m->configured || event==NULL || (unsigned)event->result<(unsigned)TV4_FRAME_SENT_LOCAL ||
        (unsigned)event->result>(unsigned)TV4_FRAME_OWNERSHIP_UNCERTAIN)return TV4_QUEUE_ARGUMENT;
    if(event->ticket.owner!=m->queue || event->ticket.generation!=m->generation || event->ticket.route!=m->route || event->ticket.serial==0)return TV4_QUEUE_STALE;
    unsigned tail=atomic_load_explicit(&m->tail,memory_order_relaxed);
    unsigned next=(tail+1u)%m->slots;
    /* Acquire pairs with consumer release AFTER its copy and wipe: producer
     * cannot reuse a slot while consumer still reads/wipes its old contents. */
    if(next==atomic_load_explicit(&m->head,memory_order_acquire))return TV4_QUEUE_BUSY;
    m->entries[tail]=*event;
    /* Publish complete non-atomic slot to the consumer's acquire load. */
    atomic_store_explicit(&m->tail,next,memory_order_release);return TV4_QUEUE_ADMITTED;
}
tv4_frame_queue_result tv4_completion_mailbox_take(tv4_completion_mailbox *m,tv4_driver_event *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(m==NULL || !m->configured || out==NULL)return TV4_QUEUE_ARGUMENT;
    unsigned head=atomic_load_explicit(&m->head,memory_order_relaxed);
    if(head==atomic_load_explicit(&m->tail,memory_order_acquire))return TV4_QUEUE_WAIT;
    *out=m->entries[head];memset(&m->entries[head],0,sizeof(m->entries[head]));
    atomic_store_explicit(&m->head,(head+1u)%m->slots,memory_order_release);return TV4_QUEUE_FRAME;
}
tv4_frame_queue_result tv4_completion_mailbox_empty(const tv4_completion_mailbox *m)
{
    if(m==NULL || !m->configured)return TV4_QUEUE_ARGUMENT;
    unsigned head=atomic_load_explicit(&m->head,memory_order_relaxed);
    return head==atomic_load_explicit(&m->tail,memory_order_acquire)?TV4_QUEUE_OK:TV4_QUEUE_BUSY;
}
bool tv4_completion_owner_init(tv4_completion_owner *s,const tv4_fragment_config *f,
    tv4_frame_queue_limits custody,unsigned capacity,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now)
{
    if(s==NULL || s->configured || s->mailbox.configured || capacity==0 || capacity>TV4_COMPLETION_MAILBOX_MAX)return false;
    if(!tv4_driver_events_init(&s->events,f,custody,1,route,generation,last_fragment,last_ticket,now))return false;
    if(!tv4_completion_mailbox_init(&s->mailbox,capacity,&s->events.egress.queue))return false;
    s->configured=true;return true;
}
tv4_frame_queue_result tv4_completion_owner_post(tv4_completion_owner *s,const tv4_driver_event *event)
{return s==NULL || !s->configured?TV4_QUEUE_ARGUMENT:tv4_completion_mailbox_post(&s->mailbox,event);}
tv4_frame_queue_result tv4_completion_owner_poll(tv4_completion_owner *s,uint64_t generation,uint32_t now)
{return s==NULL || !s->configured?TV4_QUEUE_ARGUMENT:tv4_driver_events_poll(&s->events,generation,now);}
tv4_frame_queue_result tv4_completion_owner_service(tv4_completion_owner *s,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result polled=tv4_completion_owner_poll(s,generation,now);if(polled==TV4_QUEUE_ARGUMENT)return polled;
    if(s->events.count!=0)return tv4_driver_events_service(&s->events,generation,now);
    if(!s->has_pending) {
        tv4_frame_queue_result got=tv4_completion_mailbox_take(&s->mailbox,&s->pending);
        if(got!=TV4_QUEUE_FRAME)return got==TV4_QUEUE_WAIT && polled!=TV4_QUEUE_OK?polled:got;
        s->has_pending=true;
    }
    tv4_frame_queue_result posted=tv4_driver_events_post(&s->events,&s->pending);
    if(posted!=TV4_QUEUE_ADMITTED)return posted; /* retain actual owned pending */
    memset(&s->pending,0,sizeof(s->pending));s->has_pending=false;
    return tv4_driver_events_service(&s->events,generation,now);
}
static bool waiting(const tv4_completion_owner *s)
{return s->has_pending || tv4_completion_mailbox_empty(&s->mailbox)!=TV4_QUEUE_OK;}
tv4_channel_result tv4_completion_owner_begin(tv4_completion_owner *s,const tv4_channel_message *message,uint64_t generation,uint32_t now)
{
    if(message==NULL)return TV4_CHANNEL_ARGUMENT;
    tv4_frame_queue_result r=tv4_completion_owner_poll(s,generation,now);
    if(r!=TV4_QUEUE_OK)return r==TV4_QUEUE_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    if(waiting(s))return TV4_CHANNEL_BUSY;
    return tv4_driver_events_begin(&s->events,message,generation,now);
}
tv4_frame_queue_result tv4_completion_owner_pump(tv4_completion_owner *s,uint64_t generation,uint32_t now)
{return s==NULL || !s->configured?TV4_QUEUE_ARGUMENT:tv4_driver_events_pump(&s->events,generation,now);}
tv4_frame_queue_result tv4_completion_owner_claim(tv4_completion_owner *s,tv4_frame_delivery *out,uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(out==NULL)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result r=tv4_completion_owner_poll(s,generation,now);if(r!=TV4_QUEUE_OK)return r;
    if(waiting(s))return TV4_QUEUE_BUSY;
    return tv4_driver_events_claim(&s->events,out,generation,now);
}
tv4_channel_result tv4_completion_owner_advance(tv4_completion_owner *s,tv4_channel_phase phase,tv4_update_id id,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_completion_owner_poll(s,generation,now);
    if(r!=TV4_QUEUE_OK)return r==TV4_QUEUE_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    if(waiting(s))return TV4_CHANNEL_BUSY;
    return tv4_driver_events_advance(&s->events,phase,id,generation,now);
}
tv4_frame_queue_result tv4_completion_owner_cancel(tv4_completion_owner *s,uint64_t generation,uint32_t now)
{return s==NULL || !s->configured?TV4_QUEUE_ARGUMENT:tv4_driver_events_cancel(&s->events,generation,now);}
tv4_frame_queue_result tv4_completion_owner_clear(tv4_completion_owner *s,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_completion_owner_poll(s,generation,now);if(r==TV4_QUEUE_ARGUMENT)return r;
    if(waiting(s))return TV4_QUEUE_HELD;
    return tv4_driver_events_clear(&s->events,generation,now);
}
