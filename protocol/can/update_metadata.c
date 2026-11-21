#include "update_metadata.h"
#include <limits.h>
#include <string.h>

_Static_assert(TV4_META_BUNDLE_MAX==1215u,"Review container bound");
_Static_assert(sizeof(tv4_metadata_rx)<=2048u,"Review metadata RX storage");
_Static_assert(sizeof(tv4_metadata_document)<=1280u,"Review owned snapshot bound");
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static uint16_t get16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8));}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8u));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool valid_binding(const tv4_update_binding *b)
{return b!=NULL && b->image.id.boot!=0 && b->image.id.transfer!=0 && b->image.image_bytes!=0;}
static bool lengths(uint16_t doc,uint16_t sig,uint16_t key)
{return doc>0 && doc<=TV4_META_DOCUMENT_MAX && sig>=8 && sig<=TV4_META_SIGNATURE_MAX && key>0 && key<=TV4_META_KEY_MAX;}
tv4_metadata_result tv4_metadata_encode(const tv4_metadata_document *m,uint8_t *out,size_t capacity,size_t *written)
{
    if(written!=NULL)*written=0;
    if(m==NULL || out==NULL || written==NULL)return TV4_META_ARGUMENT;
    if(!valid_binding(&m->binding))return TV4_META_BINDING;
    if(!lengths(m->document_bytes,m->signature_bytes,m->key_id_bytes))return TV4_META_LENGTH;
    size_t n=TV4_META_HEADER_BYTES+(size_t)m->document_bytes+m->signature_bytes+m->key_id_bytes;
    if(capacity<n)return TV4_META_LENGTH;
    out[0]='T';out[1]='M';out[2]=1;out[3]=0;
    put16(out+4,m->document_bytes);put16(out+6,m->signature_bytes);put16(out+8,m->key_id_bytes);out[10]=0;out[11]=0;
    put32(out+12,m->binding.image.id.boot);put32(out+16,m->binding.image.id.transfer);put32(out+20,m->binding.image.image_bytes);
    memcpy(out+24,m->binding.image.image_sha256,32);memcpy(out+56,m->binding.metadata_sha256,32);
    uint8_t *p=out+TV4_META_HEADER_BYTES;
    memcpy(p,m->key_id,m->key_id_bytes);p+=m->key_id_bytes;
    memcpy(p,m->signature,m->signature_bytes);p+=m->signature_bytes;memcpy(p,m->document,m->document_bytes);
    *written=n;return TV4_META_OK;
}
tv4_metadata_result tv4_metadata_decode(const uint8_t *bytes,size_t length,const tv4_update_binding *expected,tv4_metadata_document *out)
{
    if(out==NULL)return TV4_META_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(bytes==NULL || expected==NULL)return TV4_META_ARGUMENT;
    if(!valid_binding(expected))return TV4_META_BINDING;
    if(length<TV4_META_BUNDLE_MIN || length>TV4_META_BUNDLE_MAX)return TV4_META_LENGTH;
    if(bytes[0]!='T' || bytes[1]!='M' || bytes[2]!=1 || bytes[3]!=0 || bytes[10]!=0 || bytes[11]!=0)return TV4_META_FORMAT;
    uint16_t doc=get16(bytes+4),sig=get16(bytes+6),key=get16(bytes+8);
    if(!lengths(doc,sig,key) || length!=TV4_META_HEADER_BYTES+(size_t)doc+sig+key)return TV4_META_LENGTH;
    out->binding.image.id=(tv4_update_id){get32(bytes+12),get32(bytes+16)};
    out->binding.image.image_bytes=get32(bytes+20);memcpy(out->binding.image.image_sha256,bytes+24,32);
    memcpy(out->binding.metadata_sha256,bytes+56,32);
    if(!tv4_update_binding_equal(expected,&out->binding)){memset(out,0,sizeof(*out));return TV4_META_BINDING;}
    out->document_bytes=doc;out->signature_bytes=sig;out->key_id_bytes=key;
    const uint8_t *p=bytes+TV4_META_HEADER_BYTES;
    memcpy(out->key_id,p,key);p+=key;memcpy(out->signature,p,sig);p+=sig;memcpy(out->document,p,doc);
    return TV4_META_OK;
}
static void wipe(tv4_metadata_rx *rx)
{memset(rx->bytes,0,sizeof(rx->bytes));memset(&rx->stream.block,0,sizeof(rx->stream.block));rx->stream.retained=false;}
static tv4_metadata_result fail(tv4_metadata_rx *rx,tv4_metadata_result error)
{wipe(rx);rx->state=TV4_META_FAILED;rx->error=error;return error;}
tv4_metadata_result tv4_metadata_rx_poll(tv4_metadata_rx *rx,uint64_t generation,uint32_t now)
{
    if(rx==NULL || !rx->configured)return TV4_META_ARGUMENT;
    if(generation!=rx->generation)return fail(rx,TV4_META_CONTEXT);
    if(now-rx->observed>(uint32_t)INT32_MAX)return fail(rx,TV4_META_CLOCK);
    rx->observed=now;
    if(rx->state==TV4_META_FAILED)return rx->error;
    if(rx->state==TV4_META_IDLE)return TV4_META_OK;
    if(tv4_update_rx_poll(&rx->stream,now)!=TV4_UPDATE_OK) {
        tv4_update_error e=rx->stream.context.error;
        return fail(rx,e==TV4_UPDATE_ERROR_CLOCK?TV4_META_CLOCK:
            e==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT || e==TV4_UPDATE_ERROR_TOTAL_TIMEOUT?TV4_META_TIMEOUT:TV4_META_STREAM);
    }
    return TV4_META_OK;
}
tv4_metadata_result tv4_metadata_rx_init(tv4_metadata_rx *rx,tv4_update_limits limits,
    uint32_t boot,uint64_t generation,uint32_t last_transfer,uint32_t now)
{
    if(rx==NULL)return TV4_META_ARGUMENT;
    if(rx->configured)return TV4_META_STATE;
    if(boot==0 || generation==0 || limits.max_image_bytes<TV4_META_BUNDLE_MIN || limits.max_image_bytes>TV4_META_BUNDLE_MAX)
        return TV4_META_ARGUMENT;
    if(!tv4_update_rx_init(&rx->stream,limits,boot,now))return TV4_META_ARGUMENT;
    rx->limits=limits;rx->boot=boot;rx->generation=generation;rx->last_transfer=last_transfer;
    rx->observed=now;rx->state=TV4_META_IDLE;rx->error=TV4_META_OK;rx->configured=true;wipe(rx);return TV4_META_OK;
}
tv4_metadata_result tv4_metadata_rx_begin(tv4_metadata_rx *rx,const tv4_update_descriptor *transfer,
    const tv4_update_binding *binding,uint64_t generation,uint32_t now)
{
    tv4_metadata_result r=tv4_metadata_rx_poll(rx,generation,now);if(r!=TV4_META_OK)return r;
    if(rx->state!=TV4_META_IDLE)return TV4_META_STATE;
    if(transfer==NULL || binding==NULL)return TV4_META_ARGUMENT;
    if(!valid_binding(binding))return TV4_META_BINDING;
    if(transfer->id.boot!=rx->boot || binding->image.id.boot!=rx->boot || transfer->id.transfer<=rx->last_transfer ||
        transfer->id.transfer==binding->image.id.transfer)return TV4_META_IDENTITY;
    if(transfer->image_bytes<TV4_META_BUNDLE_MIN || transfer->image_bytes>rx->limits.max_image_bytes)return TV4_META_LENGTH;
    rx->last_transfer=transfer->id.transfer;rx->binding=*binding;wipe(rx);
    /* Fresh metadata identity only. Never reinitialize the separate image RX. */
    if(!tv4_update_rx_init(&rx->stream,rx->limits,rx->boot,now) || tv4_update_rx_begin(&rx->stream,transfer,now)!=TV4_UPDATE_OK)
        return fail(rx,TV4_META_STREAM);
    rx->state=TV4_META_RECEIVING;return TV4_META_OK;
}
tv4_update_result tv4_metadata_rx_receive(tv4_metadata_rx *rx,const tv4_update_block *block,
    tv4_update_ack *ack,uint64_t generation,uint32_t now)
{
    if(ack!=NULL)memset(ack,0,sizeof(*ack));
    if(ack==NULL || block==NULL)return TV4_UPDATE_REJECT_ARGUMENT;
    if(tv4_metadata_rx_poll(rx,generation,now)!=TV4_META_OK)return TV4_UPDATE_ERROR;
    if(rx->state==TV4_META_IDLE)return TV4_UPDATE_REJECT_STATE;
    tv4_update_result r=tv4_update_rx_receive(&rx->stream,block,ack,now);
    if(r==TV4_UPDATE_PENDING) {
        const tv4_update_block *pending=&rx->stream.block;
        if(rx->state!=TV4_META_RECEIVING || pending->offset>rx->stream.context.descriptor.image_bytes ||
            pending->length>rx->stream.context.descriptor.image_bytes-pending->offset) {
            (void)fail(rx,TV4_META_STREAM);return TV4_UPDATE_ERROR;
        }
        memcpy(rx->bytes+pending->offset,pending->bytes,pending->length);
        r=tv4_update_rx_consume(&rx->stream,pending->id,pending->sequence,true,ack,now);
        if(r!=TV4_UPDATE_ACK){memset(ack,0,sizeof(*ack));(void)fail(rx,TV4_META_STREAM);return TV4_UPDATE_ERROR;}
        if(rx->stream.context.state==TV4_UPDATE_COMPLETE_UNVERIFIED)rx->state=TV4_META_COMPLETE_UNVERIFIED;
    }
    if(r!=TV4_UPDATE_ACK)memset(ack,0,sizeof(*ack));
    return r;
}
tv4_metadata_result tv4_metadata_rx_take(tv4_metadata_rx *rx,tv4_metadata_document *out,uint64_t generation,uint32_t now)
{
    if(out!=NULL)memset(out,0,sizeof(*out));
    if(out==NULL)return TV4_META_ARGUMENT;
    tv4_metadata_result r=tv4_metadata_rx_poll(rx,generation,now);if(r!=TV4_META_OK)return r;
    if(rx->state!=TV4_META_COMPLETE_UNVERIFIED)return TV4_META_STATE;
    r=tv4_metadata_decode(rx->bytes,rx->stream.context.descriptor.image_bytes,&rx->binding,out);
    if(r!=TV4_META_OK)return fail(rx,r);
    memset(rx->bytes,0,sizeof(rx->bytes));rx->state=TV4_META_TAKEN;return TV4_META_TAKEN_UNVERIFIED;
}
tv4_metadata_result tv4_metadata_rx_discard(tv4_metadata_rx *rx,uint64_t generation,uint32_t now)
{
    tv4_metadata_result r=tv4_metadata_rx_poll(rx,generation,now);
    if(r==TV4_META_ARGUMENT)return r;
    if(rx->state!=TV4_META_IDLE)
        (void)tv4_update_rx_abort(&rx->stream,rx->stream.context.descriptor.id,now);
    wipe(rx);
    if(rx->error==TV4_META_CONTEXT || rx->error==TV4_META_CLOCK)return rx->error;
    rx->state=TV4_META_IDLE;rx->error=TV4_META_OK;return TV4_META_OK;
}
