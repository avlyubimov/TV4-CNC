#include "update_egress.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_update_egress)<=1024u,"Review egress owner budget");
bool tv4_update_egress_init(tv4_update_egress *s,const tv4_fragment_config *f,
    tv4_frame_queue_limits q,uint32_t route,uint64_t generation,uint16_t last_fragment,uint32_t last_ticket,uint32_t now)
{
    if(s==NULL || s->configured || s->channel.fragments.storage.configured || s->queue.configured || f==NULL ||
        f->version!=TV4_FRAGMENT_VERSION || f->progress_timeout_ms==0 ||
        f->progress_timeout_ms>f->total_timeout_ms || f->total_timeout_ms>INT32_MAX || f->max_frames==0 ||
        q.capacity==0 || q.capacity>TV4_FRAME_QUEUE_MAX || q.residency_ms==0 || q.residency_ms>INT32_MAX ||
        route==0 || generation==0)return false;
    if(!tv4_frame_queue_init(&s->queue,q,route,generation,last_ticket,now) ||
        !tv4_channel_tx_init(&s->channel,f,route,generation,last_fragment,now))return false;
    s->failed=false;s->configured=true;return true;
}
tv4_frame_queue_result tv4_update_egress_poll(tv4_update_egress *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result q=tv4_frame_queue_poll(&s->queue,generation,now);
    tv4_fragment_result c=tv4_channel_tx_poll(&s->channel,generation,now);
    if(q!=TV4_QUEUE_OK || c!=TV4_FRAG_OK)s->failed=true;
    return s->failed?TV4_QUEUE_ERROR:TV4_QUEUE_OK;
}
tv4_channel_result tv4_update_egress_begin(tv4_update_egress *s,const tv4_channel_message *message,
    uint64_t generation,uint32_t now)
{
    if(s==NULL || message==NULL || !s->configured)return TV4_CHANNEL_ARGUMENT;
    if(tv4_update_egress_poll(s,generation,now)!=TV4_QUEUE_OK)return TV4_CHANNEL_ERROR;
    if(tv4_frame_queue_quiescent(&s->queue,generation,now)!=TV4_QUEUE_OK)return TV4_CHANNEL_BUSY;
    return tv4_channel_tx_begin(&s->channel,message,generation,now);
}
tv4_frame_queue_result tv4_update_egress_pump(tv4_update_egress *s,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_update_egress_poll(s,generation,now);if(r!=TV4_QUEUE_OK)return r;
    if(!s->channel.active)return TV4_QUEUE_WAIT;
    if(s->queue.count==s->queue.limits.capacity)return TV4_QUEUE_BUSY;
    tv4_fragment_frame frame;
    if(tv4_channel_tx_peek(&s->channel,&frame,generation,now)!=TV4_FRAG_FRAME){s->failed=true;return TV4_QUEUE_ERROR;}
    tv4_frame_ticket ticket;
    r=tv4_frame_queue_admit(&s->queue,&frame,&ticket,generation,now);
    if(r!=TV4_QUEUE_ADMITTED){if(r==TV4_QUEUE_ERROR)s->failed=true;return r;}
    tv4_fragment_result c=tv4_channel_tx_commit(&s->channel,&frame,generation,now);
    if(c!=TV4_FRAG_COMMITTED && c!=TV4_FRAG_FINISHED) {
        s->failed=true;(void)tv4_frame_queue_cancel(&s->queue,generation,now);return TV4_QUEUE_ERROR;
    }
    return TV4_QUEUE_ADMITTED;
}
tv4_frame_queue_result tv4_update_egress_claim(tv4_update_egress *s,tv4_frame_delivery *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(out==NULL)return TV4_QUEUE_ARGUMENT;
    tv4_frame_queue_result r=tv4_update_egress_poll(s,generation,now);if(r!=TV4_QUEUE_OK)return r;
    return tv4_frame_queue_claim(&s->queue,out,generation,now);
}
tv4_frame_queue_result tv4_update_egress_complete(tv4_update_egress *s,tv4_frame_ticket ticket,
    tv4_frame_driver_result result,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    (void)tv4_channel_tx_poll(&s->channel,s->channel.fragments.storage.generation,now);
    tv4_frame_queue_result r=tv4_frame_queue_complete(&s->queue,ticket,result,now);
    if(s->queue.stopped || s->channel.error!=TV4_CHANNEL_NO_ERROR)s->failed=true;
    return r==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED && s->failed?TV4_QUEUE_RECLAIMED_FAILED:r;
}
tv4_channel_result tv4_update_egress_advance(tv4_update_egress *s,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now)
{
    tv4_frame_queue_result r=tv4_update_egress_poll(s,generation,now);
    if(r!=TV4_QUEUE_OK)return r==TV4_QUEUE_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    if(tv4_frame_queue_quiescent(&s->queue,generation,now)!=TV4_QUEUE_OK)return TV4_CHANNEL_BUSY;
    return tv4_channel_tx_advance(&s->channel,phase,id,generation,now);
}
tv4_frame_queue_result tv4_update_egress_cancel(tv4_update_egress *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    (void)tv4_channel_tx_cancel(&s->channel,generation,now);s->failed=true;
    return tv4_frame_queue_cancel(&s->queue,generation,now);
}
tv4_frame_queue_result tv4_update_egress_clear(tv4_update_egress *s,uint64_t generation,uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_QUEUE_ARGUMENT;
    (void)tv4_update_egress_poll(s,generation,now);
    if(s->queue.count!=0 || s->queue.flight!=0)return TV4_QUEUE_HELD;
    if(!s->failed)return TV4_QUEUE_STATE;
    if(!s->queue.stopped)(void)tv4_frame_queue_cancel(&s->queue,generation,now);
    if(s->channel.error==TV4_CHANNEL_NO_ERROR && s->channel.fragments.storage.state!=TV4_FRAG_FAILED &&
        s->channel.fragments.storage.state!=TV4_FRAG_CANCELLED)
        (void)tv4_channel_tx_cancel(&s->channel,generation,now);
    if(tv4_channel_tx_clear(&s->channel,generation,now)!=TV4_FRAG_OK)return TV4_QUEUE_ERROR;
    if(tv4_frame_queue_clear(&s->queue,generation,now)!=TV4_QUEUE_OK)return TV4_QUEUE_ERROR;
    s->failed=false;return TV4_QUEUE_OK;
}
