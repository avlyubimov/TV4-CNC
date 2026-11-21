#include "update_channel.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_channel_tx)<=768u,"Review channel TX storage");
_Static_assert(sizeof(tv4_channel_rx)<=704u,"Review channel RX storage");
_Static_assert(sizeof(tv4_channel_message)<=320u,"Review channel snapshot");
static bool id_equal(tv4_update_id a,tv4_update_id b)
{ return a.boot==b.boot && a.transfer==b.transfer; }
static bool id_valid(tv4_update_id id){return id.boot!=0 && id.transfer!=0;}
static tv4_channel_result allowed(const tv4_channel_policy *p,const tv4_channel_message *m)
{
    if(m->kind==TV4_CHANNEL_MS)return p->phase==TV4_CHANNEL_SETUP?TV4_CHANNEL_OK:TV4_CHANNEL_STATE;
    if(m->kind!=TV4_CHANNEL_WIRE)return TV4_CHANNEL_CONTENT;
    tv4_wire_kind kind=m->body.wire.kind;
    if(p->phase==TV4_CHANNEL_NEGOTIATION)
        return kind>=TV4_WIRE_REQUEST && kind<=TV4_WIRE_CONFIRM?TV4_CHANNEL_OK:TV4_CHANNEL_STATE;
    if(p->phase==TV4_CHANNEL_METADATA || p->phase==TV4_CHANNEL_IMAGE) {
        tv4_update_id expected=p->phase==TV4_CHANNEL_METADATA?p->metadata:p->image;
        tv4_update_id actual;
        if(kind==TV4_WIRE_DATA)actual=m->body.wire.body.data.id;
        else if(kind==TV4_WIRE_DATA_ACK)actual=m->body.wire.body.ack.id;
        else return TV4_CHANNEL_STATE;
        return id_equal(actual,expected)?TV4_CHANNEL_OK:TV4_CHANNEL_IDENTITY;
    }
    if(p->phase==TV4_CHANNEL_OUTCOME && (kind==TV4_WIRE_OUTCOME || kind==TV4_WIRE_OUTCOME_ACK))
        return id_equal(m->body.wire.body.outcome.binding.image.id,p->image)?TV4_CHANNEL_OK:TV4_CHANNEL_IDENTITY;
    return TV4_CHANNEL_STATE;
}
static tv4_channel_result advance(tv4_channel_policy *p,tv4_channel_phase next,tv4_update_id id)
{
    if((unsigned)next>(unsigned)TV4_CHANNEL_OUTCOME)return TV4_CHANNEL_ARGUMENT;
    if(next==TV4_CHANNEL_NEGOTIATION && p->phase==TV4_CHANNEL_OUTCOME) {
        if(id.boot!=0 || id.transfer!=0)return TV4_CHANNEL_ARGUMENT;
        *p=(tv4_channel_policy){0};return TV4_CHANNEL_OK;
    }
    if((unsigned)next!=(unsigned)p->phase+1u)return TV4_CHANNEL_STATE;
    if(next==TV4_CHANNEL_METADATA) {
        if(!id_valid(id) || id.transfer<2)return TV4_CHANNEL_IDENTITY;
        p->metadata=id;
    }else if(next==TV4_CHANNEL_IMAGE) {
        if(!id_valid(id) || id.transfer==UINT32_MAX || id.boot!=p->metadata.boot ||
            id.transfer+1u!=p->metadata.transfer)return TV4_CHANNEL_IDENTITY;
        p->image=id;
    }else if(id.boot!=0 || id.transfer!=0)return TV4_CHANNEL_ARGUMENT;
    p->phase=next;return TV4_CHANNEL_OK;
}
bool tv4_channel_tx_init(tv4_channel_tx *tx,const tv4_fragment_config *config,
    uint32_t route,uint64_t generation,uint16_t last,uint32_t now)
{
    if(tx==NULL || tx->fragments.storage.configured)return false;
    if(tv4_fragment_tx_init_raw(&tx->fragments,config,route,generation,last,now)!=TV4_FRAG_OK)return false;
    tx->policy=(tv4_channel_policy){0};memset(tx->scratch,0,sizeof(tx->scratch));
    memset(&tx->outbox,0,sizeof(tx->outbox));tx->active=false;tx->queued=false;
    tx->error=TV4_CHANNEL_NO_ERROR;tx->started=now;tx->progress=now;return true;
}
bool tv4_channel_rx_init(tv4_channel_rx *rx,const tv4_fragment_config *config,
    uint32_t route,uint64_t generation,uint16_t last,uint32_t now)
{
    if(rx==NULL || rx->fragments.storage.configured)return false;
    if(tv4_fragment_rx_init_raw(&rx->fragments,config,route,generation,last,now)!=TV4_FRAG_OK)return false;
    rx->policy=(tv4_channel_policy){0};memset(rx->scratch,0,sizeof(rx->scratch));
    rx->error=TV4_CHANNEL_NO_ERROR;return true;
}
static void wipe_tx(tv4_channel_tx *tx)
{
    memset(tx->scratch,0,sizeof(tx->scratch));memset(&tx->outbox,0,sizeof(tx->outbox));
    tx->active=false;tx->queued=false;
}
tv4_fragment_result tv4_channel_tx_poll(tv4_channel_tx *tx,uint64_t generation,uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_fragment_tx_poll(&tx->fragments,generation,now);
    if(r!=TV4_FRAG_OK){if(r==TV4_FRAG_ERROR){wipe_tx(tx);tx->error=TV4_CHANNEL_FRAGMENT;}return r;}
    if(tx->error!=TV4_CHANNEL_NO_ERROR)return TV4_FRAG_ERROR;
    if(tx->active) {
        tv4_fragment_config config=tx->fragments.storage.config;
        if(now-tx->started>=config.total_timeout_ms)tx->error=TV4_CHANNEL_QUEUE_TOTAL;
        else if(now-tx->progress>=config.progress_timeout_ms)tx->error=TV4_CHANNEL_QUEUE_PROGRESS;
        if(tx->error!=TV4_CHANNEL_NO_ERROR){wipe_tx(tx);return TV4_FRAG_ERROR;}
    }
    return TV4_FRAG_OK;
}
tv4_fragment_result tv4_channel_rx_poll(tv4_channel_rx *rx,uint64_t generation,uint32_t now)
{
    if(rx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_fragment_rx_poll(&rx->fragments,generation,now);
    if(r==TV4_FRAG_ERROR){memset(rx->scratch,0,sizeof(rx->scratch));rx->error=TV4_CHANNEL_FRAGMENT;}
    return r==TV4_FRAG_OK && rx->error!=TV4_CHANNEL_NO_ERROR?TV4_FRAG_ERROR:r;
}
tv4_channel_result tv4_channel_tx_begin(tv4_channel_tx *tx,const tv4_channel_message *m,
    uint64_t generation,uint32_t now)
{
    if(tx==NULL || m==NULL)return TV4_CHANNEL_ARGUMENT;
    tv4_fragment_result r=tv4_channel_tx_poll(tx,generation,now);
    if(r!=TV4_FRAG_OK)return r==TV4_FRAG_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    if(tx->active || tx->queued || tx->fragments.storage.state!=TV4_FRAG_IDLE)return TV4_CHANNEL_BUSY;
    tv4_channel_result a=allowed(&tx->policy,m);if(a!=TV4_CHANNEL_OK)return a;
    size_t n=0;bool encoded=m->kind==TV4_CHANNEL_WIRE?
        tv4_update_wire_encode(&m->body.wire,tx->scratch,sizeof(tx->scratch),&n)==TV4_WIRE_OK:
        tv4_metadata_setup_encode(&m->body.setup,tx->scratch,sizeof(tx->scratch),&n)==TV4_SETUP_OK;
    if(!encoded){memset(tx->scratch,0,sizeof(tx->scratch));return TV4_CHANNEL_CONTENT;}
    r=tv4_fragment_tx_begin_raw(&tx->fragments,tx->scratch,n,generation,now);
    memset(tx->scratch,0,sizeof(tx->scratch));
    if(r!=TV4_FRAG_OK)return r==TV4_FRAG_ERROR?TV4_CHANNEL_ERROR:TV4_CHANNEL_CONTENT;
    tx->active=true;tx->started=now;tx->progress=now;return TV4_CHANNEL_OK;
}
tv4_fragment_result tv4_channel_tx_peek(tv4_channel_tx *tx,tv4_fragment_frame *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(tx==NULL || out==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_tx_poll(tx,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(!tx->active)return TV4_FRAG_STATE;
    if(!tx->queued) {
        r=tv4_fragment_tx_peek(&tx->fragments,&tx->outbox,generation,now);if(r!=TV4_FRAG_FRAME)return r;
        r=tv4_fragment_tx_commit(&tx->fragments,&tx->outbox,generation,now);
        if(r!=TV4_FRAG_COMMITTED && r!=TV4_FRAG_FINISHED)return r;
        tx->queued=true;
    }
    *out=tx->outbox;return TV4_FRAG_FRAME;
}
tv4_fragment_result tv4_channel_tx_commit(tv4_channel_tx *tx,const tv4_fragment_frame *frame,
    uint64_t generation,uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_tx_poll(tx,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(!tx->queued)return TV4_FRAG_STATE;
    if(frame==NULL)return TV4_FRAG_ARGUMENT;
    if(frame->length!=tx->outbox.length || memcmp(frame->bytes,tx->outbox.bytes,tx->outbox.length))return TV4_FRAG_ORDER;
    tx->queued=false;memset(&tx->outbox,0,sizeof(tx->outbox));tx->progress=now;
    if(tx->fragments.storage.state==TV4_FRAG_IDLE){tx->active=false;return TV4_FRAG_FINISHED;}
    return TV4_FRAG_COMMITTED;
}
tv4_fragment_result tv4_channel_rx_receive(tv4_channel_rx *rx,uint32_t route,
    const tv4_fragment_frame *frame,uint64_t generation,uint32_t now)
{
    if(rx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_rx_poll(rx,generation,now);if(r!=TV4_FRAG_OK)return r;
    return tv4_fragment_rx_receive(&rx->fragments,route,frame,generation,now);
}
tv4_channel_result tv4_channel_rx_take(tv4_channel_rx *rx,tv4_channel_message *out,
    uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(rx==NULL || out==NULL)return TV4_CHANNEL_ARGUMENT;
    tv4_fragment_result r=tv4_channel_rx_poll(rx,generation,now);
    if(r!=TV4_FRAG_OK)return r==TV4_FRAG_ARGUMENT?TV4_CHANNEL_ARGUMENT:TV4_CHANNEL_ERROR;
    size_t n=0;r=tv4_fragment_rx_take_raw(&rx->fragments,rx->scratch,sizeof(rx->scratch),&n,generation,now);
    if(r!=TV4_FRAG_RAW_TAKEN_UNCHECKED)return r==TV4_FRAG_STATE?TV4_CHANNEL_STATE:TV4_CHANNEL_ERROR;
    bool decoded=false;
    if(rx->scratch[0]=='T' && rx->scratch[1]=='U') {
        decoded=tv4_update_wire_decode(rx->scratch,n,&out->body.wire)==TV4_WIRE_OK;out->kind=TV4_CHANNEL_WIRE;
    }else if(rx->scratch[0]=='M' && rx->scratch[1]=='S') {
        decoded=tv4_metadata_setup_decode(rx->scratch,n,&out->body.setup)==TV4_SETUP_OK;out->kind=TV4_CHANNEL_MS;
    }
    memset(rx->scratch,0,sizeof(rx->scratch));
    if(!decoded){memset(out,0,sizeof(*out));rx->error=TV4_CHANNEL_CODEC;return TV4_CHANNEL_ERROR;}
    tv4_channel_result a=allowed(&rx->policy,out);
    if(a!=TV4_CHANNEL_OK){memset(out,0,sizeof(*out));return a;}
    return TV4_CHANNEL_TAKEN_UNVERIFIED;
}
tv4_channel_result tv4_channel_tx_advance(tv4_channel_tx *tx,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now)
{
    if(tx==NULL)return TV4_CHANNEL_ARGUMENT;
    if(tv4_channel_tx_poll(tx,generation,now)!=TV4_FRAG_OK)return TV4_CHANNEL_ERROR;
    if(tx->active || tx->queued || tx->fragments.storage.state!=TV4_FRAG_IDLE)return TV4_CHANNEL_BUSY;
    return advance(&tx->policy,phase,id);
}
tv4_channel_result tv4_channel_rx_advance(tv4_channel_rx *rx,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now)
{
    if(rx==NULL)return TV4_CHANNEL_ARGUMENT;
    if(tv4_channel_rx_poll(rx,generation,now)!=TV4_FRAG_OK)return TV4_CHANNEL_ERROR;
    if(rx->fragments.storage.state!=TV4_FRAG_IDLE)return TV4_CHANNEL_BUSY;
    return advance(&rx->policy,phase,id);
}
tv4_fragment_result tv4_channel_tx_cancel(tv4_channel_tx *tx,uint64_t generation,uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_tx_poll(tx,generation,now);if(r!=TV4_FRAG_OK)return r;
    r=tv4_fragment_tx_cancel(&tx->fragments,generation,now);if(r==TV4_FRAG_OK)wipe_tx(tx);return r;
}
tv4_fragment_result tv4_channel_rx_cancel(tv4_channel_rx *rx,uint64_t generation,uint32_t now)
{
    if(rx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_rx_poll(rx,generation,now);if(r!=TV4_FRAG_OK)return r;
    r=tv4_fragment_rx_cancel(&rx->fragments,generation,now);if(r==TV4_FRAG_OK)memset(rx->scratch,0,sizeof(rx->scratch));return r;
}
static bool sticky(const tv4_fragment_storage *s)
{ return s->error==TV4_FRAG_CONTEXT || s->error==TV4_FRAG_CLOCK || s->error==TV4_FRAG_EXHAUSTED; }
tv4_fragment_result tv4_channel_tx_clear(tv4_channel_tx *tx,uint64_t generation,uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_tx_poll(tx,generation,now);
    if(r==TV4_FRAG_ARGUMENT || sticky(&tx->fragments.storage))return r;
    if(tx->error!=TV4_CHANNEL_NO_ERROR && tx->fragments.storage.state!=TV4_FRAG_FAILED &&
        tx->fragments.storage.state!=TV4_FRAG_CANCELLED) {
        r=tv4_fragment_tx_cancel(&tx->fragments,generation,now);if(r!=TV4_FRAG_OK)return r;
    }
    r=tv4_fragment_tx_clear(&tx->fragments,generation,now);
    if(r==TV4_FRAG_OK){wipe_tx(tx);tx->error=TV4_CHANNEL_NO_ERROR;}return r;
}
tv4_fragment_result tv4_channel_rx_clear(tv4_channel_rx *rx,uint64_t generation,uint32_t now)
{
    if(rx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=tv4_channel_rx_poll(rx,generation,now);
    if(r==TV4_FRAG_ARGUMENT || sticky(&rx->fragments.storage))return r;
    if(rx->error!=TV4_CHANNEL_NO_ERROR && rx->fragments.storage.state!=TV4_FRAG_FAILED &&
        rx->fragments.storage.state!=TV4_FRAG_CANCELLED) {
        r=tv4_fragment_rx_cancel(&rx->fragments,generation,now);if(r!=TV4_FRAG_OK)return r;
    }
    r=tv4_fragment_rx_clear(&rx->fragments,generation,now);
    if(r==TV4_FRAG_OK){memset(rx->scratch,0,sizeof(rx->scratch));rx->error=TV4_CHANNEL_NO_ERROR;}return r;
}
