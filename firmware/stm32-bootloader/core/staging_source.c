#include "staging_source.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_boot_stage)<=192,"staging state storage regression");
static bool separate(const void *a,size_t as,const void *b,size_t bs)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return a!=NULL && b!=NULL && as<=UINTPTR_MAX-x && bs<=UINTPTR_MAX-y && (x+as<=y || y+bs<=x);
}
static bool outside(const tv4_boot_stage *s,const void *p,size_t n)
{
    return separate(s,sizeof(*s),p,n) && separate(s->buffer,s->capacity,p,n) &&
        (s->rx==NULL || separate(s->rx,sizeof(*s->rx),p,n));
}
static bool same_image(const tv4_update_descriptor *a,const tv4_update_descriptor *b)
{
    return a->id.boot==b->id.boot && a->id.transfer==b->id.transfer && a->image_bytes==b->image_bytes &&
        memcmp(a->image_sha256,b->image_sha256,TV4_UPDATE_HASH_BYTES)==0;
}
static bool limits_equal(tv4_update_limits a,tv4_update_limits b)
{
    return a.max_image_bytes==b.max_image_bytes && a.block_bytes==b.block_bytes && a.retry_limit==b.retry_limit &&
        a.ack_timeout_ms==b.ack_timeout_ms && a.progress_timeout_ms==b.progress_timeout_ms && a.total_timeout_ms==b.total_timeout_ms;
}
static bool receiver_matches(const tv4_boot_stage *s)
{
    const tv4_update_context *c=&s->rx->context;
    return c->configured && c->error==TV4_UPDATE_ERROR_NONE && c->local_boot==s->boot && same_image(&c->descriptor,&s->descriptor) &&
        limits_equal(c->limits,s->limits) && c->started_ms==s->rx_started_ms && c->progress_ms==s->rx_progress_ms &&
        c->next_offset==s->written && c->next_sequence==s->next_sequence;
}
bool tv4_boot_stage_init(tv4_boot_stage *s,const tv4_boot_stage_config *c)
{
    if(s==NULL || c==NULL || s->configured || !separate(s,sizeof(*s),c,sizeof(*c)) ||
        c->capacity==0 || c->capacity>UINT32_MAX || c->boot==0 || c->object==0 ||
        !separate(s,sizeof(*s),c->buffer,c->capacity) || !separate(c,sizeof(*c),c->buffer,c->capacity)) return false;
    *s=(tv4_boot_stage){.configured=true,.buffer=c->buffer,.capacity=c->capacity,.boot=c->boot,.object=c->object};
    return true;
}
bool tv4_boot_stage_begin(tv4_boot_stage *s,tv4_update_rx *rx)
{
    if(s==NULL || !s->configured || s->state!=TV4_STAGE_EMPTY || rx==NULL || !outside(s,rx,sizeof(*rx))) return false;
    const tv4_update_context *c=&rx->context;
    if(!c->configured || c->state!=TV4_UPDATE_RECEIVING || c->error!=TV4_UPDATE_ERROR_NONE || rx->retained ||
        c->local_boot!=s->boot || c->descriptor.id.boot!=s->boot || c->descriptor.id.transfer<=s->last_transfer ||
        c->descriptor.image_bytes==0 || c->descriptor.image_bytes>s->capacity || c->descriptor.image_bytes>c->limits.max_image_bytes ||
        c->limits.block_bytes==0 || c->limits.block_bytes>TV4_UPDATE_BLOCK_MAX || c->next_offset!=0 || c->next_sequence!=0 ||
        s->generation==UINT64_MAX) return false;
    s->descriptor=c->descriptor;s->limits=c->limits;s->last_transfer=c->descriptor.id.transfer;++s->generation;
    s->rx_started_ms=c->started_ms;s->rx_progress_ms=c->progress_ms;s->written=0;s->next_sequence=0;
    s->active_token=0;s->released_token=0;s->rx=rx;s->state=TV4_STAGE_RECEIVING;return true;
}
tv4_update_result tv4_boot_stage_consume(tv4_boot_stage *s,tv4_update_ack *ack,uint32_t now)
{
    if(s==NULL || ack==NULL || !s->configured) return TV4_UPDATE_REJECT_ARGUMENT;
    if(!outside(s,ack,sizeof(*ack))) return TV4_UPDATE_REJECT_ARGUMENT;
    if(s->state!=TV4_STAGE_RECEIVING || s->rx==NULL) return TV4_UPDATE_REJECT_STATE;
    tv4_update_block block;
    tv4_update_result r=tv4_update_rx_pending(s->rx,&block,now);
    if(r!=TV4_UPDATE_PENDING) {
        if(r==TV4_UPDATE_REJECT_STATE && s->rx->context.state==TV4_UPDATE_RECEIVING && receiver_matches(s)) return TV4_UPDATE_WAIT;
        s->state=TV4_STAGE_INVALID;return r;
    }
    if(s->written>s->descriptor.image_bytes) {s->state=TV4_STAGE_INVALID;return TV4_UPDATE_REJECT_CONTENT;}
    uint32_t remaining=s->descriptor.image_bytes-s->written;
    uint32_t expected=remaining<s->limits.block_bytes ? remaining : s->limits.block_bytes;
    if(!receiver_matches(s) || block.id.boot!=s->descriptor.id.boot || block.id.transfer!=s->descriptor.id.transfer ||
        block.offset!=s->written || block.sequence!=s->next_sequence || block.length==0 ||
        block.length!=expected || block.length>TV4_UPDATE_BLOCK_MAX) {
        s->state=TV4_STAGE_INVALID;return TV4_UPDATE_REJECT_CONTENT;
    }
    memcpy(s->buffer+s->written,block.bytes,block.length);s->written+=block.length;
    r=tv4_update_rx_consume(s->rx,block.id,block.sequence,true,ack,now);
    if(r!=TV4_UPDATE_ACK) {s->state=TV4_STAGE_INVALID;return r;}
    s->next_sequence=s->rx->context.next_sequence;s->rx_progress_ms=s->rx->context.progress_ms;
    if(s->written==s->descriptor.image_bytes) {s->state=TV4_STAGE_SEALED;s->rx=NULL;}
    return r;
}
bool tv4_boot_stage_discard(tv4_boot_stage *s)
{
    if(s==NULL || !s->configured || s->state==TV4_STAGE_LEASED) return false;
    memset(s->buffer,0,s->written);s->written=0;s->next_sequence=0;s->rx_started_ms=0;s->rx_progress_ms=0;
    s->descriptor=(tv4_update_descriptor){0};s->limits=(tv4_update_limits){0};s->rx=NULL;
    s->active_token=0;s->released_token=0;s->state=TV4_STAGE_EMPTY;return true;
}
static tv4_boot_source_view view(const tv4_boot_stage *s)
{
    return (tv4_boot_source_view){.token=s->active_token,.object=s->object,.generation=s->generation,
        .id=s->descriptor.id,.image_bytes=s->descriptor.image_bytes};
}
bool tv4_boot_stage_acquire(tv4_boot_stage *s,const tv4_update_descriptor *id,tv4_boot_source_view *out)
{
    if(s==NULL || !s->configured || s->state!=TV4_STAGE_SEALED || id==NULL || out==NULL ||
        !outside(s,id,sizeof(*id)) || !outside(s,out,sizeof(*out)) || !separate(id,sizeof(*id),out,sizeof(*out)) ||
        !same_image(id,&s->descriptor) || s->token_counter==UINT64_MAX) return false;
    s->active_token=++s->token_counter;s->released_token=0;s->state=TV4_STAGE_LEASED;*out=view(s);return true;
}
bool tv4_boot_stage_inspect(const tv4_boot_stage *s,uint64_t token,tv4_boot_source_view *out)
{
    if(s==NULL || !s->configured || s->state!=TV4_STAGE_LEASED || token==0 || token!=s->active_token ||
        out==NULL || !outside(s,out,sizeof(*out))) return false;
    *out=view(s);return true;
}
bool tv4_boot_stage_read(const tv4_boot_stage *s,uint64_t token,uint32_t offset,
    uint8_t *bytes,size_t capacity,size_t *count,bool *eof)
{
    if(s==NULL || !s->configured || s->state!=TV4_STAGE_LEASED || token==0 || token!=s->active_token ||
        offset>s->written || capacity==0 || capacity>TV4_BOOT_CHUNK_BYTES || bytes==NULL || count==NULL || eof==NULL ||
        !outside(s,bytes,capacity) || !outside(s,count,sizeof(*count)) || !outside(s,eof,sizeof(*eof)) ||
        !separate(bytes,capacity,count,sizeof(*count)) || !separate(bytes,capacity,eof,sizeof(*eof)) ||
        !separate(count,sizeof(*count),eof,sizeof(*eof))) return false;
    size_t n=s->written-offset;if(n>capacity)n=capacity;
    memcpy(bytes,s->buffer+offset,n);*count=n;*eof=offset+n==s->written;return true;
}
bool tv4_boot_stage_release(tv4_boot_stage *s,uint64_t token)
{
    if(s==NULL || !s->configured || token==0) return false;
    if(s->state==TV4_STAGE_SEALED && token==s->released_token) return true;
    if(s->state!=TV4_STAGE_LEASED || token!=s->active_token) return false;
    s->released_token=token;s->active_token=0;s->state=TV4_STAGE_SEALED;return true;
}
