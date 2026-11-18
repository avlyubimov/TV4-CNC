#include "memory_probe.h"
static bool pattern(volatile uint8_t *bytes,size_t count) {
    bool ok=true;
    for(unsigned pass=0;pass<2;pass++) {
        uint8_t value=pass?0x5a:0xa5;
        for(size_t i=0;i<count;i++)bytes[i]=value;
        for(size_t i=0;i<count;i++)if(bytes[i]!=value)ok=false;
    }
    for(size_t i=0;i<count;i++)bytes[i]=0;
    return ok;
}
bool tv4_ota_memory_probe_buffers(tv4_ota_session *s) {
    if(!s || s->result.state!=TV4_SESSION_IDLE || s->image.whole_hash.context || s->image.body_hash.context)return false;
    /* All buffers must be cleared even if an earlier pattern fails. */
    bool manifest=pattern(s->manifest,sizeof(s->manifest));
    bool envelope=pattern(s->envelope,sizeof(s->envelope));
    bool chunk=pattern(s->http.chunk,sizeof(s->http.chunk));
    return manifest && envelope && chunk;
}
