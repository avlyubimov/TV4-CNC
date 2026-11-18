#include "memory.h"
#include "memory_probe.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    unsigned snapshots,allocations,frees,fail_snapshot;
    bool fail_alloc,wrong_region,wrong_end,deplete;
    tv4_ota_heap heap;
    void *pointer;
} fixture;
static bool snapshot(void *context,tv4_ota_heap *heap) {
    fixture *f=context;++f->snapshots;if(f->snapshots==f->fail_snapshot)return false;
    *heap=f->heap;
    if(f->snapshots==2) {
        heap->psram_free-=sizeof(tv4_ota_session);
        if(f->deplete)heap->internal_largest=1;
    }
    return true;
}
static void *allocate(void *context,size_t bytes) {
    fixture *f=context;f->allocations++;assert(bytes==sizeof(tv4_ota_session));
    if(f->fail_alloc)return NULL;
    f->pointer=malloc(bytes);assert(f->pointer);memset(f->pointer,0xa5,bytes);return f->pointer;
}
static bool external(void *context,const void *p) {
    fixture *f=context;return !f->wrong_region && (!f->wrong_end || p==f->pointer);
}
static void release(void *context,void *p) {
    fixture *f=context;assert(p==f->pointer);f->frees++;free(p);f->pointer=NULL;
}
static fixture healthy(void) {fixture f={0};f.heap=(tv4_ota_heap){65536,32768,1024*1024,512*1024};return f;}
/* Synthetic test floors only, NOT reviewed production/TLS requirements. */
static const tv4_ota_memory_policy policy={true,32768,16384,65536};
static tv4_ota_memory_result acquire(tv4_ota_memory *lease,fixture *f,tv4_ota_memory_policy p) {
    tv4_ota_memory_ops ops={f,snapshot,allocate,external,release};
    return tv4_ota_memory_acquire(lease,&p,&ops);
}
static void empty(const tv4_ota_memory *l,const fixture *f) {assert(!l->session && !f->pointer);}
int main(void) {
    tv4_ota_memory l={0};fixture f=healthy();assert(tv4_ota_workspace_bytes==sizeof(tv4_ota_session));
    assert(tv4_ota_memory_release(NULL)==TV4_MEMORY_ARGUMENT);
    assert(tv4_ota_memory_release(&l)==TV4_MEMORY_OK);
    assert(acquire(&l,&f,policy)==TV4_MEMORY_OK);
    for(size_t i=0;i<sizeof(*l.session);i++)assert(!((unsigned char *)l.session)[i]);
    assert(acquire(&l,&f,policy)==TV4_MEMORY_BUSY && f.allocations==1);
    assert(!tv4_ota_memory_bench_start()); /* Host never schedules a task. */
    l.session->candidate.size_bytes=123;l.session->http.response.status=200;l.session->image.expected=456;
    assert(tv4_ota_memory_probe_buffers(l.session));
    assert(l.session->candidate.size_bytes==123 && l.session->http.response.status==200 && l.session->image.expected==456);
    for(size_t i=0;i<sizeof(l.session->manifest);i++)assert(!l.session->manifest[i]);
    for(size_t i=0;i<sizeof(l.session->envelope);i++)assert(!l.session->envelope[i]);
    for(size_t i=0;i<sizeof(l.session->http.chunk);i++)assert(!l.session->http.chunk[i]);
    l.session->manifest[0]=123;l.session->result.state=TV4_SESSION_IMAGE;
    assert(!tv4_ota_memory_probe_buffers(l.session) && l.session->manifest[0]==123);
    l.session->result.state=TV4_SESSION_IDLE;
    assert(tv4_ota_hash_open(&l.session->image.whole_hash));
    assert(!tv4_ota_memory_probe_buffers(l.session) && l.session->manifest[0]==123);
    tv4_ota_hash_close(&l.session->image.whole_hash);
    assert(tv4_ota_memory_probe_buffers(l.session));
    assert(!tv4_ota_memory_probe_buffers(NULL));
    for(int state=TV4_SESSION_MANIFEST;state<=TV4_SESSION_FINISHING;state++) {
        l.session->result.state=(tv4_session_state)state;
        assert(tv4_ota_memory_release(&l)==TV4_MEMORY_BUSY && !f.frees);
    }
    l.session->result.state=TV4_SESSION_IMAGE_VERIFIED;
    assert(tv4_ota_memory_release(&l)==TV4_MEMORY_OK && f.frees==1);empty(&l,&f);
    for(int state=TV4_SESSION_IDLE;state<=TV4_SESSION_CANCELLED;state++) {
        if(state>=TV4_SESSION_MANIFEST && state<=TV4_SESSION_FINISHING)continue;
        f=healthy();assert(acquire(&l,&f,policy)==TV4_MEMORY_OK);
        l.session->result.state=(tv4_session_state)state;
        assert(tv4_ota_hash_open(&l.session->image.whole_hash));
        assert(tv4_ota_hash_open(&l.session->image.body_hash));
        assert(tv4_ota_memory_release(&l)==TV4_MEMORY_OK && f.frees==1);empty(&l,&f);
    }
    for(unsigned fail=1;fail<=2;fail++) {
        f=healthy();f.fail_snapshot=fail;assert(acquire(&l,&f,policy)==TV4_MEMORY_UNAVAILABLE);empty(&l,&f);
        assert(f.allocations==(fail==2) && f.frees==(fail==2));
    }
    f=healthy();f.fail_alloc=true;assert(acquire(&l,&f,policy)==TV4_MEMORY_ALLOCATION);empty(&l,&f);
    f=healthy();f.wrong_region=true;assert(acquire(&l,&f,policy)==TV4_MEMORY_WRONG_REGION && f.frees==1);empty(&l,&f);
    f=healthy();f.wrong_end=true;assert(acquire(&l,&f,policy)==TV4_MEMORY_WRONG_REGION && f.frees==1);empty(&l,&f);
    f=healthy();f.deplete=true;assert(acquire(&l,&f,policy)==TV4_MEMORY_HEADROOM && f.frees==1);empty(&l,&f);
    f=healthy();f.heap.internal_free=1;assert(acquire(&l,&f,policy)==TV4_MEMORY_HEADROOM && !f.allocations);
    f=healthy();f.heap.internal_largest=12288;assert(acquire(&l,&f,policy)==TV4_MEMORY_HEADROOM && !f.allocations);
    f=healthy();f.heap.psram_free=policy.psram_free_floor+sizeof(tv4_ota_session)-1;
    assert(acquire(&l,&f,policy)==TV4_MEMORY_PSRAM && !f.allocations);
    f=healthy();f.heap.psram_largest=sizeof(tv4_ota_session)-1;assert(acquire(&l,&f,policy)==TV4_MEMORY_PSRAM && !f.allocations);
    tv4_ota_memory_policy p=policy;p.psram_free_floor=SIZE_MAX;
    f=healthy();assert(acquire(&l,&f,p)==TV4_MEMORY_PSRAM && !f.allocations);
    p=policy;p.reviewed=false;f=healthy();assert(acquire(&l,&f,p)==TV4_MEMORY_NOT_REVIEWED && !f.snapshots);
    p=policy;p.internal_free_floor=0;assert(acquire(&l,&f,p)==TV4_MEMORY_ARGUMENT);
    p=policy;p.internal_largest_floor=p.internal_free_floor+1;assert(acquire(&l,&f,p)==TV4_MEMORY_ARGUMENT);
    assert(tv4_ota_memory_acquire(&l,&policy,NULL)==TV4_MEMORY_ARGUMENT);
    assert(tv4_ota_memory_acquire_psram(&l,&policy)==TV4_MEMORY_UNAVAILABLE);empty(&l,&f);
    /* Repeated retries cannot leak the bounded workspace or crypto contexts. */
    for(unsigned i=0;i<100;i++) {
        f=healthy();assert(acquire(&l,&f,policy)==TV4_MEMORY_OK);
        assert(tv4_ota_memory_release(&l)==TV4_MEMORY_OK && f.frees==1);
    }
    printf("PSRAM-only admission/low-memory/race/region/cleanup/retry PASS; host workspace=%u bytes\n",tv4_ota_workspace_bytes);
}
