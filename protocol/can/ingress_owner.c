#include "ingress_owner.h"
#include <limits.h>
#include <string.h>
_Static_assert(UINT_MAX==UINT32_MAX && ATOMIC_INT_LOCK_FREE==2,"RX owner requires native uint32 atomics");
static bool valid(const tv4_ingress_owner *s){return s!=NULL && s->configured;}
static unsigned state(const tv4_ingress_owner *s){return atomic_load_explicit(&s->gate,memory_order_acquire);}
static uint32_t epoch(const tv4_ingress_owner *s){return atomic_load_explicit(&s->epoch,memory_order_acquire);}
static void request_pause(tv4_ingress_owner *s)
{if(state(s)==TV4_GATE_OPEN){s->advanced=false;atomic_store_explicit(&s->gate,TV4_GATE_REQUESTED,memory_order_release);}}
static void fail(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    if(!s->failed){s->failed=true;(void)tv4_channel_rx_cancel(&s->channel,generation,now);}
    request_pause(s);
}
bool tv4_ingress_owner_init(tv4_ingress_owner *s,const tv4_fragment_config *fragments,
    unsigned capacity,uint32_t residency_ms,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_serial,uint32_t last_epoch,uint32_t now)
{
    if(s==NULL || s->configured || capacity==0 || capacity>TV4_FRAME_INGRESS_MAX ||
        residency_ms==0 || residency_ms>=UINT32_C(0x80000000) || route==0 || generation==0 || last_epoch==UINT32_MAX)return false;
    if(!tv4_channel_rx_init(&s->channel,fragments,route,generation,last_fragment,now))return false;
    if(!tv4_frame_ingress_init(&s->ingress,capacity,residency_ms,route,generation,last_serial,now))return false;
    atomic_init(&s->gate,TV4_GATE_OPEN);atomic_init(&s->epoch,last_epoch+1u);
    s->failed=false;s->advanced=false;s->configured=true;return true;
}
tv4_ingress_gate_result tv4_ingress_owner_permit(tv4_ingress_owner *s,tv4_ingress_permit *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(!valid(s) || out==NULL)return TV4_GATE_ARGUMENT;
    if(tv4_frame_ingress_faults(&s->ingress)!=0)return TV4_GATE_FAULT;
    unsigned gate=state(s);if(gate!=TV4_GATE_OPEN)return (tv4_ingress_gate_result)gate;
    *out=(tv4_ingress_permit){s,epoch(s)};return TV4_GATE_OPEN;
}
tv4_ingress_result tv4_ingress_owner_offer(tv4_ingress_owner *s,tv4_ingress_permit permit,
    uint32_t route,uint64_t generation,const tv4_fragment_frame *frame,uint32_t captured,uint32_t now)
{
    if(!valid(s))return TV4_INGRESS_ARGUMENT;
    if(permit.owner!=s || permit.epoch!=epoch(s))return TV4_INGRESS_STALE;
    if(tv4_frame_ingress_faults(&s->ingress)!=0)return TV4_INGRESS_FAULT;
    if(state(s)!=TV4_GATE_OPEN)return TV4_INGRESS_BUSY;
    return tv4_frame_ingress_offer(&s->ingress,route,generation,frame,captured,now);
}
tv4_ingress_result tv4_ingress_owner_retry(tv4_ingress_owner *s,uint32_t now)
{
    if(!valid(s))return TV4_INGRESS_ARGUMENT;
    if(state(s)==TV4_GATE_PAUSED)return TV4_INGRESS_BUSY;
    return tv4_frame_ingress_retry(&s->ingress,now);
}
tv4_ingress_gate_result tv4_ingress_owner_pause_request(tv4_ingress_owner *s,tv4_ingress_pause *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(!valid(s) || out==NULL)return TV4_GATE_ARGUMENT;
    unsigned gate=state(s);if(gate!=TV4_GATE_REQUESTED)return (tv4_ingress_gate_result)gate;
    *out=(tv4_ingress_pause){s,epoch(s)};return TV4_GATE_REQUESTED;
}
tv4_ingress_gate_result tv4_ingress_owner_ack(tv4_ingress_owner *s,tv4_ingress_pause request,uint32_t now)
{
    if(!valid(s))return TV4_GATE_ARGUMENT;
    if(request.owner!=s || request.epoch!=epoch(s))return TV4_GATE_STALE;
    unsigned gate=state(s);if(gate==TV4_GATE_PAUSED)return TV4_GATE_PAUSED;
    if(gate!=TV4_GATE_REQUESTED)return TV4_GATE_STALE;
    tv4_ingress_result r=tv4_frame_ingress_retry(&s->ingress,now);
    if(r==TV4_INGRESS_HELD)return TV4_GATE_BUSY;
    if(r!=TV4_INGRESS_ADMITTED && r!=TV4_INGRESS_WAIT)return TV4_GATE_FAULT;
    atomic_store_explicit(&s->gate,TV4_GATE_PAUSED,memory_order_release);return TV4_GATE_PAUSED;
}
tv4_ingress_result tv4_ingress_owner_lost(tv4_ingress_owner *s)
{return valid(s)?tv4_frame_ingress_lost(&s->ingress):TV4_INGRESS_ARGUMENT;}
tv4_fragment_result tv4_ingress_owner_poll(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    if(!valid(s))return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_rx_poll(&s->channel,generation,now);
    if(r!=TV4_FRAG_OK || tv4_frame_ingress_faults(&s->ingress)!=0 || s->failed){fail(s,generation,now);return TV4_FRAG_ERROR;}
    return TV4_FRAG_OK;
}
tv4_ingress_result tv4_ingress_owner_stage(tv4_ingress_owner *s,tv4_ingress_token *out)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(!valid(s) || out==NULL)return TV4_INGRESS_ARGUMENT;
    return tv4_frame_ingress_stage(&s->ingress,out);
}
tv4_fragment_result tv4_ingress_owner_receive(tv4_ingress_owner *s,tv4_ingress_token token,uint64_t generation,uint32_t now)
{
    if(!valid(s))return TV4_FRAG_ARGUMENT;
    (void)tv4_ingress_owner_poll(s,generation,now);
    if(tv4_frame_ingress_faults(&s->ingress)!=0)return TV4_FRAG_ERROR;
    tv4_fragment_frame frame;tv4_ingress_result r=tv4_frame_ingress_check(&s->ingress,token,&frame,now);
    if(r==TV4_INGRESS_STALE)return TV4_FRAG_STALE;
    if(r!=TV4_INGRESS_FRAME){if(r==TV4_INGRESS_FAULT)fail(s,generation,now);return r==TV4_INGRESS_ARGUMENT?TV4_FRAG_ARGUMENT:TV4_FRAG_ERROR;}
    if(s->failed){memset(&frame,0,sizeof(frame));return TV4_FRAG_STATE;}
    tv4_fragment_result result=tv4_channel_rx_receive(&s->channel,s->ingress.route,&frame,generation,now);
    memset(&frame,0,sizeof(frame));
    if(tv4_ingress_owner_poll(s,generation,now)!=TV4_FRAG_OK)return TV4_FRAG_ERROR;
    return result;
}
tv4_channel_result tv4_ingress_owner_take(tv4_ingress_owner *s,tv4_channel_message *out,uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(!valid(s) || out==NULL)return TV4_CHANNEL_ARGUMENT;
    if(tv4_ingress_owner_poll(s,generation,now)!=TV4_FRAG_OK)return TV4_CHANNEL_ERROR;
    return tv4_channel_rx_take(&s->channel,out,generation,now);
}
tv4_ingress_gate_result tv4_ingress_owner_pause(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    if(!valid(s))return TV4_GATE_ARGUMENT;
    if(tv4_ingress_owner_poll(s,generation,now)!=TV4_FRAG_OK)return TV4_GATE_FAULT;
    request_pause(s);return (tv4_ingress_gate_result)state(s);
}
static tv4_ingress_gate_result drained(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    if(!valid(s))return TV4_GATE_ARGUMENT;
    if(tv4_ingress_owner_poll(s,generation,now)!=TV4_FRAG_OK)return TV4_GATE_FAULT;
    if(state(s)!=TV4_GATE_PAUSED)return TV4_GATE_BUSY;
    return tv4_frame_ingress_empty(&s->ingress)==TV4_INGRESS_WAIT?TV4_GATE_PAUSED:TV4_GATE_BUSY;
}
tv4_ingress_gate_result tv4_ingress_owner_quiescent(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    tv4_ingress_gate_result r=drained(s,generation,now);if(r!=TV4_GATE_PAUSED)return r;
    return s->channel.fragments.storage.state==TV4_FRAG_IDLE?TV4_GATE_PAUSED:TV4_GATE_BUSY;
}
tv4_channel_result tv4_ingress_owner_advance(tv4_ingress_owner *s,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now)
{
    tv4_ingress_gate_result r=tv4_ingress_owner_quiescent(s,generation,now);
    if(r!=TV4_GATE_PAUSED)return r==TV4_GATE_FAULT?TV4_CHANNEL_ERROR:r==TV4_GATE_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_BUSY;
    if(s->advanced)return TV4_CHANNEL_STATE;
    tv4_channel_result result=tv4_channel_rx_advance(&s->channel,phase,id,generation,now);
    if(result==TV4_CHANNEL_OK)s->advanced=true;
    if(tv4_ingress_owner_poll(s,generation,now)!=TV4_FRAG_OK)return TV4_CHANNEL_ERROR;
    return result;
}
tv4_ingress_gate_result tv4_ingress_owner_resume(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    tv4_ingress_gate_result r=drained(s,generation,now);if(r!=TV4_GATE_PAUSED)return r;
    uint32_t e=epoch(s);if(e==UINT32_MAX){fail(s,generation,now);return TV4_GATE_FAULT;}
    s->advanced=false;atomic_store_explicit(&s->epoch,e+1u,memory_order_release);
    atomic_store_explicit(&s->gate,TV4_GATE_OPEN,memory_order_release);return TV4_GATE_OPEN;
}
tv4_fragment_result tv4_ingress_owner_recover(tv4_ingress_owner *s,uint64_t generation,uint32_t now)
{
    if(!valid(s))return TV4_FRAG_ARGUMENT;
    (void)tv4_ingress_owner_poll(s,generation,now);
    tv4_fragment_error error=s->channel.fragments.storage.error;
    if(!s->failed || (error!=TV4_FRAG_PROGRESS_TIMEOUT && error!=TV4_FRAG_TOTAL_TIMEOUT) ||
        tv4_frame_ingress_faults(&s->ingress)!=0)return TV4_FRAG_ERROR;
    if(state(s)!=TV4_GATE_PAUSED || tv4_frame_ingress_empty(&s->ingress)!=TV4_INGRESS_WAIT)return TV4_FRAG_BUSY;
    tv4_fragment_result r=tv4_channel_rx_clear(&s->channel,generation,now);
    if(r==TV4_FRAG_OK){s->failed=false;s->advanced=false;}return r;
}
