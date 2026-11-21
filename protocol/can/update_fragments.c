#include "update_fragments.h"
#include <limits.h>
#include <string.h>

_Static_assert(TV4_WIRE_MESSAGE_MAX == 286u, "Review envelope length bound");
_Static_assert(sizeof(tv4_fragment_tx) <= 384u, "Review TX storage budget");
_Static_assert(sizeof(tv4_fragment_rx) <= 384u, "Review RX storage budget");
static uint16_t get16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8)); }
static void put16(uint8_t *p, uint16_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v >> 8); }
static uint16_t frames_for(uint16_t n)
{ return (uint16_t)((n + TV4_FRAGMENT_CHUNK - 1u) / TV4_FRAGMENT_CHUNK); }
static uint16_t chunk_at(uint16_t total, uint16_t offset)
{ uint16_t n=(uint16_t)(total-offset); return n>TV4_FRAGMENT_CHUNK?TV4_FRAGMENT_CHUNK:n; }
static void wipe(tv4_fragment_storage *s)
{ memset(s->bytes,0,sizeof(s->bytes)); s->total=0;s->cursor=0;s->attempts=0; }
static tv4_fragment_result fail(tv4_fragment_storage *s, tv4_fragment_error error)
{ wipe(s);s->state=TV4_FRAG_FAILED;s->error=error;return TV4_FRAG_ERROR; }
static bool active(const tv4_fragment_storage *s)
{ return s->state==TV4_FRAG_SENDING || s->state==TV4_FRAG_RECEIVING || s->state==TV4_FRAG_READY_BYTES_UNCHECKED; }
static tv4_fragment_result observe(tv4_fragment_storage *s, uint64_t generation, uint32_t now)
{
    if(s==NULL || !s->configured)return TV4_FRAG_ARGUMENT;
    if(generation!=s->generation)return fail(s,TV4_FRAG_CONTEXT);
    if(now-s->observed>(uint32_t)INT32_MAX)return fail(s,TV4_FRAG_CLOCK);
    s->observed=now;
    if(s->state==TV4_FRAG_FAILED)return TV4_FRAG_ERROR;
    if(active(s)) {
        if(now-s->started>=s->config.total_timeout_ms)return fail(s,TV4_FRAG_TOTAL_TIMEOUT);
        if(s->state!=TV4_FRAG_READY_BYTES_UNCHECKED && now-s->progress>=s->config.progress_timeout_ms)
            return fail(s,TV4_FRAG_PROGRESS_TIMEOUT);
    }
    return TV4_FRAG_OK;
}
static tv4_fragment_result init(tv4_fragment_storage *s, const tv4_fragment_config *config,
    uint32_t route, uint64_t generation, uint16_t counter, uint32_t now)
{
    if(s==NULL || config==NULL)return TV4_FRAG_ARGUMENT;
    if(s->configured)return TV4_FRAG_STATE;
    if(config->version!=TV4_FRAGMENT_VERSION || config->progress_timeout_ms==0 ||
        config->progress_timeout_ms>config->total_timeout_ms ||
        config->total_timeout_ms>(uint32_t)INT32_MAX || config->max_frames==0 || route==0 || generation==0)
        return TV4_FRAG_ARGUMENT;
    memset(s,0,sizeof(*s));s->config=*config;s->route=route;s->generation=generation;
    s->counter=counter;s->observed=now;s->configured=true;return TV4_FRAG_OK;
}
tv4_fragment_result tv4_fragment_tx_init(tv4_fragment_tx *tx, const tv4_fragment_config *config,
    uint32_t route, uint64_t generation, uint16_t counter, uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=init(&tx->storage,config,route,generation,counter,now);
    if(r==TV4_FRAG_OK)tx->peeked=false;
    return r;
}
tv4_fragment_result tv4_fragment_rx_init(tv4_fragment_rx *rx, const tv4_fragment_config *config,
    uint32_t route, uint64_t generation, uint16_t counter, uint32_t now)
{ return rx==NULL?TV4_FRAG_ARGUMENT:init(&rx->storage,config,route,generation,counter,now); }
tv4_fragment_result tv4_fragment_tx_begin(tv4_fragment_tx *tx, const tv4_wire_message *message,
    uint64_t generation, uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_storage *s=&tx->storage;
    tv4_fragment_result r=observe(s,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(s->state!=TV4_FRAG_IDLE)return TV4_FRAG_BUSY;
    if(message==NULL)return TV4_FRAG_ARGUMENT;
    if(s->counter==UINT16_MAX)return fail(s,TV4_FRAG_EXHAUSTED);
    size_t n=0;
    if(tv4_update_wire_encode(message,s->bytes,sizeof(s->bytes),&n)!=TV4_WIRE_OK)
        { wipe(s);return TV4_FRAG_BAD_FRAME; }
    if(frames_for((uint16_t)n)>s->config.max_frames){wipe(s);return TV4_FRAG_LIMIT;}
    ++s->counter;s->total=(uint16_t)n;s->cursor=0;s->started=now;s->progress=now;
    s->state=TV4_FRAG_SENDING;tx->peeked=false;return TV4_FRAG_OK;
}
static void make_frame(const tv4_fragment_storage *s, tv4_fragment_frame *out)
{
    memset(out,0,sizeof(*out));put16(out->bytes,s->counter);
    put16(out->bytes+2,s->cursor==0?(uint16_t)(TV4_FRAGMENT_START | s->total):s->cursor);
    uint16_t n=chunk_at(s->total,s->cursor);
    out->length=(uint8_t)(TV4_FRAGMENT_HEADER+n);memcpy(out->bytes+TV4_FRAGMENT_HEADER,s->bytes+s->cursor,n);
}
tv4_fragment_result tv4_fragment_tx_peek(tv4_fragment_tx *tx, tv4_fragment_frame *out,
    uint64_t generation, uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(tx==NULL || out==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_result r=observe(&tx->storage,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(tx->storage.state!=TV4_FRAG_SENDING)return TV4_FRAG_STATE;
    make_frame(&tx->storage,out);tx->peeked=true;return TV4_FRAG_FRAME;
}
tv4_fragment_result tv4_fragment_tx_commit(tv4_fragment_tx *tx, const tv4_fragment_frame *submitted,
    uint64_t generation, uint32_t now)
{
    if(tx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_storage *s=&tx->storage;
    tv4_fragment_result r=observe(s,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(s->state!=TV4_FRAG_SENDING || !tx->peeked)return TV4_FRAG_STATE;
    if(submitted==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_frame expected;make_frame(s,&expected);
    if(submitted->length!=expected.length || memcmp(submitted->bytes,expected.bytes,expected.length)!=0)
        return TV4_FRAG_ORDER;
    s->cursor=(uint16_t)(s->cursor+expected.length-TV4_FRAGMENT_HEADER);s->progress=now;tx->peeked=false;
    if(s->cursor==s->total){wipe(s);s->state=TV4_FRAG_IDLE;return TV4_FRAG_FINISHED;}
    return TV4_FRAG_COMMITTED;
}
tv4_fragment_result tv4_fragment_rx_receive(tv4_fragment_rx *rx, uint32_t route,
    const tv4_fragment_frame *frame, uint64_t generation, uint32_t now)
{
    if(rx==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_storage *s=&rx->storage;
    tv4_fragment_result r=observe(s,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(route!=s->route)return TV4_FRAG_ROUTE;
    if(s->state==TV4_FRAG_READY_BYTES_UNCHECKED)return TV4_FRAG_BUSY;
    if(s->state!=TV4_FRAG_IDLE && s->state!=TV4_FRAG_RECEIVING)return TV4_FRAG_STATE;
    if(frame==NULL)return TV4_FRAG_ARGUMENT;
    if(frame->length>TV4_FRAGMENT_BYTES || frame->length<2)return TV4_FRAG_BAD_FRAME;
    uint16_t id=get16(frame->bytes);
    if(s->state==TV4_FRAG_RECEIVING) {
        if(id!=s->counter)return TV4_FRAG_BUSY;
        if(s->attempts==s->config.max_frames)return fail(s,TV4_FRAG_FRAME_BUDGET);
        ++s->attempts;
    }
    if(id==0 || frame->length<=TV4_FRAGMENT_HEADER)return TV4_FRAG_BAD_FRAME;
    uint16_t field=get16(frame->bytes+2);
    if((field & TV4_FRAGMENT_START)!=0) {
        uint16_t n=(uint16_t)(field & (TV4_FRAGMENT_START-1u));
        if(n<TV4_WIRE_HEADER_BYTES || n>TV4_WIRE_MESSAGE_MAX || frame->length!=TV4_FRAGMENT_BYTES)
            return TV4_FRAG_BAD_FRAME;
        if(s->state==TV4_FRAG_RECEIVING) {
            if(n!=s->total || memcmp(s->bytes,frame->bytes+TV4_FRAGMENT_HEADER,TV4_FRAGMENT_CHUNK)!=0)
                return fail(s,TV4_FRAG_CONFLICT);
            return TV4_FRAG_DUPLICATE;
        }
        if(id<=s->counter)return TV4_FRAG_STALE;
        if(frames_for(n)>s->config.max_frames)return TV4_FRAG_LIMIT;
        s->counter=id;s->total=n;s->cursor=TV4_FRAGMENT_CHUNK;s->attempts=1;
        s->started=now;s->progress=now;s->state=TV4_FRAG_RECEIVING;
        memcpy(s->bytes,frame->bytes+TV4_FRAGMENT_HEADER,TV4_FRAGMENT_CHUNK);return TV4_FRAG_PENDING;
    }
    if(s->state==TV4_FRAG_IDLE)return id<=s->counter?TV4_FRAG_STALE:TV4_FRAG_ORDER;
    if(field==0 || field%TV4_FRAGMENT_CHUNK!=0 || field>=s->total)return TV4_FRAG_ORDER;
    uint16_t n=chunk_at(s->total,field);
    if(frame->length!=TV4_FRAGMENT_HEADER+n)return TV4_FRAG_BAD_FRAME;
    if(field>s->cursor)return TV4_FRAG_ORDER;
    if(field<s->cursor) {
        if(memcmp(s->bytes+field,frame->bytes+TV4_FRAGMENT_HEADER,n)!=0)return fail(s,TV4_FRAG_CONFLICT);
        return TV4_FRAG_DUPLICATE;
    }
    memcpy(s->bytes+field,frame->bytes+TV4_FRAGMENT_HEADER,n);
    s->cursor=(uint16_t)(s->cursor+n);s->progress=now;
    if(s->cursor==s->total){s->state=TV4_FRAG_READY_BYTES_UNCHECKED;return TV4_FRAG_READY;}
    return TV4_FRAG_PENDING;
}
tv4_fragment_result tv4_fragment_rx_take(tv4_fragment_rx *rx, tv4_wire_message *out,
    uint64_t generation, uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(rx==NULL || out==NULL)return TV4_FRAG_ARGUMENT;
    tv4_fragment_storage *s=&rx->storage;
    tv4_fragment_result r=observe(s,generation,now);if(r!=TV4_FRAG_OK)return r;
    if(s->state!=TV4_FRAG_READY_BYTES_UNCHECKED)return TV4_FRAG_STATE;
    if(tv4_update_wire_decode(s->bytes,s->total,out)!=TV4_WIRE_OK)return fail(s,TV4_FRAG_CODEC);
    wipe(s);s->state=TV4_FRAG_IDLE;return TV4_FRAG_TAKEN;
}
tv4_fragment_result tv4_fragment_tx_poll(tv4_fragment_tx *tx, uint64_t generation, uint32_t now)
{ return tx==NULL?TV4_FRAG_ARGUMENT:observe(&tx->storage,generation,now); }
tv4_fragment_result tv4_fragment_rx_poll(tv4_fragment_rx *rx, uint64_t generation, uint32_t now)
{ return rx==NULL?TV4_FRAG_ARGUMENT:observe(&rx->storage,generation,now); }
static tv4_fragment_result cancel(tv4_fragment_storage *s, uint64_t generation, uint32_t now)
{
    tv4_fragment_result r=observe(s,generation,now);if(r!=TV4_FRAG_OK)return r;
    wipe(s);s->state=TV4_FRAG_CANCELLED;return TV4_FRAG_OK;
}
tv4_fragment_result tv4_fragment_tx_cancel(tv4_fragment_tx *tx, uint64_t generation, uint32_t now)
{ if(tx==NULL)return TV4_FRAG_ARGUMENT;tx->peeked=false;return cancel(&tx->storage,generation,now); }
tv4_fragment_result tv4_fragment_rx_cancel(tv4_fragment_rx *rx, uint64_t generation, uint32_t now)
{ return rx==NULL?TV4_FRAG_ARGUMENT:cancel(&rx->storage,generation,now); }
static tv4_fragment_result clear(tv4_fragment_storage *s, uint64_t generation, uint32_t now)
{
    tv4_fragment_result r=observe(s,generation,now);
    if(r==TV4_FRAG_ARGUMENT)return r;
    if(s->error==TV4_FRAG_CONTEXT || s->error==TV4_FRAG_CLOCK || s->error==TV4_FRAG_EXHAUSTED)
        return TV4_FRAG_ERROR;
    if(s->state!=TV4_FRAG_FAILED && s->state!=TV4_FRAG_CANCELLED)return TV4_FRAG_STATE;
    wipe(s);s->state=TV4_FRAG_IDLE;s->error=TV4_FRAG_NO_ERROR;return TV4_FRAG_OK;
}
tv4_fragment_result tv4_fragment_tx_clear(tv4_fragment_tx *tx, uint64_t generation, uint32_t now)
{ if(tx==NULL)return TV4_FRAG_ARGUMENT;tx->peeked=false;return clear(&tx->storage,generation,now); }
tv4_fragment_result tv4_fragment_rx_clear(tv4_fragment_rx *rx, uint64_t generation, uint32_t now)
{ return rx==NULL?TV4_FRAG_ARGUMENT:clear(&rx->storage,generation,now); }
