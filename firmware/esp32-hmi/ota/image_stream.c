#include "image_stream.h"
#include <limits.h>
#include <string.h>
enum { HEADER,SEGMENT,DATA,PADDING,CHECKSUM,TAIL,END };
static uint32_t le32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned le16(const uint8_t *p) {return (unsigned)p[0]|(unsigned)p[1]<<8;}
static bool fail(tv4_image_stream *s,tv4_image_error error) {
    s->state=TV4_IMAGE_FAILED;s->error=error;
    tv4_ota_hash_close(&s->whole_hash);tv4_ota_hash_close(&s->body_hash);return false;
}
void tv4_image_close(tv4_image_stream *s) {
    if(!s)return;
    tv4_ota_hash_close(&s->whole_hash);tv4_ota_hash_close(&s->body_hash);memset(s,0,sizeof(*s));
}
bool tv4_image_start(tv4_image_stream *s,const tv4_ota_candidate *candidate,
                     tv4_image_limits limits,uint32_t now_ms) {
    if(!s || s->state!=TV4_IMAGE_IDLE || s->whole_hash.context || s->body_hash.context)return false;
    if(!candidate || candidate->size_bytes<24+8+256+1+32 || candidate->size_bytes>0x300000 ||
       !candidate->version[0] || !memchr(candidate->version,0,32) ||
       !limits.chip_revision_known || !limits.idle_timeout_ms || !limits.total_timeout_ms ||
       limits.idle_timeout_ms>INT32_MAX || limits.total_timeout_ms>INT32_MAX ||
       limits.total_timeout_ms<limits.idle_timeout_ms)return fail(s,TV4_IMAGE_ARGUMENT);
    s->limits=limits;s->expected=candidate->size_bytes;
    memcpy(s->expected_digest,candidate->sha256,32);strcpy(s->version,candidate->version);
    s->started_ms=s->progress_ms=now_ms;s->checksum=0xef;
    if(!tv4_ota_hash_open(&s->whole_hash) || !tv4_ota_hash_open(&s->body_hash))return fail(s,TV4_IMAGE_CRYPTO);
    s->state=TV4_IMAGE_RECEIVING;return true;
}
bool tv4_image_poll(tv4_image_stream *s,uint32_t now_ms) {
    if(!s || s->state!=TV4_IMAGE_RECEIVING)return false;
    if((uint32_t)(now_ms-s->progress_ms)>=s->limits.idle_timeout_ms ||
       (uint32_t)(now_ms-s->started_ms)>=s->limits.total_timeout_ms)return fail(s,TV4_IMAGE_TIMEOUT);
    return true;
}
static bool consume(tv4_image_stream *s,const uint8_t *p,size_t n,bool body) {
    if(body && !tv4_ota_hash_update(&s->body_hash,p,n))return fail(s,TV4_IMAGE_CRYPTO);
    s->received+=(uint32_t)n;return true;
}
static bool check_header(tv4_image_stream *s) {
    const uint8_t *h=s->header;
    unsigned min=le16(h+15),max=le16(h+17);
    if(!min)min=(unsigned)h[14]*100;
    if(h[0]!=0xe9 || !h[1] || h[1]>16 || h[2]!=2 || h[3]!=0x4f ||
       le16(h+12)!=9 || h[23]!=1 || s->limits.chip_revision<min ||
       s->limits.chip_revision>max)return fail(s,TV4_IMAGE_HEADER);
    s->segments=h[1];s->phase=SEGMENT;s->used=0;return true;
}
static bool check_descriptor(tv4_image_stream *s) {
    const uint8_t *d=s->descriptor;
    if(le32(d)!=0xabcd5432 || le32(d+4)!=0 || !memchr(d+16,0,32) ||
       !memchr(d+48,0,32) || strcmp((const char *)d+16,s->version) ||
       strcmp((const char *)d+48,"tv4_hmi"))return fail(s,TV4_IMAGE_DESCRIPTOR);
    return true;
}
bool tv4_image_feed(tv4_image_stream *s,const uint8_t *bytes,size_t length,uint32_t now_ms) {
    if(!tv4_image_poll(s,now_ms))return false;
    if(!bytes || !length || length>TV4_OTA_CHUNK_MAX)return fail(s,TV4_IMAGE_ARGUMENT);
    if(length>s->expected-s->received)return fail(s,TV4_IMAGE_LENGTH);
    if(!tv4_ota_hash_update(&s->whole_hash,bytes,length))return fail(s,TV4_IMAGE_CRYPTO);
    size_t left=length;const uint8_t *p=bytes;
    while(left) {
        size_t n=0;
        if(s->phase==HEADER || s->phase==SEGMENT) {
            size_t capacity=s->phase==HEADER?sizeof(s->header):sizeof(s->segment);
            uint8_t *target=s->phase==HEADER?s->header:s->segment;
            n=capacity-s->used;if(n>left)n=left;
            memcpy(target+s->used,p,n);s->used+=(unsigned)n;
            if(!consume(s,p,n,true))return false;
            if(s->used==capacity) {
                if(s->phase==HEADER) {if(!check_header(s))return false;}
                else {
                    uint32_t size=le32(s->segment+4);
                    if(!size || size%4 || size>s->expected-s->received || (!s->index && size<256))return fail(s,TV4_IMAGE_SEGMENT);
                    s->segment_left=size;s->phase=DATA;s->used=0;
                }
            }
        } else if(s->phase==DATA) {
            n=s->segment_left;if(n>left)n=left;
            if(!s->index && s->descriptor_used<sizeof(s->descriptor)) {
                size_t copy=sizeof(s->descriptor)-s->descriptor_used;if(copy>n)copy=n;
                memcpy(s->descriptor+s->descriptor_used,p,copy);s->descriptor_used+=(unsigned)copy;
                if(s->descriptor_used==sizeof(s->descriptor) && !check_descriptor(s))return false;
            }
            for(size_t i=0;i<n;i++)s->checksum^=p[i];
            if(!consume(s,p,n,true))return false;
            s->segment_left-=(uint32_t)n;
            if(!s->segment_left) {
                if(++s->index<s->segments)s->phase=SEGMENT;
                else {s->padding_left=15-(s->received%16);s->phase=PADDING;}
            }
        } else if(s->phase==PADDING) {
            n=s->padding_left;if(n>left)n=left;
            for(size_t i=0;i<n;i++)if(p[i])return fail(s,TV4_IMAGE_CHECKSUM);
            if(!consume(s,p,n,true))return false;
            s->padding_left-=(uint32_t)n;if(!s->padding_left)s->phase=CHECKSUM;
        } else if(s->phase==CHECKSUM) {
            n=1;if(*p!=s->checksum)return fail(s,TV4_IMAGE_CHECKSUM);
            if(!consume(s,p,n,true))return false;
            if(!tv4_ota_hash_finish(&s->body_hash,s->body_digest))return fail(s,TV4_IMAGE_CRYPTO);
            s->phase=TAIL;s->used=0;
        } else if(s->phase==TAIL) {
            n=sizeof(s->tail)-s->used;if(n>left)n=left;
            memcpy(s->tail+s->used,p,n);s->used+=(unsigned)n;
            if(!consume(s,p,n,false))return false;
            if(s->used==sizeof(s->tail)) {
                if(memcmp(s->tail,s->body_digest,32))return fail(s,TV4_IMAGE_DIGEST);
                s->phase=END;
            }
        } else return fail(s,TV4_IMAGE_LENGTH);
        p+=n;left-=n;
    }
    s->progress_ms=now_ms;return true;
}
bool tv4_image_finish(tv4_image_stream *s,uint32_t now_ms) {
    if(!tv4_image_poll(s,now_ms))return false;
    if(s->received!=s->expected || s->phase!=END)return fail(s,TV4_IMAGE_LENGTH);
    uint8_t digest[32];
    if(!tv4_ota_hash_finish(&s->whole_hash,digest))return fail(s,TV4_IMAGE_CRYPTO);
    if(memcmp(digest,s->expected_digest,32))return fail(s,TV4_IMAGE_DIGEST);
    s->state=TV4_IMAGE_VERIFIED;return true;
}
void tv4_image_abort(tv4_image_stream *s,bool cancelled) {
    if(!s || s->state!=TV4_IMAGE_RECEIVING)return;
    (void)fail(s,TV4_IMAGE_IO);if(cancelled) {s->state=TV4_IMAGE_CANCELLED;s->error=TV4_IMAGE_NONE;}
}
