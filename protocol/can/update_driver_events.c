#include "update_driver_events.h"
#include <string.h>

_Static_assert(sizeof(tv4_driver_events)<=1216u,"Review bound driver event owner budget");
bool tv4_driver_events_init(tv4_driver_events *s,const tv4_fragment_config *fragments,
    tv4_frame_queue_limits custody,uint8_t capacity,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now)
{
    if(s==NULL || s->configured || capacity==0 || capacity>TV4_DRIVER_EVENTS_MAX)return false;
    if(!tv4_update_egress_init(&s->egress,fragments,custody,route,generation,last_fragment,last_ticket,now))return false;
    s->capacity=capacity;s->configured=true;return true;
}
tv4_channel_result tv4_driver_events_begin(tv4_driver_events *s,const tv4_channel_message *message,
    uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured || message==NULL)return TV4_CHANNEL_ARGUMENT;
    if(tv4_update_egress_poll(&s->egress,generation,now)!=TV4_QUEUE_OK)return TV4_CHANNEL_ERROR;
    if(s->count!=0)return TV4_CHANNEL_BUSY;
    return tv4_update_egress_begin(&s->egress,message,generation,now);
}
tv4_frame_queue_result tv4_driver_events_pump(tv4_driver_events *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    return tv4_update_egress_pump(&s->egress,generation,now);
}
tv4_frame_queue_result tv4_driver_events_claim(tv4_driver_events *s,tv4_frame_delivery *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(s==NULL || !s->configured || out==NULL)return TV4_QUEUE_ARGUMENT;
    if(tv4_update_egress_poll(&s->egress,generation,now)!=TV4_QUEUE_OK)return TV4_QUEUE_ERROR;
    if(s->count!=0)return TV4_QUEUE_BUSY;
    return tv4_update_egress_claim(&s->egress,out,generation,now);
}
tv4_frame_queue_result tv4_driver_events_post(tv4_driver_events *s,const tv4_driver_event *event)
{
    if(s==NULL || !s->configured || event==NULL || (unsigned)event->result<(unsigned)TV4_FRAME_SENT_LOCAL ||
        (unsigned)event->result>(unsigned)TV4_FRAME_OWNERSHIP_UNCERTAIN)return TV4_QUEUE_ARGUMENT;
    const tv4_frame_queue *q=&s->egress.queue;
    if(event->ticket.owner!=q || event->ticket.route!=q->route || event->ticket.generation!=q->generation ||
        event->ticket.serial==0)return TV4_QUEUE_STALE;
    if(s->count==s->capacity)return TV4_QUEUE_BUSY;
    unsigned tail=((unsigned)s->head+s->count)%s->capacity;
    s->entries[tail]=*event;++s->count;return TV4_QUEUE_ADMITTED;
}
tv4_frame_queue_result tv4_driver_events_poll(tv4_driver_events *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    return tv4_update_egress_poll(&s->egress,generation,now);
}
tv4_frame_queue_result tv4_driver_events_service(tv4_driver_events *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result polled=tv4_update_egress_poll(&s->egress,generation,now);
    if(s->count==0)return polled==TV4_QUEUE_OK?TV4_QUEUE_WAIT:polled;
    const tv4_driver_event *event=&s->entries[s->head];
    tv4_frame_queue_result result=tv4_update_egress_complete(&s->egress,event->ticket,event->result,now);
    memset(&s->entries[s->head],0,sizeof(s->entries[s->head]));
    s->head=(uint8_t)(((unsigned)s->head+1u)%s->capacity);--s->count;return result;
}
tv4_channel_result tv4_driver_events_advance(tv4_driver_events *s,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_CHANNEL_ARGUMENT;
    if(tv4_update_egress_poll(&s->egress,generation,now)!=TV4_QUEUE_OK)return TV4_CHANNEL_ERROR;
    if(s->count!=0)return TV4_CHANNEL_BUSY;
    return tv4_update_egress_advance(&s->egress,phase,id,generation,now);
}
tv4_frame_queue_result tv4_driver_events_cancel(tv4_driver_events *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    return tv4_update_egress_cancel(&s->egress,generation,now);
}
tv4_frame_queue_result tv4_driver_events_clear(tv4_driver_events *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    (void)tv4_update_egress_poll(&s->egress,generation,now);
    if(s->count!=0)return TV4_QUEUE_HELD;
    return tv4_update_egress_clear(&s->egress,generation,now);
}
